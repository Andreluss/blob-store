#pragma once
#include <string>
#include <iostream>
#include <logging.hpp>
#include <grpcpp/create_channel.h>
#include <services/master_service.grpc.pb.h>
#include <services/worker_service.grpc.pb.h>
#include "network_utils.hpp"

using namespace std::string_literals;

// master mock impl
class MasterServiceMockImpl: public master::MasterService::Service
{
    MasterConfig config;
    const std::vector<std::string> mock_workers = {"worker-0.worker-service:50042"};
public:
    explicit MasterServiceMockImpl(const MasterConfig& config): config(config) {}
    ~MasterServiceMockImpl() override = default;
    grpc::Status Healthcheck(grpc::ServerContext* context, const master::HealthcheckRequest* request,
        master::HealthcheckResponse* response) override
    {
        return grpc::Status::OK;
    }
    grpc::Status GetWorkersToSaveBlob(grpc::ServerContext* context, const master::GetWorkersToSaveBlobRequest* request,
        master::GetWorkersToSaveBlobResponse* response) override
    {
        Logger::info("Getting workers to save blob ", request->blob_hash());

        for (const auto& worker: mock_workers) {
            Logger::debug("Next address: ", worker);
            const auto next_address = response->add_addresses();
            *next_address = worker;
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
        response->set_addresses(mock_workers.front());
        return grpc::Status::OK;
    }
    grpc::Status DeleteBlob(grpc::ServerContext* context, const master::DeleteBlobRequest* request,
        master::DeleteBlobResponse* response) override
    {
        Logger::info("Delete blob ", request->blob_hash(), " request received");

        auto worker_address = mock_workers.front();
        const auto worker_stub = worker::WorkerService::NewStub(grpc::CreateChannel(worker_address, grpc::InsecureChannelCredentials()));

        worker::DeleteBlobRequest worker_request;
        worker_request.set_blob_hash(request->blob_hash());
        worker::DeleteBlobResponse worker_response;
        grpc::ClientContext client_context;

        Logger::info("Sending delete blob request to worker [", worker_address, "]");

        const auto status = worker_stub->DeleteBlob(&client_context, worker_request, &worker_response);
        if (not status.ok()) {
            Logger::error("Failed to delete blob from worker [", worker_address, "]: ", status.error_message());
            return grpc::Status(grpc::CANCELLED, "Failed to delete blob from worker " + worker_address);
        }
        Logger::info("Successfully deleted blob from worker [", worker_address, "]");
        return grpc::Status::OK;
    }
};