#pragma once

#include <functional>
#include "glad/gl.h"
#include "GLFW/glfw3.h"

namespace client::context {
    GLFWwindow *init();
    void register_key_callback(int key_index, void(*callback)(int action_index));
    void register_mouse_callback(int button_index, void(*callback)(int action_index));
    void toggle_fullscreen();
    void set_vsync_enabled(bool vsync_enabled);
    void terminate();
}
