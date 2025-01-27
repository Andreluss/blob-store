//
// Created by mjacniacki on 04.01.25.
//

#include "worker_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "services/frontend_service.grpc.pb.h"
#include <iostream>
#include "environment.hpp"

using namespace std;

void run_worker(const WorkerConfig& config)
{
    const std::string container_port = std::to_string(config.container_port);
    const std::string master_service_address = config.master_service;
    const std::string worker_service_address = config.my_service_address;
    std::cout << "This worker service address: " << worker_service_address << std::endl;
    std::cout << "Master service address: " << master_service_address << std::endl;

    const std::string server_address("0.0.0.0:" + container_port);
    const auto master_channel = grpc::CreateChannel(master_service_address,
                                              grpc::InsecureChannelCredentials());
    WorkerServiceImpl worker_service(master_channel);

    const auto server =
        grpc::ServerBuilder()
            .AddListeningPort(server_address, grpc::InsecureServerCredentials())
            .RegisterService(&worker_service)
            .BuildAndStart();

    std::cout << "Worker service is running with container port " << container_port << std::endl;
    server->Wait();
}

int main() {
    run_worker(WorkerConfig::LoadFromEnv());
}