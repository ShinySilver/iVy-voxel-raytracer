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
#include "client/shaders/experimental_1/primary_ray.glsl"
#include "client/shaders/experimental_1/minpool_horizontal.glsl"
#include "client/shaders/experimental_1/minpool_vertical.glsl"
#include "client/shaders/experimental_1/depth_prepass.glsl"
#include "client/shaders/experimental_1/postprocess_normals.glsl"
#include "server/server.h"
#include "server/generators/generator.h"
#include "glm/gtc/type_ptr.hpp"

namespace client::renderers {

    ExperimentalRenderer::ExperimentalRenderer() : Renderer("64-tree") {
        // Initializing the renderer shader, SSBO and framebuffer
        main_pass_shader = client::util::build_program(primary_ray_glsl, GL_COMPUTE_SHADER);
        depth_prepass_shader = client::util::build_program(depth_prepass_glsl, GL_COMPUTE_SHADER);
        minpool_horizontal_shader = client::util::build_program(minpool_horizontal_glsl, GL_COMPUTE_SHADER);
        minpool_vertical_shader = client::util::build_program(minpool_vertical_glsl, GL_COMPUTE_SHADER);
        postprocess_normals_shader = client::util::build_program(postprocess_normals_glsl, GL_COMPUTE_SHADER);
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
        if (framebuffer_texture) destroy_texture(framebuffer_texture);
        if (depth_texture) destroy_texture(depth_texture);
        if (intermediate_depth_texture) destroy_texture(intermediate_depth_texture);
        if (lowres_depth_texture) destroy_texture(lowres_depth_texture);
        if (voxel_and_normal_texture) destroy_texture(voxel_and_normal_texture);
        glDeleteProgram(main_pass_shader);
        glDeleteProgram(depth_prepass_shader);
        glDeleteProgram(minpool_horizontal_shader);
        glDeleteProgram(minpool_vertical_shader);
        glDeleteProgram(postprocess_normals_shader);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteBuffers(1, &memory_pool_SSBO);
    }

    void ExperimentalRenderer::render() {
        // Low-res depth prepass
        glUseProgram(depth_prepass_shader);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, memory_pool_SSBO);
        bind_texture(lowres_depth_texture, 0, GL_R16F, GL_WRITE_ONLY);
        glUniform2ui(glGetUniformLocation(depth_prepass_shader, "screen_size"), uint32_t(ceil(framebuffer_resolution_x / 2.)), uint32_t(ceil(framebuffer_resolution_y / 2.)));
        glUniform3f(glGetUniformLocation(depth_prepass_shader, "camera_position"), client::camera::position.x, client::camera::position.y, client::camera::position.z);
        glUniformMatrix4fv(glGetUniformLocation(depth_prepass_shader, "view_matrix"), 1, GL_FALSE, &camera::view_matrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(depth_prepass_shader, "projection_matrix"), 1, GL_FALSE, &projection_matrix[0][0]);
        glUniform1ui(glGetUniformLocation(depth_prepass_shader, "tree_depth"), IVY_REGION_TREE_DEPTH);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f / 2.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f / 2.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Min pooling, in order to create a full-res depth texture
        glUseProgram(minpool_horizontal_shader);
        bind_texture(lowres_depth_texture, 0, GL_R16F, GL_READ_ONLY);
        bind_texture(intermediate_depth_texture, 1, GL_R16F, GL_WRITE_ONLY);
        glUniform2ui(glGetUniformLocation(minpool_horizontal_shader, "screen_size"), (uint32_t) framebuffer_resolution_x, (uint32_t) framebuffer_resolution_y);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glUseProgram(minpool_vertical_shader);
        bind_texture(intermediate_depth_texture, 1, GL_R16F, GL_READ_ONLY);
        bind_texture(depth_texture, 0, GL_R16F, GL_WRITE_ONLY);
        glUniform2ui(glGetUniformLocation(minpool_vertical_shader, "screen_size"), (uint32_t) framebuffer_resolution_x, (uint32_t) framebuffer_resolution_y);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Primary ray. Takes a lot of parameters & full res depth and outputs voxel ids in one texture, normal and depth in another
        glUseProgram(main_pass_shader);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, memory_pool_SSBO);
        bind_texture(depth_texture, 0, GL_R16F, GL_READ_ONLY);
        bind_texture(voxel_and_normal_texture, 1, GL_RGBA8, GL_WRITE_ONLY);
        bind_texture(intermediate_depth_texture, 2, GL_R16F, GL_WRITE_ONLY);
        glUniform2ui(glGetUniformLocation(main_pass_shader, "screen_size"), (uint32_t) framebuffer_resolution_x, (uint32_t) framebuffer_resolution_y);
        glUniform3f(glGetUniformLocation(main_pass_shader, "camera_position"), client::camera::position.x, client::camera::position.y, client::camera::position.z);
        glUniformMatrix4fv(glGetUniformLocation(main_pass_shader, "view_matrix"), 1, GL_FALSE, &camera::view_matrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(main_pass_shader, "projection_matrix"), 1, GL_FALSE, &projection_matrix[0][0]);
        glUniform1ui(glGetUniformLocation(main_pass_shader, "tree_depth"), IVY_REGION_TREE_DEPTH);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Postprocess normals. Takes normal and depth, and modify them in order to "smooth" faraway voxel normals and reduce Moiré patterns
        glUseProgram(postprocess_normals_shader);
        bind_texture(intermediate_depth_texture, 0, GL_R16F, GL_READ_ONLY);
        bind_texture(voxel_and_normal_texture, 1, GL_RGBA8, GL_READ_ONLY);
        bind_texture(framebuffer_texture, 2, GL_RGBA8, GL_WRITE_ONLY);
        glUniform3f(glGetUniformLocation(postprocess_normals_shader, "camera_position"), client::camera::position.x, client::camera::position.y, client::camera::position.z);
        glUniformMatrix4fv(glGetUniformLocation(postprocess_normals_shader, "view_matrix"), 1, GL_FALSE, &camera::view_matrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(postprocess_normals_shader, "projection_matrix"), 1, GL_FALSE, &projection_matrix[0][0]);
        glDispatchCompute(GLuint(ceilf(float(framebuffer_resolution_x) / 8.0f)), GLuint(ceilf(float(framebuffer_resolution_y) / 8.0f)), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Secondary ray. Takes both the voxel ids texture and normal&depth texture, and write to the color texture
        // glUseProgram(normalpool_shader);
        // glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // At last, draw on the screen
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
        if (depth_texture) destroy_texture(depth_texture);
        if (intermediate_depth_texture) destroy_texture(intermediate_depth_texture);
        if (lowres_depth_texture) destroy_texture(lowres_depth_texture);
        if (voxel_and_normal_texture) destroy_texture(voxel_and_normal_texture);
        glViewport(0, 0, resolution_x, resolution_y);
        framebuffer_resolution_x = std::max(1, resolution_x);
        framebuffer_resolution_y = std::max(1, resolution_y);
        projection_matrix = glm::perspective(glm::radians(80.0f), (float) framebuffer_resolution_x / float(framebuffer_resolution_y), 0.1f, 100.0f);
        framebuffer_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_RGBA8, GL_NONE);
        voxel_and_normal_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_RGBA8, GL_NONE);
        depth_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_R16F, GL_NONE);
        intermediate_depth_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_R16F, GL_NONE);
        lowres_depth_texture = client::util::create_texture(uint32_t(ceil(framebuffer_resolution_x / 2.)), uint32_t(ceil(framebuffer_resolution_y / 2.)), GL_R16F, GL_NONE);
        glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, framebuffer_texture, 0);
    }
}