#pragma once

#include <string>
#include "xxhash.h"
#include <stdexcept>
/// Incremental hashing for blob chunks.
/// + faster (50GB/s vs 0.5GB/s)
/// - non-cryptographic, more collisions
/// https://github.com/Cyan4973/xxHash
/// Example usage:
///   BlobHasher blob_hasher;
///   blob_hasher.add_chunk("blabla");
///   blob_hasher.add_chunk("hello");
///   std::string output_hash = blob_hasher.finalize();
class BlobHasher {
    XXH64_state_t* state;
public:
    BlobHasher() {
        XXH64_hash_t seed = 0;
        state = XXH64_createState();
        if (state == NULL) {
            throw std::runtime_error("XXH64_createState failed");
        }

        /* Initialize state with selected seed */
        if (XXH64_reset(state, seed) == XXH_ERROR) {
            throw std::runtime_error("XXH64_reset failed");
        }
    }

    void add_chunk(const std::string& bytes) {
        if (XXH64_update(state, bytes.c_str(), bytes.size()) == XXH_ERROR) {
            throw std::runtime_error("XXH64_update failed");
        }
    }

    BlobHasher& operator+=(const std::string& chunk) {
        add_chunk(chunk);
        return *this;
    }

    /// Return the hash of all data. Call ONLY ONCE per object.
    /// typedef uint64_t XXH64_hash_t;
    std::string finalize() {
        /* Produce the final hash value */
        XXH64_hash_t const hash = XXH64_digest(state);
        XXH64_freeState(state);
        return std::to_string(hash);
    }
};
