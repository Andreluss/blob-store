//
// Created by mjacniacki on 09.01.25.
//
#pragma once
#include "services/master_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>

class MasterServiceImpl final : public master::MasterService::Service {
public:
    boost::uuids::random_generator generator;
    MasterServiceImpl()
    {
        boost::uuids::uuid uuid = generator();
    }
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
    grpc::Status NotifyBlobSaved(
        grpc::ServerContext* context,
        const master::NotifyBlobSavedRequest* request,
        master::NotifyBlobSavedResponse* response) override
    {
        return grpc::Status::CANCELLED;
    }
private:

};
