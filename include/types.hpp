#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <uWebSockets/App.h>
#include <nlohmann/json.hpp>

constexpr char ENDPOINT[] = "/chat";
constexpr int DEFAULT_PORT = 9003;
constexpr size_t MAX_HISTORY = 50;

struct PerSocketData {
    std::string user_id;
    std::string channel_id;
    std::string room_id;
    size_t rate_limit_nacks = 0;
};

// Global state
extern std::unordered_map<
    std::string,
    std::unordered_map<
        std::string,
        std::unordered_set<uWS::WebSocket<false, true, PerSocketData>*>
    >
> room_map;

extern std::unordered_map<
    std::string,
    std::unordered_map<
        std::string,
        std::deque<nlohmann::json>>
> message_queues;

extern uWS::App* global_app;
