//
// Created by mjacniacki on 09.01.25.
//
#include "services/master_service.grpc.pb.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "master_db_repository.hpp"
#include "master_service.hpp"
#include "logging.hpp"

namespace master
{
    class GetWorkersToSaveBlobRequest;
}

MasterServiceImpl::MasterServiceImpl(MasterDbRepository *db) {
   this->db = db;
}
grpc::Status MasterServiceImpl::GetWorkersToSaveBlob(
    grpc::ServerContext* context,
    const master::GetWorkersToSaveBlobRequest* request,
    master::GetWorkersToSaveBlobResponse* response)
{
    Logger::info("GetWorkersToSaveBlob");
    auto blob_size_mb = static_cast<int64_t>(request->size_mb());
    Logger::info("Blob size ", blob_size_mb);

    return db->getWorkersWithFreeSpace(blob_size_mb, 3)
    .and_then([&](auto workers) -> Expected<std::monostate, grpc::Status> {
        Logger::info("Found workers with enough free space ", workers[0].worker_address, " ", workers[1].worker_address, " ", workers[2].worker_address);
        for (const auto& worker : workers)
        {
            // Add to response
            std::string* workerAddress = response->add_addresses();
            *workerAddress = worker.worker_address;

            // Add blob copy to database with state "during creation"
            const auto dto = BlobCopyDTO(request->blob_hash(), worker.worker_address, BLOB_STATUS_DURING_CREATION, blob_size_mb);
            auto result = db->addBlobEntry(dto);
            if (not result.has_value()) return result;

            // Update worker state to mark locked space
            auto worker_state_dto = WorkerStateDTO(worker.worker_address,worker.available_space_mb,
                                                         blob_size_mb + worker.locked_space_mb,
                                                         worker.last_heartbeat_epoch_ts);
            result = db->updateWorkerState(worker_state_dto);
            if (not result.has_value()) return result;
        }
        return std::monostate();
    })
    .output<grpc::Status>(
        [](auto _) { return grpc::Status::OK; },
        [](auto err) { Logger::error(err.error_message()); return err; });
}

grpc::Status MasterServiceImpl::GetWorkerWithBlob(
    grpc::ServerContext* context,
    const master::GetWorkerWithBlobRequest* request,
    master::GetWorkerWithBlobResponse* response) {
    std::string blob_hash = request->blob_hash();
    Logger::info("GetWorkerWithBlob with hash ", blob_hash);

    return db->querySavedBlobByHash(request->blob_hash())
    .and_then([&](auto blob_copies) -> Expected<BlobCopyDTO, grpc::Status> {
        if (blob_copies.empty()) {
            return grpc::Status(grpc::CANCELLED, "Error: Blob with requested hash doesn't exist");
        }
        // choose one of the copies
        BlobCopyDTO &blob_copy = blob_copies[0];
        Logger::info("Blob found on ", blob_copies.size(), " workers, choosing ", blob_copy.worker_address);
        return blob_copy;
    })
    .and_then([&](auto blob_copy) -> Expected<WorkerStateDTO, grpc::Status> {
        return db->getWorkerState(blob_copy.worker_address);
    }).output<grpc::Status>([&](auto worker_state){
        response->set_addresses(worker_state.worker_address);
        return grpc::Status::OK;
    }, [](auto err) { Logger::error(err.error_message()); return err; });
}


grpc::Status MasterServiceImpl::NotifyBlobSaved(
    grpc::ServerContext* context,
    const master::NotifyBlobSavedRequest* request,
    master::NotifyBlobSavedResponse* response)
{
    Logger::info("NotifyBlobSaved ", request->blob_hash(), " ", request->worker_address());
    return db->queryBlobByHashAndWorkerId(request->blob_hash(), request->worker_address())
    .and_then([&](auto blob_dtos) -> Expected<BlobCopyDTO, grpc::Status> {
        if (blob_dtos.size() != 1) {
            return grpc::Status(grpc::CANCELLED, "Wrong query result");
        }
        auto blob = blob_dtos[0];
        blob.state = BLOB_STATUS_SAVED;
        return blob;
    })
    .and_then([&](auto blob) -> Expected<std::monostate, grpc::Status> {
        auto update_blob_result = db->updateBlobEntry(blob);
        if (not update_blob_result.has_value()) return update_blob_result;
        auto result = db->getWorkerState(request->worker_address());
        if (not result.has_value()) return result.error();
        auto worker_state = result.value();
        worker_state.locked_space_mb -= blob.size_mb;
        worker_state.available_space_mb -= blob.size_mb;
        return db->updateWorkerState(worker_state);
    }).output<grpc::Status>([&](auto _){ return grpc::Status::OK; },
        [](auto err) { Logger::error(err.error_message()); return err; });
}

grpc::Status MasterServiceImpl::RegisterWorker(grpc::ServerContext* context, const master::RegisterWorkerRequest* request, master::RegisterWorkerResponse* response)
{
    Logger::info("RegisterWorker");
    // If there was a worker with the same address before then delete it (it must have been restarted)
    return db->deleteWorkerState(request->address())
    .and_then([&](auto _) -> Expected<std::monostate, grpc::Status> {
        return db->deleteBlobEntriesByWorkerAddress(request->address());
    })
    .and_then([&](auto _) -> Expected<std::monostate, grpc::Status> {
        auto worker_state = WorkerStateDTO(request->address(), request->space_available(), 0, 0);
        return db->addWorkerState(worker_state);
    }).output<grpc::Status>([&](auto _){ return grpc::Status::OK; },
        [](auto err) { Logger::error(err.error_message()); return err; });
}
