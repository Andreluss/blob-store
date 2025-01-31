#include <blob_hasher.hpp>

#include "client.hpp"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "services/frontend_service.grpc.pb.h"
#include "environment.hpp"
#include "logging.hpp"
#include <iostream>
#include <string>

void run_frontend_ping(const std::string frontend_load_balancer_address)
{
    int cnt = 5;
    while (cnt--) {
        auto channel = grpc::CreateChannel(frontend_load_balancer_address, grpc::InsecureChannelCredentials());
        const auto frontend_stub = frontend::Frontend::NewStub(std::move(channel));

        // wait 1 second on current thread
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Logger::info("Requesting HealthCheck...");

        grpc::ClientContext client_ctx;
        const frontend::HealthcheckRequest request;
        frontend::HealthcheckResponse response;

        if (const auto status = frontend_stub->HealthCheck(&client_ctx, request, &response); status.ok()) {
            Logger::info("Frontend Ping OK");
        } else {
            Logger::error("Frontend Ping FAILED ", status.error_code(), " ", status.error_message());
        }
    }
}

std::string run_frontend_upload_blob(const std::string frontend_load_balancer_address)
{
    int c = 1;
    while (c--) {
        auto channel = grpc::CreateChannel(frontend_load_balancer_address, grpc::InsecureChannelCredentials());
        const auto frontend_stub = frontend::Frontend::NewStub(std::move(channel));

        std::this_thread::sleep_for(std::chrono::seconds(1));
        Logger::info("Uploading blob...");

        grpc::ClientContext client_ctx;
        frontend::UploadBlobRequest request;
        frontend::UploadBlobResponse response;

        auto writer = frontend_stub->UploadBlob(&client_ctx, &response);
        if (writer)
        {
            std::string raw_blob {"ABRAKADABRA"};
            Logger::info("Starting blob upload! | OK ");

            auto* blob_info = new frontend::BlobInfo;
            blob_info->set_size_bytes(raw_blob.size());
            request.set_allocated_info(blob_info);

            if (not writer->Write(request))
            {
                Logger::error("Failed to upload blob HEADER!");
            } else
            {
                Logger::info("Sent blob header! | OK ");

                request.set_chunk_data(raw_blob);
                if (not writer->Write(request))
                {
                    Logger::error("Failed to upload blob DATA!");
                }
                else
                {
                    Logger::info("Sent blob data! | OK ");
                    writer->WritesDone();

                    if (auto status = writer->Finish(); status.ok())
                    {
                        Logger::info("DONE - status OK, blob_hash: ", response.blob_hash());
                        return response.blob_hash();
                    }
                    else
                    {
                        Logger::error("DONE - status ERROR ", status.error_code(), " ", status.error_message());
                    }
                }
            }
        }
        else
        {
            Logger::error("Starting blob upload! | FAILED ");
        }
    }
    return "";
}

void run_frontend_get_blob(const std::string& frontend_load_balancer_address, const std::string& blob_hash) {
    auto channel = grpc::CreateChannel(frontend_load_balancer_address, grpc::InsecureChannelCredentials());
    const auto frontend_stub = frontend::Frontend::NewStub(std::move(channel));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    Logger::info("Retrieving blob with hash: ", blob_hash);

    grpc::ClientContext client_ctx;
    frontend::GetBlobRequest request;
    request.set_blob_hash(blob_hash);

    std::string received_data;

    // Set up the reader for the streaming response
    auto reader = frontend_stub->GetBlob(&client_ctx, request);
    frontend::GetBlobResponse response;

    // Read all chunks from the stream
    Logger::info("Start receiving...");
    while (reader->Read(&response)) {
        Logger::info("next chunk");
        received_data += response.chunk_data();
    }
    Logger::info("Finished receiving...");

    // Check the status of the RPC
    const auto status = reader->Finish();
    if (!status.ok()) {
        Logger::error("Failed to retrieve blob: ", status.error_code(), " ", status.error_message());
        return;
    }

    Logger::info("Retrieved blob data: ", received_data);

    // Verify the data matches what we uploaded
    const std::string expected_data = "ABRAKADABRA";
    if (received_data == expected_data) {
        Logger::info("Blob verification successful - data matches!");
    } else {
        Logger::error("Blob verification failed - data mismatch!");
        Logger::error("Expected: ", expected_data);
        Logger::error("Received: ", received_data);
    }
}

void run_frontend_delete_blob(const std::string& frontend_load_balancer_address, const std::string& blob_hash) {
    auto channel = grpc::CreateChannel(frontend_load_balancer_address, grpc::InsecureChannelCredentials());
    const auto frontend_stub = frontend::Frontend::NewStub(std::move(channel));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    Logger::info("Deleting blob with hash: ", blob_hash);

    grpc::ClientContext client_ctx;
    frontend::DeleteBlobRequest request;
    request.set_blob_hash(blob_hash);

    frontend::DeleteBlobResponse response;

    // Make the RPC call
    const auto status = frontend_stub->DeleteBlob(&client_ctx, request, &response);
    if (status.ok()) {
        Logger::info("Blob deletion successful!");
    } else {
        Logger::error("Failed to delete blob: ", status.error_code(), " ", status.error_message());
    }
}

int main(const int argc, char** argv) {
    // read the frontend (load-balances) address from the command line
    const std::string frontend_address = [&] {
        if (argc < 2) {
            Logger::error("Usage: " + std::string(argv[0]) + " <frontend_address>:<port>");
            exit(1);
        }
        return std::string(argv[1]);
    }();
    Logger::info("Client will connect to frontend at ", frontend_address);
    run_frontend_ping(frontend_address);
    std::string blob_hash = run_frontend_upload_blob(frontend_address);
    if (!blob_hash.empty()) {
        run_frontend_get_blob(frontend_address, blob_hash);
        run_frontend_delete_blob(frontend_address, blob_hash);
    }
}