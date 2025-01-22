#pragma once
#include "services/master_service.grpc.pb.h"
#include "services/frontend_service.grpc.pb.h"
#include "services/worker_service.grpc.pb.h"
#include "blob_hasher.hpp"
#include "utils.hpp"
#include "expected.hpp"
#include <grpc++/grpc++.h>

class FrontendServiceImpl final : public frontend::Frontend::Service
{
    std::unique_ptr<master::MasterService::Stub> master_stub_;

    auto send_blob_to_worker(const BlobFile& blob, const std::string& blob_hash,
                                    const common::ipv4Address& worker_address) -> std::optional<std::string>;

    auto receive_and_hash_blob(
        grpc::ServerReader<frontend::UploadBlobRequest>* reader) -> Expected<
        std::pair<BlobFile, std::string>, grpc::Status>;

    auto get_workers_from_master(std::string blob_hash,
                                        const std::unique_ptr<master::MasterService::Stub>& master_stub)  -> Expected<google::protobuf::RepeatedPtrField<common::ipv4Address>, std::string>;

    auto get_worker_with_blob_id(std::string blob_id) const -> Expected<common::ipv4Address, std::string>;

    auto start_get_blob_from_worker(std::string blob_id, const common::ipv4Address& worker_address)
        -> std::unique_ptr<grpc::ClientReader<worker::GetBlobResponse>>;

public:
    explicit FrontendServiceImpl(const std::shared_ptr<grpc::Channel>& master_channel)
    : master_stub_(master::MasterService::NewStub(master_channel))
    {}

    grpc::Status UploadBlob(grpc::ServerContext* context, grpc::ServerReader<frontend::UploadBlobRequest>* reader,
                            frontend::UploadBlobResponse* response) override;

    grpc::Status GetBlob(grpc::ServerContext* context, const frontend::GetBlobRequest* request,
                         grpc::ServerWriter<frontend::GetBlobResponse>* writer) override;

    grpc::Status DeleteBlob(grpc::ServerContext* context, const frontend::DeleteBlobRequest* request,
                            frontend::DeleteBlobResponse* response) override;

    grpc::Status HealthCheck(grpc::ServerContext* context, const frontend::HealthcheckRequest* request,
                             frontend::HealthcheckResponse* response) override;
};
