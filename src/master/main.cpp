#include <environment.hpp>

#include "master_service_mock.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>
#include <services/master_service.grpc.pb.h>

#include "master_db_repository.hpp"
#include "master_service.hpp"
using namespace std::string_literals;

void run_echo()
{
    const std::string server_address("0.0.0.0:50042");
    EchoServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    Logger::info("Echo server listening on ", server_address);
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

    Logger::info("Master Mock (", config.ordinal, ") listening on port ", container_port);
    server->Wait();
}

void run_master(const MasterConfig& config)
{
    const std::string container_port = std::to_string(config.container_port);
    const std::string server_address("0.0.0.0:" + container_port);
    Logger::info("Master service address: ", server_address);
    MasterDbRepository db(
        config.project_id,
        config.spanner_instance_id,
        config.db_name
    );

    MasterServiceImpl master_service = MasterServiceImpl(&db);

    const auto server =
        grpc::ServerBuilder()
            .AddListeningPort(server_address, grpc::InsecureServerCredentials())
            .RegisterService(&master_service)
            .BuildAndStart();

    Logger::info("Master service is running with container port ", container_port);
    server->Wait();
}

int main() {
    run_master(MasterConfig::LoadFromEnv());
}
