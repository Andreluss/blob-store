#pragma once
#include "services/master_service.grpc.pb.h"
#include "services/frontend_service.grpc.pb.h"
#include <grpc++/grpc++.h>

class FrontendServiceImpl final : public frontend::Frontend::Service
{
    int masters_count_;
    [[nodiscard]] std::string get_master_service_address_based_on_hash(const std::string& hash) const {
        const auto idx = std::hash<std::string>{}(hash) % masters_count_;
        return "master-" + std::to_string(idx) + ".master-service:50042";
    }
public:
    explicit FrontendServiceImpl(const int masters_count): masters_count_(masters_count) {}

    grpc::Status UploadBlob(grpc::ServerContext* context, grpc::ServerReader<frontend::UploadBlobRequest>* reader,
                            frontend::UploadBlobResponse* response) override;

    grpc::Status GetBlob(grpc::ServerContext* context, const frontend::GetBlobRequest* request,
                         grpc::ServerWriter<frontend::GetBlobResponse>* writer) override;

    grpc::Status DeleteBlob(grpc::ServerContext* context, const frontend::DeleteBlobRequest* request,
                            frontend::DeleteBlobResponse* response) override;

    grpc::Status HealthCheck(grpc::ServerContext* context, const frontend::HealthcheckRequest* request,
                             frontend::HealthcheckResponse* response) override;
};
