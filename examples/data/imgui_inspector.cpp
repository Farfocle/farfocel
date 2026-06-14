/**
 * @file imgui_inspector.cpp
 * @author Kiju
 *
 * @brief Demonstrates the world inspector devtools panel.
 */

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "fr/core/ctx.hpp"
#include "fr/core/math.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/inspector.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/window.hpp"

// ======================================================================= Parts

struct Health {
    F32 current{100.0f};
    F32 max{100.0f};

    FR_SHAPE({
        FR_PROP(current);
        FR_PROP(max);
    })
};

FR_TYPE(Health);

struct Velocity {
    fr::Vec3 linear{};
    F32 speed{1.0f};

    FR_SHAPE({
        FR_PROP(linear);
        FR_PROP(speed);
    })
};

FR_TYPE(Velocity);

// ======================================================================== Game

static void on_sdl_event(void *event_data, void * /*user_data*/) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(event_data));
}

static void inspector_system(fr::Scope scope) {
    auto &state = scope.get_resource<fr::devtools::InspectorState>();

    ImGui::SetNextWindowSize(ImVec2(1100.0f, 660.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("World Inspector")) {
        fr::devtools::inspector_ui(scope.world(), state);
    }

    ImGui::End();
}

struct Game {
    fr::World world;
    fr::Thing player;
    fr::Window window;
    fr::WindowInput input;
    bool running{true};

    bool init() {
        if (!do_init_window()) {
            return false;
        }

        do_init_things();
        do_init_systems();

        return true;
    }

    void run_loop() {
        while (running) {
            if (!window.poll_events(input) || input.is_key_pressed(fr::Key::Escape)) {
                break;
            }

            do_begin_frame();
            world.run();
            world.commit();
            do_end_frame();
        }
    }

    void shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        window.close();
    }

private:
    bool do_init_window() {
        fr::WindowProperties props{
            .title = "World Inspector",
            .width = 1200,
            .height = 720,
            .vsync = true,
            .api = fr::GRAPHICS_API::OPENGL,
        };

        if (!window.init(props)) {
            return false;
        }

        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
            window.close();
            return false;
        }

        window.set_event_callback(on_sdl_event, nullptr);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL3_InitForOpenGL(static_cast<SDL_Window *>(window.get_native_window()),
                                     static_cast<SDL_GLContext>(window.get_native_context()));
        ImGui_ImplOpenGL3_Init("#version 450 core");

        return true;
    }

    void do_init_things() {
        player = world.spawn();
        world.insert(player, fr::RelationsPart{});
        world.insert(player, fr::LocalTransformPart{});
        world.insert(player, Health{.current = 100.0f, .max = 100.0f});
        world.insert(player, Velocity{.linear = {1.0f, 0.0f, 0.0f}, .speed = 5.0f});

        world.emplace_resource<fr::devtools::InspectorState>();

        world.commit();
    }

    void do_init_systems() {
        world.schedule(fr::Stage::Update, inspector_system);
    }

    void do_begin_frame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void do_end_frame() {
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        window.swap_buffers();
    }
};

int main() {
    fr::init_core_ctx();

    {
        Game game;

        if (game.init()) {
            game.run_loop();
        }

        game.shutdown();
    }

    fr::shutdown_core_ctx();
    return 0;
}
