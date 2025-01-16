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
bool MasterDbRepository::addEntry(const std::string& uuid,
             const std::string& hash,
             const std::string& worker_id) const
{
    try {
        auto mutation = spanner::InsertMutationBuilder(
            "blob_copy",
            {"uuid", "hash", "worker_id"})
            .EmplaceRow(uuid, hash, worker_id)
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
std::vector<std::tuple<std::string, std::string, std::string>> MasterDbRepository::queryByHash(const std::string& hash) {
    std::vector<std::tuple<std::string, std::string, std::string>> results;
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
            hash = std::get<0>(*row);
            worker_id = std::get<0>(*row);

            results.emplace_back(uuid, hash, worker_id);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error querying entries: " << e.what() << std::endl;
    }

    return results;
}

// Method to delete entry by UUID
bool MasterDbRepository::deleteEntry(const std::string& uuid) const
{
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

