//
// Created by mjacniacki on 16.01.25.
//

#include <iostream>

#include "master_db_repository.hpp"

int main() {
    try {
        // Create database connection
        // Replace with your actual project, instance, and database IDs
        MasterDbRepository db(
            "blobs-project-449409",
            "blob-store-main",
            "app-master"
        );
        db.addWorkerState(WorkerStateDTO("xd", 1, 2, 3));
        db.addWorkerState(WorkerStateDTO("xd2", 1, 2, 3));
        auto res = db.getWorkerState("xd");
        db.deleteWorkerState("xd");
        db.deleteWorkerState("xd2");
        std::cout << res.worker_address <<'\n';

        db.addBlobEntry(BlobCopyDTO( "hash123","worker123", "SAVED", 123));

        auto results = db.queryBlobByHash("hash123");
        for (const auto& [address, worker_id, state, size_mb] : results) {
            std::cout << "address" << worker_id
                     << ", Size mb: " << size_mb
                     << ", State: " << state
                     << std::endl;
        }

        // Example: Delete entry
        bool deleted = db.deleteBlobEntriesByWorkerAddress( "worker123");
        if (deleted) {
            std::cout << "Entry deleted successfully" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}