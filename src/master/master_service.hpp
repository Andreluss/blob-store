//
// Created by mjacniacki on 09.01.25.
//
#pragma once
#include "services/master_service.grpc.pb.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "master_db_repository.hpp"
#include "../common/ip_utils.hpp"

#define BLOB_STATUS_DURING_CREATION "DURING_CREATION"

class MasterServiceImpl final : public master::MasterService::Service {
public:
    explicit MasterServiceImpl(MasterDbRepository *db)
    {
       this->db = db;
    }
    boost::uuids::random_generator uuidGenerator;
    MasterServiceImpl();
    grpc::Status GetWorkersToSaveBlob(
        grpc::ServerContext* context,
        const master::GetWorkersToSaveBlobRequest* request,
        master::GetWorkersToSaveBlobResponse* response) override {
        uint64_t spaceToLock = request->sizemb();
        response->set_message(request->blobid());

        // Find workers
        auto workers = db->getWorkersWithFreeSpace(request->sizemb(), 3);
        for (const auto& worker : workers)
        {
            // Add to response
            auto address = parseIPv4WithPort(worker.ip_address);
            common::ipv4Address *workerAddress = response->add_addresses();
            workerAddress->set_address(address.address());
            workerAddress->set_port(address.port());

            // Add blob copy to database with state "during creation"
            const boost::uuids::uuid blobCopyUuid = uuidGenerator();
            const auto dto = BlobCopyDTO(to_string(blobCopyUuid), request->blobid(), worker.worker_id, BLOB_STATUS_DURING_CREATION);
            if (!db->addBlobEntry(dto)) {
                // Potencjalnie zostawiamy bloby ze statusem DURING_CREATION. Trzeba je usunąć, ale to nie jest dobre miejsce na to
                // Jako że wszystko może się wysypać w dowolnej chwili to najlepiej mieć osobny serwis który co jakiś czas sprawdza
                // czy są bloby during creation, które mają ten stan już długo. Jeśli tak, to pytają workera czy ma
                // taki blob i zmieniają odpowiednio status, lub usuwają.
                return grpc::Status::CANCELLED;
            }
        }

        return grpc::Status::OK;
    }

    grpc::Status GetWorkerWithBlob(
        grpc::ServerContext* context,
        const master::GetWorkerWithBlobRequest* request,
        master::GetWorkerWithBlobResponse* response) override
    {
        try {
            auto rows = db->queryBlobByHash(request->blobid());
            if (rows.empty()) {
                response->set_message("Error: Blob with requested ID doesn't exist");
                return grpc::Status::CANCELLED;
            }
            // choose one of the copies
            BlobCopyDTO &blobCopy = rows[0];
            common::ipv4Address address = common::ipv4Address();
            WorkerStateDTO workerState = db->getWorkerState(blobCopy.worker_id);
            common::ipv4Address workerAddress = parseIPv4WithPort(workerState.ip_address);
            response->set_allocated_address(new common::ipv4Address(workerAddress));
            return grpc::Status::OK;
        } catch (std::runtime_error& e) {
            std::cerr << e.what();
            response->set_message("Error: Querying database resulted in an error");
            return grpc::Status::CANCELLED;
        }
    }

    grpc::Status NotifyBlobSaved(
        grpc::ServerContext* context,
        const master::NotifyBlobSavedRequest* request,
        master::NotifyBlobSavedResponse* response) override
    {
        const boost::uuids::uuid blobCopyUuid = uuidGenerator();
        const auto dto = BlobCopyDTO(to_string(blobCopyUuid), request->blobid(), request->workderid());
        if (!db->addBlobEntry(dto)) {
            return grpc::Status::CANCELLED;
        }
        return grpc::Status::OK;
    }
private:
    MasterDbRepository *db;
};
