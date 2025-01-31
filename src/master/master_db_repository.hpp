//
// Created by mjacniacki on 16.01.25.
//
#ifndef MASTER_DB_REPOSITORY_HPP
#define MASTER_DB_REPOSITORY_HPP

#include <google/cloud/spanner/client.h>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "expected.hpp"

#define BLOB_STATUS_DURING_CREATION "DURING_CREATION"
#define BLOB_STATUS_SAVED "SAVED"

namespace spanner = ::google::cloud::spanner;

struct BlobCopyDTO {
    std::string hash, worker_address, state;
    int64_t size_mb;
    BlobCopyDTO(std::string hash, std::string worker_address, std::string state, int64_t size_mb) :
        hash(std::move(hash)), worker_address(std::move(worker_address)), state(std::move(state)), size_mb(size_mb) {}
    [[nodiscard]] std::string to_string() const
    {
        return "hash: " + hash + ", "
             + "worker_address: " + worker_address + ", "
             + "state: " + state + ", "
             + "size_mb: " + std::to_string(size_mb);
    }
};

struct WorkerStateDTO {
    std::string worker_address;
    int64_t available_space_mb;
    int64_t locked_space_mb;
    int64_t last_heartbeat_epoch_ts;

    WorkerStateDTO(std::string worker_address,
                  int64_t available_space_mb,
                  int64_t locked_space_mb,
                  int64_t last_heartbeat_epoch_ts)
        : worker_address(std::move(worker_address))
        , available_space_mb(available_space_mb)
        , locked_space_mb(locked_space_mb)
        , last_heartbeat_epoch_ts(last_heartbeat_epoch_ts) {}
    [[nodiscard]] std::string to_string() const
    {
        return "worker_address: " + worker_address + ", "
             + "available_space_mb: " + std::to_string(available_space_mb) + ", "
             + "locked_space_mb: " + std::to_string(locked_space_mb);
    }
};

class MasterDbRepository {
public:
    // Constructor and destructor
    MasterDbRepository(std::string const& project_id,
                      std::string const& instance_id,
                      std::string const& database_id);
    ~MasterDbRepository() = default;

    // Delete copy constructor and assignment operator
    MasterDbRepository(const MasterDbRepository&) = delete;
    MasterDbRepository& operator=(const MasterDbRepository&) = delete;

    // Database operations
    auto addBlobEntry(const BlobCopyDTO& entry) -> Expected<std::monostate, grpc::Status>;
    auto updateBlobEntry(const BlobCopyDTO& entry) -> Expected<std::monostate, grpc::Status>;
    auto querySavedBlobByHash(const std::string& hash) -> Expected<std::vector<BlobCopyDTO>, grpc::Status>;
    auto queryBlobByHashAndWorkerId(const std::string& hash, const std::string& worker_address) -> Expected<std::vector<BlobCopyDTO>, grpc:: Status>;
    auto deleteBlobEntryByHash(const std::string& hash) -> Expected<std::monostate, grpc::Status>;
    auto deleteBlobEntriesByWorkerAddress(const std::string& worker_address) -> Expected<std::monostate, grpc::Status>;
    auto addWorkerState(const WorkerStateDTO& worker_state) -> Expected<std::monostate, grpc::Status>;
    auto updateWorkerState(const WorkerStateDTO& worker_state) -> Expected<std::monostate, grpc::Status>;
    auto deleteWorkerState(const std::string& worker_address) -> Expected<std::monostate, grpc::Status>;
    auto getWorkerState(const std::string& worker_address) -> Expected<WorkerStateDTO, grpc::Status>;
    auto getWorkersWithFreeSpace(int64_t spaceNeeded, int32_t num_workers) -> Expected<std::vector<WorkerStateDTO>, grpc::Status>;

private:
    std::shared_ptr<spanner::Client> client;
};
#endif //MASTER_DB_REPOSITORY_HPP
