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

#define BLOB_STATUS_DURING_CREATION "DURING_CREATION"
#define BLOB_STATUS_SAVED "SAVED"

namespace spanner = ::google::cloud::spanner;

struct BlobCopyDTO {
    std::string hash, worker_address, state;
    int64_t size_mb;
    BlobCopyDTO(std::string hash, std::string worker_address, std::string state, int64_t size_mb) :
        hash(std::move(hash)), worker_address(std::move(worker_address)), state(std::move(state)), size_mb(size_mb) {}
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
    bool addBlobEntry(const BlobCopyDTO &entry) const;
    bool updateBlobEntry(const BlobCopyDTO& entry) const;
    std::vector<BlobCopyDTO> querySavedBlobByHash(const std::string& hash) const;
    std::vector<BlobCopyDTO> queryBlobByHashAndWorkerId(const std::string& hash, const std::string& worker_address) const;
    bool deleteBlobEntryByHash(const std::string& hash) const;
    bool deleteBlobEntriesByWorkerAddress(const std::string& worker_address) const;
    bool addWorkerState(const WorkerStateDTO& worker_state) const;
    bool updateWorkerState(const WorkerStateDTO& worker_state) const;
    bool deleteWorkerState(const std::string& worker_address) const;
    WorkerStateDTO getWorkerState(const std::string& worker_address) const;
    std::vector<WorkerStateDTO> getWorkersWithFreeSpace(int64_t spaceNeeded, int32_t num_workers) const;

private:
    std::shared_ptr<spanner::Client> client;
};
#endif //MASTER_DB_REPOSITORY_HPP
