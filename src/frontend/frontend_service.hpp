//
// Created by andrez on 1/18/25.
//
#pragma once
#include "services/master_service.grpc.pb.h"
#include "services/frontend_service.grpc.pb.h"
#include "services/worker_service.grpc.pb.h"
#include "blob_hasher.hpp"
#include <filesystem>
#include <fstream>
#include "utils.hpp"
#include "expected.hpp"

// User-defined literal "_S" that converts C-string to std::string
static std::string operator""_S(const char* str, std::size_t) {
    return {str};
}

class FrontendServiceImpl final : public frontend::Frontend::Service
{
    std::unique_ptr<master::MasterService::Stub> master_stub_;

    static std::string failed_request(const std::string& error_message, const std::string& performed_action) {
        std::cerr << "Failed to " << performed_action << ": " << error_message << std::endl;
        return "Failed to " + performed_action + ".";
    }

    // Returns an optional ERROR string
    static std::optional<std::string> send_blob_to_worker(const BlobFile& blob, const std::string& blob_hash, common::ipv4Address worker_address)
    {
        try
        {
            const auto address_string = address_to_string(worker_address);
            std::cerr << "Sending blob to worker at " << address_string << std::endl;

            const auto worker_channel = grpc::CreateChannel(address_string, grpc::InsecureChannelCredentials());
            const auto worker_stub = worker::WorkerService::NewStub(worker_channel);

            grpc::ClientContext client_context;
            worker::SaveBlobResponse save_blob_response;
            const auto writer = worker_stub->SaveBlob(&client_context, &save_blob_response);

            for (const auto& chunk: blob)
            {
                worker::SaveBlobRequest save_blob_request;
                save_blob_request.set_hash(blob_hash);
                save_blob_request.set_size_bytes(blob.size());
                save_blob_request.set_chunk_data(chunk.data(), chunk.size());
                if (!writer->Write(save_blob_request)) {
                    return "Failed to save blob to worker - broken stream";
                }
            }

            return std::nullopt;
        }
        catch (const BlobFile::FileSystemException& fse)
        {
            return "Error while sending blob to worker: "_S + fse.what();
        }
    }

    Expected<std::pair<BlobFile, std::string>, grpc::Status> receive_and_hash_blob(
        grpc::ServerReader<frontend::UploadBlobRequest>* reader)
    {
        frontend::UploadBlobRequest request;

        // 1. Read blob info
        reader->Read(&request);
        if (!request.has_info()) {
            return grpc::Status(grpc::INVALID_ARGUMENT, "Request should start with blob info.");
        }
        const auto blob_size = request.info().size_bytes();

        // 2. Read chunks
        try
        {
            BlobHasher blob_hasher;
            auto blob_file = BlobFile::New("temp.blob");
            while (reader->Read(&request))
            {
                if (!request.has_chunk_data()) {
                    return grpc::Status(grpc::INVALID_ARGUMENT, "Request missing chunk data.");
                }

                blob_file.append_chunk(request.chunk_data());
                blob_hasher.add_chunk(request.chunk_data());
            }

            auto blob_hash = blob_hasher.finalize();
            if (blob_file.size() != blob_size) {
                return grpc::Status(grpc::INVALID_ARGUMENT, "Blob file size mismatch.");
            }

            return std::make_pair(blob_file, blob_hash);
        }
        catch (const BlobFile::FileSystemException& fse)
        {
            return grpc::Status(grpc::CANCELLED, fse.what());
        }
    }

    static auto get_workers_from_master(std::string blob_hash,
                                 const std::unique_ptr<master::MasterService::Stub>& master_stub) -> Expected<
            google::protobuf::RepeatedPtrField<common::ipv4Address>, std::string>
    {
        // Ask master for workers to store blob.
        grpc::ClientContext client_context;
        master::GetWorkersToSaveBlobRequest get_workers_request;
        get_workers_request.set_blobid(blob_hash);
        master::GetWorkersToSaveBlobResponse get_workers_response;
        if (const auto status = master_stub->GetWorkersToSaveBlob(&client_context, get_workers_request,
                                                                   &get_workers_response); !status.ok()) {
            return status.error_message();
        }
        if (!get_workers_response.success()) {
            return get_workers_response.message();
        }

        return get_workers_response.addresses();
    }
public:
    explicit FrontendServiceImpl(const std::shared_ptr<grpc::Channel>& master_channel)
    : master_stub_(master::MasterService::NewStub(master_channel))
    {
    }

    grpc::Status UploadBlob(grpc::ServerContext* context, grpc::ServerReader<frontend::UploadBlobRequest>* reader,
                            frontend::UploadBlobResponse* response) override
    {
        // Receive and hash the blob in chunks.
        auto result = receive_and_hash_blob(reader);
        if (!result.has_value()) {
            return result.error();
        }
        const auto& [blob_file, blob_hash] = result.value();

        // TODO-soon: select master based on the hash.

        // Ask master for workers to save blob.
        auto master_result = get_workers_from_master(blob_hash, master_stub_);
        if (!master_result.has_value()) {
            response->set_upload_result(failed_request(master_result.error(), "get workers from master"));
            return grpc::Status::CANCELLED;
        }
        const auto& workers = master_result.value();

        for (const auto& worker_address : workers)
        {
            if (auto error_string = send_blob_to_worker(blob_file, blob_hash, worker_address); error_string)
            {
                response->set_upload_result(*error_string);
                return grpc::Status::CANCELLED;
            }
        }

        response->set_upload_result("Successfully saved blob to " + std::to_string(blob_file.size()) + " workers");
        return grpc::Status::OK;
    }

    grpc::Status GetBlob(grpc::ServerContext* context, const frontend::GetBlobRequest* request,
                         grpc::ServerWriter<frontend::GetBlobResponse>* writer) override
    {
        return Service::GetBlob(context, request, writer);
    }

    grpc::Status DeleteBlob(grpc::ServerContext* context, const frontend::DeleteBlobRequest* request,
                            frontend::DeleteBlobResponse* response) override
    {
        // Send delete message to master.
        grpc::ClientContext client_context;
        master::DeleteBlobResponse master_response;
        master::DeleteBlobRequest master_request;
        master_request.set_blobid(request->hash());
        const auto master_status = master_stub_->DeleteBlob(&client_context, master_request, &master_response);

        // Match on result.
        if (master_status.ok())
        {
            response->set_delete_result(master_response.message());
            if (master_response.success())
            {
                return grpc::Status::OK;
            }
            else
            {
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "Failed to delete blob");
            }
        }
        else
        {
            auto error_message = "Error with request to master: " + master_status.error_message();
            std::cerr << error_message << std::endl;
            response->set_delete_result(error_message);
            return grpc::Status::CANCELLED;
        }
    }

    grpc::Status HealthCheck(grpc::ServerContext* context, const frontend::HealthcheckRequest* request,
                             frontend::HealthcheckResponse* response) override
    {
        response->set_message("Frontend healthcheck: OK");
        return grpc::Status::OK;
    }
};

