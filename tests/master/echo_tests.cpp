//
// Created by mjacniacki on 06.01.25.
//
#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "services/echo_service.grpc.pb.h"
#include "echo_service.hpp"

class EchoServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start the Echo server
        std::string server_address("localhost:50051");
        service_ = std::make_unique<EchoServiceImpl>();

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(service_.get());
        server_ = builder.BuildAndStart();

        channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub_ = echo::EchoService::NewStub(channel_);
    }

    void TearDown() override {
        server_->Shutdown();
        server_->Wait();
    }

    std::unique_ptr<EchoServiceImpl> service_;
    std::unique_ptr<grpc::Server> server_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<echo::EchoService::Stub> stub_;
};

// Passing Test: Simple Echo RPC
TEST_F(EchoServiceTest, SimpleEcho) {
    echo::EchoRequest request;
    request.set_message("Hello, Echo!");

    echo::EchoResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub_->Echo(&context, request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.message(), "Hello, Echo!");
    EXPECT_FALSE(response.timestamp().empty());
}

// Failing Test
TEST_F(EchoServiceTest, EmptyEcho) {
    echo::EchoRequest request; // No message set
    echo::EchoResponse response;
    grpc::ClientContext context;
    request.set_message("Hello, Echo!");

    grpc::Status status = stub_->Echo(&context, request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.message(), ""); // Some stupid assertion
}

