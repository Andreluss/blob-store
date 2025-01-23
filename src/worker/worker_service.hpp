#ifndef BLOB_STORE_WORKER_SERVICE_HPP
#define BLOB_STORE_WORKER_SERVICE_HPP

#include <filesystem>
#include <fstream>
#include "services/worker_service.grpc.pb.h"
#include "services/master_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include "blob_hasher.hpp"

// We assume that blobs are stored in the blobs/ directory which is created in the same
// directory as the executable.
const std::string BLOBS_PATH = "blobs/";
const uint64_t MAX_CHUNK_SIZE = 1024 * 1024;


class WorkerServiceImpl final : public worker::WorkerService::Service {
    std::unique_ptr<master::MasterService::Stub> master_stub_;
public:
    explicit WorkerServiceImpl(const std::shared_ptr<grpc::Channel>& channel)
            : master_stub_(master::MasterService::NewStub(channel)) {}

    grpc::Status Healthcheck(
            grpc::ServerContext *context,
            const worker::HealthcheckRequest *request,
            worker::HealthcheckResponse *response) override;

    grpc::Status GetFreeStorage(
            grpc::ServerContext *context,
            const worker::GetFreeStorageRequest *request,
            worker::GetFreeStorageResponse *response) override;

    grpc::Status SaveBlob(
            grpc::ServerContext *context,
            grpc::ServerReader<worker::SaveBlobRequest> *reader,
            worker::SaveBlobResponse *response) override;

    grpc::Status GetBlob(
            grpc::ServerContext *context,
            const worker::GetBlobRequest *request,
            grpc::ServerWriter<worker::GetBlobResponse> *writer) override;

    grpc::Status DeleteBlob(
            grpc::ServerContext *context,
            const worker::DeleteBlobRequest *request,
            worker::DeleteBlobResponse *response) override;
};

#endif //BLOB_STORE_WORKER_SERVICE_HPP
