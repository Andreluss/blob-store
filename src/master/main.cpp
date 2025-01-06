#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <ctime>
#include <thread>
#include <grpc++/grpc++.h>
#include "services/echo_service.grpc.pb.h"

class EchoServiceImpl final : public echo::EchoService::Service {
public:
    grpc::Status Echo(grpc::ServerContext* context,
                     const echo::EchoRequest* request,
                     echo::EchoResponse* response) override {

        std::cout << "Received: " << request->message() << std::endl;

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        // Set response
        response->set_message(request->message());
        response->set_timestamp(std::ctime(&time));

        return grpc::Status::OK;
    }

    grpc::Status ServerStreamingEcho(grpc::ServerContext* context,
                                   const echo::EchoRequest* request,
                                   grpc::ServerWriter<echo::EchoResponse>* writer) override {

        echo::EchoResponse response;

        // Send 5 responses with increasing delays
        for (int i = 0; i < 5; i++) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);

            response.set_message(request->message() + " [" + std::to_string(i + 1) + "/5]");
            response.set_timestamp(std::ctime(&time));

            writer->Write(response);

            // Add small delay between messages
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        return grpc::Status::OK;
    }
};

int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50051");
    EchoServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
    return 0;
}