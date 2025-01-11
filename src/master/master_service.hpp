//
// Created by mjacniacki on 09.01.25.
//
#pragma once
#include "services/master_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class MasterServiceImpl final : public master::MasterService::Service {
public:
    grpc::Status GetWorkersToSaveBlob(
        grpc::ServerContext* context,
        const master::GetWorkersToSaveBlobRequest* request,
        master::GetWorkersToSaveBlobResponse* response) override {
        response->set_message(request->blobid());
        return grpc::Status::OK;
    }
    grpc::Status GetWorkerWithBlob(
        grpc::ServerContext* context,
        const master::GetWorkerWithBlobRequest* request,
        master::GetWorkerWithBlobResponse* response) override
    {
        return grpc::Status::CANCELLED;
    }
};
