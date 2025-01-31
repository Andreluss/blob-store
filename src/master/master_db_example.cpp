//
// Created by mjacniacki on 16.01.25.
//

#include <iostream>
#include <sys/stat.h>

#include "master_db_repository.hpp"

int main() {
    // Create database connection
    // Replace with your actual project, instance, and database IDs
    MasterDbRepository db(
        "blobs-project-449409",
        "blob-store-main",
        "app-master"
    );
    auto status = db.addWorkerState(WorkerStateDTO("xd", 1, 2, 3))
    .and_then([&](auto _) -> Expected<std::monostate, grpc::Status>
    {
        return db.addWorkerState(WorkerStateDTO("xd2", 1, 2, 3));
    })
    .and_then([&](auto _) -> Expected<WorkerStateDTO, grpc::Status>
    {
        return db.getWorkerState("xd");
    })
    .and_then([&](const WorkerStateDTO &worker_state_dto) -> Expected<std::monostate, grpc::Status>
    {
        std::cout << worker_state_dto.worker_address <<'\n';
        return std::monostate();
    })
    .and_then([&](auto _) -> Expected<std::monostate, grpc::Status>
    {
        return db.deleteWorkerState("xd");
    })
    .and_then([&](auto _) -> Expected<std::monostate, grpc::Status>
    {
        return db.deleteWorkerState("xd2");
    })
    .output<grpc::Status>(
        [](auto _) { return grpc::Status::OK; },
        std::identity()
        );
    std::cout << "Operations on worker_state table status: " << (status.ok() ? "OK" : "ERROR") << '\n';

    status = db.addBlobEntry(BlobCopyDTO( "hash123","worker123", "SAVED", 123))
    .and_then([&](auto _) -> Expected<std::vector<BlobCopyDTO>, grpc::Status>
    {
        return db.querySavedBlobByHash("hash123");
    })
    .and_then([&](auto results) -> Expected<std::monostate, grpc::Status>
    {
        for (const auto& [address, worker_id, state, size_mb] : results) {
            std::cout << "address" << worker_id
                     << ", Size mb: " << size_mb
                     << ", State: " << state
                     << std::endl;
        }
        return db.deleteBlobEntriesByWorkerAddress( "worker123");
    })
    .and_then([&](auto results) -> Expected<std::monostate, grpc::Status>
    {
        std::cout << "Entry deleted successfully" << std::endl;
        return std::monostate();
    })
    .output<grpc::Status>(
        [](auto _) { return grpc::Status::OK; },
        std::identity());

    std::cout << "Operations on blob_copy table status: " << (status.ok() ? "OK" : "ERROR") << '\n';
    return 0;
}