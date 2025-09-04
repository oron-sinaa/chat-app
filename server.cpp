#include "types.hpp"
#include "handler.hpp"
#include "logger.hpp"
#include <csignal>
#include <cstdlib>

std::unordered_map<std::string,std::unordered_map<std::string,std::unordered_set<uWS::WebSocket<false,true,PerSocketData>*>>> room_map;
std::unordered_map<std::string,std::unordered_map<std::string,std::deque<nlohmann::json>>> message_queues;
uWS::App* global_app = nullptr;

void sigint_handler(int) {
    LOG_INFO("SIGINT received. Closing all connections...");
    for (auto &ch : room_map)
        for (auto &room : ch.second)
            for (auto* ws : room.second) ws->close();
    if (global_app) global_app->close();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigint_handler);

    int port = DEFAULT_PORT;
    if (argc > 1) { port = std::atoi(argv[1]); if (port <=0) { LOG_WARN("Invalid port argument. Using default"); port=DEFAULT_PORT; } }

    uWS::App app;
    global_app = &app;

    app.ws<PerSocketData>(ENDPOINT,{
        .open = [](auto* ws){ LOG_INFO("Connection opened"); },
        .message = [](auto* ws,std::string_view msg,uWS::OpCode){ handleMessage(ws,msg); },
        .close = [](auto* ws,int,std::string_view){
            auto* ud = ws->getUserData();
            if(!ud->room_id.empty()){
                auto& conns = room_map[ud->channel_id][ud->room_id];
                conns.erase(ws);
                nlohmann::json disconnect_msg{{"event","disconnected"},{"user_id",ud->user_id},{"channel_id",ud->channel_id},{"room_id",ud->room_id},{"timestamp",current_timestamp()}};
                broadcast(ud->channel_id,ud->room_id,disconnect_msg,ws);
                LOG_INFO("User " << ud->user_id << " disconnected");
            } else LOG_INFO("Connection closed (not in room)");
        }
    }).listen(port,[port](auto* sock){ 
        if(sock) LOG_INFO("Listening on ws://localhost:" << port << ENDPOINT);
        else LOG_WARN("Failed to listen on port " << port);
    }).run();
}
