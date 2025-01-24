#include "frontend_service.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <grpc++/grpc++.h>
#include "utils.hpp"
#include "environment.hpp"

void run_frontend()
{
    const std::string container_port = "50042";
    const std::string server_address = "0.0.0.0" + container_port;
    const std::string master_address =
        get_env_var(ENV_MASTER_SERVICE).value_or("master-service");

    const auto master_channel =
        grpc::CreateChannel(master_address,grpc::InsecureChannelCredentials());

    FrontendServiceImpl frontend_service(master_channel);

    const auto server =
        grpc::ServerBuilder()
        .AddListeningPort(server_address, grpc::InsecureServerCredentials())
        .RegisterService(&frontend_service)
        .BuildAndStart();

    std::cout << "Frontend service is running on port " << container_port << ", " << std::endl <<
                 "master accessed at " << master_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    run_frontend();

    return 0;
}
