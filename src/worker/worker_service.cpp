#include "worker_service.hpp"
#include "blob_hasher.hpp"
#include "blob_file.hpp"
#include "expected.hpp"
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include "services/worker_service.grpc.pb.h"
#include "services/master_service.grpc.pb.h"

///---- BEGIN HELPERS ----///
auto get_free_storage() -> Expected<uint64_t, grpc::Status> {
    try {
        auto space = std::filesystem::space(BLOBS_PATH);
        return (uint64_t) space.free;
    } catch (const std::filesystem::filesystem_error &fse) {
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
                request_hash = request.hash();
                blob_file = BlobFile::New(request_hash);
            }

            blob_file->append_chunk(request.chunk_data());
            blob_hasher.add_chunk(request.chunk_data());
        }

        auto blob_hash = blob_hasher.finalize();
        if (request_hash != blob_hash) {
            return grpc::Status(grpc::INVALID_ARGUMENT, "Blob hash mismatch.");
        }

        return blob_hash;
    }
    catch (const BlobFile::FileSystemException &fse) {
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}

auto send_blob_to_frontend(const worker::GetBlobRequest *request,
                           grpc::ServerWriter<worker::GetBlobResponse> *writer) -> Expected<std::monostate, grpc::Status> {
    try {
        BlobFile blob_file = BlobFile::Load(request->hash());
        for (auto chunk: blob_file) {
            worker::GetBlobResponse response;
            response.set_chunk_data(chunk);
            if (not writer->Write(response)) {
                return grpc::Status(grpc::INVALID_ARGUMENT, "Write stream was closed.");
            }
        }
        return std::monostate{};
    }
    catch (const BlobFile::FileSystemException &fse) {
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}

auto delete_file(const std::string &hash) -> Expected<std::monostate, grpc::Status> {
    auto filepath = BLOBS_PATH + hash;

    try {
        if (not std::filesystem::exists(filepath)) {
            return grpc::Status(grpc::CANCELLED, "Error: File not found: " + filepath);
        }

        std::filesystem::remove(filepath);
        return std::monostate{};
    }
    catch (const BlobFile::FileSystemException &fse) {
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}
///---- END HELPERS ----///

///---- BEGIN WORKER SERVICE ----///
grpc::Status WorkerServiceImpl::Healthcheck(grpc::ServerContext *context,
                                            const worker::HealthcheckRequest *request,
                                            worker::HealthcheckResponse *response) {
    return grpc::Status::OK;
}

grpc::Status WorkerServiceImpl::GetFreeStorage(grpc::ServerContext *context,
                                               const worker::GetFreeStorageRequest *request,
                                               worker::GetFreeStorageResponse *response) {
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
                } else {
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

    return send_blob_to_frontend(request, writer)
            .output<grpc::Status>(
                    [](auto _) { return grpc::Status::OK; },
                    std::identity()
            );
}

grpc::Status WorkerServiceImpl::DeleteBlob(grpc::ServerContext *context,
                                           const worker::DeleteBlobRequest *request,
                                           worker::DeleteBlobResponse *response) {

    return delete_file(request->hash())
            .output<grpc::Status>(
                    [](auto _) { return grpc::Status::OK; },
                    std::identity()
            );
}
///---- END WORKER SERVICE ----///
