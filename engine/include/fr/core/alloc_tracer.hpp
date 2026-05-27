/**
 * @file alloc_tracer.hpp
 * @author Kiju
 *
 * @brief Thread-safe allocation tracer that records allocations, rellocations and deallocations.
 * Used to view and debug memory usage.
 *
 * @details Under the hood `AllocTracer` uses a fixed-sized buffer that functions like a ring
 * buffer.
 */

#pragma once

#include <mutex>
#include <new>
#include <utility>

#include "fr/core/alloc_typedefs.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Thread-safe allocation tracer that records allocations, rellocations and deallocations.
 * Used to view and debug memory usage.
 */
class AllocTracer {
public:
    // ----------------------------------------------- Constructors & Destructor
    AllocTracer() = delete;

    /**
     * @brief Construct an allocation tracer.
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

    // ----------------------------------------------------------------- Methods

    /**
     * @brief Returns the current number of frames in the buffer.
     */
    USize size() const noexcept {
        return (m_size < m_capacity) ? m_size : m_capacity;
    }

    /**
     * @brief Returns the number of all recorded frames.
     * @note Because the buffer wraps around once full, the actual number of recorded frames may
     * differ from the saturation of the buffer.
     */
    USize count() const noexcept {
        return m_size;
    }

    /**
     * @brief Returns the capacity of the tracer.
     */
    USize capacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Checks if the tracer is empty.
     */
    bool is_empty() const noexcept {
        return m_size == 0;
    }

    /**
     * @brief Checks if the tracer buffer is full.
     */
    bool is_full() const noexcept {
        return m_size >= m_capacity;
    }

    /**
     * @brief Returns a slice of recorded frames.
     */
    Slice<const AllocFrame> frames() const noexcept {
        return Slice(m_frames, size());
    }

    /**
     * @brief Records an allocation frame.
     */
    void record(AllocFrame &&frame) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_frames[m_size % m_capacity] = std::move(frame);
        ++m_size;
    }

private:
    // -------------------------------------------------------- Member Variables
    AllocFrame *m_frames{nullptr};
    USize m_capacity{0};
    USize m_size{0};
    std::mutex m_mutex{};
};
} // namespace fr
