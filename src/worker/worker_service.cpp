#include "worker_service.hpp"
#include "blob_hasher.hpp"
#include "blob_file.hpp"
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

auto receive_blob_from_frontend(grpc::ServerReader<worker::SaveBlobRequest> *reader) -> Expected<std::string, grpc::Status> {
    worker::SaveBlobRequest request;

    try {
        BlobHasher blob_hasher;
        std::optional<BlobFile> blob_file;
        std::string request_hash;

        while (reader->Read(&request)) {
            if (request_hash.empty()) {
                request_hash = request.hash();
                blob_file = BlobFile::New(BLOBS_PATH + request_hash);
            }

            blob_file->append_chunk(request.chunk_data());
            blob_hasher.add_chunk(request.chunk_data());
        }

        auto blob_hash = blob_hasher.finalize();
        if (request_hash!= blob_hash) {
            return grpc::Status(grpc::INVALID_ARGUMENT, "Blob hash mismatch.");
        }

        return blob_hash;
    }
    catch (const BlobFile::FileSystemException& fse) {
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
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

auto delete_file(const std::string &hash) -> Expected<std::monostate, std::string> {
    auto filepath = BLOBS_PATH + hash;

    try {
        if (std::filesystem::exists(filepath)) {
            std::filesystem::remove(filepath);
        } else {
            return "Error: File not found: " + filepath;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        return "Error: Filesystem error.";
    }

    return std::monostate{};
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
            [](auto _) { return grpc::Status::OK; },
            [&](auto &error) {
                response->set_message(error);
                return grpc::Status::CANCELLED;
            }
        );
}

grpc::Status WorkerServiceImpl::SaveBlob(grpc::ServerContext *context,
                                         grpc::ServerReader<worker::SaveBlobRequest> *reader,
                                         worker::SaveBlobResponse *response) {

    return receive_blob_from_frontend(reader)
        .and_then([&](const std::string &hash) -> Expected<std::monostate, grpc::Status> {
            master::NotifyBlobSavedRequest notify_request;
            notify_request.set_workderid("workerId");
            notify_request.set_blobid(hash);

            grpc::ClientContext client_context;
            master::NotifyBlobSavedResponse notify_response;

            auto status = master_stub_->NotifyBlobSaved(&client_context, notify_request,
                                                        &notify_response);

            if (status.ok()) {
                return std::monostate{};
            }
            else {
                return grpc::Status(grpc::CANCELLED, status.error_message());
            }
        })
        .output<grpc::Status>(
            [](auto _) { return grpc::Status::OK; },
            [&](auto &error) { return grpc::Status::CANCELLED; }
        );
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

    return delete_file(request->hash())
        .output<grpc::Status>(
            [&](auto _) {
                response->set_message("OK");
                return grpc::Status::OK;
            },
            [&](auto &error) {
                response->set_message(error);
                return grpc::Status::CANCELLED;
            }
        );
}
///---- END WORKER SERVICE ----///
