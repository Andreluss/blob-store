//
// Created by mjacniacki on 16.01.25.
//
#include <google/cloud/spanner/client.h>
#include <google/cloud/spanner/mutations.h>
#include <iostream>
#include <vector>
#include <master_db_repository.hpp>

namespace spanner = ::google::cloud::spanner;
MasterDbRepository::MasterDbRepository (
    std::string const& project_id,
    std::string const& instance_id,
    std::string const& database_id)
{
    try {
        auto db = spanner::Database(project_id, instance_id, database_id);
        client = std::make_shared<spanner::Client>(
            spanner::MakeConnection(db));

        std::cout << "Successfully connected to Spanner database" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        throw;
    }
}

    // Method to add new entry
bool MasterDbRepository::addBlobEntry(const BlobCopyDTO& entry) const {
    try {
        auto mutation = spanner::InsertMutationBuilder(
            "blob_copy",
            {"uuid", "hash", "worker_id"})
            .EmplaceRow(entry.uuid, entry.hash, entry.worker_id)
            .Build();

        auto commit_result = client->Commit(
            spanner::Mutations{mutation});

        if (!commit_result) {
            throw std::runtime_error(commit_result.status().message());
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error adding entry: " << e.what() << std::endl;
        return false;
    }
}

    // Method to query entries by hash
std::vector<BlobCopyDTO> MasterDbRepository::queryBlobByHash(const std::string& hash) const {
    std::vector<BlobCopyDTO> results;
    try {
        auto query = spanner::SqlStatement(
            "SELECT uuid, hash, worker_id FROM blob_copy "
            "WHERE hash = $1",
            {{"p1", spanner::Value(hash)}});

        auto rows = client->ExecuteQuery(query);

        for (auto const& row : spanner::StreamOf<std::tuple<std::string, std::string, std::string>>(rows)) {
            if (!row) {
                throw std::runtime_error(row.status().message());
            }

            std::string uuid, hash, worker_id;
            uuid = std::get<0>(*row);
            hash = std::get<1>(*row);
            worker_id = std::get<2>(*row);
            results.emplace_back(uuid, hash, worker_id);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error querying entries: " << e.what() << std::endl;
    }

    return results;
}

// Method to delete entry by UUID
bool MasterDbRepository::deleteBlobEntry(const std::string& uuid) const {
    try {
        auto mutation = spanner::DeleteMutationBuilder(
            "blob_copy",
            spanner::KeySet().AddKey(
                spanner::MakeKey(uuid)))
            .Build();

        auto commit_result = client->Commit(
            spanner::Mutations{mutation});

        if (!commit_result) {
            throw std::runtime_error(commit_result.status().message());
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting entry: " << e.what() << std::endl;
        return false;
    }
}

bool MasterDbRepository::addWorkerState(const WorkerStateDTO& worker_state) const {
    try {
        auto mutation = spanner::InsertMutationBuilder( "worker_state", {"worker_id",
            "ip_address", "available_space_mb", "locked_space_mb", "last_heartbeat_epoch_ts"})
            .EmplaceRow(worker_state.worker_id, worker_state.ip_address, worker_state.available_space_mb,
                        worker_state.locked_space_mb, worker_state.last_heartbeat_epoch_ts).Build();
        auto commit_result = client->Commit(spanner::Mutations{mutation});
        return commit_result.ok();
    } catch (const std::exception&) {
        return false;
    }
}

bool MasterDbRepository::updateWorkerState(const WorkerStateDTO& worker_state) const {
    try {
        auto mutation = spanner::UpdateMutationBuilder(
            "worker_state",
            {"worker_id", "ip_address", "available_space_mb", "locked_space_mb", "last_heartbeat_epoch_ts"})
        .EmplaceRow(worker_state.worker_id, worker_state.ip_address, worker_state.available_space_mb,
                    worker_state.locked_space_mb, worker_state.last_heartbeat_epoch_ts).Build();
        auto commit_result = client->Commit(spanner::Mutations{mutation});
        return commit_result.ok();
    } catch (const std::exception&) {
        return false;
    }
}

bool MasterDbRepository::deleteWorkerState(const std::string& worker_id) const {
    try {
        auto mutation = spanner::DeleteMutationBuilder("worker_state", spanner::KeySet().AddKey(
            spanner::MakeKey(worker_id))).Build();
        auto commit_result = client->Commit(spanner::Mutations{mutation});
        return commit_result.ok();
    } catch (const std::exception&) {
        return false;
    }
}

WorkerStateDTO MasterDbRepository::getWorkerState(const std::string& worker_id) const {
    auto query = spanner::SqlStatement(
        "SELECT worker_id, ip_address, available_space_mb, locked_space_mb, last_heartbeat_epoch_ts "
        "FROM worker_state "
        "WHERE worker_id = $1",
        {{"p1", spanner::Value(worker_id)}});

    auto rows = client->ExecuteQuery(query);
    auto stream = spanner::StreamOf<std::tuple<std::string, std::string, int64_t, int64_t, int64_t>>(rows);
    for (auto const& row : stream)
    {
        if (!row) throw std::runtime_error(row.status().message());
        return WorkerStateDTO{
            std::get<0>(*row),
            std::get<1>(*row),
            std::get<2>(*row),
            std::get<3>(*row),
            std::get<4>(*row)
        };
    }
    throw std::runtime_error("Error: No worker state exists with given id");
}
