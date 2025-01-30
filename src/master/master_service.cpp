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

#define BLOB_STATUS_DURING_CREATION "DURING_CREATION"
#define BLOB_STATUS_SAVED "SAVED"

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
    Logger::info("Found workers with enough free space ", workers[0].worker_id, workers[1].worker_id, workers[2].worker_id);
    for (const auto& worker : workers)
    {
        // Add to response
        auto address = worker.ip_address;
        Logger::info("address of one of the workers: ", address);
        std::string* workerAddress = response->add_addresses();
        *workerAddress = address;

        // Add blob copy to database with state "during creation"
        Logger::info("Generating blob copy uuid");
        const boost::uuids::uuid blobCopyUuid = uuidGenerator();
        const auto dto = BlobCopyDTO(to_string(blobCopyUuid), request->blob_hash(), worker.worker_id, BLOB_STATUS_DURING_CREATION, blob_size_mb);
        Logger::info("Saving BlobCopyDTO");
        if (!db->addBlobEntry(dto)) {
            // Potencjalnie zostawiamy bloby ze statusem DURING_CREATION. Trzeba je usunąć, ale to nie jest dobre miejsce na to
            // Jako że wszystko może się wysypać w dowolnej chwili to najlepiej mieć osobny serwis który co jakiś czas sprawdza
            // czy są bloby during creation, które mają ten stan już długo. Jeśli tak, to pytają workera czy ma
            // taki blob i zmieniają odpowiednio status, lub usuwają.
            Logger::error("Error while adding blob entry dto to spanner");
            return grpc::Status::CANCELLED;
        }
        const auto worker_state_dto = WorkerStateDTO(worker.worker_id, worker.ip_address, worker.available_space_mb,
                                                     blob_size_mb + worker.locked_space_mb, worker.last_heartbeat_epoch_ts);
        Logger::info("Updating worker ", worker.worker_id);
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
        auto rows = db->queryBlobByHash(request->blob_hash());
        if (rows.empty()) {
            // response->set_message("Error: Blob with requested ID doesn't exist");
            Logger::error("Error: Blob with ID ", request->blob_hash(), " doesn't exist");
            return grpc::Status::CANCELLED;
        }
        // choose one of the copies
        BlobCopyDTO &blobCopy = rows[0];
        common::ipv4Address address = common::ipv4Address();
        WorkerStateDTO workerState = db->getWorkerState(blobCopy.worker_id);
        response->set_addresses(workerState.ip_address);
        return grpc::Status::OK;
    } catch (std::runtime_error& e) {
        std::cerr << e.what();
        // response->set_message("Error: Querying database resulted in an error");
        return grpc::Status::CANCELLED;
    }
}


grpc::Status MasterServiceImpl::NotifyBlobSaved(
    grpc::ServerContext* context,
    const master::NotifyBlobSavedRequest* request,
    master::NotifyBlobSavedResponse* response)
{
    Logger::info("NotifyBlobSaved ", request->blob_hash(), " ", request->worker_id());
    auto blobDTOs = db->queryBlobByHashAndWorkerId(request->blob_hash(), request->worker_id());
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
    auto worker_state = db->getWorkerState(request->worker_id());
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
    const boost::uuids::uuid worker_id = uuidGenerator();
    auto worker_state = WorkerStateDTO(to_string(worker_id), request->address(), request->space_available(), 0, 0);
    if(!db->addWorkerState(worker_state)) {
        return grpc::Status::CANCELLED;
    }
    response->set_worker_id(to_string(worker_id));
    return grpc::Status::OK;
}
