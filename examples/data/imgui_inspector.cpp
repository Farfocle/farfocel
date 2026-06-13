#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "fr/core/ctx.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/window.hpp"
#include "fr/renderer/imgui_archive.hpp"

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
    fr::Vec3 linear{0.0f, 0.0f, 0.0f};
    F32 speed{1.0f};

    FR_SHAPE({
        FR_PROP(linear);
        FR_PROP(speed);
    })
};

FR_TYPE(Velocity);

struct GameConfig {
    U32 tick_rate{60};
    F32 gravity{9.81f};
    bool paused{false};

    FR_SHAPE({
        FR_PROP(tick_rate);
        FR_PROP(gravity);
        FR_PROP(paused);
    })
};

FR_TYPE(GameConfig);

// ========================================================================= App

static void on_sdl_event(void *event_data, void * /*user_data*/) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(event_data));
}

int main() {
    fr::init_core_ctx();

    {
        fr::World world;

        // --------------------------------------------------------------- World

        world.emplace_resource<GameConfig>(GameConfig{
            .tick_rate = 30,
            .gravity = 20.0f,
        });

        // ---- player (root)
        fr::Thing player = world.spawn();
        world.emplace_now<fr::RelationsPart>(player);

        {
            fr::LocalTransformPart t;
            t.position = {0.0f, 1.8f, 0.0f};
            t.scale = {1.0f, 1.0f, 1.0f};
            world.emplace_now<fr::LocalTransformPart>(player, t);
        }

        world.emplace_now<Health>(player, Health{.current = 100.0f, .max = 100.0f});
        world.emplace_now<Velocity>(player, Velocity{.linear = {1.0f, 0.0f, 0.0f}, .speed = 5.0f});

        // ---- 3 enemies, each parented to player
        for (int i = 0; i < 3; ++i) {
            fr::Thing enemy = world.spawn();

            world.emplace_now<fr::RelationsPart>(enemy);

            {
                fr::LocalTransformPart t;
                t.position = {static_cast<float>(i) * 4.0f - 4.0f, 0.0f, 5.0f};
                world.emplace_now<fr::LocalTransformPart>(enemy, t);
            }

            world.emplace_now<Health>(
                enemy, Health{.current = 40.0f + static_cast<float>(i) * 10.0f, .max = 80.0f});
            world.emplace_now<Velocity>(enemy,
                                        Velocity{.linear = {-1.0f, 0.0f, 0.0f}, .speed = 2.5f});

            world.attach_child_now(player, enemy);

            // ---- 2 projectiles parented to each enemy
            for (int j = 0; j < 2; ++j) {
                fr::Thing proj = world.spawn();
                world.emplace_now<fr::RelationsPart>(proj);

                fr::LocalTransformPart t;
                t.position = {0.0f, 0.5f, static_cast<float>(j) * 0.5f};
                t.scale = {0.2f, 0.2f, 0.2f};

                world.emplace_now<fr::LocalTransformPart>(proj, t);
                world.emplace_now<Velocity>(proj,
                                            Velocity{.linear = {0.0f, 0.0f, 1.0f}, .speed = 15.0f});
                world.attach_child_now(enemy, proj);
            }
        }

        // -------------------------------------------------------------- Window

        fr::Window window{};
        fr::WindowProperties props{};
        props.title = "World Inspector";
        props.width = 1100;
        props.height = 700;
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

        // --------------------------------------------------------------- ImGui

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

        // ---------------------------------------------------------------- Loop

        while (running) {
            if (!window.poll_events(input) || input.is_key_pressed(fr::Key::Escape)) {
                running = false;
                break;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowSize(ImVec2(600, 650), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
            ImGui::Begin("World Inspector");

            fr::ImGuiWriterArchive archive{};
            call_shape(archive, world);

            ImGui::End();

            glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            window.swap_buffers();
        }

        // ------------------------------------------------------------- Cleanup

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        window.close();
    }

    fr::shutdown_core_ctx();
    return 0;
}
