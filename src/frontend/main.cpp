#include <iostream>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>
#include "services/echo_service.grpc.pb.h"
#include "frontend_service.hpp"

int main(int argc, char** argv) {
    std::cerr << address_to_string(0x7f000001, 42) << " = 127.0.0.1:42\n";
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

    std::cout << "Frontend service is running on " << server_address << std::endl;
    server->Wait();

    return 0;
}