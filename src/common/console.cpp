#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include "ivy_log.h"
#include "common/console.h"

namespace console {

    // Internal storage for registered commands and printer callbacks
    namespace {
        std::vector<Command> registeredCommands;
        std::vector<void (*)(const char *message)> registeredPrinters;

        void callPrinters(const char *message) {
            for (auto &printer: registeredPrinters) {
                printer(message);
            }
        }
    }

    void printf(const char *message) {
        callPrinters(message);
    }

    void parse(const char **command) {
        if (!command || !*command) {
            error("Empty command.");
            return;
        }

        for (const auto &cmd: registeredCommands) {
            bool match = true;
            for (int i = 0; i < cmd.keywords_count; ++i) {
                if (!command[i] || strcmp(cmd.keywords[i], command[i]) != 0) {
                    match = false;
                    break;
                }
            }

            if (match) {
                if (cmd.callback) {
                    cmd.callback(command + cmd.keywords_count);
                } else {
                    error("Command matched but no callback defined.");
                }
                return;
            }
        }

        error("Command not recognized.");
    }

    void autocomplete(const char **command, const char **suggestions, const int suggestions_count) {
        if (!command || !*command || !suggestions || suggestions_count <= 0) {
            return;
        }

        int suggestionIndex = 0;
        for (const auto &cmd: registeredCommands) {
            bool match = true;
            for (int i = 0; i < cmd.keywords_count; ++i) {
                if (!command[i] || strncmp(cmd.keywords[i], command[i], strlen(command[i])) != 0) {
                    match = false;
                    break;
                }
            }

            if (match && suggestionIndex < suggestions_count) {
                suggestions[suggestionIndex++] = cmd.keywords[0]; // Suggest the first keyword as the suggestion
            }

            if (suggestionIndex >= suggestions_count) {
                break;
            }
        }

        // Null-terminate the remaining suggestions
        for (int i = suggestionIndex; i < suggestions_count; ++i) {
            suggestions[i] = nullptr;
        }
    }

    void register_command(const Command &command) {
        registeredCommands.push_back(command);
    }

    void register_printer(void(*callback)(const char *message)) {
        registeredPrinters.push_back(callback);
    }
}
