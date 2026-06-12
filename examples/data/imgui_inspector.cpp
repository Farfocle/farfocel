#include <glad/gl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <SDL3/SDL.h>

#include "fr/core/ctx.hpp"
#include "fr/core/imgui_archive.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/data/world.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/window.hpp"

// ======================================================================= Parts

struct Pos {
    F32 x{0.0f};
    F32 y{0.0f};

    FR_SHAPE({
        FR_PROP(x);
        FR_PROP(y);
    })
};

FR_TYPE(Pos);

struct Health {
    F32 current{100.0f};
    F32 max{100.0f};

    FR_SHAPE({
        FR_PROP(current);
        FR_PROP(max);
    })
};

FR_TYPE(Health);

struct GameConfig {
    U32 tick_rate{60};
    F32 gravity{9.81f};

    FR_SHAPE({
        FR_PROP(tick_rate);
        FR_PROP(gravity);
    })
};

FR_TYPE(GameConfig);

// ========================================================================= App

static void on_sdl_event(void *event_data, void * /*user_data*/) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(event_data));
}

int main(int /*argc*/, char ** /*argv*/) {
    fr::init_core_ctx();

    {
        // ------------------------------------------------------------------ World

        fr::World world;
        world.emplace_resource<GameConfig>(GameConfig{.tick_rate = 30, .gravity = 20.0f});

        fr::Thing a = world.spawn();
        world.emplace_now<Pos>(a, Pos{.x = 1.0f, .y = 2.0f});
        world.emplace_now<Health>(a, Health{.current = 80.0f, .max = 100.0f});

        fr::Thing b = world.spawn();
        world.emplace_now<Pos>(b, Pos{.x = 5.0f, .y = 3.0f});

        // ----------------------------------------------------------------- Window

        fr::Window window{};
        fr::WindowProperties props{};
        props.title = "World Inspector";
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

            ImGui::SetNextWindowSize(ImVec2(500, 550), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
            ImGui::Begin("World Inspector");

            fr::ImGuiWriterArchive archive{};
            call_shape(archive, world);

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
