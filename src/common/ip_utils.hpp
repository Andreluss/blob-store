//
// Created by mjacniacki on 16.01.25.
//

#include <array>
#include <stdexcept>
#include <string>
#include <common.pb.h>

inline common::ipv4Address parseIPv4WithPort(const std::string& ipPortStr) {
    common::ipv4Address result;
    size_t colonPos = ipPortStr.find(':');

    if (colonPos == std::string::npos) {
        throw std::invalid_argument("No port number found in input string");
    }

    std::string ipStr = ipPortStr.substr(0, colonPos);
    std::string portStr = ipPortStr.substr(colonPos + 1);

    // Parse port
    try {
        uint32_t port = std::stoul(portStr);
        if (port > 65535) {
            throw std::out_of_range("Port number must be between 0 and 65535");
        }
        result.set_port(port);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid port number");
    }

    // Parse IP address
    std::array<uint8_t, 4> octets{0};
    size_t start = 0;
    size_t pos = 0;
    int octetIndex = 0;

    while (start < ipStr.length() && octetIndex < 4) {
        pos = ipStr.find('.', start);
        if (pos == std::string::npos) {
            pos = ipStr.length();
        }

        std::string octetStr = ipStr.substr(start, pos - start);
        try {
            long value = std::stol(octetStr);
            if (value < 0 || value > 255) {
                throw std::out_of_range("Octet must be between 0 and 255");
            }
            octets[octetIndex++] = static_cast<uint8_t>(value);
        } catch (const std::exception&) {
            throw std::invalid_argument("Invalid IP address format");
        }

        start = pos + 1;
    }

    if (octetIndex != 4 || start <= ipStr.length()) {
        throw std::invalid_argument("Invalid IP address format");
    }

    // Combine octets into uint32_t
    result.set_address((octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3]);

    return result;
}

inline std::string formatIPv4WithPort(const common::ipv4Address& addr) {
    if (addr.port() > 65535) {
        throw std::invalid_argument("Port number must be between 0 and 65535");
    }

    // Extract octets from address
    uint8_t octet1 = (addr.address() >> 24) & 0xFF;
    uint8_t octet2 = (addr.address() >> 16) & 0xFF;
    uint8_t octet3 = (addr.address() >> 8) & 0xFF;
    uint8_t octet4 = addr.address() & 0xFF;

    return std::to_string(octet1) + "." +
           std::to_string(octet2) + "." +
           std::to_string(octet3) + "." +
           std::to_string(octet4) + ":" +
           std::to_string(addr.port());
}
