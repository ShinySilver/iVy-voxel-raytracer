#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "ivy_gl.h"
#include "ivy_log.h"
#include "ivy_time.h"
#include "client/client.h"
#include "client/context.h"
#include "client/camera.h"
#include "client/gui/debug.h"
#include "client/gui/chat.h"
#include "client/renderers/experimental_renderer.h"
#include "client/shaders/main_pass.glsl"
#include "server/server.h"
#include "server/generators/generator.h"

namespace client::renderers {

    ExperimentalRenderer::ExperimentalRenderer() : Renderer("64-tree") {
        // Initializing the renderer shader, SSBO and framebuffer
        char templated_shader_code[sizeof(main_pass_glsl)];
        snprintf(templated_shader_code, sizeof(main_pass_glsl), main_pass_glsl, 0);
        main_pass_shader = client::util::build_program(templated_shader_code, GL_COMPUTE_SHADER);
        glCreateBuffers(1, &memory_pool_SSBO);
        glCreateFramebuffers(1, &framebuffer);
        context::get_framebuffer_size(&framebuffer_resolution_x, &framebuffer_resolution_y);
        resize(framebuffer_resolution_x, framebuffer_resolution_y);

        // Initializing the world view
        info("Initializing world view with a single %ldx%ldx%ld region", IVY_REGION_WIDTH, IVY_REGION_WIDTH, IVY_REGION_WIDTH);
        auto t0 = time_us();
        server::world_generator->generate_view(0, 0, 0, view);
        info("Generated world view in %.2f ms!", double (time_us()-t0)/1e3);
        glNamedBufferData(memory_pool_SSBO, (long) memory_pool.size(), memory_pool.to_pointer(0), GL_STATIC_COPY);
    }

    ExperimentalRenderer::~ExperimentalRenderer() {
        glDeleteProgram(main_pass_shader);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteBuffers(1, &memory_pool_SSBO);
    }

    void ExperimentalRenderer::render() {
        // Then, doing the rendering of the world view
        glUseProgram(main_pass_shader);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, memory_pool_SSBO);
        bind_texture(framebuffer_texture, 0, GL_RGBA8, GL_WRITE_ONLY);
        glUniform2ui(glGetUniformLocation(main_pass_shader, "screen_size"), (uint32_t) framebuffer_resolution_x, (uint32_t) framebuffer_resolution_y);
        glUniform3f(glGetUniformLocation(main_pass_shader, "camera_position"), client::camera::position.x, client::camera::position.y, client::camera::position.z);
        glUniformMatrix4fv(glGetUniformLocation(main_pass_shader, "view_matrix"), 1, GL_FALSE, &camera::view_matrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(main_pass_shader, "projection_matrix"), 1, GL_FALSE, &projection_matrix[0][0]);
        glUniform1ui(glGetUniformLocation(main_pass_shader, "tree_depth"), IVY_REGION_TREE_DEPTH);
        glUniform1i(glGetUniformLocation(main_pass_shader, "tree_step_limit"), tree_step_limit);
        glUniform1i(glGetUniformLocation(main_pass_shader, "dda_step_limit"), dda_step_limit);
        glUniform3f(glGetUniformLocation(main_pass_shader, "sun_direction"), sun_direction.x, sun_direction.y, sun_direction.z);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glBlitNamedFramebuffer(framebuffer, 0,
                               0, 0, framebuffer_resolution_x, framebuffer_resolution_y,
                               0, 0, framebuffer_resolution_x, framebuffer_resolution_y,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        gui::debug::render();
        gui::chat::render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ExperimentalRenderer::resize(int resolution_x, int resolution_y) {
        if (framebuffer_texture) destroy_texture(framebuffer_texture);
        glViewport(0, 0, resolution_x, resolution_y);
        framebuffer_resolution_x = std::max(1, resolution_x);
        framebuffer_resolution_y = std::max(1, resolution_y);
        projection_matrix = glm::perspective(glm::radians(80.0f), (float) framebuffer_resolution_x / float(framebuffer_resolution_y), 0.1f, 100.0f);
        framebuffer_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_RGBA8, GL_NONE);
        glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, framebuffer_texture, 0);
    }
}