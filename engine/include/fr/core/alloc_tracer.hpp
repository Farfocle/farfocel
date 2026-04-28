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

#include "fr/core/alloc_typedefs.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Allocation tracer is used to record allocations, rellocations and deallocations.
 *
 * It uses a raw memory buffer allocated at construction. If the buffer is full recordining of the
 * frames wraps around.
 */
class AllocTracer {
public:
    AllocTracer() = delete;
    /**
     * @brief Construct an allocation tracer.
     *
     * @param capacity Number of frames to store.
     */
    AllocTracer(USize capacity) noexcept {
        FR_ASSERT(capacity > 0, "capacity must be non-zero");

        m_capacity = capacity;
        m_frames = new (std::nothrow) AllocFrame[capacity];
    }

    ~AllocTracer() {
        delete[] m_frames;
    }

    /**
     * @brief Returns the current size of the buffer.
     *
     * @return Buffer size.
     */
    USize size() const noexcept {
        return m_size % m_capacity;
    }

    /**
     * @brief Returns the number of all recorded frames.
     *
     * @return Count of recorded frames.
     * @note Because the buffer wraps around once full, the actual number of recorded frames may
     * differ from the saturation of the buffer.
     */
    USize count() const noexcept {
        return m_size;
    }

    /**
     * @brief Returns the capacity of the tracer.
     *
     * @return Capacity in frames.
     */
    USize capacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Checks if the tracer is empty.
     *
     * @return True if no frames recorded.
     */
    bool is_empty() const noexcept {
        return m_size == 0;
    }

    /**
     * @brief Checks if the tracer buffer is full.
     *
     * @return True if buffer is full.
     */
    bool is_full() const noexcept {
        return m_size == m_capacity;
    }

    /**
     * @brief Returns a slice of recorded frames.
     *
     * @return Slice of AllocFrame.
     */
    Slice<const AllocFrame> frames() const noexcept {
        return Slice(m_frames, m_size);
    }

    /**
     * @brief Records an allocation frame.
     *
     * @param frame Frame to record.
     */
    void record(AllocFrame &&frame) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);

        // @noexcept: AllocationFrame is a trivially constructible POD - std::construct_at will not
        // throw.
        std::construct_at(m_frames + m_size, std::move(frame));
        ++m_size;
    }

private:
    AllocFrame *m_frames{nullptr};
    USize m_capacity{0};
    USize m_size{0};
    std::mutex m_mutex{};
};
} // namespace fr
