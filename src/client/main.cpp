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

[[noreturn]] void run_frontend_upload_blob(const std::string frontend_load_balancer_address)
{
    while (true) {
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
                        std::cerr << "DONE - status OK" << std::endl;
                    }
                    else
                    {
                        std::cerr << "DONE - status " << status.error_code() << " " << status.error_message() << std::endl;
                    }
                }
            }
        }
        else
        {
            std::cout << "Starting blob upload! | FAILED " << std::endl;
        }
    }
}

int main() {
    run_frontend_ping("34.118.88.122:50042");
    run_frontend_upload_blob("34.118.88.122:50042");
}