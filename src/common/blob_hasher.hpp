#pragma once

#include <string>
#include "stdlib.h"   /* abort() */
#include "xxhash.h"

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
    /// TODO: We probably should set seed in config.
    BlobHasher(XXH64_hash_t seed = 0) {
        state = XXH64_createState();
        if (state==NULL) abort();

        /* Initialize state with selected seed */
        if (XXH64_reset(state, seed) == XXH_ERROR) abort();
    }

    void add_chunk(const std::string& bytes) {
        if (XXH64_update(state, bytes.c_str(), bytes.size()) == XXH_ERROR) abort();
    }

    /// Return the hash of all data. Call ONLY ONCE per object.
    /// typedef uint64_t XXH64_hash_t;
    XXH64_hash_t finalize() {
        /* Produce the final hash value */
        XXH64_hash_t const hash = XXH64_digest(state);
        XXH64_freeState(state);
        return hash;
    }
};
