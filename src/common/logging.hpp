#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

static auto operator<< (auto& out, auto x) -> decltype(x.end(),out) {
    out << "{";
    for (int i = 0; auto e : x)
        out << (i++ ? ", " : "") << e;
    out << "}";
    return out;
}

class Logger {
    struct Color {
        static constexpr const char* Red = "\033[31m";
        static constexpr const char* Green = "\033[32m";
        static constexpr const char* Yellow = "\033[33m";
        static constexpr const char* Blue = "\033[34m";
        static constexpr const char* Magenta = "\033[35m";
        static constexpr const char* Cyan = "\033[36m";
        static constexpr const char* Reset = "\033[0m";
    };

    // Function that returns the string with time in a format like this: 2024-04-25T18:21:00.010 (with parts of seconds)
    // https://gist.github.com/bschlinker/844a88c09dcf7a61f6a8df1e52af7730
    static std::string getCurrentTime() {
        const auto now = std::chrono::system_clock::now();
        const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
        std::stringstream nowSs;
        nowSs << std::put_time(std::localtime(&nowAsTimeT), "%FT%T")
              << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
        return nowSs.str();
    }

    template<typename... Args>
    static std::string concatenateArgs(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << args); // Fold expression (C++17)
        return oss.str();
    }

    static void _print(const std::string& color, const std::string& type, const std::string& message) {
        std::cerr << getCurrentTime() << " " << color << "[" << type << "]"
                  << Color::Reset << " " << message << std::endl;
    }

public:
    template<typename... Args>
    static void debug(Args&&... args) {
        _print(Color::Cyan, "Debug", concatenateArgs(std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void info(Args&&... args) {
        _print(Color::Green, "Info", concatenateArgs(std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void error(Args&&... args) {
        _print(Color::Red, "Error", concatenateArgs(std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void warn(Args&&... args) {
        _print(Color::Yellow, "Warn", concatenateArgs(std::forward<Args>(args)...));
    }
};
