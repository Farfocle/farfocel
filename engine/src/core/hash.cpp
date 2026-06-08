/**
 * @file hash.cpp
 * @author Kiju
 * @brief Implements hash functions using wyhash.
 *
 * @details wyhash.h defines sprp() and is_prime() as plain (non-static, non-inline) C functions.
 * Including it in a header would emit those symbols into every translation unit and cause
 * ODR violations at link time. This file is the single TU that owns the wyhash include.
 */
#include "wyhash.h"

#include "fr/core/hash.hpp"
#include "fr/core/macros.hpp"

namespace fr {

Hash hash_bytes(const void *ptr, USize len) noexcept {
    FR_ASSERT(len == 0 || ptr != nullptr, "pointer must be non-null if size is non-zero");
    return Hash::from_raw(wyhash(ptr, len, HASH_SEED, _wyp));
}

U64 fr_wyhash64(U64 a, U64 b) noexcept {
    return wyhash64(a, b);
}

} // namespace fr
