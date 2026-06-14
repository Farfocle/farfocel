/**
 * @file input.hpp
 * @author Tfoedy
 *
 * @brief Global window input state representation for the current frame.
 */

#pragma once

#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/platform/keycode.hpp"

namespace fr {

/**
 * @brief Per-frame window input state.
 *
 * @details
 * Held states persist until a matching release event or focus loss.
 * Pressed/released states and mouse deltas are transient and reset every frame.
 */
struct WindowInput {
    /** @brief Raw flags indicating if a key is currently held down. */
    bool keys_down[static_cast<USize>(Key::MaxKeys)]{false};

    /** @brief Raw flags indicating if a key was just pressed this frame. */
    bool keys_pressed[static_cast<USize>(Key::MaxKeys)]{false};

    /** @brief Raw flags indicating if a key was just released this frame. */
    bool keys_released[static_cast<USize>(Key::MaxKeys)]{false};

    /** @brief Raw flags indicating if a mouse button is currently held down. */
    bool mouse_down[static_cast<USize>(MouseButton::MaxButtons)]{false};

    /** @brief Raw flags indicating if a mouse button was just pressed this frame. */
    bool mouse_pressed[static_cast<USize>(MouseButton::MaxButtons)]{false};

    /** @brief Raw flags indicating if a mouse button was just released this frame. */
    bool mouse_released[static_cast<USize>(MouseButton::MaxButtons)]{false};

    /** @brief Absolute mouse cursor position on the X axis in window pixels. */
    F32 mouse_x{0.0f};

    /** @brief Absolute mouse cursor position on the Y axis in window pixels. */
    F32 mouse_y{0.0f};

    /** @brief Relative mouse movement on the X axis since the last frame. */
    F32 mouse_delta_x{0.0f};

    /** @brief Relative mouse movement on the Y axis since the last frame. */
    F32 mouse_delta_y{0.0f};

    /** @brief Horizontal mouse wheel delta accumulated during this frame. */
    F32 mouse_wheel_x{0.0f};

    /** @brief Vertical mouse wheel delta accumulated during this frame. */
    F32 mouse_wheel_y{0.0f};

    [[nodiscard]] bool is_key_down(Key key) const noexcept {
        return keys_down[static_cast<USize>(key)];
    }

    [[nodiscard]] bool is_key_pressed(Key key) const noexcept {
        return keys_pressed[static_cast<USize>(key)];
    }

    [[nodiscard]] bool is_key_released(Key key) const noexcept {
        return keys_released[static_cast<USize>(key)];
    }

    [[nodiscard]] bool is_mouse_down(MouseButton button) const noexcept {
        return mouse_down[static_cast<USize>(button)];
    }

    [[nodiscard]] bool is_mouse_pressed(MouseButton button) const noexcept {
        return mouse_pressed[static_cast<USize>(button)];
    }

    [[nodiscard]] bool is_mouse_released(MouseButton button) const noexcept {
        return mouse_released[static_cast<USize>(button)];
    }

    /**
     * @brief Clears transient per-frame input state.
     */
    void reset_frame_state() noexcept {
        fr::mem::set_raw_range(keys_pressed, 0, static_cast<USize>(Key::MaxKeys));
        fr::mem::set_raw_range(keys_released, 0, static_cast<USize>(Key::MaxKeys));

        fr::mem::set_raw_range(mouse_pressed, 0, static_cast<USize>(MouseButton::MaxButtons));
        fr::mem::set_raw_range(mouse_released, 0, static_cast<USize>(MouseButton::MaxButtons));

        mouse_delta_x = 0.0f;
        mouse_delta_y = 0.0f;

        mouse_wheel_x = 0.0f;
        mouse_wheel_y = 0.0f;
    }

    /**
     * @brief Clears both held and transient input state.
     *
     * @details
     * This is primarily used on focus loss to avoid stuck keys/buttons.
     */
    void reset_all() noexcept {
        fr::mem::set_raw_range(keys_down, 0, static_cast<USize>(Key::MaxKeys));
        fr::mem::set_raw_range(mouse_down, 0, static_cast<USize>(MouseButton::MaxButtons));

        reset_frame_state();
    }

    /**
     * @brief Backward-compatible alias for frame reset.
     */
    void reset() noexcept {
        reset_frame_state();
    }
};

} // namespace fr
