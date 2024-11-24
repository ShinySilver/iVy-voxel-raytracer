#pragma once

#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "glm/vec3.hpp"

enum RenderType : int {
    DEFAULT,
    WAVE_TIME,
    PIXEL_DDA_STEPS,
    PIXEL_TREE_STEPS
};

namespace client::renderer {
    void init(GLFWwindow *window);
    void render();
    void terminate();
    void set_render_type(RenderType type = DEFAULT);
    void set_tree_step_limit(int i);
    void set_dda_step_limit(int i);
    void set_sun_direction(glm::vec3 v);
};
