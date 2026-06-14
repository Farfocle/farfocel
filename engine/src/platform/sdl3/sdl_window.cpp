#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <new>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"
#include "fr/platform/window.hpp"

namespace fr {
namespace {

[[nodiscard]] bool is_valid_key_scancode(USize scancode) noexcept {
    return scancode < static_cast<USize>(Key::MaxKeys);
}

[[nodiscard]] bool is_valid_mouse_button(USize button) noexcept {
    return button < static_cast<USize>(MouseButton::MaxButtons);
}

} // namespace

struct WindowState {
    SDL_Window *window{nullptr};
    SDL_GLContext gl_context{nullptr};
};

Window::~Window() noexcept {
    close();
}

Window::Window(Window &&other) noexcept
    : m_state(other.m_state),
      m_width(other.m_width),
      m_height(other.m_height),
      m_minimized(other.m_minimized),
      m_focused(other.m_focused),
      m_resized_this_frame(other.m_resized_this_frame),
      m_mouse_mode(other.m_mouse_mode),
      m_ignore_next_mouse_motion(other.m_ignore_next_mouse_motion),
      m_event_callback(other.m_event_callback),
      m_event_data(other.m_event_data) {
    other.m_state = nullptr;
    other.m_width = 0;
    other.m_height = 0;
    other.m_minimized = false;
    other.m_focused = true;
    other.m_resized_this_frame = false;
    other.m_mouse_mode = MouseMode::Normal;
    other.m_ignore_next_mouse_motion = false;
    other.m_event_callback = nullptr;
    other.m_event_data = nullptr;
}

Window &Window::operator=(Window &&other) noexcept {
    if (this != &other) {
        close();

        m_state = other.m_state;
        m_width = other.m_width;
        m_height = other.m_height;
        m_minimized = other.m_minimized;
        m_focused = other.m_focused;
        m_resized_this_frame = other.m_resized_this_frame;
        m_mouse_mode = other.m_mouse_mode;
        m_ignore_next_mouse_motion = other.m_ignore_next_mouse_motion;
        m_event_callback = other.m_event_callback;
        m_event_data = other.m_event_data;

        other.m_state = nullptr;
        other.m_width = 0;
        other.m_height = 0;
        other.m_minimized = false;
        other.m_focused = true;
        other.m_resized_this_frame = false;
        other.m_mouse_mode = MouseMode::Normal;
        other.m_ignore_next_mouse_motion = false;
        other.m_event_callback = nullptr;
        other.m_event_data = nullptr;
    }

    return *this;
}

bool Window::init(const WindowProperties &properties) noexcept {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        FR_LOG_ERR("[Window] SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    U32 window_flags = SDL_WINDOW_RESIZABLE;

    if (properties.api == GRAPHICS_API::OPENGL) {
        window_flags |= SDL_WINDOW_OPENGL;

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    }

    if (properties.fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "allocator must be non-null");

    m_state =
        static_cast<WindowState *>(alloc->allocate(sizeof(WindowState), alignof(WindowState)));
    new (m_state) WindowState();

    String title = String::from_view(properties.title);

    m_state->window =
        SDL_CreateWindow(title.c_str(), properties.width, properties.height, window_flags);

    if (!m_state->window) {
        FR_LOG_ERR("[Window] SDL_CreateWindow failed: {}", SDL_GetError());
        close();
        return false;
    }

    if (properties.api == GRAPHICS_API::OPENGL) {
        m_state->gl_context = SDL_GL_CreateContext(m_state->window);
        if (!m_state->gl_context) {
            FR_LOG_ERR("[Window] SDL_GL_CreateContext failed: {}", SDL_GetError());
            close();
            return false;
        }

        SDL_GL_SetSwapInterval(properties.vsync ? 1 : 0);
    }

    m_width = properties.width;
    m_height = properties.height;
    m_minimized = false;
    m_focused = true;
    m_resized_this_frame = false;
    m_mouse_mode = MouseMode::Normal;
    m_ignore_next_mouse_motion = false;

    return true;
}

void Window::close() noexcept {
    if (m_state) {
        set_mouse_mode(MouseMode::Normal);

        if (m_state->gl_context) {
            SDL_GL_DestroyContext(m_state->gl_context);
            m_state->gl_context = nullptr;
        }

        if (m_state->window) {
            SDL_DestroyWindow(m_state->window);
            m_state->window = nullptr;
        }

        m_state->~WindowState();
        get_ambient_ctx().alloc->deallocate(m_state, sizeof(WindowState), alignof(WindowState));
        m_state = nullptr;
    }

    SDL_Quit();
}

bool Window::poll_events(WindowInput &out_input) noexcept {
    out_input.reset_frame_state();
    m_resized_this_frame = false;

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (m_event_callback) {
            m_event_callback(&event, m_event_data);
        }

        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            return false;

        case SDL_EVENT_WINDOW_RESIZED:
            m_width = static_cast<U32>(event.window.data1);
            m_height = static_cast<U32>(event.window.data2);
            m_resized_this_frame = true;
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
            m_minimized = true;
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            m_minimized = false;
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            m_focused = true;
            m_ignore_next_mouse_motion = true;
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            m_focused = false;
            out_input.reset_all();
            break;

        case SDL_EVENT_KEY_DOWN: {
            const USize scancode = static_cast<USize>(event.key.scancode);

            if (is_valid_key_scancode(scancode)) {
                if (!out_input.keys_down[scancode]) {
                    out_input.keys_pressed[scancode] = true;
                }

                out_input.keys_down[scancode] = true;
            }
            break;
        }

        case SDL_EVENT_KEY_UP: {
            const USize scancode = static_cast<USize>(event.key.scancode);

            if (is_valid_key_scancode(scancode)) {
                out_input.keys_down[scancode] = false;
                out_input.keys_released[scancode] = true;
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            const USize button = static_cast<USize>(event.button.button);

            if (is_valid_mouse_button(button)) {
                if (!out_input.mouse_down[button]) {
                    out_input.mouse_pressed[button] = true;
                }

                out_input.mouse_down[button] = true;
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const USize button = static_cast<USize>(event.button.button);

            if (is_valid_mouse_button(button)) {
                out_input.mouse_down[button] = false;
                out_input.mouse_released[button] = true;
            }
            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
            out_input.mouse_x = event.motion.x;
            out_input.mouse_y = event.motion.y;

            if (m_ignore_next_mouse_motion) {
                m_ignore_next_mouse_motion = false;
                break;
            }

            out_input.mouse_delta_x += event.motion.xrel;
            out_input.mouse_delta_y += event.motion.yrel;
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            out_input.mouse_wheel_x += event.wheel.x;
            out_input.mouse_wheel_y += event.wheel.y;
            break;

        default:
            break;
        }
    }

    return true;
}

void Window::swap_buffers() noexcept {
    FR_ASSERT(m_state && m_state->window, "Window must be initialized");

    if (m_state->gl_context) {
        SDL_GL_SwapWindow(m_state->window);
    }
}

bool Window::set_mouse_mode(MouseMode mode) noexcept {
    if (!m_state || !m_state->window) {
        return false;
    }

    bool ok = true;

    switch (mode) {
    case MouseMode::Normal:
        ok = SDL_SetWindowRelativeMouseMode(m_state->window, false) && ok;
        ok = SDL_CaptureMouse(false) && ok;
        SDL_ShowCursor();
        break;

    case MouseMode::Hidden:
        ok = SDL_SetWindowRelativeMouseMode(m_state->window, false) && ok;
        ok = SDL_CaptureMouse(false) && ok;
        SDL_HideCursor();
        break;

    case MouseMode::Captured:
        ok = SDL_SetWindowRelativeMouseMode(m_state->window, false) && ok;
        ok = SDL_CaptureMouse(true) && ok;
        SDL_ShowCursor();
        break;

    case MouseMode::Relative:
        ok = SDL_CaptureMouse(true) && ok;
        ok = SDL_SetWindowRelativeMouseMode(m_state->window, true) && ok;
        SDL_HideCursor();
        m_ignore_next_mouse_motion = true;
        break;
    }

    if (!ok) {
        FR_LOG_ERR("[Window] Failed to set mouse mode: {}", SDL_GetError());
        return false;
    }

    m_mouse_mode = mode;
    return true;
}

void Window::set_cursor_visible(bool visible) noexcept {
    if (visible) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

void Window::set_mouse_position(F32 x, F32 y) noexcept {
    if (!m_state || !m_state->window) {
        return;
    }

    SDL_WarpMouseInWindow(m_state->window, x, y);
    m_ignore_next_mouse_motion = true;
}

void *Window::get_native_window() const noexcept {
    return m_state ? m_state->window : nullptr;
}

void *Window::get_native_context() const noexcept {
    return m_state ? m_state->gl_context : nullptr;
}

} // namespace fr
