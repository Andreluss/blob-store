//
// Created by andrez on 1/18/25.
//
#pragma once
#include <services/master_service.grpc.pb.h>

#include "services/frontend_service.grpc.pb.h"

class FrontendServiceImpl final : public frontend::Frontend::Service
{
    std::unique_ptr<master::MasterService::Stub> master_stub_;
public:
    explicit FrontendServiceImpl(const std::shared_ptr<grpc::Channel>& master_channel)
    : master_stub_(master::MasterService::NewStub(master_channel))
    {
    }

    grpc::Status UploadBlob(grpc::ServerContext* context, grpc::ServerReader<frontend::UploadBlobRequest>* reader,
                            frontend::UploadBlobResponse* response) override
    {
        return Service::UploadBlob(context, reader, response);
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

