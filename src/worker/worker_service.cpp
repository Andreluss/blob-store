#include "worker_service.hpp"
#include "blob_file.hpp"
#include "blob_hasher.hpp"
#include "expected.hpp"
#include "logging.hpp"
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include "services/worker_service.grpc.pb.h"
#include "services/master_service.grpc.pb.h"

///---- BEGIN HELPERS ----///
auto get_free_storage() -> Expected<uint64_t, grpc::Status> {
    try {
        auto space = std::filesystem::space(BLOBS_PATH);
        Logger::info("Free storage: ", space.free);
        return (uint64_t) space.free;
    } catch (const std::filesystem::filesystem_error &fse) {
        Logger::error("Error while getting free storage: ", fse.what());
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}

auto receive_blob_from_frontend(
        grpc::ServerReader<worker::SaveBlobRequest> *reader) -> Expected<std::string, grpc::Status> {
    worker::SaveBlobRequest request;

    try {
        BlobHasher blob_hasher;
        std::optional<BlobFile> blob_file;
        std::string request_hash;

        while (reader->Read(&request)) {
            if (request_hash.empty()) {
                request_hash = request.blob_hash();
                Logger::info("Start receiving, hash: ", request_hash);
                blob_file = BlobFile::New(request_hash);
            }

            Logger::info("Received chunk size: ", ssize(request.chunk_data()));
            *blob_file += request.chunk_data();
            blob_hasher += request.chunk_data();
        }

        auto blob_hash = blob_hasher.finalize();
        if (request_hash != blob_hash) {
            Logger::error("Blob hash mismatch: ", request_hash, " != ", blob_hash);
            blob_file->remove();
            return grpc::Status(grpc::INVALID_ARGUMENT, "Blob hash mismatch.");
        }

        Logger::info("Finish receiving, hash: ", request_hash);
        return request_hash;
    }
    catch (const BlobFile::FileSystemException &fse) {
        Logger::error("Error while receiving blob: ", fse.what());
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}

auto send_blob_to_frontend(const worker::GetBlobRequest *request,
                           grpc::ServerWriter<worker::GetBlobResponse> *writer) -> Expected<std::monostate, grpc::Status> {
    try {
        BlobFile blob_file = BlobFile::Load(request->blob_hash());
        for (auto chunk: blob_file) {
            worker::GetBlobResponse response;
            response.set_chunk_data(chunk);
            if (not writer->Write(response)) {
                Logger::error("Write stream was closed.");
                return grpc::Status(grpc::INVALID_ARGUMENT, "Write stream was closed.");
            }
            Logger::info("Sent chunk size: ", ssize(chunk));
        }
        Logger::info("Blob sent successfully.");
        return std::monostate{};
    }
    catch (const BlobFile::FileSystemException &fse) {
        Logger::error("Error while sending blob: ", fse.what());
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}

auto delete_file(const std::string &hash) -> Expected<std::monostate, grpc::Status> {
    auto filepath = BLOBS_PATH + hash;

    try {
        if (not std::filesystem::exists(filepath)) {
            Logger::error("Error while deleting blob: file not found ", filepath);
            return grpc::Status(grpc::CANCELLED, "Error while deleting blob: file not found " + filepath);
        }

        std::filesystem::remove(filepath);
        Logger::info("Blob deleted successfylly: ", filepath);
        return std::monostate{};
    }
    catch (const BlobFile::FileSystemException &fse) {
        Logger::error("Error while deleting blob: ", fse.what());
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}
///---- END HELPERS ----///

///---- BEGIN WORKER SERVICE ----///
grpc::Status WorkerServiceImpl::Healthcheck(grpc::ServerContext *context,
                                            const worker::HealthcheckRequest *request,
                                            worker::HealthcheckResponse *response) {
    Logger::info("Healthcheck request received");

    return grpc::Status::OK;
}

grpc::Status WorkerServiceImpl::GetFreeStorage(grpc::ServerContext *context,
                                               const worker::GetFreeStorageRequest *request,
                                               worker::GetFreeStorageResponse *response) {
    Logger::info("GetFreeStorage request received");

    return get_free_storage()
            .and_then([&](uint64_t storage) -> Expected<std::monostate, grpc::Status> {
                response->set_storage(storage);
                return std::monostate{};
            })
            .output<grpc::Status>(
                    [](auto _) { return grpc::Status::OK; },
                    std::identity()
            );
}

grpc::Status WorkerServiceImpl::SaveBlob(grpc::ServerContext *context,
                                         grpc::ServerReader<worker::SaveBlobRequest> *reader,
                                         worker::SaveBlobResponse *response) {
    Logger::info("SaveBlob request received");

    return receive_blob_from_frontend(reader)
            .and_then([&](const std::string &hash) -> Expected<std::monostate, grpc::Status> {
                master::NotifyBlobSavedRequest notify_request;
                notify_request.set_worker_id("workerId");
                notify_request.set_blob_hash(hash);
                Logger::info("Notifying master: ", notify_request.DebugString());

                grpc::ClientContext client_context;
                master::NotifyBlobSavedResponse notify_response;

                auto status = master_stub_->NotifyBlobSaved(&client_context, notify_request,
                                                            &notify_response);

                if (status.ok()) {
                    Logger::info("Notified master successfully.");
                    return std::monostate{};
                } else {
                    Logger::error("Error while notifying master: ", status.error_message());
                    return grpc::Status(grpc::CANCELLED, status.error_message());
                }
            })
            .output<grpc::Status>(
                    [](auto _) { return grpc::Status::OK; },
                    std::identity()
            );
}

grpc::Status WorkerServiceImpl::GetBlob(grpc::ServerContext *context,
                                        const worker::GetBlobRequest *request,
                                        grpc::ServerWriter<worker::GetBlobResponse> *writer) {
    Logger::info("GetBlob request received");

    return send_blob_to_frontend(request, writer)
            .output<grpc::Status>(
                    [](auto _) { return grpc::Status::OK; },
                    std::identity()
            );
}

grpc::Status WorkerServiceImpl::DeleteBlob(grpc::ServerContext *context,
                                           const worker::DeleteBlobRequest *request,
                                           worker::DeleteBlobResponse *response) {
    Logger::info("DeleteBlob request received");

    return delete_file(request->blob_hash())
            .output<grpc::Status>(
                    [](auto _) { return grpc::Status::OK; },
                    std::identity()
            );
}
///---- END WORKER SERVICE ----///
