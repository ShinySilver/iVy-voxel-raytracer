#include "debug.h"

#include "imgui.h"
#include "client/context.h"
#include "client/camera.h"
#include "client/gui/chat.h"
#include "client/client.h"
#include "server/server.h"

bool client::gui::debug::is_enabled = false;

void client::gui::debug::render() {
    if (!is_enabled) return;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImGuiIO &io = ImGui::GetIO();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 window_pos = {work_pos.x + 10.0f, work_pos.y + 10.0f};
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, {0, 0});
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Debug", &is_enabled, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Position: (%.2f; %.2f; %.2f)", client::camera::position.x, client::camera::position.y, client::camera::position.z);
        ImGui::Text("Direction: (%.2f; %.2f; %.2f)", client::camera::direction.x, client::camera::direction.y, client::camera::direction.z);
        ImGui::Text("Window resolution: %dx%d (%s)", int(viewport->Size.x), int(viewport->Size.y), context::is_fullscreen() ? "Fullscreen" : "Windowed");
        ImGui::Text("Memory-pool allocation: %.2lf MiB", (double) memory_pool.allocated() / 1024.0 / 1024.0);
        ImGui::Text("Memory-pool usage: %.2lf MiB", (double) memory_pool.used() / 1024.0 / 1024.0);
        ImGui::Text("Worldgen: %s", server::world_generator->get_name());
        ImGui::Text("Chat: %s", chat::is_enabled ? "Opened" : "Closed");
        ImGui::Text("Framerate: %.1f FPS (%.2f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
    }
}
