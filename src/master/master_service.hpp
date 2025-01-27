//
// Created by mjacniacki on 09.01.25.
//
#pragma once
#include "services/master_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

class MasterServiceImpl final : public master::MasterService::Service {
public:
    boost::uuids::random_generator uuidGenerator;
    grpc::Status GetWorkersToSaveBlob(
        grpc::ServerContext* context,
        const master::GetWorkersToSaveBlobRequest* request,
        master::GetWorkersToSaveBlobResponse* response) override;
    grpc::Status GetWorkerWithBlob(
        grpc::ServerContext* context,
        const master::GetWorkerWithBlobRequest* request,
        master::GetWorkerWithBlobResponse* response) override;
    grpc::Status NotifyBlobSaved(grpc::ServerContext* context, const master::NotifyBlobSavedRequest* request,
                                 master::NotifyBlobSavedResponse* response);
    MasterServiceImpl(MasterDbRepository* db);
private:
    MasterDbRepository *db;
};
