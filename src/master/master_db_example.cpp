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
            "blob-store-447713",
            "blob-store-main",
            "app-master"
        );
        db.addWorkerState(WorkerStateDTO("xd", "xd2", 1, 2, 3));
        db.addWorkerState(WorkerStateDTO("xd2", "xd2", 1, 2, 3));
        auto res = db.getWorkerState("xd");
        db.deleteWorkerState("xd");
        db.deleteWorkerState("xd2");
        std::cout << res.worker_id << " " << res.ip_address;

        db.addBlobEntry(BlobCopyDTO("123e4567-e89b-12d3-a456-426614174000",
                   "hash123",
                   "worker123", ""));

        auto results = db.queryBlobByHash("hash123");
        for (const auto& [uuid, hash, worker_id, state] : results) {
            std::cout << "UUID: " << uuid
                     << ", Hash: " << hash
                     << ", Worker ID: " << worker_id << std::endl;
        }

        // Example: Delete entry
        bool deleted = db.deleteBlobEntry(
            "123e4567-e89b-12d3-a456-426614174000");
        if (deleted) {
            std::cout << "Entry deleted successfully" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}