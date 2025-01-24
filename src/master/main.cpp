#include "master_service_mock.hpp"
#include "echo_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>
#include <services/master_service.grpc.pb.h>

void run_echo()
{
    const std::string server_address("0.0.0.0:50051");
    EchoServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Master server listening on " << server_address << std::endl;
    server->Wait();
}

void run_mock()
{
    const std::string container_port = "50051";
    const std::string server_address("0.0.0.0:" + container_port);
    MasterServiceMockImpl service;

    const auto server =
        grpc::ServerBuilder()
        .AddListeningPort(server_address, grpc::InsecureServerCredentials())
        .RegisterService(&service)
        .BuildAndStart();

    std::cout << "Master Mock listening on port " << container_port << std::endl;
    server->Wait();
}

int main() {
    run_mock();
    return 0;
}
