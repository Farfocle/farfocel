#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/platform/window.hpp"

namespace fr {
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
      m_focused(other.m_focused) {
    other.m_state = nullptr;
}

Window &Window::operator=(Window &&other) noexcept {
    if (this != &other) {
        close();
        m_state = other.m_state;
        m_width = other.m_width;
        m_height = other.m_height;
        m_minimized = other.m_minimized;
        m_focused = other.m_focused;
        other.m_state = nullptr;
    }
    return *this;
}

bool Window::init(const WindowProperties &properties) noexcept {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
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

    if (properties.fullscreen)
        window_flags |= SDL_WINDOW_FULLSCREEN;

    Alloc *alloc = get_ambient_ctx().alloc;
    m_state =
        static_cast<WindowState *>(alloc->allocate(sizeof(WindowState), alignof(WindowState)));
    new (m_state) WindowState();

    fr::String title = fr::String::from_view(properties.title);

    m_state->window =
        SDL_CreateWindow(title.data(), properties.width, properties.height, window_flags);
    if (!m_state->window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        close();
        return false;
    }

    if (properties.api == GRAPHICS_API::OPENGL) {
        m_state->gl_context = SDL_GL_CreateContext(m_state->window);
        if (!m_state->gl_context) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GL_CreateContext failed: %s",
                         SDL_GetError());
            close();
            return false;
        }

        SDL_GL_SetSwapInterval(properties.vsync ? 1 : 0);
    }

    // SDL_SetWindowRelativeMouseMode(m_state->window, true);

    m_width = properties.width;
    m_height = properties.height;

    return true;
}

void Window::close() noexcept {
    if (m_state) {
        if (m_state->gl_context)
            SDL_GL_DestroyContext(m_state->gl_context);
        if (m_state->window)
            SDL_DestroyWindow(m_state->window);

        m_state->~WindowState();
        get_ambient_ctx().alloc->deallocate(m_state, sizeof(WindowState), alignof(WindowState));

        m_state = nullptr;
    }

    SDL_Quit();
}

bool Window::poll_events(WindowInput &out_input) noexcept {
    out_input.reset();
    m_resized_this_frame = false;

    SDL_Event evnt;
    while (SDL_PollEvent(&evnt)) {
        if (m_event_callback) {
            m_event_callback(&evnt, m_event_data);
        }

        switch (evnt.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            return false;

        // WINDOW STATE
        case SDL_EVENT_WINDOW_RESIZED:
            m_width = static_cast<U32>(evnt.window.data1);
            m_height = static_cast<U32>(evnt.window.data2);
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
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            m_focused = false;
            break;

        // INPUT STATE
        case SDL_EVENT_KEY_DOWN:
            if (evnt.key.scancode < 512) {
                if (!out_input.keys_down[evnt.key.scancode]) {
                    out_input.keys_pressed[evnt.key.scancode] = true;
                }
                out_input.keys_down[evnt.key.scancode] = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            if (evnt.key.scancode < 512) {
                out_input.keys_down[evnt.key.scancode] = false;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            out_input.mouse_x = evnt.motion.x;
            out_input.mouse_y = evnt.motion.y;
            out_input.mouse_delta_x += evnt.motion.xrel;
            out_input.mouse_delta_y += evnt.motion.yrel;
            break;
        }
    }
    return true;
}
void Window::swap_buffers() noexcept {
    FR_ASSERT(m_state && m_state->window, "Window must be initialized");
    if (m_state->gl_context)
        SDL_GL_SwapWindow(m_state->window);
}

void *Window::get_native_window() const noexcept {
    return m_state ? m_state->window : nullptr;
}
void *Window::get_native_context() const noexcept {
    return m_state ? m_state->gl_context : nullptr;
}
} // namespace fr
