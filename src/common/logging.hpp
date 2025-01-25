#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <iomanip>


// ----------------------------------- Common classes -----------------------------------

struct Color {
    static constexpr const char* Red = "\033[31m";
    static constexpr const char* Green = "\033[32m";
    static constexpr const char* Yellow = "\033[33m";
    static constexpr const char* Blue = "\033[34m";
    static constexpr const char* Magenta = "\033[35m";
    static constexpr const char* Cyan = "\033[36m";
    static constexpr const char* Reset = "\033[0m";
};

class Logger {
private:
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
public:
    static void debug(std::string color, std::string message) {
        std::cerr << color << message << Color::Reset << std::endl << std::flush;
    }
    static void info(const char* color, std::string message) {
        std::cerr << color << message << Color::Reset << std::endl;
    }
    static void error(std::string message) {
        std::cerr << Color::Red << "[Error] " << Color::Reset << message << std::endl;
    }
    static void warn(std::string message) {
        std::cerr << Color::Yellow << "[Warning] " << Color::Reset << message << std::endl;
    }

    static void report(const std::string &senderIpPort, const std::string &receiverIpPort,
                       const std::string &time, const std::string &message) {
        if (BlackLadyDebug) std::cerr << std::flush;
        std::cout << "[" << senderIpPort << "," << receiverIpPort << "," << time << "] " << message << std::flush; // << std::endl;
        if (BlackLadyDebug) std::cerr << std::flush;
    }
};
