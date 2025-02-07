#pragma once

namespace console {

    struct CommandParameter {
        const char *label;
        const bool optional;
        const char **values;
        const int valueCount;
    };

    struct Command {
        const char **keywords;
        const int keywords_count;
        const CommandParameter **parameters;
        const int parameters_count;
        void(*callback)(const char **parameters);
    };

    void printf(const char *message);
    void parse(const char **command);
    void autocomplete(const char **command, const char **suggestions, int suggestions_count);

    void register_command(const Command &command);
    void register_printer(void(*callback)(const char *message));
}