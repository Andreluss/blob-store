#include "frontend_service.hpp"
#include "utils.hpp"
#include "expected.hpp"
#include "blob_hasher.hpp"
#include <fstream>
#include <services/worker_service.grpc.pb.h>

// ------------------------------------ helpers ---------------------------------------------------------

// User-defined literal "_S" that converts C-string to std::string
static std::string operator""_S(const char* str, std::size_t) {
    return {str};
}

auto send_blob_to_worker(const BlobFile& blob, const std::string& blob_hash,
    const common::ipv4Address& worker_address) -> Expected<std::monostate, std::string>
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
            save_blob_request.set_chunk_data(chunk.data(), chunk.size());
            if (!writer->Write(save_blob_request)) {
                return "Failed to save blob to worker - broken stream";
            }
        }

        return std::monostate{};
    }
    catch (const BlobFile::FileSystemException& fse)
    {
        return "Error while sending blob to worker: "_S + fse.what();
    }
}

auto start_get_blob_from_worker(std::string blob_id, const common::ipv4Address& worker_address)
    -> std::unique_ptr< ::grpc::ClientReader< ::worker::GetBlobResponse>>
{
    const auto address_string = address_to_string(worker_address);
    const auto worker_channel = grpc::CreateChannel(address_string, grpc::InsecureChannelCredentials());
    const auto worker_stub = worker::WorkerService::NewStub(worker_channel);

    worker::GetBlobRequest worker_request;
    worker_request.set_hash(blob_id);
    grpc::ClientContext client_context;

    auto reader = worker_stub->GetBlob(&client_context, worker_request);
    return reader;
}

auto receive_and_hash_blob(grpc::ServerReader<frontend::UploadBlobRequest>* reader)
    -> Expected<std::pair<BlobFile, std::string>, grpc::Status>
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

auto get_workers_from_master(std::string blob_hash, const std::unique_ptr<master::MasterService::Stub>& master_stub)
    -> Expected<google::protobuf::RepeatedPtrField<common::ipv4Address>, grpc::Status>
{
    // Ask master for workers to store blob.
    grpc::ClientContext client_context;
    master::GetWorkersToSaveBlobRequest get_workers_request;
    get_workers_request.set_blobid(blob_hash);
    master::GetWorkersToSaveBlobResponse get_workers_response;
    if (const auto status = master_stub->GetWorkersToSaveBlob(&client_context, get_workers_request,
                                                              &get_workers_response); !status.ok()) {
        return grpc::Status(grpc::CANCELLED, status.error_message());
    }
    if (!get_workers_response.success()) {
        return grpc::Status(grpc::CANCELLED, get_workers_response.message());
    }

    return get_workers_response.addresses();
}

auto get_worker_with_blob_id(const auto& master_stub_, std::string blob_id)
    -> Expected<common::ipv4Address, std::string>
{
    master::GetWorkerWithBlobRequest request;
    request.set_blobid(blob_id);
    master::GetWorkerWithBlobResponse response;
    grpc::ClientContext client_context;
    master_stub_->GetWorkerWithBlob(&client_context, request, &response);
    if (const auto status = master_stub_->GetWorkerWithBlob(&client_context, request, &response); !status.ok()) {
        return status.error_message();
    }
    if (!response.success()) {
        return response.message();
    }
    auto actually_one_address = response.addresses();
    return actually_one_address;
}

static std::string failed_request(const std::string& error_message, const std::string& performed_action) {
    std::cerr << "Failed to " << performed_action << ": " << error_message << std::endl;
    return "Failed to " + performed_action + ".";
}

// ---------------------------------- implementation ----------------------------------------------------

// TODO-soon: select master based on the hash.
grpc::Status FrontendServiceImpl::UploadBlob(grpc::ServerContext* context,
    grpc::ServerReader<frontend::UploadBlobRequest>* reader, frontend::UploadBlobResponse* response)
{
    // Receive and hash the blob in chunks.
    return receive_and_hash_blob(reader)
    .and_then([&](auto filehash)->Expected<int, grpc::Status> {

    auto &[blob_file, blob_hash] = filehash;
    return get_workers_from_master(blob_hash, master_stub_)
    .and_then([&](const auto& workers)->Expected<int, grpc::Status>{

    for (const auto& worker_address : workers) {
        auto send_blob_result = send_blob_to_worker(blob_file, blob_hash, worker_address);
        if (not send_blob_result.has_value()) {
            return grpc::Status(grpc::CANCELLED, send_blob_result.error());
        }
    }

    blob_file.remove();
    response->set_upload_result("Successfully saved blob to " + std::to_string(blob_file.size()) + " workers");
    return 0; });})

    .output<grpc::Status>(
        [](auto _) { return grpc::Status::OK;},
        [](auto err) { return err; }
    );
}

template<typename C>
static auto f_const(C const_value){
    return [const_value](auto x) { return const_value; };
}

// TODO: choose master based on hash
grpc::Status FrontendServiceImpl::GetBlob(grpc::ServerContext* context, const frontend::GetBlobRequest* request,
                                          grpc::ServerWriter<frontend::GetBlobResponse>* writer)
{
    const auto blob_id = request->hash();

    return get_worker_with_blob_id(master_stub_, blob_id)
    .and_then([&](const common::ipv4Address& worker_address)->Expected<std::monostate, std::string> {

        const auto reader = start_get_blob_from_worker(blob_id, worker_address);

        frontend::GetBlobResponse response;
        worker::GetBlobResponse worker_response;
        while (reader->Read(&worker_response)) {
            response.set_chunk_data(worker_response.chunk_data());
            if (!writer->Write(response)) {
                return "Broken client write stream - can't write next chunk";
            }
        }

        return std::monostate();
    })

    .output<grpc::Status>(
        f_const(grpc::Status::OK),
        [](auto error_str) { return grpc::Status(grpc::StatusCode::CANCELLED, error_str); }
    );
}

grpc::Status FrontendServiceImpl::DeleteBlob(grpc::ServerContext* context, const frontend::DeleteBlobRequest* request,
    frontend::DeleteBlobResponse* response)
{
    grpc::ClientContext client_context;
    master::DeleteBlobResponse master_response;
    master::DeleteBlobRequest master_request;
    master_request.set_blobid(request->hash());

    if (const auto master_status = master_stub_->DeleteBlob(&client_context, master_request, &master_response);
        not master_status.ok()) {
        response->set_delete_result(failed_request(master_status.error_message(),
                                                   "delete request to master"));
        return grpc::Status::CANCELLED;
    }

    response->set_delete_result(master_response.message());
    if (not master_response.success()) {
        return grpc::Status(grpc::StatusCode::CANCELLED, "Failed to delete blob");
    }
    return grpc::Status::OK;
}

grpc::Status FrontendServiceImpl::HealthCheck(grpc::ServerContext* context, const frontend::HealthcheckRequest* request,
    frontend::HealthcheckResponse* response)
{
    response->set_message("Frontend healthcheck: OK");
    return grpc::Status::OK;
}