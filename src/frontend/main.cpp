#include <iostream>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>
#include "services/echo_service.grpc.pb.h"
#include "frontend_service.hpp"

class EchoClient {
public:
    EchoClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(echo::EchoService::NewStub(channel)) {}

    // Simple echo RPC
    void Echo(const std::string& message) {
        echo::EchoRequest request;
        request.set_message(message);

        echo::EchoResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub_->Echo(&context, request, &response);

        if (status.ok()) {
            std::cout << "Echo response: " << response.message() << std::endl;
            std::cout << "Timestamp: " << response.timestamp();
        } else {
            std::cout << "RPC failed: " << status.error_message() << std::endl;
        }
    }

    // Server streaming echo RPC
    void ServerStreamingEcho(const std::string& message) {
        echo::EchoRequest request;
        request.set_message(message);

        grpc::ClientContext context;
        std::unique_ptr<grpc::ClientReader<echo::EchoResponse>> reader(
            stub_->ServerStreamingEcho(&context, request));

        echo::EchoResponse response;
        while (reader->Read(&response)) {
            std::cout << "Received: " << response.message() << std::endl;
            std::cout << "Timestamp: " << response.timestamp();
        }

        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cout << "ServerStreamingEcho rpc failed." << std::endl;
        }
    }

private:
    std::unique_ptr<echo::EchoService::Stub> stub_;
};

[[maybe_unused]] void run_echo_client()
{
    EchoClient client(
        grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials())
    );

    std::string message;
    while (true) {
        std::cout << "\nEnter message (or 'quit' to exit, 'stream' for streaming): ";
        std::getline(std::cin, message);

        if (message == "quit") {
            break;
        }
        else if (message == "stream") {
            std::cout << "Enter message for streaming: ";
            std::getline(std::cin, message);
            client.ServerStreamingEcho(message);
        }
        else {
            client.Echo(message);
        }
    }
}

int main(int argc, char** argv) {
    const std::string server_address("0.0.0.0:50042");
    const std::string master_address = "127.0.0.1:50052"; // TODO: get this from env variable

    const auto master_channel =
        grpc::CreateChannel(master_address,grpc::InsecureChannelCredentials());

    FrontendServiceImpl frontend_service(master_channel);

    const auto server =
        grpc::ServerBuilder()
        .AddListeningPort(server_address, grpc::InsecureServerCredentials())
        .RegisterService(&frontend_service)
        .BuildAndStart();

    std::cout << "Frontend service is running on " << server_address << std::endl;
    server->Wait();

    return 0;
}