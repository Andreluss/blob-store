#include "worker_service.hpp"
#include "blob_hasher.hpp"
#include "expected.hpp"
#include <filesystem>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include "services/worker_service.grpc.pb.h"
#include "services/master_service.grpc.pb.h"

///---- BEGIN HELPERS ----///
enum RetCode {
    SUCCESS,
    FINISHED,
    ERROR_FILE_NOT_FOUND,
    ERROR_FILESYSTEM,
};

auto get_free_storage() -> Expected<uint64_t, std::string> {
    try {
        auto space = std::filesystem::space(BLOBS_PATH);
        return (uint64_t) space.free;
    } catch (const std::filesystem::filesystem_error &e) {
        return "Error while getting free storage.";
    }
}

auto try_create_blob_file(const std::string &hash) {
    auto filepath = BLOBS_PATH + hash;

    std::ofstream outfile(filepath, std::ios::out);
    if (!outfile) {
        std::cerr << "Error: Unable to create file: " << filepath << '\n';
        return false;
    }
    outfile.close();
    return true;
}

auto append_to_file(const std::string &hash, const std::string &data) {
    auto filepath = BLOBS_PATH + hash;

    std::ofstream outfile(filepath, std::ios::binary | std::ios::app);
    if (!outfile) {
        std::cerr << "Error: Unable to open file for appending: " << filepath << '\n';
        return RetCode::ERROR_FILESYSTEM;
    }
    outfile.write(data.c_str(), (ssize_t) data.size());
    outfile.close();
    return RetCode::SUCCESS;
}

auto set_next_chunk(
        worker::GetBlobResponse &response,
        uint64_t already_sent,
        const std::string &hash) {
    auto filepath = BLOBS_PATH + hash;

    std::ifstream infile(filepath, std::ios::binary);
    if (!infile) {
        std::cerr << "Error: Unable to open file for reading: " << filepath << '\n';
        return RetCode::ERROR_FILESYSTEM;
    }

    infile.seekg((ssize_t)already_sent, std::ios::beg);
    if (infile.eof()) {
        return RetCode::FINISHED;
    }

    char buffer[MAX_CHUNK_SIZE];
    infile.read(buffer, MAX_CHUNK_SIZE);
    size_t bytes_read = infile.gcount();

    response.set_chunk_data(std::string(buffer, bytes_read));

    return infile.eof() ? RetCode::FINISHED : RetCode::SUCCESS;
}

auto delete_file(const std::string &hash) {
    auto filepath = BLOBS_PATH + hash;

    try {
        if (std::filesystem::exists(filepath)) {
            std::filesystem::remove(filepath);
        } else {
            std::cerr << "Error: File does not exist: " << filepath << '\n';
            return RetCode::ERROR_FILE_NOT_FOUND;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Filesystem error: " << e.what() << '\n';
        return RetCode::ERROR_FILESYSTEM;
    }

    return RetCode::SUCCESS;
}
///---- END HELPERS ----///

///---- BEGIN WORKER SERVICE ----///
grpc::Status WorkerServiceImpl::Healthcheck(grpc::ServerContext *context,
                                            const worker::HealthcheckRequest *request,
                                            worker::HealthcheckResponse *response) {
    response->set_message("OK");
    return grpc::Status::OK;
}

grpc::Status WorkerServiceImpl::GetFreeStorage(grpc::ServerContext *context,
                                               const worker::GetFreeStorageRequest *request,
                                               worker::GetFreeStorageResponse *response) {
    return get_free_storage()
        .and_then([&](uint64_t storage) -> Expected<std::monostate, std::string> {
            response->set_message("OK");
            response->set_storage(storage);
            return std::monostate{};
        })
        .output<grpc::Status>(
            [](auto _) {return grpc::Status::OK;},
            [&](auto &error) {
                response->set_message(error);
                return grpc::Status::CANCELLED;
            }
        );
}

grpc::Status WorkerServiceImpl::SaveBlob(grpc::ServerContext *context,
                                         grpc::ServerReader<worker::SaveBlobRequest> *reader,
                                         worker::SaveBlobResponse *response) {
    worker::SaveBlobRequest request;
    bool created_file = false;
    BlobHasher blob_hasher;

    auto create_file_if_needed = [&](const std::string &filename) -> RetCode {
        if (not created_file) {
            if (not try_create_blob_file(filename)) {
                return RetCode::ERROR_FILESYSTEM;
            }
            created_file = true;
        }
        return RetCode::SUCCESS;
    };

    while (reader->Read(&request)) {
        if (create_file_if_needed(request.hash()) == RetCode::ERROR_FILESYSTEM) {
            response->set_message("Error while creating file.");
            return grpc::Status::CANCELLED;
        }

        blob_hasher.add_chunk(request.chunk_data());

        if (append_to_file(request.hash(), request.chunk_data()) == RetCode::ERROR_FILESYSTEM) {
            response->set_message("Error appending bytes to file.");
            delete_file(request.hash());
            return grpc::Status::CANCELLED;
        }
    }

    if (blob_hasher.finalize() != request.hash()) {
        response->set_message("Error: Hash mismatch.");
        return grpc::Status::CANCELLED;
    }

    auto notify_master = [&]() -> grpc::Status {
        master::NotifyBlobSavedRequest notify_request;
        notify_request.set_workderid("workerId");
        notify_request.set_blobid(request.hash());

        grpc::ClientContext client_context;
        master::NotifyBlobSavedResponse notify_response;

        auto status = master_stub_->NotifyBlobSaved(&client_context, notify_request,
                                                    &notify_response);
        if (!status.ok()) {
            std::cerr << "Error: " << status.error_message() << '\n';
            response->set_message("Error notifying master.");
            return grpc::Status::CANCELLED;
        }
        else {
            response->set_message("OK");
            return grpc::Status::OK;
        }
    };

    return notify_master();
}

grpc::Status WorkerServiceImpl::GetBlob(grpc::ServerContext *context,
                                        const worker::GetBlobRequest *request,
                                        grpc::ServerWriter<worker::GetBlobResponse> *writer) {

    uint64_t bytes_sent = 0;
    worker::GetBlobResponse response;
    RetCode return_value;

    while ((return_value = set_next_chunk(response, bytes_sent, request->hash())) ==
           RetCode::SUCCESS) {
        bytes_sent += response.chunk_data().size();
        writer->Write(response);
    }

    if (return_value == RetCode::FINISHED) {
        return grpc::Status::OK;
    } else {
        return grpc::Status::CANCELLED;
    }
}

grpc::Status WorkerServiceImpl::DeleteBlob(grpc::ServerContext *context,
                                           const worker::DeleteBlobRequest *request,
                                           worker::DeleteBlobResponse *response) {
    auto return_code = delete_file(request->hash());

    switch (return_code) {
        case RetCode::SUCCESS:
            response->set_message("OK");
            return grpc::Status::OK;
        case RetCode::ERROR_FILE_NOT_FOUND:
            response->set_message("Error: File not found.");
            return grpc::Status::CANCELLED;
        case RetCode::ERROR_FILESYSTEM:
            response->set_message("Error: Filesystem error.");
            return grpc::Status::CANCELLED;
        default:
            response->set_message("Error: Unknown error.");
            return grpc::Status::CANCELLED;
    }
}
///---- END WORKER SERVICE ----///
