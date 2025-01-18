#pragma once

#include <string>

/// Incremental hashing for blob chunks. Example usage:
///   BlobHasher blob_hasher;
///   blob_hasher.add_chunk("blabla");
///   blob_hasher.add_chunk("hello");
///   std::string output_hash = blob_hasher.finalize();
class BlobHasher {
    std::string hash_ = "UGABUGABUGABOO";
    int ptr = 0;
public:
    void add_chunk(const std::string& bytes) {
        for (int i = 0; i < bytes.size(); i++) {
            ptr = (ptr + bytes[i]) % hash_.size();
            hash_[ptr] ^= bytes[i];
        }
    }

    /// Return the hash of all data. Call ONLY ONCE per object.
    std::string finalize() {
        return hash_;
    }
};

// TODO-soon: implement hashing with:
// a) SHA-256 using OpenSSL or Crypto++ library
// b) xxHash (faster, non-cryptographic, more collisions): https://github.com/Cyan4973/xxHash
