#include "frontend_service.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>
#include "utils.hpp"
#include "logging.hpp"

int main(int argc, char** argv) {
    Logger::info(address_to_string(0x7f000001, 42), " = 127.0.0.1:42");
    const std::string server_address("0.0.0.0:50042");
    const std::string master_address = "127.0.0.1:50052"; // TODO: get this from env variable

    const auto master_channel =
        grpc::CreateChannel(master_address,grpc::InsecureChannelCredentials());

    FrontendServiceImpl frontend_service(master_channel);

    const auto server =
        grpc::ServerBuilder()
        .AddListeningPort(server_address, grpc::InsecureServerCredentials())
        .RegisterService(&frontend_service)
        .BuildAndStart();

    Logger::info("Frontend service is running on ", server_address);
    server->Wait();

    return 0;
}
