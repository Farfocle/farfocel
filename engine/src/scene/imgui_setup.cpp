/**
 * @file setup.cpp
 * @brief ImGui SDL3 + OpenGL3 backend setup implementations.
 */

#include "fr/scene/imgui_setup.hpp"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <ImGuizmo.h>
#include <imgui.h>

namespace fr::scene {

bool imgui_init(const ImGuiSetupDesc &desc) noexcept {
    if (!desc.window) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    if (desc.nav_keyboard) {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }

    if (desc.dark_style) {
        ImGui::StyleColorsDark();
    }

    SDL_Window *sdl_window =
        static_cast<SDL_Window *>(desc.window->get_native_window());
    SDL_GLContext gl_ctx =
        static_cast<SDL_GLContext>(desc.window->get_native_context());

    if (!ImGui_ImplSDL3_InitForOpenGL(sdl_window, gl_ctx)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init(desc.glsl_version)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    desc.window->set_event_callback(
        [](void *ev, void *) {
            ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(ev));
        },
        nullptr);

    return true;
}

void imgui_shutdown(Window &window) noexcept {
    window.set_event_callback(nullptr, nullptr);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void imgui_begin_frame() noexcept {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void imgui_end_frame() noexcept {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace fr::scene
