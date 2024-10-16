#pragma once

#include "glad/gl.h"
#include "GLFW/glfw3.h"

namespace client::gui {
    void init(GLFWwindow *window);
    void render();
    void terminate();
}
