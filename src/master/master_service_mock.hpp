#pragma once
#include <string>
#include <iostream>
#include <services/master_service.grpc.pb.h>

#include "network_utils.hpp"

// master mock impl
class MasterServiceMockImpl: public master::MasterService::Service
{
    MasterConfig config;
public:
    explicit MasterServiceMockImpl(const MasterConfig& config): config(config) {}
    ~MasterServiceMockImpl() override {}
    grpc::Status Healthcheck(grpc::ServerContext* context, const master::HealthcheckRequest* request,
        master::HealthcheckResponse* response) override
    {
        return grpc::Status::OK;
    }
    grpc::Status GetWorkersToSaveBlob(grpc::ServerContext* context, const master::GetWorkersToSaveBlobRequest* request,
        master::GetWorkersToSaveBlobResponse* response) override
    {
        std::cout << "Getting workers to save blob " << request->blobid() << std::endl;
        for (const auto& worker: config.workers) {
            std::cout << "Worker: " << worker << std::endl;
            auto worker_address = response->add_addresses();
            std::cout << "Worker address: " << worker_address << std::endl;
            auto x = address_of_string(worker);
            std::cout << "Worker address: " << x.address() << " " << x.port() << std::endl;
            worker_address->set_address(x.address());
            worker_address->set_port(x.port());
        }
        return grpc::Status::OK;
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