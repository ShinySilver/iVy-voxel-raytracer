#include "debug.h"

#include "imgui.h"
#include "client/context.h"
#include "client/camera.h"
#include "client/gui/chat.h"
#include "client/client.h"
#include "server/server.h"
#include "ivy_log.h"

namespace client::gui::debug {

    bool is_enabled = false;

    namespace {
        bool is_initialized = false;
        void on_framebuffer_resize(int width, int height);

        void initialize() {
            is_initialized = true;
            client::context::register_framebuffer_callback(on_framebuffer_resize);
        }

        float averaged_frame_duration = 0.0f;
        void on_framebuffer_resize(int width, int height) {
            averaged_frame_duration = 0.0f;
        }
    }

    void render() {
        if (!is_enabled) return;
        if (!is_initialized) initialize();
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        const ImGuiIO &io = ImGui::GetIO();
        int x, y;
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 window_pos = {work_pos.x + 10.0f, work_pos.y + 10.0f};
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, {0, 0});
        ImGui::SetNextWindowBgAlpha(0.35f);
        if(averaged_frame_duration == 0.0f) averaged_frame_duration = 1000.0f / io.Framerate;
        else averaged_frame_duration = 0.99f * averaged_frame_duration + 0.01f * 1000.0f / io.Framerate;
        if (ImGui::Begin("Debug", &is_enabled, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("Position: (%.2f; %.2f; %.2f)", client::camera::position.x, client::camera::position.y, client::camera::position.z);
            ImGui::Text("Direction: (%.2f; %.2f; %.2f)", client::camera::direction.x, client::camera::direction.y, client::camera::direction.z);
            context::get_window_size(&x, &y);
            ImGui::Text("Window resolution: %dx%d (%s)", x, y, context::is_fullscreen() ? "Fullscreen" : "Windowed");
            context::get_framebuffer_size(&x, &y);
            ImGui::Text("Framebuffer resolution: %dx%d", x, y);
            ImGui::Text("Memory-pool allocation: %.2lf MiB", (double) memory_pool.allocated() / 1024.0 / 1024.0);
            ImGui::Text("Memory-pool usage: %.2lf MiB", (double) memory_pool.used() / 1024.0 / 1024.0);
            ImGui::Text("Worldgen: %s", server::world_generator->get_name());
            ImGui::Text("Chat: %s", chat::is_enabled ? "Opened" : "Closed");
            ImGui::Text("Framerate: %.0f FPS (~%.2f ms/frame)", io.Framerate, averaged_frame_duration);
        }
        ImGui::End();
    }
}