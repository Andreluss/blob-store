//
// Created by mjacniacki on 04.01.25.
//

#include "worker_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "services/frontend_service.grpc.pb.h"
#include <iostream>
#include <thread>

#include "environment.hpp"
#include "logging.hpp"

using namespace std;

void run_worker(const WorkerConfig& config)
{
    const std::string container_port = std::to_string(config.container_port);
    const std::string master_service_address = config.master_service;
    const std::string worker_service_address = config.my_service_address;
    Logger::info("My service address: ", worker_service_address);
    Logger::info("There are ", config.masters_count, " master services");
    Logger::info("My master service address: ", master_service_address);

    const std::string server_address("0.0.0.0:" + container_port);
    const auto master_channel = grpc::CreateChannel(master_service_address,
                                              grpc::InsecureChannelCredentials());
    master::RegisterWorkerRequest register_worker_request = master::RegisterWorkerRequest();

    std::filesystem::create_directories(BLOBS_PATH);
    // Get free storage
    register_worker_request.set_address(worker_service_address);
    auto status = get_free_storage()
                  .and_then([&](uint64_t storage) -> Expected<std::monostate, grpc::Status> {
                      register_worker_request.set_space_available(storage);
                      return std::monostate{};
                  })
                  .output<grpc::Status>(
                          [](auto _) { return grpc::Status::OK; },
                          std::identity()
                  );
    if (not status.ok()) {
       Logger::error("Cannot get free storage");
        exit(1);
    }

    // Register at master service
    int tries = 10;
    while (tries--) {
        grpc::ClientContext client_context;
        master::RegisterWorkerResponse register_worker_response;
        auto master_stub = master::MasterService::NewStub(master_channel);
        status = master_stub->RegisterWorker(&client_context, register_worker_request, &register_worker_response);
        if (status.ok()) {
            break;
        }
        Logger::warn("Error during registration, trying again after 10 seconds");
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    if (tries == 0) {
        Logger::error("Couldn't register at master service. Is the master service running?");
    }

    WorkerServiceImpl worker_service(master_channel, worker_service_address);

    // Start server
    const auto server =
        grpc::ServerBuilder()
            .AddListeningPort(server_address, grpc::InsecureServerCredentials())
            .RegisterService(&worker_service)
            .BuildAndStart();

    Logger::info("Worker service is running with container port ", container_port);
    server->Wait();
}

int main() {
    run_worker(WorkerConfig::LoadFromEnv());
}