#pragma once
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

inline std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << ms.count() << "Z";
    return oss.str();
}

#define LOG_INFO(msg)  std::cout << "[" << current_timestamp() << "][INFO] " << msg << std::endl
#define LOG_WARN(msg)  std::cerr << "[" << current_timestamp() << "][WARN] " << msg << std::endl
#define LOG_DEBUG(msg) std::cout << "[" << current_timestamp() << "][DEBUG] " << msg << std::endl
