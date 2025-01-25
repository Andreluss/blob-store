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

void run_worker()
{
    const std::string server_address("0.0.0.0:50042");
    const std::string master_address =
        get_env_var(ENV_MASTER_SERVICE)
        .value_or("master-service-0.master-service");
    const std::string worker_hostname = [&]()
    {
        if (auto hostname = get_env_var(ENV_HOSTNAME_SELF); hostname)
            return hostname;
        const auto msg = std::string("Worker hostname not set - env ") + ENV_HOSTNAME_SELF + " doesn't exist."
        throw std::runtime_error(msg);
    };
    std::cout << "Worker hostname: " << worker_hostname << std::endl;


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