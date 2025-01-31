//
// Created by mjacniacki on 16.01.25.
//
#include <google/cloud/spanner/client.h>
#include <google/cloud/spanner/mutations.h>
#include <iostream>
#include <vector>
#include <master_db_repository.hpp>
#include <sys/stat.h>

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
            { "hash", "worker_address", "state", "size_mb"})
            .EmplaceRow(entry.hash, entry.worker_address, entry.state, entry.size_mb)
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
            {"hash", "worker_address", "state", "size_mb"})
            .EmplaceRow(entry.hash, entry.worker_address, entry.state, entry.size_mb)
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
std::vector<BlobCopyDTO> MasterDbRepository::querySavedBlobByHash(const std::string& hash) const {
    std::vector<BlobCopyDTO> results;
    try {
        auto query = spanner::SqlStatement(
            "SELECT hash, worker_address, state, size_mb FROM blob_copy "
            "WHERE hash = $1 AND state = $2",
            {{"p1", spanner::Value(hash)}, {"p2", spanner::Value(BLOB_STATUS_SAVED)}});

        auto rows = client->ExecuteQuery(query);

        using rowType = std::tuple<std::string, std::string, std::string, int64_t>;
        for (auto const& row : spanner::StreamOf<rowType>(rows)) {
            if (!row) {
                throw std::runtime_error(row.status().message());
            }

            std::string hash, worker_address, state;
            hash = std::get<0>(*row);
            worker_address = std::get<1>(*row);
            state = std::get<2>(*row);
            int64_t size_mb = std::get<3>(*row);
            results.emplace_back(hash, worker_address, state, size_mb);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error querying entries: " << e.what() << std::endl;
    }

    return results;
}

std::vector<BlobCopyDTO> MasterDbRepository::queryBlobByHashAndWorkerId(const std::string& hash, const std::string& worker_address) const {
    std::vector<BlobCopyDTO> results;
    try {
        auto query = spanner::SqlStatement(
            "SELECT hash, worker_address, state, size_mb FROM blob_copy "
            "WHERE hash = $1 AND worker_address = $2",
            {{"p1", spanner::Value(hash)}, {"p2", spanner::Value(worker_address)}});

        auto rows = client->ExecuteQuery(query);

        using rowType = std::tuple<std::string, std::string, std::string, int64_t>;
        for (auto const& row : spanner::StreamOf<rowType>(rows)) {
            if (!row) {
                throw std::runtime_error(row.status().message());
            }

            std::string hash, worker_address, state;
            hash = std::get<0>(*row);
            worker_address = std::get<1>(*row);
            state = std::get<2>(*row);
            int64_t size_mb = std::get<3>(*row);
            results.emplace_back(hash, worker_address, state, size_mb);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error querying entries: " << e.what() << std::endl;
    }

    return results;
}

bool MasterDbRepository::deleteBlobEntryByHash(const std::string& hash) const {
    try {
        auto mutation = MakeDeleteMutation(
        "blob_copy",  // Replace with your actual table name
        spanner::KeySet().AddKey(
            spanner::MakeKey(hash)  // Only specify the hash part of the composite key
            ).AddRange(
                spanner::MakeKeyBoundClosed(hash),  // Start of range (inclusive)
                spanner::MakeKeyBoundClosed(hash)   // End of range (inclusive)
            )
        );

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


bool MasterDbRepository::deleteBlobEntriesByWorkerAddress(const std::string& worker_address) const {
    std::string sql = "DELETE FROM blob_copy WHERE worker_address = $1";
    auto statement = spanner::SqlStatement( sql, {{"p1", spanner::Value(worker_address)}} );

    auto commit_result = client->Commit([statement, this](spanner::Transaction txn)
                                         -> google::cloud::StatusOr<spanner::Mutations> {
    auto dele = client->ExecuteDml( std::move(txn), statement);
    if (!dele) return std::move(dele).status();
    return spanner::Mutations{};
    });
        if (!commit_result)
    {
        Logger::error(commit_result.status());
        return false;
    }
    return true;
}


bool MasterDbRepository::addWorkerState(const WorkerStateDTO& worker_state) const {
    try {
        auto mutation = spanner::InsertMutationBuilder( "worker_state", {"worker_address",
            "available_space_mb", "locked_space_mb", "last_heartbeat_epoch_ts"})
            .EmplaceRow(worker_state.worker_address, worker_state.available_space_mb,
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
            {"worker_address", "available_space_mb", "locked_space_mb", "last_heartbeat_epoch_ts"})
        .EmplaceRow(worker_state.worker_address, worker_state.available_space_mb,
                    worker_state.locked_space_mb, worker_state.last_heartbeat_epoch_ts).Build();
        auto commit_result = client->Commit(spanner::Mutations{mutation});
        return commit_result.ok();
    } catch (const std::exception&) {
        return false;
    }
}

bool MasterDbRepository::deleteWorkerState(const std::string& worker_address) const {
    try {
        auto mutation = spanner::DeleteMutationBuilder("worker_state", spanner::KeySet().AddKey(
            spanner::MakeKey(worker_address))).Build();
        auto commit_result = client->Commit(spanner::Mutations{mutation});
        return commit_result.ok();
    } catch (const std::exception&) {
        return false;
    }
}

WorkerStateDTO MasterDbRepository::getWorkerState(const std::string& worker_address) const {
    auto query = spanner::SqlStatement(
        "SELECT worker_address, available_space_mb, locked_space_mb, last_heartbeat_epoch_ts "
        "FROM worker_state "
        "WHERE worker_address = $1",
        {{"p1", spanner::Value(worker_address)}});

    auto rows = client->ExecuteQuery(query);
    auto stream = spanner::StreamOf<std::tuple<std::string, int64_t, int64_t, int64_t>>(rows);
    for (auto const& row : stream)
    {
        if (!row) throw std::runtime_error(row.status().message());
        return WorkerStateDTO{
            std::get<0>(*row),
            std::get<1>(*row),
            std::get<2>(*row),
            std::get<3>(*row)
        };
    }
    throw std::runtime_error("Error: No worker state exists with given id");
}

std::vector<WorkerStateDTO> MasterDbRepository::getWorkersWithFreeSpace(int64_t spaceNeeded, int32_t num_workers) const {
    auto query = spanner::SqlStatement(
        "SELECT worker_address, available_space_mb, locked_space_mb, last_heartbeat_epoch_ts "
        "FROM worker_state "
        "WHERE available_space_mb - locked_space_mb >= $1 "
        "LIMIT $2",
        {{"p1", spanner::Value(spaceNeeded)}, {"p2", spanner::Value(num_workers)}});
    Logger::info("1");

    auto rows = client->ExecuteQuery(query);
    auto stream = spanner::StreamOf<std::tuple<std::string, int64_t, int64_t, int64_t>>(rows);
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
            std::get<3>(*row)
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