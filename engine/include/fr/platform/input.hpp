/**
 * @file input.hpp
 * @author Tfoedy
 *
 * @brief Global window input state representation for the current frame.
 */
#pragma once
#include "fr/core/typedefs.hpp"
#include "fr/platform/keycode.hpp"
#include <cstring>

namespace fr {
struct WindowInput {
    /** @brief Raw flags indicating if a key is currently held down. */
    bool keys_down[static_cast<USize>(Key::MaxKeys)]{false};
    /** @brief Raw flags indicating if a key was just pressed this frame. */
    bool keys_pressed[static_cast<USize>(Key::MaxKeys)]{false};

    /** @brief Absolute mouse cursor position on the X axis (pixels). */
    float mouse_x{0.0f};
    /** @brief Absolute mouse cursor position on the Y axis (pixels). */
    float mouse_y{0.0f};
    /** @brief Relative mouse movement on the X axis since the last frame. */
    float mouse_delta_x{0.0f};
    /** @brief Relative mouse movement on the Y axis since the last frame. */
    float mouse_delta_y{0.0f};
    /**
     * @brief Checks if the specified key is currently held down.
     * * @param key Strongly typed keycode (fr::Key).
     * @return true if the key is down.
     */
    bool is_key_down(Key key) const noexcept {
        return keys_down[static_cast<U16>(key)];
    }
    /**
     * @brief Checks if the specified key was pressed exactly during this frame.
     * * @param key Strongly typed keycode (fr::Key).
     * @return true if the key was just pressed (transitioned from up to down).
     */
    bool is_key_pressed(Key key) const noexcept {
        return keys_pressed[static_cast<U16>(key)];
    }
    /**
     * @brief Resets single-frame states (deltas and just_pressed flags).
     * * @note Called automatically by the Window polling system before processing new events.
     */
    void reset() noexcept {
        std::memset(keys_pressed, 0, sizeof(keys_pressed));
        mouse_delta_x = 0.0f;
        mouse_delta_y = 0.0f;
    }
};

} // namespace fr
