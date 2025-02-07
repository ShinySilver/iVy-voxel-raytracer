#include "imgui.h"
#include "ivy_time.h"
#include "ivy_log.h"
#include "client/context.h"
#include "client/gui/chat.h"
#include "common/console.h"

namespace client::gui::chat {
    namespace {

        // A few const used in the gui
        const float padding = 10.0f, input_height = 30.0f;
        const int recent_duration_seconds = 10;

        // A fixed-sized buffer for the chat input field
        char input_buffer[256] = {'\0'};

        // A cyclic buffer for the chat history
        struct Message {
            char text[256] = {'\0'};
            uint64_t timestamp_us = 0u;
            char owner_name[32] = "Player";
        } chat_history[64] = {};
        int chat_history_current = -1, chat_history_length = 0;

        void render_all() {
            // Layout
            int window_width, window_height;
            context::get_window_size(&window_width, &window_height);

            // Not displaying anything if the chat is empty
            if (chat_history_length > 0) {

                // Dynamic chat size
                float history_max_height = float(window_height) / 3.0f;
                int history_max_line_count = int(history_max_height / ImGui::GetFontSize());
                float history_height = float(history_max_line_count > chat_history_length ? chat_history_length : history_max_line_count)
                                       * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.y) + 2 * ImGui::GetStyle().ItemSpacing.y;

                // Chat History (no scrolling for now)
                ImGui::SetNextWindowBgAlpha(0.35f);
                ImGui::SetNextWindowPos(ImVec2(padding, float(window_height) - 2 * input_height - 2 * padding - float(history_height)));
                ImGui::SetNextWindowSize(ImVec2(float(window_width) - 2 * padding, history_height));
                ImGui::Begin("ChatHistory", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
                                                     ImGuiWindowFlags_NoMove);
                for (int i = 0; i < chat_history_length; i++) {
                    Message &message = chat_history[(chat_history_current - chat_history_length + i + 1) % 64];
                    ImGui::Text("[%s] %s", message.owner_name, message.text);
                }
                ImGui::End();
            }

            // Input Box
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGui::SetNextWindowPos(ImVec2(padding, window_height - input_height - padding));
            ImGui::SetNextWindowSize(ImVec2(window_width - 2 * padding, input_height));
            ImGui::Begin("ChatInput", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
                                               ImGuiWindowFlags_NoMove);
            ImGui::SetKeyboardFocusHere(0);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            if (ImGui::InputText("##input", input_buffer, sizeof(input_buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (input_buffer[0] != '\0') {
                    if (input_buffer[0] == '/') {
                        // Split line into keywords, and send to the console helper for parsing
                        const char *tokens[32]; // Maximum 32 tokens
                        int token_count = 0;
                        char *token = strtok(input_buffer+1, " ");
                        while(token && token_count < 32) {
                            tokens[token_count++] = token;
                            token = strtok(nullptr, " ");
                        }
                        console::parse(tokens);
                    } else {
                        // Add message to chat history
                        chat_history_current += 1;
                        if (chat_history_length <= 64) chat_history_length += 1;
                        strncpy(chat_history[chat_history_current].text, input_buffer, 256);
                        chat_history[chat_history_current].timestamp_us = time_us();
                    }
                    input_buffer[0] = '\0'; // Clear input buffer
                }
                is_enabled = false;
                context::set_cursor_enabled(false);
            }
            ImGui::PopStyleColor(2);
            ImGui::End();
        }

        void render_recent_messages_only() {
            // Get the current timestamp
            uint64_t current_timestamp_us = time_us();

            // Update chat_recent_count by checking timestamps of messages
            int chat_recent_count = 0;
            for (int i = chat_history_length - 1; i >= 0; i--) {
                Message &message = chat_history[(chat_history_current - chat_history_length + i + 1) % 64];
                if ((current_timestamp_us - message.timestamp_us) / 1'000'000 < recent_duration_seconds) {
                    chat_recent_count++;
                } else {
                    break; // Stop checking once a non-recent message is found
                }
            }

            // Layout
            int window_width, window_height;
            context::get_window_size(&window_width, &window_height);

            // Not displaying anything if there are no recent messages to show
            if (chat_recent_count > 0 && chat_history_length > 0) {

                // Calculate dynamic size based on the number of recent messages
                float recent_height = float(chat_recent_count) * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.y) + 2 * ImGui::GetStyle().ItemSpacing.y;

                // Display recent messages
                ImGui::SetNextWindowBgAlpha(0.35f);
                ImGui::SetNextWindowPos(ImVec2(padding, float(window_height) - 2 * input_height - recent_height - 2 * padding));
                ImGui::SetNextWindowSize(ImVec2(float(window_width) - 2 * padding, recent_height));
                ImGui::Begin("RecentMessages", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
                                                        ImGuiWindowFlags_NoMove);
                for (int i = chat_history_length - chat_recent_count; i < chat_history_length; i++) {
                    Message &message = chat_history[(chat_history_current - chat_history_length + i + 1) % 64];
                    ImGui::Text("[%s] %s", message.owner_name, message.text);
                }
                ImGui::End();
            }
        }
    }

    bool is_enabled = false;

    void clear() {
        chat_history_current = -1, chat_history_length = 0;
    }

    void render() {
        if (is_enabled) render_all();
        else render_recent_messages_only();
    }
}