/**
 * @file setup.hpp
 * @author Kiju
 * @brief ImGui backend setup helpers for SDL3 + OpenGL3.
 */

#pragma once

#include "fr/platform/window.hpp"

namespace fr::scene {

struct ImGuiSetupDesc {
    Window *window{nullptr};
    const char *glsl_version{"#version 450"};
    bool nav_keyboard{true};
    bool dark_style{true};
};

/// @brief Initializes ImGui with SDL3 + OpenGL3 backends and hooks the window event callback.
bool imgui_init(const ImGuiSetupDesc &desc) noexcept;

/// @brief Shuts down ImGui SDL3 + OpenGL3 backends and clears the window event callback.
void imgui_shutdown(Window &window) noexcept;

/// @brief Starts a new ImGui frame. Call before any ImGui widgets.
void imgui_begin_frame() noexcept;

/// @brief Finalizes and renders the ImGui frame. Call after all ImGui widgets.
void imgui_end_frame() noexcept;

} // namespace fr::scene
