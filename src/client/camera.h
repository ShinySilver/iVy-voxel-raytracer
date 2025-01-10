#pragma once

#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "glm/vec3.hpp"
#include "glm/mat4x4.hpp"

namespace client::camera {
    extern glm::vec3 position, direction;
    extern glm::mat4 view_matrix;

    void init();
    void update(GLFWwindow *window);
}