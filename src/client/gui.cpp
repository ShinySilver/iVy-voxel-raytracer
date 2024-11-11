#include "gui.h"
#include "camera.h"
#include "context.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "../common/log.h"
#include "../common/world.h"
#include "renderer.h"

static void showOverlay();
static bool is_imgui_enabled = true;
static GLFWwindow *window;

void client::gui::init(GLFWwindow *glfwWindow) {
    window = glfwWindow;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
    context::register_key_callback(GLFW_KEY_F3, [](int action) { if (action == GLFW_PRESS) is_imgui_enabled = !is_imgui_enabled; });
    info("Initialized ImGui %s", IMGUI_VERSION);
}

void client::gui::render() {
    if (!is_imgui_enabled) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    showOverlay();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void client::gui::terminate() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// TODO: show/hide FPS, show/hide VRAM, show/hide RAM, show/hide controls | realtime position rotation | controls | charts

static void showOverlay() {
    const float refresh_rate = 10.0;
    static bool p_open = true;
    static double refresh_time = ImGui::GetTime() + 1.0f / refresh_rate;
    static double frame_total = 0, frame_count = 0, last_mean = 0;

    // Update the window
    ImGuiIO &io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    const float PAD = 10.0f;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 window_pos = {work_pos.x + PAD, work_pos.y + PAD};
    ImVec2 window_pos_pivot = {0, 0};
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    window_flags |= ImGuiWindowFlags_NoMove;
    ImGui::SetNextWindowBgAlpha(0.35f);

    // Getting the actual framerate
    double frame_time = 1000.0f / io.Framerate;

    // Get video memory
    GLint currentAvailableMemoryKb = 0, totalAvailableMemoryKb = 0;
    glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &currentAvailableMemoryKb);
    glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &totalAvailableMemoryKb);
    glGetError(); // Just in case the above extension was not supported by the GPU

    // Create plot data at fixed 20 Hz rate
    if (refresh_time < ImGui::GetTime()) {
        last_mean = frame_total / frame_count;
        refresh_time += 1.0f / refresh_rate;
        frame_total = 0, frame_count = 0;
    } else {
        frame_total += frame_time;
        frame_count += 1;
    }

    // Draw the plots
    if (ImGui::Begin("Simple overlay", &p_open, window_flags)) {
        if (ImGui::CollapsingHeader("Debug stats", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position: (%.2f; %.2f; %.2f)", client::camera::position.x, client::camera::position.y, client::camera::position.z);
            ImGui::Text("Direction: (%.2f; %.2f; %.2f)", client::camera::direction.x, client::camera::direction.y, client::camera::direction.z);
            ImGui::Text("Fullscreen: %s", glfwGetWindowMonitor(window) != NULL ? "Yes" : "No");
            ImGui::Text("Memory-pool allocation: %.2lf MiB", (double) memory_pool.allocated() / 1024.0 / 1024.0);
            ImGui::Text("Memory-pool usage: %.2lf MiB", (double) memory_pool.used() / 1024.0 / 1024.0);
            ImGui::Text("Worldgen: %s", world_generator->get_name());
            ImGui::Text("Framerate: %.1f FPS (%.2f ms/frame)", 1000 / last_mean, last_mean);
        }

        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            static const char *items[]{"Unlimited", "8", "16", "32", "64", "128"};
            static int dda_selected = 0, tree_selected = 0;
            float width = ImGui::CalcTextSize(items[0]).x + 2 * ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetNextItemWidth(width);
            if (ImGui::Combo("Max DDA Steps (?)", &dda_selected, items, IM_ARRAYSIZE(items))) {
                client::renderer::set_dda_step_limit(dda_selected > 0 ? 0x4 << dda_selected : 0);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Limit the number of DDA steps.\nWhen reached, pixels are rendered red.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::SetNextItemWidth(width);
            if (ImGui::Combo("Max Tree Steps (?)", &tree_selected, items, IM_ARRAYSIZE(items))) {
                client::renderer::set_tree_step_limit(tree_selected > 0 ? 0x4 << tree_selected : 0);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Limit the number of 64-Tree node ascents/descents.\nWhen reached, pixels are rendered either\ngreen (for ascents) or blue (for descents).");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            //ImGui::Text("VSync: %s", "No");
        }
        if (ImGui::CollapsingHeader("Keybindings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("WASD: Moving");
            ImGui::Text("Space/LeftCtrl: Up/Down");
            ImGui::Text("LeftAlt: Unlock mouse cursor");
            ImGui::Text("F3: Toggle this interface");
            ImGui::Text("F11: Toggle fullscreen");
        }
    }
    ImGui::End();
}