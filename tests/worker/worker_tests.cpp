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
    std::filesystem::create_directory(BLOBS_PATH);

    worker::SaveBlobRequest request;
    worker::SaveBlobResponse response;

    grpc::ClientContext context;
    auto writer = stub_->SaveBlob(&context, &response);

    std::string blob = "Hello Huskies!";

    auto hash = (BlobHasher() += blob).finalize();
    request.set_hash(hash);
    request.set_chunk_data(blob);

    writer->Write(request);
    writer->WritesDone();
    grpc::Status status = writer->Finish();

    BlobFile blob_file = BlobFile::Load(hash);
    EXPECT_EQ(blob_file.size(), blob.size());

    auto saved_blob = std::accumulate(blob_file.begin(), blob_file.end(), std::string());
    EXPECT_EQ(saved_blob, blob);
}

TEST_F(WorkerServiceTest, GetBlob) {
    std::filesystem::create_directory(BLOBS_PATH);
    std::string message = "Skibidi sigma";

    auto hash = (BlobHasher() += message).finalize();
    auto blob_file = BlobFile::New(hash);
    blob_file += message;

    worker::GetBlobRequest request;
    worker::GetBlobResponse response;

    grpc::ClientContext context;
    request.set_hash(hash);

    auto reader = stub_->GetBlob(&context, request);
    std::string response_blob;
    while (reader->Read(&response)) {
        response_blob += response.chunk_data();
    }

    EXPECT_EQ(response_blob, message);
}

TEST_F(WorkerServiceTest, DeleteBlob) {
    std::filesystem::create_directory(BLOBS_PATH);
    std::string message = "no more skibidi sigma";

    auto hash = (BlobHasher() += message).finalize();
    auto blob_file = BlobFile::New(hash);
    blob_file += message;

    worker::DeleteBlobRequest request;
    request.set_hash(hash);

    worker::DeleteBlobResponse response;
    grpc::ClientContext context;

    auto status = stub_->DeleteBlob(&context, request, &response);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(std::filesystem::exists(BLOBS_PATH + hash));
}

TEST_F(WorkerServiceTest, MultiChunkSaveBlob) {
    std::filesystem::create_directory(BLOBS_PATH);

    worker::SaveBlobRequest request;
    worker::SaveBlobResponse response;

    grpc::ClientContext context;
    auto writer = stub_->SaveBlob(&context, &response);

    std::string blob(1024*1024, 'a');

    auto hasher = BlobHasher();
    for (int i = 0; i < 10; ++i) {
        hasher += blob;
    }
    auto hash = hasher.finalize();

    for (int i = 0; i < 10; ++i) {
        request.set_hash(hash);
        request.set_chunk_data(blob);
        writer->Write(request);
    }
    writer->WritesDone();
    grpc::Status status = writer->Finish();

    BlobFile blob_file = BlobFile::Load(hash);
    EXPECT_EQ(blob_file.size(), 10 * blob.size());
    auto saved_blob = std::accumulate(blob_file.begin(), blob_file.end(), std::string());
    EXPECT_EQ(saved_blob, std::string(10 * blob.size(), 'a'));
}
