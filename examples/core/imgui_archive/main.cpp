/**
 * @file main.cpp
 * @brief ImGuiWriterArchive example.
 *
 * Opens a minimal SDL3 + OpenGL window and renders an ImGui inspector panel that
 * traverses a part via ImGuiWriterArchive, demonstrating how any type
 * that implements the shape protocol is automatically inspectable.
 */

#include <glad/gl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <SDL3/SDL.h>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/imgui_archive.hpp"
#include "fr/core/math.hpp"
#include "fr/core/shape.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/window.hpp"

// ======================================================================= Parts

/**
 * @brief A simple transform part implementing the shape protocol.
 *
 * This represents the kind of component you'd attach to a game entity.
 * Because it uses FR_SHAPE, ImGuiWriterArchive can inspect it with zero
 * extra code — the same shape method that drives JSON serialization also
 * drives the ImGui inspector.
 */
struct TransformPart {
    fr::Vec3 position{0.0f, 0.0f, 0.0f};
    fr::Vec3 scale{1.0f, 1.0f, 1.0f};
    float rotation_deg{0.0f};
    bool visible{true};

    FR_SHAPE({
        FR_PROP(position);
        FR_PROP(scale);
        FR_PROP(rotation_deg);
        FR_PROP(visible);
    })
};

/**
 * @brief A simple point-light part, also shape-enabled.
 */
struct PointLightPart {
    fr::Vec3 color{1.0f, 0.9f, 0.7f};
    float intensity{1.0f};
    float radius{10.0f};
    bool cast_shadows{false};

    FR_SHAPE({
        FR_PROP(color);
        FR_PROP(intensity);
        FR_PROP(radius);
        FR_PROP(cast_shadows);
    })
};

// ========================================================================= App

static void on_sdl_event(void *event_data, void * /*user_data*/) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(event_data));
}

int main(int /*argc*/, char ** /*argv*/) {
    fr::init_core_ctx();

    {

        // ----------------------------------------------------------------- Window

        fr::Window window{};
        fr::WindowProperties props{};
        props.title = "ImGuiWriterArchive Example";
        props.width = 900;
        props.height = 600;
        props.vsync = true;
        props.api = fr::GRAPHICS_API::OPENGL;

        if (!window.init(props)) {
            fr::shutdown_core_ctx();
            return 1;
        }

        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
            window.close();
            fr::shutdown_core_ctx();
            return 1;
        }

        window.set_event_callback(on_sdl_event, nullptr);

        // ------------------------------------------------------------------ ImGui

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForOpenGL(static_cast<SDL_Window *>(window.get_native_window()),
                                     static_cast<SDL_GLContext>(window.get_native_context()));
        ImGui_ImplOpenGL3_Init("#version 450 core");

        // ------------------------------------------------------------------ State

        TransformPart transform{};
        PointLightPart light{};
        auto transforms = fr::DynamicArray<TransformPart>::from_repeated(10, {});

        fr::ImGuiWriterArchive archive{};

        fr::WindowInput input{};
        bool running = true;

        // ------------------------------------------------------------------- Loop

        while (running) {
            if (!window.poll_events(input) || input.is_key_pressed(fr::Key::Escape)) {
                running = false;
                break;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // ----------------------------------------------------------- Inspector

            ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
            ImGui::Begin("Shape Inspector");

            // Each prop call traverses the shape tree via ImGuiWriterArchive,
            // rendering an appropriate widget for every field automatically.
            archive.prop("Transform", transform);
            ImGui::Separator();
            archive.prop("PointLight", light);
            ImGui::Separator();
            archive.prop("Transforms", transforms);

            ImGui::End();

            // ------------------------------------------------------------- Render

            glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            window.swap_buffers();
        }

        // ---------------------------------------------------------------- Cleanup

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        window.close();
    }

    fr::shutdown_core_ctx();
    return 0;
}
