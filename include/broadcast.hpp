#pragma once
#include "types.hpp"
#include "logger.hpp"

inline void broadcast(const std::string &channel,
                      const std::string &room,
                      nlohmann::json msg,
                      uWS::WebSocket<false, true, PerSocketData>* exclude = nullptr)
{
    msg["timestamp"] = current_timestamp();
    std::string dump = msg.dump();

    auto &conns = room_map[channel][room];
    for (auto* ws : conns) {
        if (ws != exclude) {
            auto* ud = ws->getUserData();
            msg["user_id"] = ud->user_id;
            ws->send(dump, uWS::OpCode::TEXT);
        }
    }
}
