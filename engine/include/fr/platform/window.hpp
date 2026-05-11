#pragma once
#include <functional>

#include "fr/core/string_view.hpp"

namespace fr {

enum class GRAPHICS_API : U8 { OPENGL };

struct WindowState;

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
    using ResizeCallback = std::function<void(U32, U32)>;
    using MinimizeCallback = std::function<void(bool)>;

    Window() noexcept = default;
    ~Window() noexcept;

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    Window(Window &&other) noexcept;
    Window &operator=(Window &&other) noexcept;

    bool init(const WindowProperties &properties) noexcept;
    void close() noexcept;

    bool poll_events() noexcept;
    void swap_buffers() noexcept;

    U32 get_width() const noexcept;
    U32 get_height() const noexcept;
    bool is_minimized() const noexcept;
    bool is_focused() const noexcept;

    void set_resize_callback(ResizeCallback callback) noexcept;
    void set_minimize_callback(MinimizeCallback callback) noexcept;

private:
    WindowState *m_state{nullptr};

    U32 m_width{0};
    U32 m_height{0};
    bool m_minimized{false};
    bool m_focused{true};

    ResizeCallback m_resize_callback;
    MinimizeCallback m_minimize_callback;
};
} // namespace fr
