//
// Created by mjacniacki on 04.01.25.
//

#include <iostream>
#include <thread>

#include "worker_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "services/frontend_service.grpc.pb.h"
#include "environment.hpp"

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

[[noreturn]] void run_frontend_ping()
{
    const std::string frontend_load_balancer_address {"34.118.97.38:50042"};
    while (true) {
        auto channel = grpc::CreateChannel(frontend_load_balancer_address, grpc::InsecureChannelCredentials());
        const auto frontend_stub = frontend::Frontend::NewStub(std::move(channel));

        // wait 1 second on current thread
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Requesting HealthCheck...\n";

        grpc::ClientContext client_ctx;
        const frontend::HealthcheckRequest request;
        frontend::HealthcheckResponse response;

        if (const auto status = frontend_stub->HealthCheck(&client_ctx, request, &response); status.ok()) {
            std::cout << status.ok() << " | Frontend Ping OK " << std::endl;
        } else {
            std::cout << status.error_code() << " " << status.error_message() << std::endl;
        }
    }
}

int main() {
    run_worker();
}