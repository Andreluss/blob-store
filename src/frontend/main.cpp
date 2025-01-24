#include "frontend_service.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <grpc++/grpc++.h>
#include "utils.hpp"

void run_frontend()
{
    std::cout << "DUPABRAKADABRA" << std::endl;
    const std::string server_address("0.0.0.0:50042");
    const std::string master_address = "0.0.0.0:50052"; // TODO: get this from env variable

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
}

int main(int argc, char** argv) {
    run_frontend();

    return 0;
}
