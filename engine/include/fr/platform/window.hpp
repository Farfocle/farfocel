/**
 * @file window.hpp
 * @author Tfoedy
 *
 * @brief Represents an OS level window.
 * * Manages the graphics context and transforms OS messages (e.g., clicks, resizing)
 */
#pragma once
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/keycode.hpp"
#include <cstring>

namespace fr {

/**
 * @brief Signature for the raw event callback function.
 * @param event_data Raw pointer to the underlying OS event (e.g., SDL_Event).
 * @param user_data Custom pointer passed back to the caller.
 */
using EventCallbackFun = void (*)(void *event_data, void *data);

/**
 * @brief Supported graphics APIs for the window context.
 */
enum class GRAPHICS_API : U8 { OPENGL };

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
     * @brief Initializes subsystems, creates the window and the API context.
     * * @param props Configuration settings for the window.
     * @return true if the window was created successfully, false otherwise.
     */
    bool init(const WindowProperties &props) noexcept;
    /**
     * @brief Closes the window and cleans up memory resources allocated by the engine.
     */
    void close() noexcept;

    /**
     * @brief Polls the OS event queue, updating the window state and input state.
     * * @param event Reference to the WindowInput structure to be populated.
     * @return false if the OS received a quit request (e.g., closing the window).
     */
    bool poll_events(WindowInput &event) noexcept;

    void set_event_callback(EventCallbackFun callback, void *data = nullptr) noexcept {
        m_event_callback = callback;
        m_event_data = data;
    }

    /**
     * @brief Swaps the front and back buffers, presenting the rendered frame to the screen.
     */
    void swap_buffers() noexcept;

    /**
     * @brief Retrieves the current window width.
     * * @return Width in pixels.
     */
    U32 get_width() const noexcept {
        return m_width;
    }
    /**
     * @brief Retrieves the current window height.
     * * @return Height in pixels.
     */
    U32 get_height() const noexcept {
        return m_height;
    }

    /**
     * @brief Retrieves the underlying native window handle (e.g., SDL_Window*).
     * Useful for integrating third-party tools like ImGui or Vulkan surfaces.
     * @return Raw pointer to the native window.
     */
    [[nodiscard]] void *get_native_window() const noexcept;

    /**
     * @brief Retrieves the underlying native graphics context (e.g., SDL_GLContext).
     * @return Raw pointer to the native context.
     */
    [[nodiscard]] void *get_native_context() const noexcept;

    /**
     * @brief Checks if the window is currently minimized.
     * * @return true if minimized.
     */
    bool is_minimized() const noexcept {
        return m_minimized;
    }
    /**
     * @brief Checks if the application currently has input focus.
     * * @return true if focused.
     */
    bool is_focused() const noexcept {
        return m_focused;
    }

private:
    WindowState *m_state{nullptr};
    U32 m_width{0};
    U32 m_height{0};
    bool m_minimized{false};
    bool m_focused{true};
    bool m_resized_this_frame{false};

    EventCallbackFun m_event_callback{nullptr};
    void *m_event_data{nullptr};
};
} // namespace fr
