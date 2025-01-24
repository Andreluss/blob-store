#include "client.hpp"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "services/frontend_service.grpc.pb.h"
#include "environment.hpp"
#include <iostream>
#include <string>

[[noreturn]] void run_frontend_ping(const std::string frontend_load_balancer_address)
{
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
    run_frontend_ping("34.118.97.38:50042");
}