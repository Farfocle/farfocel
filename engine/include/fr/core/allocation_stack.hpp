/**
 * @file allocator_debug.hpp
 * @author Kiju
 *
 * @brief Helpers for debugging allocators. For now only AllocationStack is implemented, but
 * propably it is more than enough for our needs. AllocationStack is ment to be held in a global
 * context where allocators can access it to record allocations.
 */

#pragma once

#include <memory>
#include <mutex>
#include <new>
#include <utility>

#include "fr/core/allocator_typedefs.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Allocation stack is used to record allocations, rellocations and deallocations. It uses a
/// raw memory buffer allocated at construction. If the buffer is full recordining of the frames
/// wraps around.
class AllocationStack {
public:
    AllocationStack() = delete;
    AllocationStack(USize capacity) noexcept {
        FR_ASSERT(capacity > 0,
                  "fr::AllocationStack(USize capacity) -> Capacity has to be greater than 0");

        m_capacity = capacity;
        m_frames = new (std::nothrow) AllocationFrame[capacity];
    }

    ~AllocationStack() {
        delete[] m_frames;
    }

    /// @biref Returns the current size of the buffer.
    USize size() const noexcept {
        return m_size % m_capacity;
    }

    /// @brief Returns the number of all recorded frames. Because the buffer wraps around once full,
    /// the actual number of recorded frames may differ from the saturation of the buffer.
    USize count() const noexcept {
        return m_size;
    }

    USize capacity() const noexcept {
        return m_capacity;
    }

    bool is_empty() const noexcept {
        return m_size == 0;
    }

    bool is_full() const noexcept {
        return m_size == m_capacity;
    }

    Slice<const AllocationFrame> frames() const noexcept {
        return Slice(m_frames, m_size);
    }

    void record(AllocationFrame &&frame) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);

        // @noexcept: AllocationFrame is a trivially constructible POD - std::construct_at will not
        // throw.
        std::construct_at(m_frames + m_size, std::move(frame));
        ++m_size;
    }

private:
    AllocationFrame *m_frames{nullptr};
    USize m_capacity{0};
    USize m_size{0};
    std::mutex m_mutex{};
};
} // namespace fr
