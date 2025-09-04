#include <uWebSockets/App.h>
#include <nlohmann/json.hpp>

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <string>
#include <iostream>

constexpr char ENDPOINT[] = "/chat";
constexpr int PORT = 9003;

// ---- Per-connection data ----
struct PerSocketData {
    std::string user_id;
    std::string channel_id;
    std::string room_id;
};

// ---- Global state ----
constexpr size_t MAX_HISTORY = 50;

// room_map[channel][room] = set of active connections
std::unordered_map<
    std::string,
    std::unordered_map<
        std::string,
        std::unordered_set<uWS::WebSocket<false, true, PerSocketData> *>
    >
> room_map;

// message_queues[channel][room] = recent chat history
std::unordered_map<
    std::string,
    std::unordered_map<
        std::string,
        std::deque<nlohmann::json>>
> message_queues;

// ---- Helper: broadcast to room ----
void broadcast(const std::string &channel,
               const std::string &room,
               const nlohmann::json &msg,
               uWS::WebSocket<false, true, PerSocketData> *exclude = nullptr)
{
    auto &conns = room_map[channel][room];
    std::string dump = msg.dump();

    for (auto *ws : conns) {
        if (ws != exclude) {
            ws->send(dump, uWS::OpCode::TEXT);
        }
    }
}

// ---- Handle incoming messages ----
void handleMessage(uWS::WebSocket<false, true, PerSocketData> *ws,
                   std::string_view raw)
{
    auto j = nlohmann::json::parse(raw, nullptr, false);
    if (j.is_discarded()) {
        std::cerr << "Invalid JSON\n";
        return;
    }

    std::string action = j.value("action", "");

    // ---- Join room ----
    if (action == "join") {
        std::string channel = j["channel_id"];
        std::string room    = j["room_id"];
        std::string user    = j["user_id"];

        auto *ud = ws->getUserData();
        ud->user_id    = user;
        ud->channel_id = channel;
        ud->room_id    = room;

        room_map[channel][room].insert(ws);

        // Send join_ack with recent history
        nlohmann::json ack = {
            {"action", "join_ack"},
            {"channel_id", channel},
            {"room_id", room},
            {"user_id", user},
            {"messages", message_queues[channel][room]}
        };
        ws->send(ack.dump(), uWS::OpCode::TEXT);

        // Notify others in the room
        nlohmann::json notify = {
            {"event", "user_joined"},
            {"user_id", user},
            {"channel_id", channel},
            {"room_id", room}
        };
        broadcast(channel, room, notify, ws);
    }

    // ---- Send chat message ----
    else if (action == "send") {
        auto *ud = ws->getUserData();

        if (ud->room_id.empty()) {
            nlohmann::json nack = {
                {"action", "send_nack"},
                {"reason", "not_in_room"}
            };
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            return;
        }

        nlohmann::json bmsg = {
            {"event", "broadcast"},
            {"payload", j["payload"]},
            {"user_id", ud->user_id}
        };

        // Store in history
        auto &q = message_queues[ud->channel_id][ud->room_id];
        q.push_back(bmsg);
        if (q.size() > MAX_HISTORY) q.pop_front();

        // Broadcast to others
        broadcast(ud->channel_id, ud->room_id, bmsg, ws);
    }

    // ---- Disconnect (explicit) ----
    else if (action == "disconnect") {
        ws->close();
    }
}

int main() {

    uWS::App()
        .ws<PerSocketData>(ENDPOINT, {
            .open = [](auto *ws) {
                std::cout << "Connection opened\n";
            },

            .message = [](auto *ws, std::string_view msg, uWS::OpCode) {
                handleMessage(ws, msg);
            },

            .close = [](auto *ws, int /*code*/, std::string_view /*message*/) {
                auto *ud = ws->getUserData();

                if (!ud->room_id.empty()) {
                    auto &connections = room_map[ud->channel_id][ud->room_id];
                    connections.erase(ws);

                nlohmann::json disconnect_msg = {
                        {"event", "disconnected"},
                        {"user_id", ud->user_id}
                    };
                    broadcast(ud->channel_id, ud->room_id, disconnect_msg, ws);
                }

                std::cout << "Connection closed\n";
            }
        })

        .listen(PORT, [](auto *listen_socket) {
            if (listen_socket) {
                std::cout << "Listening on ws://localhost:" << PORT << ENDPOINT << "\n";
            }
        })

        .run();
}