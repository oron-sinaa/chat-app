cmake_minimum_required(VERSION 3.14)
project(ChatApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(chat_app
    server.cpp
)

target_include_directories(chat_app PRIVATE
    /usr/local/include
    /usr/local/include/uWebSockets
)

target_link_libraries(chat_app
    /usr/local/lib/uSockets.a
    ssl
    crypto
    z
    pthread
)
