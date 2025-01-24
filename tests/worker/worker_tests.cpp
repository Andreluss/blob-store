#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "services/worker_service.grpc.pb.h"
#include "worker_service.hpp"
#include "blob_hasher.hpp"
#include "blob_file.hpp"

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
TEST_F(WorkerServiceTest, Healthcheck) {
    worker::HealthcheckRequest request;

    worker::HealthcheckResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub_->Healthcheck(&context, request, &response);

    EXPECT_TRUE(status.ok());
}

TEST_F(WorkerServiceTest, GetFreeStorage) {
    std::filesystem::create_directory(BLOBS_PATH);

    worker::GetFreeStorageRequest request;
    worker::GetFreeStorageResponse response;

    grpc::ClientContext context;
    grpc::Status status = stub_->GetFreeStorage(&context, request, &response);

    EXPECT_TRUE(status.ok());
    std::cout << "Free storage: " << response.storage() << std::endl;
    EXPECT_GT(response.storage(), 0);
}

TEST_F(WorkerServiceTest, SaveBlob) {
    worker::SaveBlobRequest request;
    worker::SaveBlobResponse response;

    grpc::ClientContext context;
    auto writer = stub_->SaveBlob(&context, &response);

    std::string blob = "Hello, World!";

    auto hasher = BlobHasher();
    hasher.add_chunk(blob);
    auto hash = hasher.finalize();

    request.set_hash(hash);
    request.set_chunk_data(blob);

    std::cout << request.hash() << std::endl;
    std::cout << request.chunk_data() << std::endl;

    writer->Write(request);
    writer->WritesDone();
    grpc::Status status = writer->Finish();

    BlobFile blob_file = BlobFile::Load(hash);
    EXPECT_EQ(blob_file.size(), blob.size());

    std::string written_blob;
    for (auto chunk : blob_file) {
        std::cout << chunk << '\n';
        written_blob += chunk;
    }

    EXPECT_EQ(written_blob, blob);
}
