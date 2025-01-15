//
// Created by mjacniacki on 04.01.25.
//

#include <iostream>
#include "worker_service.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

using namespace std;

int main() {
    std::string server_address("0.0.0.0:50051");
    WorkerServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();

    return 0;
}