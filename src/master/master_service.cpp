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
    master::GetWorkersToSaveBlobResponse* response) {
    Logger::info("GetWorkersToSaveBlob");
    int64_t blob_size_mb = request->size_mb();
    Logger::info("Blob size ", blob_size_mb);

    // Find workers
    auto workers = db->getWorkersWithFreeSpace(request->size_mb(), 3);
    Logger::info("Found workers with enough free space ", workers[0].worker_address, workers[1].worker_address, workers[2].worker_address);
    for (const auto& worker : workers)
    {
        // Add to response
        Logger::info("address of one of the workers: ", worker.worker_address);
        std::string* workerAddress = response->add_addresses();
        *workerAddress = worker.worker_address;

        // Add blob copy to database with state "during creation"
        Logger::info("Generating blob copy uuid");
        const auto dto = BlobCopyDTO(request->blob_hash(), worker.worker_address, BLOB_STATUS_DURING_CREATION, blob_size_mb);
        Logger::info("Saving BlobCopyDTO");
        if (!db->addBlobEntry(dto)) {
            // Potencjalnie zostawiamy bloby ze statusem DURING_CREATION. Trzeba je usunąć, ale to nie jest dobre miejsce na to
            // Jako że wszystko może się wysypać w dowolnej chwili to najlepiej mieć osobny serwis który co jakiś czas sprawdza
            // czy są bloby during creation, które mają ten stan już długo. Jeśli tak, to pytają workera czy ma
            // taki blob i zmieniają odpowiednio status, lub usuwają.
            Logger::error("Error while adding blob entry dto to spanner");
            return grpc::Status::CANCELLED;
        }
        const auto worker_state_dto = WorkerStateDTO(worker.worker_address,worker.available_space_mb,
                                                     blob_size_mb + worker.locked_space_mb, worker.last_heartbeat_epoch_ts);
        Logger::info("Updating worker ", worker.worker_address);
        if (!db->updateWorkerState(worker_state_dto)) {
            Logger::error("Error while updating worker");
            return grpc::Status::CANCELLED;
        }
    }

    Logger::info("Happy ending");
    return grpc::Status::OK;
}

grpc::Status MasterServiceImpl::GetWorkerWithBlob(
    grpc::ServerContext* context,
    const master::GetWorkerWithBlobRequest* request,
    master::GetWorkerWithBlobResponse* response) {
    Logger::info("GetWorkerWithBlob");
    try {
        auto rows = db->querySavedBlobByHash(request->blob_hash());
        if (rows.empty()) {
            // response->set_message("Error: Blob with requested ID doesn't exist");
            Logger::error("Error: Blob with ID ", request->blob_hash(), " doesn't exist");
            return grpc::Status::CANCELLED;
        }
        // choose one of the copies
        BlobCopyDTO &blobCopy = rows[0];
        Logger::info("Blob found on ", rows.size(), " workers, choosing ", blobCopy.worker_address);
        WorkerStateDTO workerState = db->getWorkerState(blobCopy.worker_address);
        response->set_addresses(workerState.worker_address);
        return grpc::Status::OK;
    } catch (std::runtime_error& e) {
        std::cerr << e.what();
        return grpc::Status::CANCELLED;
    }
}


grpc::Status MasterServiceImpl::NotifyBlobSaved(
    grpc::ServerContext* context,
    const master::NotifyBlobSavedRequest* request,
    master::NotifyBlobSavedResponse* response)
{
    Logger::info("NotifyBlobSaved ", request->blob_hash(), " ", request->worker_address());
    auto blobDTOs = db->queryBlobByHashAndWorkerId(request->blob_hash(), request->worker_address());
    if (blobDTOs.size() != 1) {
        Logger::error("Wrong query result");
        return grpc::Status::CANCELLED;
    }
    auto blobDTO = blobDTOs[0];
    blobDTO.state = BLOB_STATUS_SAVED;
    if (!db->updateBlobEntry(blobDTO)) {
        Logger::error("Error when updating blob entry");
        return grpc::Status::CANCELLED;
    }
    Logger::info("Getting worker state");
    auto worker_state = db->getWorkerState(request->worker_address());
    Logger::info("Got worker state");
    worker_state.locked_space_mb -= blobDTO.size_mb;
    worker_state.available_space_mb -= blobDTO.size_mb;
    if (!db->updateWorkerState(worker_state))
    {
        Logger::error("Cannot update worker state");
        return grpc::Status::CANCELLED;
    }
    return grpc::Status::OK;
}

grpc::Status MasterServiceImpl::RegisterWorker(grpc::ServerContext* context, const master::RegisterWorkerRequest* request, master::RegisterWorkerResponse* response)
{
    Logger::info("RegisterWorker");
    // If there was a worker with the same address before then delete it (it must have been restarted)
    if(!db->deleteWorkerState(request->address())) {
        return grpc::Status::CANCELLED;
    }
    if(!db->deleteBlobEntriesByWorkerAddress(request->address())) {
        return grpc::Status::CANCELLED;
    }

    auto worker_state = WorkerStateDTO(request->address(), request->space_available(), 0, 0);
    if(!db->addWorkerState(worker_state)) {
        return grpc::Status::CANCELLED;
    }
    return grpc::Status::OK;
}
