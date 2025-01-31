#include <blob_hasher.hpp>

#include "client.hpp"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "services/frontend_service.grpc.pb.h"
#include "environment.hpp"
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
        std::cout << "Requesting HealthCheck...\n";

        grpc::ClientContext client_ctx;
        const frontend::HealthcheckRequest request;
        frontend::HealthcheckResponse response;

        if (const auto status = frontend_stub->HealthCheck(&client_ctx, request, &response); status.ok()) {
            std::cout << status.ok() << " | Frontend Ping OK " << std::endl;
        } else {
            std::cout << status.error_code() << " " << status.error_message() << std::endl;
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

        std::cout << "Uploading blob...\n";

        grpc::ClientContext client_ctx;
        frontend::UploadBlobRequest request;
        frontend::UploadBlobResponse response;

        auto writer = frontend_stub->UploadBlob(&client_ctx, &response);
        std::cerr << "DUPA\n";
        if (writer)
        {
            std::string raw_blob {"ABRAKADABRA"};
            std::cout << "Starting blob upload! | OK " << std::endl;

            auto* blob_info = new frontend::BlobInfo;
            blob_info->set_size_bytes(raw_blob.size());
            request.set_allocated_info(blob_info);

            if (not writer->Write(request))
            {
                std::cerr << "Failed to upload blob HEADER!" << std::endl;
            } else
            {
                std::cerr << "Sent blob header! | OK " << std::endl;

                request.set_chunk_data(raw_blob);
                if (not writer->Write(request))
                {
                    std::cerr << "Failed to upload blob DATA!" << std::endl;
                }
                else
                {
                    std::cerr << "Sent blob data! | OK " << std::endl;
                    writer->WritesDone();

                    if (auto status = writer->Finish(); status.ok())
                    {
                        std::cerr << "DONE - status OK, blob_hash: " << response.blob_hash() << std::endl;
                        return response.blob_hash();
                    }
                    else
                    {
                        std::cerr << "DONE - status ERROR " << status.error_code() << " " << status.error_message() << std::endl;
                    }
                }
            }
        }
        else
        {
            std::cout << "Starting blob upload! | FAILED " << std::endl;
        }
    }
    return "";
}

void run_frontend_get_blob(const std::string& frontend_load_balancer_address, const std::string& blob_hash) {
    auto channel = grpc::CreateChannel(frontend_load_balancer_address, grpc::InsecureChannelCredentials());
    const auto frontend_stub = frontend::Frontend::NewStub(std::move(channel));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Retrieving blob with hash: " << blob_hash << "\n";

    grpc::ClientContext client_ctx;
    frontend::GetBlobRequest request;
    request.set_blob_hash(blob_hash);

    std::string received_data;

    // Set up the reader for the streaming response
    auto reader = frontend_stub->GetBlob(&client_ctx, request);
    frontend::GetBlobResponse response;

    // Read all chunks from the stream
    std::cout << "Start recieving\n";
    while (reader->Read(&response)) {
        std::cout << "next chunk\n";
        received_data += response.chunk_data();
    }
    std::cout << "Finished stream\n";

    // Check the status of the RPC
    const auto status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "Failed to retrieve blob: " << status.error_code()
                  << " " << status.error_message() << std::endl;
        return;
    }

    std::cout << "Retrieved blob data: " << received_data << std::endl;

    // Verify the data matches what we uploaded
    const std::string expected_data = "ABRAKADABRA";
    if (received_data == expected_data) {
        std::cout << "Blob verification successful - data matches!" << std::endl;
    } else {
        std::cerr << "Blob verification failed - data mismatch!" << std::endl;
        std::cerr << "Expected: " << expected_data << std::endl;
        std::cerr << "Received: " << received_data << std::endl;
    }
}

void run_frontend_delete_blob(const std::string& frontend_load_balancer_address, const std::string& blob_hash) {
    auto channel = grpc::CreateChannel(frontend_load_balancer_address, grpc::InsecureChannelCredentials());
    const auto frontend_stub = frontend::Frontend::NewStub(std::move(channel));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Deleting blob with hash: " << blob_hash << "\n";

    grpc::ClientContext client_ctx;
    frontend::DeleteBlobRequest request;
    request.set_blob_hash(blob_hash);

    frontend::DeleteBlobResponse response;

    // Make the RPC call
    const auto status = frontend_stub->DeleteBlob(&client_ctx, request, &response);
    if (status.ok()) {
        std::cout << "Blob deletion successful!" << std::endl;
    } else {
        std::cerr << "Failed to delete blob: " << status.error_code()
                  << " " << status.error_message() << std::endl;
    }
}

int main(const int argc, char** argv) {
    // read the frontend (load-balances) address from the command line
    const std::string frontend_address = [&] {
        if (argc < 2) {
            std::cerr << "Usage: " + std::string(argv[0]) + " <frontend_address>:<port>" << std::endl;
            exit(1);
        }
        return std::string(argv[1]);
    }();
    std::cout << "Client will connect to frontend at " << frontend_address << std::endl;
    run_frontend_ping(frontend_address);
    std::string blob_hash = run_frontend_upload_blob(frontend_address);
    if (!blob_hash.empty()) {
        run_frontend_get_blob(frontend_address, blob_hash);
        run_frontend_delete_blob(frontend_address, blob_hash);
    }
}