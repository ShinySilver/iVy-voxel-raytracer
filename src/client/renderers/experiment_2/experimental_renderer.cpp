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
#include "client/renderers/experiment_2/experimental_renderer.h"
#include "client/shaders/experiment_2/primary_ray.glsl"
#include "client/shaders/experiment_2/secondary_ray.glsl"
#include "server/server.h"
#include "server/generators/generator.h"

namespace client::renderers {

    ExperimentalRenderer2::ExperimentalRenderer2() : Renderer("64-tree") {
        // Initializing the renderer shader, SSBO and framebuffer
        primary_ray_shader = client::util::build_program(primary_ray_glsl, GL_COMPUTE_SHADER);
        secondary_ray_shader = client::util::build_program(secondary_ray_glsl, GL_COMPUTE_SHADER);
        glCreateBuffers(1, &memory_pool_SSBO);
        glCreateFramebuffers(1, &framebuffer);
        context::get_framebuffer_size(&framebuffer_resolution_x, &framebuffer_resolution_y);
        resize(framebuffer_resolution_x, framebuffer_resolution_y);

        // Initializing the world view
        info("Initializing world view with a single %ldx%ldx%ld region", IVY_REGION_WIDTH, IVY_REGION_WIDTH, IVY_REGION_WIDTH);
        auto t0 = time_us();
        server::world_generator->generate_view(0, 0, 0, view);
        info("Generated world view in %.2f ms!", double (time_us()-t0)/1e3);
        glNamedBufferData(memory_pool_SSBO, (long) memory_pool->size(), memory_pool->to_pointer(0), GL_STATIC_COPY);
    }

    ExperimentalRenderer2::~ExperimentalRenderer2() {
        glDeleteProgram(primary_ray_shader);
        glDeleteProgram(secondary_ray_shader);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteBuffers(1, &memory_pool_SSBO);
    }

    void ExperimentalRenderer2::render() {
        // First doing the primary ray
        glUseProgram(primary_ray_shader);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, memory_pool_SSBO);
        bind_texture(intermediate_texture, 0, GL_RG8UI, GL_WRITE_ONLY);
        glUniform2ui(glGetUniformLocation(primary_ray_shader, "screen_size"), (uint32_t) framebuffer_resolution_x, (uint32_t) framebuffer_resolution_y);
        glUniform3f(glGetUniformLocation(primary_ray_shader, "camera_position"), client::camera::position.x, client::camera::position.y, client::camera::position.z);
        glUniformMatrix4fv(glGetUniformLocation(primary_ray_shader, "view_matrix"), 1, GL_FALSE, &camera::view_matrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(primary_ray_shader, "projection_matrix"), 1, GL_FALSE, &projection_matrix[0][0]);
        glUniform1ui(glGetUniformLocation(primary_ray_shader, "tree_depth"), IVY_REGION_TREE_DEPTH);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Then the secondary ray
        glUseProgram(primary_ray_shader);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, memory_pool_SSBO);
        bind_texture(intermediate_texture, 0, GL_RG8UI, GL_READ_ONLY);
        bind_texture(framebuffer_texture, 1, GL_RGBA8, GL_WRITE_ONLY);
        glUniform2ui(glGetUniformLocation(primary_ray_shader, "screen_size"), (uint32_t) framebuffer_resolution_x, (uint32_t) framebuffer_resolution_y);
        glUniform3f(glGetUniformLocation(primary_ray_shader, "sun_direction"), 0.8, 1, 0.5);
        glUniform1ui(glGetUniformLocation(primary_ray_shader, "tree_depth"), IVY_REGION_TREE_DEPTH);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // At last, rendering to screen
        glBlitNamedFramebuffer(framebuffer, 0,
                               0, 0, framebuffer_resolution_x, framebuffer_resolution_y,
                               0, 0, framebuffer_resolution_x, framebuffer_resolution_y,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // On top of that, drawing the UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        gui::debug::render();
        gui::chat::render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ExperimentalRenderer2::resize(int resolution_x, int resolution_y) {
        if (framebuffer_texture) destroy_texture(framebuffer_texture);
        if (intermediate_texture) destroy_texture(intermediate_texture);
        glViewport(0, 0, resolution_x, resolution_y);
        framebuffer_resolution_x = std::max(1, resolution_x);
        framebuffer_resolution_y = std::max(1, resolution_y);
        projection_matrix = glm::perspective(glm::radians(80.0f), (float) framebuffer_resolution_x / float(framebuffer_resolution_y), 0.1f, 100.0f);
        framebuffer_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_RGBA8, GL_NONE);
        intermediate_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_RG8UI, GL_NONE);
        glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, framebuffer_texture, 0);
    }
}