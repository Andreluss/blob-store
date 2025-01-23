#pragma once

#include "config.hpp"
#include "common.grpc.pb.h"
#include <fstream>

// TODO-soon: move to (network_)utils.hpp in common/
inline std::string address_to_string(const uint32_t ip, const uint32_t port) {
    std::stringstream ss;
    for (int byte = 3; byte >= 0; --byte)
    {
        const auto num = (ip >> (8 * byte)) & 0xff;
        ss << num;
        if (byte > 0)
        {
            ss << ".";
        }
    }
    ss << ":" << port;
    return ss.str();
}

inline std::string address_to_string(const common::ipv4Address& address) {
    return address_to_string(address.address(), address.port());
}

namespace fs = std::filesystem;

