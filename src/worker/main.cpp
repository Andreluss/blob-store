//
// Created by mjacniacki on 04.01.25.
//

#include "worker_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "services/frontend_service.grpc.pb.h"
#include "environment.hpp"
#include <iostream>

using namespace std;

void run_worker()
{
    const std::string server_address("0.0.0.0:50042");
    const std::string master_address =
        get_env_var(ENV_MASTER_SERVICE)
        .value_or("master-service-0.master-service");

    auto master_channel = grpc::CreateChannel(master_address,
                                              grpc::InsecureChannelCredentials());

    WorkerServiceImpl worker_service(master_channel);

    const auto server =
        grpc::ServerBuilder()
            .AddListeningPort(server_address, grpc::InsecureServerCredentials())
            .RegisterService(&worker_service)
            .BuildAndStart();

    std::cout << "Worker service is running on " << server_address << std::endl;
    server->Wait();
}

int main() {
    run_worker();
}