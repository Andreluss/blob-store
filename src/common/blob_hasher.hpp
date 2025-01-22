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
    /// TODO Mateusz: Jeżeli jest jakiś przypadek gdzie chcemy aby różne instancje miały inny seed, lub chcemy go
    ///     co jakiś czas zmieniać -> do configu. Jeśli nie (a wydaje mi się że nie) i jest to używane tylko w tym miejscu,
    ///     to nie ma co. Im większy config tym gorzej.
    BlobHasher(XXH64_hash_t seed = 0) {
        state = XXH64_createState();
        // TODO Mateusz: Czy na pewno to co chcemy zrobić w tej sytuacji to wywalić całą apke? Jeśli tak to proszę
        //      dodaj loga co się stało.
        if (state==NULL) abort();

        /* Initialize state with selected seed */
        if (XXH64_reset(state, seed) == XXH_ERROR) abort();
    }

    void add_chunk(const std::string& bytes) {
        // TODO Mateusz: jak wyżej
        if (XXH64_update(state, bytes.c_str(), bytes.size()) == XXH_ERROR) abort();
    }

    /// Return the hash of all data. Call ONLY ONCE per object.
    /// typedef uint64_t XXH64_hash_t;
    XXH64_hash_t finalize() {
        // TODO Mateusz: Czy chcemy żeby ta funkcja zwracała XXH64_hash_t, czy nie lepiej od razu do naszego wewnętrznego formatu (string)?
        //  Wtedy wszystkie użycia biblioteki do hashowania można by zamknąć w BlobHasher.
        /* Produce the final hash value */
        XXH64_hash_t const hash = XXH64_digest(state);
        XXH64_freeState(state);
        return hash;
    }
};
