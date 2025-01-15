//
// Created by mateusz on 14.01.25.
//

#ifndef BLOB_STORE_WORKER_SERVICE_HPP
#define BLOB_STORE_WORKER_SERVICE_HPP

#include <filesystem>
#include <fstream>
#include "services/worker_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>

const std::string BLOBS_PATH = "blobs/";
const uint64_t MAX_CHUNK_SIZE = 1024 * 1024;


class WorkerServiceImpl final : public worker::WorkerService::Service {
public:
    WorkerServiceImpl() {
        // TODO Mateusz: Robienie jakichkolwiek operacji na plikach w konstruktorze to code smell, albo dodać metodę do inicjalizacji,
        //  albo przyjąć w tej klasie założenie że te foldery są zainicjalizowane
        //  (I zrobić inicjalizacje w miejscu w którym będziemy korzystać z WorkerServiceImpl).
        try {
            if (std::filesystem::exists(BLOBS_PATH)) {
                std::cout << "Directory already exists: " << BLOBS_PATH << '\n';
            } else if (std::filesystem::create_directories(BLOBS_PATH)) {
                std::cout << "Directory created successfully: " << BLOBS_PATH << '\n';
            } else {
                std::cerr << "Failed to create directory: " << BLOBS_PATH << '\n';
            }
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Error: " << e.what() << '\n';
        }
    }

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
        if (status == RetCode::ERROR) {
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

        auto create_file_if_needed = [&](const std::string &filename) -> RetCode {
            if (not created_file) {
                if (tryCreateBlobFile(filename) == RetCode::ERROR) {
                    return RetCode::ERROR;
                }
                created_file = true;
            }
            return RetCode::SUCCESS;
        };

        while (reader->Read(&request)) {
            if (create_file_if_needed(request.hash()) == RetCode::ERROR) {
                response->set_message("Error while creating file.");
                return grpc::Status::CANCELLED;
            }
            if (append_to_file(request.hash(), request.chunk_data()) == RetCode::ERROR) {
                response->set_message("Error appending bytes to file.");
                return grpc::Status::CANCELLED;
            }
        }
        // TODO Mateusz: wywołać master NotifyBlobSaved po udanym zapisie
        // TODO Mateusz: co jeśli stream z fragmentami pliku się urwie w połowie?
        //  Trzeba pewnie usunąć to co do tej pory zapisaliśmy
        //  Myślę że warto żeby sam worker policzył hash pliku i sprawdził czy zgadza się z tym co było w requeście

        response->set_message("OK");
        return grpc::Status::OK;
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
        if (delete_file(request->hash()) == RetCode::SUCCESS) {
            response->set_message("OK");
            return grpc::Status::OK;
        } else {
            // TODO Mateusz: Dobrze byłoby mieć info czy plik nie istniał, czy istniał ale był błąd przy odczycie
            response->set_message("Error while deleting file.");
            return grpc::Status::CANCELLED;
        }
    }

private:
    enum RetCode {
        SUCCESS,
        ERROR,
        FINISHED
    };

    static std::pair<RetCode, uint64_t> get_free_storage() {
        try {
            auto space = std::filesystem::space(BLOBS_PATH);
            return {RetCode::SUCCESS, (uint64_t) space.free};
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Error: " << e.what() << '\n';
            return {RetCode::ERROR, uint64_t(0)};
        }
    }

    static bool tryCreateBlobFile(const std::string &hash) {
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
            return RetCode::ERROR;
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
            return RetCode::ERROR;
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
                return RetCode::ERROR;
            }
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Filesystem error: " << e.what() << '\n';
            return RetCode::ERROR;
        }

        return RetCode::SUCCESS;
    }
};

#endif //BLOB_STORE_WORKER_SERVICE_HPP
