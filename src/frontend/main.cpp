#include "frontend_service.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>
#include "network_utils.hpp"
#include "environment.hpp"
#include "logging.hpp"

void run_frontend(const FrontendConfig& config)
{
    const std::string container_port = std::to_string(config.container_port);
    const std::string server_address = "0.0.0.0:" + container_port;

    FrontendServiceImpl frontend_service(config.masters_count);

    const auto server =
        grpc::ServerBuilder()
        .AddListeningPort(server_address, grpc::InsecureServerCredentials())
        .RegisterService(&frontend_service)
        .BuildAndStart();

    Logger::info("Frontend service is running on ", server_address);
    Logger::info("There are ", config.masters_count, " masters. ");
    server->Wait();
}

int main() {
    const auto config = FrontendConfig::LoadFromEnv();
    run_frontend(config);

    return 0;
}
