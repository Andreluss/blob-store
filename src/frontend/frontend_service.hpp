#pragma once
#include "services/master_service.grpc.pb.h"
#include "services/frontend_service.grpc.pb.h"
#include <grpc++/grpc++.h>

class FrontendServiceImpl final : public frontend::Frontend::Service
{
    std::unique_ptr<master::MasterService::Stub> master_stub_;
public:
    explicit FrontendServiceImpl(const std::shared_ptr<grpc::Channel>& master_channel)
    : master_stub_(master::MasterService::NewStub(master_channel)) {}

    grpc::Status UploadBlob(grpc::ServerContext* context, grpc::ServerReader<frontend::UploadBlobRequest>* reader,
                            frontend::UploadBlobResponse* response) override;

    grpc::Status GetBlob(grpc::ServerContext* context, const frontend::GetBlobRequest* request,
                         grpc::ServerWriter<frontend::GetBlobResponse>* writer) override;

    grpc::Status DeleteBlob(grpc::ServerContext* context, const frontend::DeleteBlobRequest* request,
                            frontend::DeleteBlobResponse* response) override;

    grpc::Status HealthCheck(grpc::ServerContext* context, const frontend::HealthcheckRequest* request,
                             frontend::HealthcheckResponse* response) override;
};
