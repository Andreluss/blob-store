#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "services/worker_service.grpc.pb.h"
#include "worker_service.hpp"


class WorkerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string server_address("localhost:50051");
        const std::string master_address = "127.0.0.1:50052";
        auto master_channel = grpc::CreateChannel(master_address,
                                                  grpc::InsecureChannelCredentials());

        service_ = std::make_unique<WorkerServiceImpl>(master_channel);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(service_.get());
        server_ = builder.BuildAndStart();

        channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub_ = worker::WorkerService::NewStub(channel_);
    }

    void TearDown() override {
        server_->Shutdown();
        server_->Wait();
    }

    std::unique_ptr<WorkerServiceImpl> service_;
    std::unique_ptr<grpc::Server> server_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<worker::WorkerService::Stub> stub_;
};

/// Healthcheck RPC
TEST_F(WorkerServiceTest, SimpleEcho) {
    worker::HealthcheckRequest request;

    worker::HealthcheckResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub_->Healthcheck(&context, request, &response);

    EXPECT_TRUE(status.ok());
}


