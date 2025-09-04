#include <uWebSockets/App.h>
#include <nlohmann/json.hpp>

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <string>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <csignal>

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

std::unordered_map<
    std::string,
    std::unordered_map<
        std::string,
        std::unordered_set<uWS::WebSocket<false, true, PerSocketData>*>
    >
> room_map;

std::unordered_map<
    std::string,
    std::unordered_map<
        std::string,
        std::deque<nlohmann::json>>
> message_queues;

uWS::App *global_app = nullptr; // for SIGINT shutdown

// ---- Helper: current timestamp ----
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << ms.count() << "Z";
    return oss.str();
}

// ---- Helper: broadcast ----
void broadcast(const std::string &channel,
               const std::string &room,
               nlohmann::json msg,
               uWS::WebSocket<false, true, PerSocketData> *exclude = nullptr)
{
    msg["timestamp"] = current_timestamp();
    std::string dump = msg.dump();

    auto &conns = room_map[channel][room];
    for (auto *ws : conns) {
        if (ws != exclude) {
            auto *ud = ws->getUserData();
            // Use stored user_id for outgoing messages
            msg["user_id"] = ud->user_id;
            ws->send(dump, uWS::OpCode::TEXT);
        }
    }
}

// ---- Handle incoming messages with strict schema validation ----
void handleMessage(uWS::WebSocket<false, true, PerSocketData> *ws,
                   std::string_view raw)
{
    constexpr size_t MAX_CLIENT_MSG = 512;

    // --- Validate message length ---
    if (raw.size() > MAX_CLIENT_MSG) {
        std::cerr << "[WARN] Message rejected: exceeds "
                  << MAX_CLIENT_MSG << " bytes (" << raw.size() << " bytes)\n";
        nlohmann::json nack = {
            {"action", "send_nack"},
            {"reason", "message_too_large"},
            {"timestamp", current_timestamp()}
        };
        ws->send(nack.dump(), uWS::OpCode::TEXT);
        return;
    }

    // --- Parse JSON ---
    auto j = nlohmann::json::parse(raw, nullptr, false);
    if (j.is_discarded()) {
        std::cerr << "[WARN] Invalid JSON\n";
        nlohmann::json nack = {
            {"action", "send_nack"},
            {"reason", "invalid_json"},
            {"timestamp", current_timestamp()}
        };
        ws->send(nack.dump(), uWS::OpCode::TEXT);
        return;
    }

    auto *ud = ws->getUserData();
    std::string action = j.value("action", "");

    // ---- Strict schema validation helper ----
    auto validate_schema_strict = [&](const std::initializer_list<std::pair<std::string, std::string>> &fields) -> bool {
        if (j.size() != fields.size()) return false;  // must have exactly required number of keys
        for (auto &[key, type] : fields) {
            if (!j.contains(key)) return false;
            if (type == "string" && !j[key].is_string()) return false;
            if (type == "object" && !j[key].is_object()) return false;
            if (type == "array" && !j[key].is_array()) return false;
        }
        return true;
    };

    // ---- Validate schema per action ----
    if (action == "join") {
        if (!validate_schema_strict({{"action", "string"}, {"user_id", "string"}, {"channel_id", "string"}, {"room_id", "string"}})) {
            nlohmann::json nack = {
                {"action", "send_nack"},
                {"reason", "invalid_join_schema"},
                {"timestamp", current_timestamp()}
            };
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            return;
        }

        ud->channel_id = j["channel_id"];
        ud->room_id    = j["room_id"];
        ud->user_id    = j["user_id"];

        room_map[ud->channel_id][ud->room_id].insert(ws);

        nlohmann::json ack = {
            {"action", "join_ack"},
            {"channel_id", ud->channel_id},
            {"room_id", ud->room_id},
            {"user_id", ud->user_id},
            {"messages", message_queues[ud->channel_id][ud->room_id]},
            {"timestamp", current_timestamp()}
        };
        ws->send(ack.dump(), uWS::OpCode::TEXT);

        nlohmann::json notify = {
            {"event", "user_joined"},
            {"user_id", ud->user_id},
            {"channel_id", ud->channel_id},
            {"room_id", ud->room_id}
        };
        broadcast(ud->channel_id, ud->room_id, notify, ws);

        std::cout << "[INFO] User " << ud->user_id
                  << " joined channel=" << ud->channel_id
                  << " room=" << ud->room_id << "\n";
    }
    else if (action == "send") {
        if (!validate_schema_strict({{"action", "string"}, {"payload", "string"}})) {
            nlohmann::json nack = {
                {"action", "send_nack"},
                {"reason", "invalid_send_schema"},
                {"timestamp", current_timestamp()}
            };
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            return;
        }

        if (ud->room_id.empty()) {
            nlohmann::json nack = {
                {"action", "send_nack"},
                {"reason", "not_in_room"},
                {"timestamp", current_timestamp()}
            };
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            return;
        }

        nlohmann::json bmsg = {
            {"event", "broadcast"},
            {"payload", j["payload"]},
            {"user_id", ud->user_id},
            {"channel_id", ud->channel_id},
            {"room_id", ud->room_id}
        };

        auto &q = message_queues[ud->channel_id][ud->room_id];
        q.push_back(bmsg);
        if (q.size() > MAX_HISTORY) q.pop_front();

        broadcast(ud->channel_id, ud->room_id, bmsg, ws);

        std::cout << "[INFO] Message from user=" << ud->user_id
                  << " channel=" << ud->channel_id
                  << " room=" << ud->room_id
                  << " payload=" << j["payload"] << "\n";
    }
    else if (action == "disconnect") {
        if (!validate_schema_strict({{"action", "string"}})) {
            nlohmann::json nack = {
                {"action", "send_nack"},
                {"reason", "invalid_disconnect_schema"},
                {"timestamp", current_timestamp()}
            };
            ws->send(nack.dump(), uWS::OpCode::TEXT);
            return;
        }
        ws->close();
    }
    else {
        nlohmann::json nack = {
            {"action", "send_nack"},
            {"reason", "unknown_action"},
            {"timestamp", current_timestamp()}
        };
        ws->send(nack.dump(), uWS::OpCode::TEXT);
    }
}

// ---- SIGINT handler ----
void sigint_handler(int) {
    std::cout << "\n[INFO] SIGINT received. Closing all connections...\n";

    for (auto &ch : room_map) {
        for (auto &room : ch.second) {
            for (auto *ws : room.second) {
                ws->close();
            }
        }
    }
    if (global_app) {
        global_app->close();
    }
}

// ---- Main ----
int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigint_handler);

    int port = 9003; // default
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0) {
            std::cerr << "[ERROR] Invalid port argument. Using default " << port << "\n";
            port = 9003;
        }
    }

    uWS::App app;
    global_app = &app;

    app.ws<PerSocketData>(ENDPOINT, {
        .open = [](auto *ws) {
            std::cout << "[INFO] Connection opened\n";
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
                    {"user_id", ud->user_id},
                    {"channel_id", ud->channel_id},
                    {"room_id", ud->room_id},
                    {"timestamp", current_timestamp()}
                };
                broadcast(ud->channel_id, ud->room_id, disconnect_msg, ws);

                std::cout << "[INFO] User " << ud->user_id
                          << " disconnected from channel=" << ud->channel_id
                          << " room=" << ud->room_id << "\n";
            } else {
                std::cout << "[INFO] Connection closed (not in room)\n";
            }
        }
    })
    .listen(port, [port](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "[INFO] Listening on ws://localhost:" << port << ENDPOINT << "\n";
        } else {
            std::cerr << "[ERROR] Failed to listen on port " << port << "\n";
        }
    })
    .run();
}
