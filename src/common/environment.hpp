#pragma once
#include <optional>
#include <string>
#include <stdexcept>
#include <vector>

// Function to retrieve an environment variable as a std::optional<std::string>
static std::optional<std::string> get_env_var_opt(const std::string& varName) {
    if (const char* value = std::getenv(varName.c_str())) {
        return std::string(value); // Convert to std::string
    }
    return std::nullopt; // Return empty optional if not found
}

// Helper function for fetching and logging missing environment variables
static std::string get_env_var_exn(const std::string& key) {
    auto value = get_env_var_opt(key);
    if (!value) {
        throw std::runtime_error("Missing environment variable: " + key);
    }
    return value.value();
}

constexpr static auto ENV_CONTAINER_PORT = "CONTAINER_PORT";
constexpr static auto ENV_HOSTNAME_SELF = "HOSTNAME";
constexpr static auto ENV_MASTER_SERVICE = "MASTER_SERVICE";
constexpr static auto ENV_WORKERS_PER_MASTER = "WORKERS_PER_MASTER";
constexpr static auto ENV_PROJECT_ID = "PROJECT_ID";
constexpr static auto ENV_SPANNER_INSTANCE_ID = "SPANNER_INSTANCE_ID";
constexpr static auto ENV_DB_NAME = "DB_NAME";

using ServiceAddress = std::string;

/// blabla-42 -> 42
/// test-22 -> 42
static int get_ordinal_from_hostname (const std::string& hostname) {
    const auto pos = hostname.find('-');
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid hostname: " + hostname);
    }
    return std::stoi(hostname.substr(pos + 1));
}


struct MasterConfig
{
    uint16_t container_port;
    /// ordinal = master idx (0, 1, ...).  <br>
    /// i-th master manages workers [i * workers_per_master, ..., (i+1) * workers_per_master - 1]
    int ordinal;
    std::vector<ServiceAddress> workers;
    std::string project_id;
    std::string spanner_instance_id;
    std::string db_name;

    static MasterConfig LoadFromEnv() {
        uint16_t container_port = std::stoi(get_env_var_exn(ENV_CONTAINER_PORT));

        const auto ordinal = []{
            const auto hostname = get_env_var_exn(ENV_HOSTNAME_SELF);
            return get_ordinal_from_hostname(hostname);
        }();

        const auto workers = [container_port, ordinal]{
            std::vector<ServiceAddress> workers_;
            const auto count = std::stoi(get_env_var_exn(ENV_WORKERS_PER_MASTER));
            std::generate_n(std::back_inserter(workers_), count, [container_port, c=ordinal]() mutable {
                return "worker-" + std::to_string(c++) + ".worker-service:" + std::to_string(container_port);
            });
            return workers_;
        }();

        std::string project_id = get_env_var_exn(ENV_PROJECT_ID);
        std::string spanner_instance_id = get_env_var_exn(ENV_SPANNER_INSTANCE_ID);
        std::string db_name = get_env_var_exn(ENV_DB_NAME);

        return {container_port, ordinal, workers, project_id, spanner_instance_id, db_name};
    }
};

struct FrontendConfig
{
    ServiceAddress master_service {};
    uint16_t container_port {};

    static FrontendConfig LoadFromEnv() {
        FrontendConfig config;
        config.master_service = get_env_var_exn(ENV_MASTER_SERVICE);
        config.container_port = std::stoi(get_env_var_exn(ENV_CONTAINER_PORT));
        return config;
    }
private:
    FrontendConfig() = default;
};

struct WorkerConfig
{
    ServiceAddress master_service {};
    uint16_t container_port {};
    ServiceAddress my_service_address{};

    static WorkerConfig LoadFromEnv() {
        WorkerConfig config;

        config.master_service = get_env_var_exn(ENV_MASTER_SERVICE);
        config.container_port = std::stoi(get_env_var_exn(ENV_CONTAINER_PORT));

        const auto hostname = get_env_var_exn(ENV_HOSTNAME_SELF);
        config.my_service_address =
            hostname + ".worker-service:" + std::to_string(config.container_port);

        return config;
    }
private:
    WorkerConfig() = default;
};
