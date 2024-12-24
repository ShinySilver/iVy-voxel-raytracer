#include "pause_menu.h"
#include "imgui.h"
#include "../context.h"

namespace client::gui::pause_menu {
    bool is_enabled = false;

    void render() {
        if (!is_enabled) return;

        // Padding and spacing
        const ImGuiIO &io = ImGui::GetIO();
        const float text_padding = 8.0f;
        const float button_padding = 20.0f;  // Padding around button text (inside each button)
        const float row_spacing = 10.0f;     // Spacing between rows
        const float window_padding = 10.0f; // Padding around the entire window

        int screen_width, screen_height;
        context::get_window_size(&screen_width, &screen_height);

        // Step 1: Measure the size of the largest button text
        ImVec2 back_to_game_size = ImGui::CalcTextSize("Back to Game");
        ImVec2 options_size = ImGui::CalcTextSize("Options");
        ImVec2 keybindings_size = ImGui::CalcTextSize("Keybindings");
        ImVec2 save_and_quit_size = ImGui::CalcTextSize("Save and Quit");

        // Calculate the maximum button width (including padding)
        float max_button_width = std::max({back_to_game_size.x, options_size.x, keybindings_size.x, save_and_quit_size.x}) + 2 * button_padding;

        // Step 2: Calculate the button height (text height + padding)
        float button_height = back_to_game_size.y + button_padding;

        // Step 3: Calculate the window dimensions
        // The height must fit all buttons, spacings, and padding.
        float window_width = max_button_width + 2 * window_padding;
        float window_height = (4 * button_height) + (3 * row_spacing) + (2 * window_padding);

        // Step 4: Center the window on the screen
        float window_x = (screen_width - window_width) / 2.0f;
        float window_y = (screen_height - window_height) / 2.0f;

        // Step 5: Create the window
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::SetNextWindowPos(ImVec2(window_x, window_y));
        ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
        ImGui::Begin("PauseMenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        // Step 6: Render buttons, ensuring they are centered horizontally
        float button_x = (window_width - max_button_width) / 2.0f;

        // "Back to Game"
        ImGui::SetCursorPos(ImVec2(button_x, window_padding)); // Center button in the window
        if (ImGui::Button("Back to Game", ImVec2(max_button_width, button_height))) {
            is_enabled = false; // Close the pause menu
            context::set_cursor_enabled(false);
        }

        ImGui::Dummy(ImVec2(0.0f, row_spacing-text_padding)); // Spacing between rows

        // "Options"
        ImGui::SetCursorPos(ImVec2(button_x, ImGui::GetCursorPosY()));
        if (ImGui::Button("Options", ImVec2(max_button_width, button_height))) {
            // Handle Options
        }

        ImGui::Dummy(ImVec2(0.0f, row_spacing-text_padding)); // Spacing between rows

        // "Keybindings"
        ImGui::SetCursorPos(ImVec2(button_x, ImGui::GetCursorPosY()));
        if (ImGui::Button("Keybindings", ImVec2(max_button_width, button_height))) {
            // Handle Keybindings
        }

        ImGui::Dummy(ImVec2(0.0f, row_spacing-text_padding)); // Spacing between rows

        // "Save and Quit"
        ImGui::SetCursorPos(ImVec2(button_x, ImGui::GetCursorPosY()));
        if (ImGui::Button("Save and Quit", ImVec2(max_button_width, button_height))) {
            context::close_window();
        }

        ImGui::End();
    }
}
