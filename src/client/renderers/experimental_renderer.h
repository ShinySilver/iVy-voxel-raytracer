#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "client/renderers/renderer.h"
#include "client/utils/wide_tree.h"

namespace client::renderers {
    class ExperimentalRenderer final: public Renderer {
    public:
        ExperimentalRenderer();
        ~ExperimentalRenderer() override;
        void render() override;
        void resize(int resolution_x, int resolution_y) override;
    private:
        GLuint main_pass_shader = 0, depth_prepass_shader = 0, minpool_horizontal_shader = 0, minpool_vertical_shader = 0;
        GLuint memory_pool_SSBO = 0;
        GLuint framebuffer = 0;
        GLuint framebuffer_texture = 0, voxel_texture = 0, depth_texture = 0, intermediate_depth_texture = 0, lowres_depth_texture = 0;
        glm::mat4 projection_matrix = {};
        client::utils::WideTree view = {};
        int tree_step_limit = 0, dda_step_limit = 0;
        int framebuffer_resolution_x = 0, framebuffer_resolution_y = 0;
        glm::vec3 sun_direction = {0.3, 0.3, 1.0};
    };
}
