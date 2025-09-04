#pragma once
#include "types.hpp"
#include "utils.hpp"
#include "broadcast.hpp"
#include "logger.hpp"
#include <deque>
#include <chrono>

inline void handleMessage(uWS::WebSocket<false, true, PerSocketData>* ws, std::string_view raw) {
    constexpr size_t MAX_CLIENT_MSG = 512;
    LOG_DEBUG("Raw message received: " << raw);

    if (raw.size() > MAX_CLIENT_MSG) {
        LOG_WARN("Message too large from connection");
        nlohmann::json nack{{"action", "send_nack"},{"reason","message_too_large"},{"timestamp",current_timestamp()}};
        ws->send(nack.dump(), uWS::OpCode::TEXT);
        return;
    }

    auto j = nlohmann::json::parse(raw, nullptr, false);
    if (j.is_discarded()) {
        LOG_WARN("Invalid JSON received");
        nlohmann::json nack{{"action","send_nack"},{"reason","invalid_json"},{"timestamp",current_timestamp()}};
        ws->send(nack.dump(), uWS::OpCode::TEXT);
        return;
    }

    auto* ud = ws->getUserData();
    std::string action = j.value("action", "");
    LOG_INFO("Action parsed: " << action);

    if (action == "send") {
        if (!validate_schema_strict(j, {{"action","string"},{"payload","string"}})) {
            LOG_WARN("Invalid send schema from user " << ud->user_id);
            nlohmann::json nack{{"action","send_nack"},{"reason","invalid_send_schema"},{"timestamp",current_timestamp()}};
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            return;
        }

        static std::unordered_map<std::string,std::deque<std::chrono::steady_clock::time_point>> user_message_times;
        constexpr size_t MAX_MESSAGES = 5;
        constexpr auto WINDOW = std::chrono::seconds(1);

        auto now = std::chrono::steady_clock::now();
        auto& times = user_message_times[ud->user_id];
        times.push_back(now);
        while (!times.empty() && now - times.front() > WINDOW) times.pop_front();

        if (times.size() > MAX_MESSAGES) {
            ud->rate_limit_nacks++;
            LOG_WARN("Rate limit exceeded for user " << ud->user_id << " (" << times.size() << " messages in " << WINDOW.count() << "s)");
            nlohmann::json nack{{"action","send_nack"},{"reason","rate_limited"},{"timestamp",current_timestamp()}};
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            if (ud->rate_limit_nacks > 20) {
                LOG_WARN("User " << ud->user_id << " disconnected due to excessive rate limiting");
                ws->close();
            }
            return;
        }

        if (ud->room_id.empty()) {
            LOG_WARN("User " << ud->user_id << " tried to send message but is not in a room");
            nlohmann::json nack{{"action","send_nack"},{"reason","not_in_room"},{"timestamp",current_timestamp()}};
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            return;
        }

        nlohmann::json bmsg{{"event","broadcast"},{"payload",j["payload"]},{"user_id",ud->user_id},{"channel_id",ud->channel_id},{"room_id",ud->room_id}};
        auto& q = message_queues[ud->channel_id][ud->room_id];
        q.push_back(bmsg);
        if (q.size() > MAX_HISTORY) q.pop_front();
        broadcast(ud->channel_id, ud->room_id, bmsg, ws);
        LOG_INFO("Message broadcasted from user=" << ud->user_id << " payload=" << j["payload"]);
    }
    else if (action == "disconnect") {
        LOG_INFO("Disconnect requested by user " << ud->user_id);
        ws->close();
    }
    else if (action == "join") {
        std::string uid = j["user_id"];
        std::string cid = j["channel_id"];
        std::string rid = j["room_id"];
        LOG_INFO("Join request: user=" << uid << " channel=" << cid << " room=" << rid);

        auto &room_conns = room_map[cid][rid];
        bool already_connected = false;
        for (auto *existing_ws : room_conns) {
            auto *existing_ud = existing_ws->getUserData();
            if (existing_ud->user_id == uid) { already_connected = true; break; }
        }

        if (already_connected) {
            LOG_WARN("Rejecting join: user " << uid << " already connected");
            nlohmann::json nack{{"action","join_nack"},{"reason","already_connected"},{"timestamp",current_timestamp()}};
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            ws->close();
            return;
        }

        ud->user_id = uid; ud->channel_id = cid; ud->room_id = rid;
        room_conns.insert(ws);

        nlohmann::json ack{{"action","join_ack"},{"channel_id",cid},{"room_id",rid},{"user_id",uid},{"messages",message_queues[cid][rid]},{"timestamp",current_timestamp()}};
        ws->send(ack.dump(), uWS::OpCode::TEXT);
        LOG_INFO("Join ACK sent to user " << uid);

        nlohmann::json notify{{"event","user_joined"},{"user_id",uid},{"channel_id",cid},{"room_id",rid}};
        broadcast(cid,rid,notify,ws);
        LOG_INFO("User " << uid << " joined successfully");
    }
    else {
        LOG_WARN("Unknown action received: " << action);
        nlohmann::json nack{{"action","send_nack"},{"reason","unknown_action"},{"timestamp",current_timestamp()}};
        ws->send(nack.dump(), uWS::OpCode::TEXT);
    }
}
