#pragma once

#include <variant>
#include <stdexcept>
#include "common.grpc.pb.h"

// The custom expected type - like Ocaml's Or_error or Rust's Result
template <typename T, typename E>
class Expected {
public:
    // Constructors for both valid value and error value
    Expected(T value) : value_or_error(value) {}
    Expected(E error) : value_or_error(error) {}

    // Check if the result is valid (holds a T value)
    bool has_value() const {
        return std::holds_alternative<T>(value_or_error);
    }

    // Get the value or throw an exception if it is an error
    T& value() {
        if (!has_value()) {
            throw std::runtime_error("Called value() on an error result");
        }
        return std::get<T>(value_or_error);
    }

    const T& value() const {
        if (!has_value()) {
            throw std::runtime_error("Called value() on an error result");
        }
        return std::get<T>(value_or_error);
    }

    // Get the error value or throw an exception if it is a valid result
    E& error() {
        if (has_value()) {
            throw std::runtime_error("Called error() on a valid result");
        }
        return std::get<E>(value_or_error);
    }

    const E& error() const {
        if (has_value()) {
            throw std::runtime_error("Called error() on a valid result");
        }
        return std::get<E>(value_or_error);
    }

private:
    // A variant that holds either a valid value of type T or an error of type E
    std::variant<T, E> value_or_error;
};

// TODO-soon: move to (network_)utils.hpp in common/
inline std::string address_to_string(const uint32_t ip, const uint32_t port) {
    std::stringstream ss;
    for (int byte = 3; byte >= 0; --byte)
    {
        const auto num = (ip >> (8 * byte)) & 0xff;
        ss << num;
        if (byte > 0)
        {
            ss << ".";
        }
    }
    ss << ":" << port;
    return ss.str();
}

inline std::string address_to_string(const common::ipv4Address& address) {
    return address_to_string(address.address(), address.port());
}

namespace fs = std::filesystem;

class BlobFile
{
    const static fs::path BLOBS_PATH;
    const static uint64_t MAX_CHUNK_SIZE;

    fs::path file_path_;
    uint64_t size_;
    explicit BlobFile(const std::string& filename, const uint64_t size)
        : file_path_(BLOBS_PATH / filename), size_(size)
    {
    }

public:
    struct FileSystemException final : public std::runtime_error {
        explicit FileSystemException(const std::string& what) : std::runtime_error(what) {}
    };

    class ChunkIterator
    {
        uint64_t next_byte;
        uint64_t file_size_;
        fs::path file_path_;
        std::optional<std::ifstream> file_stream_{std::nullopt};
        std::optional<std::string> current_chunk{};
        void create_stream_if_necessary()
        {
            if (file_stream_ == std::nullopt)
            {
                file_stream_ = std::ifstream(file_path_, std::ios::binary | std::ios::in);
                if (!file_stream_->is_open())
                {
                    throw FileSystemException("Failed to open file " + file_path_.string());
                }
                file_stream_->seekg(next_byte);
            }
        }

        std::string read_chunk()
        {
            // read at most MAX_CHUNK_SIZE
            char buffer[MAX_CHUNK_SIZE];
            const auto bytes_to_read = std::min(MAX_CHUNK_SIZE, file_size_ - next_byte);
            file_stream_->read(buffer, bytes_to_read);
            if (const auto bytes_read = file_stream_->gcount(); bytes_read < bytes_to_read)
            {
                throw FileSystemException("Failed to read file, write at" +
                    std::to_string(next_byte) + " returned " +
                    std::to_string(bytes_read) + " instead of " +
                    std::to_string(bytes_to_read));
            }
            return std::string(buffer, bytes_to_read);
        }

    public:
        explicit ChunkIterator(const fs::path& file_path, const uint64_t file_size, const uint64_t start_pos = 0)
        : next_byte(start_pos), file_size_(file_size), file_path_(file_path) {

        }
        ~ChunkIterator () {
            if (file_stream_ && file_stream_->is_open()) {
                file_stream_->close();
            }
        }

        std::string operator*()
        {
            create_stream_if_necessary();
            if (!file_stream_->is_open()) {
                throw FileSystemException("Failed to open file");
            }
            if (!current_chunk) {
                current_chunk = read_chunk();
            }
            return *current_chunk;
        }
        ChunkIterator& operator++()
        {
            next_byte += current_chunk ? current_chunk->size() : 1;
            current_chunk = std::nullopt;
            return *this;
        }

        bool operator!= (const ChunkIterator& other) const
        {
            return next_byte != other.next_byte;
        }
    };

    ChunkIterator begin() const { return ChunkIterator(file_path_, MAX_CHUNK_SIZE, 0); }
    ChunkIterator end() const { return ChunkIterator(file_path_, MAX_CHUNK_SIZE, size()); }

    /// Throws FileSystemException, if it couldn't open the file.
    static BlobFile New(const fs::path& filename)
    {
        fs::create_directories(BLOBS_PATH);
        const fs::path file_path = BLOBS_PATH / filename;

        // create the file in truncate mode
        std::ofstream file(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            throw FileSystemException("Failed to open file " + file_path.string());
        }
        file.close();

        return BlobFile(file_path, 0);
    }

    void append_chunk(const std::string& chunk)
    {
        std::ofstream outfile(file_path_, std::ios::binary | std::ios::app);
        if (!outfile.is_open()) {
            throw FileSystemException("Failed to open file " + file_path_.string() + " for appending");
        }
        outfile.write(chunk.c_str(), chunk.size());
        outfile.close();

        size_ += chunk.size();
    }

    size_t size() const
    {
        // Note: could've used fs::file_size(file_path_)
        // but the metadata might have not been flushed yet
        return size_;
    }
};
const fs::path BlobFile::BLOBS_PATH = "blobs";
const uint64_t BlobFile::MAX_CHUNK_SIZE = BlobStoreConfig::MAX_CHUNK_SIZE;