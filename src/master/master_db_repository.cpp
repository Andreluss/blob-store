//
// Created by mjacniacki on 16.01.25.
//
#include <google/cloud/spanner/client.h>
#include <google/cloud/spanner/mutations.h>
#include <iostream>
#include <vector>
#include <master_db_repository.hpp>
#include <sys/stat.h>

#include "expected.hpp"
#include "logging.hpp"

// Helper
grpc::Status to_grpc_status(const google::cloud::Status& status)
{
    return {status.ok() ? grpc::OK : grpc::CANCELLED, status.message()};
}

// Methods implementation
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

auto MasterDbRepository::addBlobEntry(const BlobCopyDTO& entry) -> Expected<std::monostate, grpc::Status> {
    Logger::debug("MasterDbRepository::addBlobEntry ", entry.to_string());
    auto mutation = spanner::InsertMutationBuilder(
        "blob_copy",
        { "hash", "worker_address", "state", "size_mb"})
        .EmplaceRow(entry.hash, entry.worker_address, entry.state, entry.size_mb)
        .Build();

    auto commit_result = client->Commit(
        spanner::Mutations{mutation});

    if (!commit_result) {
        return to_grpc_status(commit_result.status());
    }
    return std::monostate();
}

auto MasterDbRepository::updateBlobEntry(const BlobCopyDTO& entry) -> Expected<std::monostate, grpc::Status> {
    Logger::debug("MasterDbRepository::updateBlobEntry ", entry.to_string());
    auto mutation = spanner::UpdateMutationBuilder(
        "blob_copy",
        {"hash", "worker_address", "state", "size_mb"})
        .EmplaceRow(entry.hash, entry.worker_address, entry.state, entry.size_mb)
        .Build();

    auto commit_result = client->Commit(spanner::Mutations{mutation});

    if (!commit_result) {
        return to_grpc_status(commit_result.status());
    }
    return std::monostate();
}

    // Method to query entries by hash
auto MasterDbRepository::querySavedBlobByHash(const std::string& hash) -> Expected<std::vector<BlobCopyDTO>, grpc::Status>{
    Logger::debug("MasterDbRepository::querySavedBlobByHash ", hash);
    std::vector<BlobCopyDTO> results;
        auto query = spanner::SqlStatement(
            "SELECT hash, worker_address, state, size_mb FROM blob_copy "
            "WHERE hash = $1 AND state = $2",
            {{"p1", spanner::Value(hash)}, {"p2", spanner::Value(BLOB_STATUS_SAVED)}});

        auto rows = client->ExecuteQuery(query);

        using rowType = std::tuple<std::string, std::string, std::string, int64_t>;
        for (auto const& row : spanner::StreamOf<rowType>(rows)) {
            if (!row) {
                return to_grpc_status(row.status());
            }

            std::string hash_, worker_address, state;
            hash_ = std::get<0>(*row);
            worker_address = std::get<1>(*row);
            state = std::get<2>(*row);
            int64_t size_mb = std::get<3>(*row);
            results.emplace_back(hash_, worker_address, state, size_mb);
        }

    return results;
}

auto MasterDbRepository::queryBlobByHashAndWorkerId(const std::string& hash, const std::string& worker_address) -> Expected<std::vector<BlobCopyDTO>, grpc::Status> {
    Logger::debug("MasterDbRepository::queryBlobByHashAndWorkerId ", hash, " ", worker_address);
    std::vector<BlobCopyDTO> results;
    auto query = spanner::SqlStatement(
        "SELECT hash, worker_address, state, size_mb FROM blob_copy "
        "WHERE hash = $1 AND worker_address = $2",
        {{"p1", spanner::Value(hash)}, {"p2", spanner::Value(worker_address)}});

    auto rows = client->ExecuteQuery(query);

    using rowType = std::tuple<std::string, std::string, std::string, int64_t>;
    for (auto const& row : spanner::StreamOf<rowType>(rows)) {
        if (!row) {
            return to_grpc_status(row.status());
        }

        std::string hash, worker_address, state;
        hash = std::get<0>(*row);
        worker_address = std::get<1>(*row);
        state = std::get<2>(*row);
        int64_t size_mb = std::get<3>(*row);
        results.emplace_back(hash, worker_address, state, size_mb);
    }

    return results;
}

auto MasterDbRepository::deleteBlobEntryByHash(const std::string& hash) -> Expected<std::monostate, grpc::Status>
{
    Logger::debug("MasterDbRepository::deleteBlobEntryByHash ", hash);
    auto mutation = MakeDeleteMutation(
        "blob_copy",
        spanner::KeySet().AddKey(
            spanner::MakeKey(hash)
        ).AddRange(
            spanner::MakeKeyBoundClosed(hash),
            spanner::MakeKeyBoundClosed(hash)
        )
    );

    auto commit_result = client->Commit(spanner::Mutations{mutation});

    if (!commit_result) {
        return to_grpc_status(commit_result.status());
    }
    return std::monostate();
}

auto MasterDbRepository::deleteBlobEntriesByWorkerAddress(const std::string& worker_address) -> Expected<std::monostate,
    grpc::Status>
{
    Logger::debug("MasterDbRepository::deleteBlobEntriesByWorkerAddress ", worker_address);
    std::string sql = "DELETE FROM blob_copy WHERE worker_address = $1";
    auto statement = spanner::SqlStatement(sql, {{"p1", spanner::Value(worker_address)}});

    auto commit_result = client->Commit([statement, this](spanner::Transaction txn)
        -> google::cloud::StatusOr<spanner::Mutations> {
            auto dele = client->ExecuteDml(std::move(txn), statement);
            if (!dele) return std::move(dele).status();
            return spanner::Mutations{};
    });

    if (!commit_result) {
        Logger::error(commit_result.status());
        return to_grpc_status(commit_result.status());
    }
    return std::monostate();
}


auto MasterDbRepository::addWorkerState(const WorkerStateDTO& worker_state) -> Expected<std::monostate, grpc::Status>
{
    Logger::debug("MasterDbRepository::addWorkerState ", worker_state.to_string());
    auto mutation = spanner::InsertMutationBuilder( "worker_state", {"worker_address",
        "available_space_mb", "locked_space_mb", "last_heartbeat_epoch_ts"})
        .EmplaceRow(worker_state.worker_address, worker_state.available_space_mb,
                    worker_state.locked_space_mb, worker_state.last_heartbeat_epoch_ts).Build();

    auto commit_result = client->Commit(spanner::Mutations{mutation});

    if (!commit_result) {
        return to_grpc_status(commit_result.status());
    }
    return std::monostate();
}

auto MasterDbRepository::updateWorkerState(const WorkerStateDTO& worker_state) -> Expected<std::monostate, grpc::Status>
{
    Logger::debug("MasterDbRepository::updateWorkerState ", worker_state.to_string());
    auto mutation = spanner::UpdateMutationBuilder(
        "worker_state",
        {"worker_address", "available_space_mb", "locked_space_mb", "last_heartbeat_epoch_ts"})
    .EmplaceRow(worker_state.worker_address, worker_state.available_space_mb,
                worker_state.locked_space_mb, worker_state.last_heartbeat_epoch_ts).Build();

    auto commit_result = client->Commit(spanner::Mutations{mutation});

    if (!commit_result) {
        return to_grpc_status(commit_result.status());
    }
    return std::monostate();
}

auto MasterDbRepository::deleteWorkerState(const std::string& worker_address) -> Expected<std::monostate, grpc::Status>
{
    Logger::debug("MasterDbRepository::deleteWorkerState ", worker_address);
    auto mutation = spanner::DeleteMutationBuilder("worker_state", spanner::KeySet().AddKey(
        spanner::MakeKey(worker_address))).Build();

    auto commit_result = client->Commit(spanner::Mutations{mutation});

    if (!commit_result) {
        return to_grpc_status(commit_result.status());
    }
    return std::monostate();
}

auto MasterDbRepository::getWorkerState(const std::string& worker_address) -> Expected<WorkerStateDTO, grpc::Status> {
    Logger::debug("MasterDbRepository::getWorkerState ", worker_address);
    auto query = spanner::SqlStatement(
        "SELECT worker_address, available_space_mb, locked_space_mb, last_heartbeat_epoch_ts "
        "FROM worker_state "
        "WHERE worker_address = $1",
        {{"p1", spanner::Value(worker_address)}});

    auto rows = client->ExecuteQuery(query);
    auto stream = spanner::StreamOf<std::tuple<std::string, int64_t, int64_t, int64_t>>(rows);

    for (auto const& row : stream) {
        if (!row) {
            return to_grpc_status(row.status());
        }
        return WorkerStateDTO{
            std::get<0>(*row),
            std::get<1>(*row),
            std::get<2>(*row),
            std::get<3>(*row)
        };
    }

    return grpc::Status(grpc::NOT_FOUND, "No worker state exists with given id");
}

auto MasterDbRepository::getWorkersWithFreeSpace(int64_t spaceNeeded, int32_t num_workers) -> Expected<std::vector<WorkerStateDTO>, grpc::Status> {
    Logger::debug("MasterDbRepository::getWorkersWithFreeSpace ", spaceNeeded, " ", num_workers);
    auto query = spanner::SqlStatement(
        "SELECT worker_address, available_space_mb, locked_space_mb, last_heartbeat_epoch_ts "
        "FROM worker_state "
        "WHERE available_space_mb - locked_space_mb >= $1 "
        "LIMIT $2",
        {{"p1", spanner::Value(spaceNeeded)}, {"p2", spanner::Value(num_workers)}});

    auto rows = client->ExecuteQuery(query);
    auto stream = spanner::StreamOf<std::tuple<std::string, int64_t, int64_t, int64_t>>(rows);
    std::vector<WorkerStateDTO> result;

    for (auto const& row : stream) {
        if (!row) {
            return to_grpc_status(row.status());
        }
        result.emplace_back(
            std::get<0>(*row),
            std::get<1>(*row),
            std::get<2>(*row),
            std::get<3>(*row)
        );
    }

    if (result.size() < num_workers) {
        return grpc::Status(
            grpc::RESOURCE_EXHAUSTED,
            "Requested " + std::to_string(num_workers) + " workers, but only "
            + std::to_string(result.size()) + " workers matching the criteria exist"
        );
    }

    return result;
}