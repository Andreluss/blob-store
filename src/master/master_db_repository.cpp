//
// Created by mjacniacki on 16.01.25.
//
#include <google/cloud/spanner/client.h>
#include <google/cloud/spanner/mutations.h>
#include <iostream>
#include <vector>
#include <master_db_repository.hpp>
#include "logging.hpp"

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
            {"uuid", "hash", "worker_id", "state", "size_mb"})
            .EmplaceRow(entry.uuid, entry.hash, entry.worker_id, entry.state, entry.size_mb)
            .Build();

        auto commit_result = client->Commit(
            spanner::Mutations{mutation});

        if (!commit_result) {
            throw std::runtime_error(commit_result.status().message());
        }
        return true;
    } catch (const std::exception& e) {
        Logger::error("Error adding entry: ", e.what());
        return false;
    }
}

bool MasterDbRepository::updateBlobEntry(const BlobCopyDTO& entry) const {
    try {
        auto mutation = spanner::UpdateMutationBuilder(
            "blob_copy",
            {"uuid", "hash", "worker_id", "state", "size_mb"})
            .EmplaceRow(entry.uuid, entry.hash, entry.worker_id, entry.state, entry.size_mb)
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
            "SELECT uuid, hash, worker_id, state, size_mb FROM blob_copy "
            "WHERE hash = $1",
            {{"p1", spanner::Value(hash)}});

        auto rows = client->ExecuteQuery(query);

        using rowType = std::tuple<std::string, std::string, std::string, std::string, int64_t>;
        for (auto const& row : spanner::StreamOf<rowType>(rows)) {
            if (!row) {
                throw std::runtime_error(row.status().message());
            }

            std::string uuid, hash, worker_id, state;
            uuid = std::get<0>(*row);
            hash = std::get<1>(*row);
            worker_id = std::get<2>(*row);
            state = std::get<3>(*row);
            int64_t size_mb = std::get<4>(*row);
            results.emplace_back(uuid, hash, worker_id, state, size_mb);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error querying entries: " << e.what() << std::endl;
    }

    return results;
}

std::vector<BlobCopyDTO> MasterDbRepository::queryBlobByHashAndWorkerId(const std::string& hash, const std::string& worker_id) const {
    std::vector<BlobCopyDTO> results;
    try {
        auto query = spanner::SqlStatement(
            "SELECT uuid, hash, worker_id, state, size_mb FROM blob_copy "
            "WHERE hash = $1 AND worker_id = $2",
            {{"p1", spanner::Value(hash)}, {"p2", spanner::Value(worker_id)}});

        auto rows = client->ExecuteQuery(query);

        using rowType = std::tuple<std::string, std::string, std::string, std::string, int64_t>;
        for (auto const& row : spanner::StreamOf<rowType>(rows)) {
            if (!row) {
                throw std::runtime_error(row.status().message());
            }

            std::string uuid, hash, worker_id, state;
            uuid = std::get<0>(*row);
            hash = std::get<1>(*row);
            worker_id = std::get<2>(*row);
            state = std::get<3>(*row);
            int64_t size_mb = std::get<4>(*row);
            results.emplace_back(uuid, hash, worker_id, state, size_mb);
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

std::vector<WorkerStateDTO> MasterDbRepository::getWorkersWithFreeSpace(int64_t spaceNeeded, int32_t num_workers) const {
    auto query = spanner::SqlStatement(
        "SELECT worker_id, ip_address, available_space_mb, locked_space_mb, last_heartbeat_epoch_ts "
        "FROM worker_state "
        "WHERE available_space_mb - locked_space_mb >= $1 "
        "LIMIT $2",
        {{"p1", spanner::Value(spaceNeeded)}, {"p2", spanner::Value(num_workers)}});
    Logger::info("1");

    auto rows = client->ExecuteQuery(query);
    auto stream = spanner::StreamOf<std::tuple<std::string, std::string, int64_t, int64_t, int64_t>>(rows);
    std::vector<WorkerStateDTO> result;
    Logger::info("2");
    for (auto const& row : stream)
    {
        Logger::info("3");
        Logger::error(row.status().message());
        if (!row) throw std::runtime_error(row.status().message());
        result.emplace_back(
            std::get<0>(*row),
            std::get<1>(*row),
            std::get<2>(*row),
            std::get<3>(*row),
            std::get<4>(*row)
        );
    }
    Logger::info("4");
    if (result.size() < num_workers)
    {
        Logger::error("Error: requested " + std::to_string(num_workers) + " workers, but only "
                                    + std::to_string(result.size()) + " workers matching the criteria exist");
        throw std::runtime_error("");
    }
    return result;
}