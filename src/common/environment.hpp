#pragma once
#include <optional>
#include <string>

// Function to retrieve an environment variable as a std::optional<std::string>
static std::optional<std::string> get_env_var(const std::string& varName) {
    if (const char* value = std::getenv(varName.c_str())) {
        return std::string(value); // Convert to std::string
    }
    return std::nullopt; // Return empty optional if not found
}

constexpr static auto ENV_HOSTNAME_SELF = "HOSTNAME";
constexpr static auto ENV_MASTER_SERVICE = "MASTER_SERVICE";
