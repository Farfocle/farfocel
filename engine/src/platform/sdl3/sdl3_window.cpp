#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include "fr/core/ctx.hpp"
#include "fr/platform/window.hpp"

#include "fr/core/alloc.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"

#include <SDL3/SDL.h>

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
      m_focused(other.m_focused),
      m_resize_callback(std::move(other.m_resize_callback)),
      m_minimize_callback(std::move(other.m_minimize_callback)) {
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
        m_resize_callback = std::move(other.m_resize_callback);
        m_minimize_callback = std::move(other.m_minimize_callback);
        other.m_state = nullptr;
    }
    return *this;
}

bool Window::init(const WindowProperties &properties) noexcept {
    if (!SDL_Init(SDL_INIT_VIDEO))
        return false;

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
        close();
        return false;
    }

    if (properties.api == GRAPHICS_API::OPENGL) {
        m_state->gl_context = SDL_GL_CreateContext(m_state->window);
        if (!m_state->gl_context) {
            close();
            return false;
        }
        SDL_GL_SetSwapInterval(properties.vsync ? 1 : 0);
    }

    SDL_SetWindowRelativeMouseMode(m_state->window, true);

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
    }

    SDL_Quit();
}

bool Window::poll_events() noexcept {
    SDL_Event evnt;
    while (SDL_PollEvent(&evnt)) {
        switch (evnt.type) {
        case SDL_EVENT_QUIT:
            return false;

        case SDL_EVENT_WINDOW_RESIZED: {

            m_width = static_cast<U32>(evnt.window.data1);
            m_height = static_cast<U32>(evnt.window.data2);
            if (m_resize_callback)
                m_resize_callback(m_width, m_height);
            break;
        }

        case SDL_EVENT_WINDOW_MINIMIZED: {

            m_minimized = true;
            if (m_minimize_callback)
                m_minimize_callback(true);
            break;
        }

        case SDL_EVENT_WINDOW_RESTORED: {

            m_minimized = false;
            if (m_minimize_callback)
                m_minimize_callback(false);
            break;
        }

        case SDL_EVENT_WINDOW_FOCUS_GAINED: {

            m_focused = true;
            break;
        }

        case SDL_EVENT_WINDOW_FOCUS_LOST: {

            m_focused = false;
            break;
        }
        }
    }

    return true;
}

void Window::swap_buffers() noexcept {
    FR_ASSERT(m_state && m_state->window, "Window must be initialized");
    if (m_state->gl_context)
        SDL_GL_SwapWindow(m_state->window);
}

U32 Window::get_width() const noexcept {
    return m_width;
}
U32 Window::get_height() const noexcept {
    return m_height;
}
bool Window::is_minimized() const noexcept {
    return m_minimized;
}
bool Window::is_focused() const noexcept {
    return m_focused;
}

void Window::set_resize_callback(ResizeCallback callback) noexcept {
    m_resize_callback = std::move(callback);
}

void Window::set_minimize_callback(MinimizeCallback callback) noexcept {
    m_minimize_callback = std::move(callback);
}

} // namespace fr
