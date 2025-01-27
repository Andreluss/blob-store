#pragma once

#include "common.grpc.pb.h"
#include <fstream>

// TODO-soon: move to (network_)utils.hpp in common/
inline std::string address_to_string(const uint32_t ip, const uint32_t port) {
    std::stringstream ss;
    for (int byte = 3; byte >= 0; --byte) {
        const auto num = (ip >> (8 * byte)) & 0xff;
        ss << num;
        if (byte > 0) {
            ss << ".";
        }
    }
    ss << ":" << port;
    return ss.str();
}

inline std::string address_to_string(const common::ipv4Address& address) {
    return address_to_string(address.address(), address.port());
}

inline common::ipv4Address address_of_string(const std::string& address) {
    common::ipv4Address addr;
    auto [ip, port] = std::pair<uint32_t, uint32_t>{};
    std::istringstream ss(address);
    for (int byte = 3; byte >= 0; --byte) {
        std::string num;
        if (byte > 0) {
            std::getline(ss, num, '.');
        } else
        {
            std::getline(ss, num, ':');
        }
        ip |= std::stoi(num) << (8 * byte);
    }
    std::string port_str;
    std::getline(ss, port_str, '\n');
    port = std::stoi(port_str);
    addr.set_address(ip);
    addr.set_port(port);
    return addr;
}