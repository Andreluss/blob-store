//
// Created by mateusz on 14.01.25.
//

#ifndef BLOB_STORE_WORKER_SERVICE_HPP
#define BLOB_STORE_WORKER_SERVICE_HPP

#include <filesystem>
#include <fstream>
#include "services/worker_service.grpc.pb.h"
#include "services/master_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>

// We assume that blobs are stored in the blobs/ directory which is created in the same
// directory as the executable.
const std::string BLOBS_PATH = "blobs/";
const uint64_t MAX_CHUNK_SIZE = 1024 * 1024;


class WorkerServiceImpl final : public worker::WorkerService::Service {
public:
    explicit WorkerServiceImpl(const std::shared_ptr<grpc::Channel>& channel)
            : master_stub_(master::MasterService::NewStub(channel)) {}

    grpc::Status Healthcheck(
            grpc::ServerContext *context,
            const worker::HealthcheckRequest *request,
            worker::HealthcheckResponse *response) override {
        response->set_message("OK");
        return grpc::Status::OK;
    }

    grpc::Status GetFreeStorage(
            grpc::ServerContext *context,
            const worker::GetFreeStorageRequest *request,
            worker::GetFreeStorageResponse *response) override {
        auto [status, storage] = get_free_storage();
        if (status == RetCode::ERROR_FILESYSTEM) {
            response->set_message("Error while getting free storage.");
            return grpc::Status::CANCELLED;
        }
        response->set_message("OK");
        response->set_storage(storage);
        return grpc::Status::OK;
    }

    grpc::Status SaveBlob(
            grpc::ServerContext *context,
            grpc::ServerReader<worker::SaveBlobRequest> *reader,
            worker::SaveBlobResponse *response) override {

        worker::SaveBlobRequest request;
        bool created_file = false;
        std::string calculated_hash;

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

            calculated_hash = recalculate_hash(request.chunk_data(), calculated_hash);

            if (append_to_file(request.hash(), request.chunk_data()) == RetCode::ERROR_FILESYSTEM) {
                response->set_message("Error appending bytes to file.");
                delete_file(request.hash());
                return grpc::Status::CANCELLED;
            }
        }

        if (calculated_hash != request.hash()) {
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

    grpc::Status GetBlob(
            grpc::ServerContext *context,
            const worker::GetBlobRequest *request,
            grpc::ServerWriter<worker::GetBlobResponse> *writer) override {

        uint64_t bytes_sent = 0;
        worker::GetBlobResponse response;
        RetCode return_value;

        while ((return_value = set_next_chunk(response, bytes_sent, request->hash())) ==
        RetCode::SUCCESS) {
            bytes_sent += response.size_bytes();
            writer->Write(response);
        }

        if (return_value == RetCode::FINISHED) {
            return grpc::Status::OK;
        } else {
            return grpc::Status::CANCELLED;
        }
    }

    grpc::Status DeleteBlob(
            grpc::ServerContext *context,
            const worker::DeleteBlobRequest *request,
            worker::DeleteBlobResponse *response) override {
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

private:
    std::unique_ptr<master::MasterService::Stub> master_stub_;

    enum RetCode {
        SUCCESS,
        FINISHED,
        ERROR_FILE_NOT_FOUND,
        ERROR_FILESYSTEM,
    };

    static std::string recalculate_hash(const std::string &data, const std::string &previous_hash) {
        // TODO: Decide how we want to calculate hash.
        return previous_hash;
    }

    static std::pair<RetCode, uint64_t> get_free_storage() {
        try {
            auto space = std::filesystem::space(BLOBS_PATH);
            return {RetCode::SUCCESS, (uint64_t) space.free};
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Error: " << e.what() << '\n';
            return {RetCode::ERROR_FILESYSTEM, uint64_t(0)};
        }
    }

    static bool try_create_blob_file(const std::string &hash) {
        auto filepath = BLOBS_PATH + hash;

        std::ofstream outfile(filepath, std::ios::out);
        if (!outfile) {
            std::cerr << "Error: Unable to create file: " << filepath << '\n';
            return false;
        }
        outfile.close();
        return true;
    }

    static RetCode append_to_file(const std::string &hash, const std::string &data) {
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

    static RetCode set_next_chunk(
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

        response.set_size_bytes(bytes_read);
        response.set_chunk_data(std::string(buffer, bytes_read));

        return infile.eof() ? RetCode::FINISHED : RetCode::SUCCESS;
    }

    static RetCode delete_file(const std::string &hash) {
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
};

#endif //BLOB_STORE_WORKER_SERVICE_HPP
