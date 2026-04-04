/**
 * @file heap_allocator.hpp
 * @author Kiju
 *
 * @brief This file is just a facade for a general purpose heap allocator.
 */

#include "fr/core/malloc_allocator.hpp"

namespace fr {
/// @brief General purpose heap allocator.
using HeapAllocator = MallocAllocator;
} // namespace fr
