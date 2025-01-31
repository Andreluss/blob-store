#include "frontend_service.hpp"
#include <environment.hpp>
#include "expected.hpp"
#include "blob_hasher.hpp"
#include "blob_file.hpp"
#include <fstream>
#include <logging.hpp>
#include <services/worker_service.grpc.pb.h>

// ------------------------------------ helpers ---------------------------------------------------------

// User-defined literal "_S" that converts C-string to std::string
static std::string operator""_S(const char* str, std::size_t) {
    return {str};
}
struct NetworkAddress : public std::string{};

auto send_blob_to_worker(const BlobFile& blob, const std::string& blob_hash,
    const std::string& worker_address) -> Expected<std::monostate, std::string>
{
    Logger::info("Sending blob to worker at ", worker_address);
    try
    {
        const auto worker_channel = grpc::CreateChannel(worker_address, grpc::InsecureChannelCredentials());
        const auto worker_stub = worker::WorkerService::NewStub(worker_channel);

        grpc::ClientContext client_context;
        worker::SaveBlobResponse save_blob_response;
        Logger::debug("Starting to save blob ", blob_hash, " to worker");
        const auto writer = worker_stub->SaveBlob(&client_context, &save_blob_response);

        for (const auto& chunk: blob)
        {
            Logger::debug("Saving next chunk of size ", chunk.size());
            worker::SaveBlobRequest save_blob_request;
            save_blob_request.set_blob_hash(blob_hash);
            save_blob_request.set_chunk_data(chunk.data(), chunk.size());
            if (!writer->Write(save_blob_request)) {
                return "Failed to save blob to worker - broken stream";
            }
        }
        writer->WritesDone();
        auto status = writer->Finish();
        if (not status.ok())
        {
            return status.error_message();
        }

        return std::monostate{};
    }
    catch (const BlobFile::FileSystemException& fse)
    {
        return "Error while sending blob to worker: "_S + fse.what();
    }
}

auto receive_and_hash_blob(grpc::ServerReader<frontend::UploadBlobRequest>* reader)
    -> Expected<std::pair<BlobFile, std::string>, grpc::Status>
{
    Logger::info("Receiving and hashing blob.");
    frontend::UploadBlobRequest request;

    // 1. Read blob info
    reader->Read(&request);
    if (!request.has_info()) {
        return grpc::Status(grpc::INVALID_ARGUMENT, "Request should start with blob info.");
    }
    const auto blob_size = request.info().size_bytes();
    Logger::info("Receiving blob of size ", blob_size);

    // 2. Read chunks
    try
    {
        BlobHasher blob_hasher;
        auto blob_filename = "temp" + std::to_string(rand()) + ".blob";
        auto blob_file = BlobFile::New(blob_filename);
        Logger::debug("Opened blob file for writing: ", blob_filename);

        while (reader->Read(&request))
        {
            if (!request.has_chunk_data()) {
                return grpc::Status(grpc::INVALID_ARGUMENT, "Request missing chunk data.");
            }

            Logger::debug("Received chunk of size ", request.chunk_data().size());
            blob_file.append_chunk(request.chunk_data());
            blob_hasher.add_chunk(request.chunk_data());
        }

        auto blob_hash = blob_hasher.finalize();
        if (blob_file.size() != blob_size) {
            return grpc::Status(grpc::INVALID_ARGUMENT, "Blob file size mismatch.");
        }

        Logger::info("Blob fully received and hashed: ", blob_hash);
        return std::make_pair(blob_file, blob_hash);
    }
    catch (const BlobFile::FileSystemException& fse)
    {
        return grpc::Status(grpc::CANCELLED, fse.what());
    }
}

auto get_workers_from_master(std::string blob_hash, const std::string& master_address)
    -> Expected<std::vector<std::string>, grpc::Status>
{
    Logger::info("Requesting workers from master at ", master_address);
    const auto master_stub = master::MasterService::NewStub(grpc::CreateChannel(master_address, grpc::InsecureChannelCredentials()));
    //google::protobuf::RepeatedPtrField<common::ipv4Address>
    // Ask master for workers to store blob.
    grpc::ClientContext client_context;
    master::GetWorkersToSaveBlobRequest get_workers_request;
    get_workers_request.set_blob_hash(blob_hash);
    master::GetWorkersToSaveBlobResponse get_workers_response;
    if (const auto status = master_stub->GetWorkersToSaveBlob(&client_context, get_workers_request,
                                                              &get_workers_response); !status.ok()) {
        return grpc::Status(grpc::CANCELLED, status.error_message());
    }

    std::vector addresses (get_workers_response.addresses().begin(), get_workers_response.addresses().end());
    Logger::info("Received workers from master: ", addresses);
    return addresses;
}

auto get_worker_with_blob_id(std::string blob_id, const std::string& master_address)
    -> Expected<NetworkAddress, std::string>
{
    Logger::info("Getting worker with blob id ", blob_id, " from master at ", master_address);
    master::GetWorkerWithBlobRequest request;
    request.set_blob_hash(blob_id);
    master::GetWorkerWithBlobResponse response;
    grpc::ClientContext client_context;

    const auto master_stub_ = master::MasterService::NewStub(grpc::CreateChannel(master_address, grpc::InsecureChannelCredentials()));
    if (const auto status = master_stub_->GetWorkerWithBlob(&client_context, request, &response); !status.ok()) {
        return status.error_message();
    }
    Logger::info("Got worker address");
    return NetworkAddress(response.addresses());
}

static std::string failed_request(const std::string& error_message, const std::string& performed_action) {
    Logger::error("Failed to ", performed_action, ": ", error_message);
    return "Failed to " + performed_action + ".";
}

// ---------------------------------- implementation ----------------------------------------------------

grpc::Status FrontendServiceImpl::UploadBlob(grpc::ServerContext* context,
    grpc::ServerReader<frontend::UploadBlobRequest>* reader, frontend::UploadBlobResponse* response)
{
    Logger::info("Upload blob request received.");
    // Receive and hash the blob in chunks.
    return receive_and_hash_blob(reader)
    .and_then([&](auto filehash)->Expected<int, grpc::Status> {

    auto &[blob_file, blob_hash] = filehash;
    Logger::info("Received blob with hash ", blob_hash);
    return get_workers_from_master(blob_hash, get_master_service_address_based_on_hash(blob_hash))
    .and_then([&](const auto& workers)->Expected<int, grpc::Status>{

    for (const auto& worker_address : workers) {
        auto send_blob_result = send_blob_to_worker(blob_file, blob_hash, worker_address);
        if (not send_blob_result.has_value()) {
            Logger::info("Saving blob to worker ", worker_address, " failed: ", send_blob_result.error());
            return grpc::Status(grpc::CANCELLED, send_blob_result.error());
        }
    }

    Logger::info("Blob saved to workers successfully.");
    response->set_blob_hash(blob_hash);
    blob_file.remove();
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

grpc::Status FrontendServiceImpl::GetBlob(grpc::ServerContext* context, const frontend::GetBlobRequest* request,
                                          grpc::ServerWriter<frontend::GetBlobResponse>* writer)
{
    Logger::info("GetBlob request");
    const auto& blob_id = request->blob_hash();

    return get_worker_with_blob_id(blob_id, get_master_service_address_based_on_hash(blob_id))
    .and_then([&](const NetworkAddress& worker_address)->Expected<std::monostate, std::string> {
        const auto worker_channel = grpc::CreateChannel(worker_address, grpc::InsecureChannelCredentials());
        const auto worker_stub = worker::WorkerService::NewStub(worker_channel);

        worker::GetBlobRequest worker_request;
        worker_request.set_blob_hash(blob_id);
        grpc::ClientContext client_context;

        const auto reader = worker_stub->GetBlob(&client_context, worker_request);

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
        [](const auto& error_str) { return grpc::Status(grpc::StatusCode::CANCELLED, error_str); }
    );
}

grpc::Status FrontendServiceImpl::DeleteBlob(grpc::ServerContext* context, const frontend::DeleteBlobRequest* request,
    frontend::DeleteBlobResponse* response)
{
    Logger::info("DeleteBlob request");
    auto blob_hash = request->blob_hash();
    auto master_address = get_master_service_address_based_on_hash(blob_hash);
    const auto master_stub_ = master::MasterService::NewStub(grpc::CreateChannel(master_address, grpc::InsecureChannelCredentials()));
    grpc::ClientContext client_context;
    master::DeleteBlobResponse master_response;
    master::DeleteBlobRequest master_request;
    master_request.set_blob_hash(blob_hash);

    Logger::info("Request to delete blob ", blob_hash, " from master at ", master_address);
    if (const auto master_status = master_stub_->DeleteBlob(&client_context, master_request, &master_response);
        not master_status.ok()) {
        response->set_delete_result(failed_request(master_status.error_message(),
                                                   "delete request to master"));
        return grpc::Status::CANCELLED;
    }

    response->set_delete_result("Blob deleted successfully.");
    return grpc::Status::OK;
}

grpc::Status FrontendServiceImpl::HealthCheck(grpc::ServerContext* context, const frontend::HealthcheckRequest* request,
    frontend::HealthcheckResponse* response)
{
    Logger::info("Health check request logger \n");
    return grpc::Status::OK;
}