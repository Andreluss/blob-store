//
// Created by mjacniacki on 06.01.25.
//
#pragma once
#include "services/echo_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <string>

class EchoServiceImpl final : public echo::EchoService::Service {
public:
    grpc::Status Echo(grpc::ServerContext* context,
                      const echo::EchoRequest* request,
                      echo::EchoResponse* response) override {
        response->set_message(request->message());
        response->set_timestamp(GetCurrentTimestamp());
        return grpc::Status::OK;
    }

    grpc::Status ServerStreamingEcho(grpc::ServerContext* context,
                                      const echo::EchoRequest* request,
                                      grpc::ServerWriter<echo::EchoResponse>* writer) override {
        for (int i = 0; i < 3; ++i) {
            echo::EchoResponse response;
            response.set_message(request->message());
            response.set_timestamp(GetCurrentTimestamp());
            writer->Write(response);
        }
        return grpc::Status::OK;
    }

private:
    std::string GetCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        return std::ctime(&time);
    }
};
