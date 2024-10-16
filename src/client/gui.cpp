#include "gui.h"
#include "camera.h"
#include "context.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "../common/log.h"
#include "../common/world.h"

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
    const int value_count = 90;
    const float refresh_rate = 20.0;
    static bool p_open = true;
    static float mean[value_count] = {}, vram[value_count] = {};
    static int values_offset = 0;
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
        mean[values_offset] = float(last_mean);
        vram[values_offset] = float(totalAvailableMemoryKb - currentAvailableMemoryKb);
        values_offset = (values_offset + 1) % value_count;
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
            //ImGui::Text("VSync: %s", "No");
        }
        if (ImGui::CollapsingHeader("Keybindings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("WASD: Moving");
            ImGui::Text("Space/LeftCtrl: Up/Down");
            ImGui::Text("LeftAlt: Unlock mouse cursor");
            ImGui::Text("F3: Toggle this interface");
            ImGui::Text("F11: Toggle fullscreen");
        }
        if (ImGui::CollapsingHeader("Advanced Charts")) {
            char overlay[256];
            sprintf(overlay, "%.3f ms/frame (%.1f FPS)", last_mean, 1000. / last_mean);
            ImGui::PlotLines("", mean, value_count, values_offset, overlay, 0.0f, 7.0f, ImVec2(200, 60.0f));
            sprintf(overlay, "%.2f MiB available", currentAvailableMemoryKb / 1024.);
            ImGui::PlotLines("", vram, value_count, values_offset, overlay, 0.0f, (float) totalAvailableMemoryKb, ImVec2(200, 60.0f));
        }
    }
    ImGui::End();
}