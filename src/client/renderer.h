#pragma once

#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "glm/vec3.hpp"

namespace client::renderer {
    void init(GLFWwindow *window);
    void render();
    void terminate();
    void set_tree_step_limit(int i);
    void set_dda_step_limit(int i);
    void set_sun_direction(glm::vec3 v);
};
