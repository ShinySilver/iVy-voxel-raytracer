#pragma once

#include <functional>
#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "glm/vec2.hpp"

namespace client::context {
    GLFWwindow *init();
    void terminate();

    void register_key_callback(int key_index, void(*callback)(int action_index), int priority = 0);
    void register_mouse_callback(int button_index, void(*callback)(int action_index), int priority = 0);
    void register_framebuffer_callback(void(*callback)(int width, int height));

    void set_fullscreen(bool fullscreen);
    bool is_fullscreen();

    void set_vsync_enabled(bool vsync_enabled);
    bool is_vsync_enabled();

    void set_cursor_enabled(bool b);
    bool is_cursor_enabled();

    void get_window_size(int *width, int *height);
    void get_framebuffer_size(int *width, int *height);
}
