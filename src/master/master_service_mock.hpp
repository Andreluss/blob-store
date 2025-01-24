#pragma once
#include <services/master_service.grpc.pb.h>

// master mock impl
class MasterServiceMockImpl: public master::MasterService::Service
{
public:
    ~MasterServiceMockImpl() override;
    grpc::Status Healthcheck(grpc::ServerContext* context, const master::HealthcheckRequest* request,
        master::HealthcheckResponse* response) override
    {
        return grpc::Status::OK;
    }
    grpc::Status GetWorkersToSaveBlob(grpc::ServerContext* context, const master::GetWorkersToSaveBlobRequest* request,
        master::GetWorkersToSaveBlobResponse* response) override
    {
        std::cout << "Getting workers to save blob " << request->blobid() << std::endl;
        return grpc::Status::CANCELLED;
    }
    grpc::Status NotifyBlobSaved(grpc::ServerContext* context, const master::NotifyBlobSavedRequest* request,
        master::NotifyBlobSavedResponse* response) override
    {
        return grpc::Status::OK;
    }
    grpc::Status GetWorkerWithBlob(grpc::ServerContext* context, const master::GetWorkerWithBlobRequest* request,
        master::GetWorkerWithBlobResponse* response) override
    {
        return grpc::Status::CANCELLED;
    }
    grpc::Status DeleteBlob(grpc::ServerContext* context, const master::DeleteBlobRequest* request,
        master::DeleteBlobResponse* response) override
    {
        std::cout << "Deleting blob" << std::endl;
        return grpc::Status::OK;
    }
};