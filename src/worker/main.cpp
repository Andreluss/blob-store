//
// Created by mjacniacki on 04.01.25.
//

#include <iostream>
#include "worker_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

using namespace std;

int main() {
    const std::string server_address("0.0.0.0:50051");
    const std::string master_address = "127.0.0.1:50052";

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

    return 0;
}