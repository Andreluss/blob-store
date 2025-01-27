#include <environment.hpp>

#include "master_service_mock.hpp"
#include "echo_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>
#include <services/master_service.grpc.pb.h>
#include "network_utils.hpp"
using namespace std::string_literals;

void run_echo()
{
    const std::string server_address("0.0.0.0:50042");
    EchoServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Master server listening on " << server_address << std::endl;
    server->Wait();
}

void run_mock(const MasterConfig& config)
{
    const std::string container_port = std::to_string(config.container_port);
    const std::string server_address("0.0.0.0:" + container_port);
    MasterServiceMockImpl service(config);

    const auto server =
        grpc::ServerBuilder()
        .AddListeningPort(server_address, grpc::InsecureServerCredentials())
        .RegisterService(&service)
        .BuildAndStart();

    std::cout << "Master Mock (" << config.ordinal << ") listening on port " << container_port << std::endl;
    std::cout << "My workers: " << std::endl;
    std::ranges::copy(config.workers, std::ostream_iterator<std::string>(std::cout, "\n"));
    std::cout << "---" << std::endl;
    server->Wait();
}

int main() {
    run_mock(MasterConfig::LoadFromEnv());
}
