/**
 * @file window.hpp
 * @author Tfoedy
 *
 * @brief Represents an OS level window.
 */

#pragma once

#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/platform/input.hpp"

namespace fr {

/**
 * @brief Signature for raw platform event callbacks.
 *
 * @param event_data Raw pointer to the underlying OS event.
 * @param data Custom user pointer.
 */
using EventCallbackFun = void (*)(void *event_data, void *data);

/**
 * @brief Supported graphics APIs for the window context.
 */
enum class GRAPHICS_API : U8 {
    OPENGL,
};

/**
 * @brief Mouse/cursor handling mode for the window.
 */
enum class MouseMode : U8 {
    /**
     * @brief Cursor visible, free, absolute motion.
     */
    Normal,

    /**
     * @brief Cursor hidden, free, absolute motion.
     */
    Hidden,

    /**
     * @brief Cursor visible and captured by the application.
     */
    Captured,

    /**
     * @brief Cursor hidden and relative mouse motion enabled.
     *
     * @details
     * Preferred mode for FPS/free camera controls.
     */
    Relative,
};

struct WindowState;

/**
 * @brief Initial configuration properties used during window creation.
 */
struct WindowProperties {
    StringView title{"Farfocel"};
    U32 width{1280};
    U32 height{720};
    bool fullscreen{false};
    bool vsync{true};
    GRAPHICS_API api{GRAPHICS_API::OPENGL};
};

class Window {
public:
    Window() = default;
    ~Window() noexcept;

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    Window(Window &&other) noexcept;
    Window &operator=(Window &&other) noexcept;

    /**
     * @brief Initializes subsystems, creates the window and the graphics context.
     */
    bool init(const WindowProperties &props) noexcept;

    /**
     * @brief Closes the window and releases platform resources.
     */
    void close() noexcept;

    /**
     * @brief Polls OS events and updates input state.
     *
     * @return false if the OS received a quit or close request.
     */
    bool poll_events(WindowInput &input) noexcept;

    void set_event_callback(EventCallbackFun callback, void *data = nullptr) noexcept {
        m_event_callback = callback;
        m_event_data = data;
    }

    /**
     * @brief Swaps front and back buffers.
     */
    void swap_buffers() noexcept;

    /**
     * @brief Sets mouse/cursor behavior.
     *
     * @return true if the platform accepted the requested mode.
     */
    bool set_mouse_mode(MouseMode mode) noexcept;

    /**
     * @brief Returns currently requested mouse mode.
     */
    [[nodiscard]] MouseMode get_mouse_mode() const noexcept {
        return m_mouse_mode;
    }

    /**
     * @brief Shows or hides the cursor without changing capture/relative state.
     */
    void set_cursor_visible(bool visible) noexcept;

    /**
     * @brief Moves the cursor to a window-local position.
     */
    void set_mouse_position(F32 x, F32 y) noexcept;

    U32 get_width() const noexcept {
        return m_width;
    }

    U32 get_height() const noexcept {
        return m_height;
    }

    [[nodiscard]] void *get_native_window() const noexcept;
    [[nodiscard]] void *get_native_context() const noexcept;

    bool is_minimized() const noexcept {
        return m_minimized;
    }

    bool is_focused() const noexcept {
        return m_focused;
    }

    bool was_resized_this_frame() const noexcept {
        return m_resized_this_frame;
    }

private:
    WindowState *m_state{nullptr};

    U32 m_width{0};
    U32 m_height{0};

    bool m_minimized{false};
    bool m_focused{true};
    bool m_resized_this_frame{false};

    MouseMode m_mouse_mode{MouseMode::Normal};
    bool m_ignore_next_mouse_motion{false};

    EventCallbackFun m_event_callback{nullptr};
    void *m_event_data{nullptr};
};

} // namespace fr
