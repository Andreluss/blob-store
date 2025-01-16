//
// Created by mjacniacki on 16.01.25.
//
#ifndef MASTER_DB_REPOSITORY_HPP
#define MASTER_DB_REPOSITORY_HPP

#include <google/cloud/spanner/client.h>
#include <string>
#include <vector>
#include <tuple>
#include <memory>

namespace spanner = ::google::cloud::spanner;

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
    bool addEntry(const std::string& uuid,
                 const std::string& hash,
                 const std::string& worker_id) const;

    std::vector<std::tuple<std::string, std::string, std::string>>
    queryByHash(const std::string& hash);

    bool deleteEntry(const std::string& uuid) const;

private:
    std::shared_ptr<spanner::Client> client;
};

#endif //MASTER_DB_REPOSITORY_HPP