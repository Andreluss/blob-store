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

namespace spanner = ::google::cloud::spanner;

struct BlobCopyDTO {
    std::string uuid, hash, worker_id, state;
    BlobCopyDTO(std::string  uuid, std::string hash, std::string worker_id, std::string state) :
        uuid(std::move(uuid)), hash(std::move(hash)), worker_id(std::move(worker_id)), state(std::move(state)) {}
};

struct WorkerStateDTO {
    std::string worker_id;
    std::string ip_address;
    int64_t available_space_mb;
    int64_t locked_space_mb;
    int64_t last_heartbeat_epoch_ts;

    WorkerStateDTO(std::string worker_id,
                  std::string ip_address,
                  int64_t available_space_mb,
                  int64_t locked_space_mb,
                  int64_t last_heartbeat_epoch_ts)
        : worker_id(std::move(worker_id))
        , ip_address(std::move(ip_address))
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
    std::vector<BlobCopyDTO> queryBlobByHash(const std::string& hash) const;
    bool deleteBlobEntry(const std::string& uuid) const;
    bool addWorkerState(const WorkerStateDTO& worker_state) const;
    bool updateWorkerState(const WorkerStateDTO& worker_state) const;
    bool deleteWorkerState(const std::string& worker_id) const;
    WorkerStateDTO getWorkerState(const std::string& worker_id) const;
    std::vector<WorkerStateDTO> getWorkersWithFreeSpace(int64_t spaceNeeded, int32_t num_workers) const;

private:
    std::shared_ptr<spanner::Client> client;
};
#endif //MASTER_DB_REPOSITORY_HPP