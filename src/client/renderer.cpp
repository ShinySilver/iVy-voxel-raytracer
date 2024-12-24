#include "renderer.h"
#include "ivy_gl.h"
#include "ivy_log.h"
#include "ivy_time.h"
#include "camera.h"
#include "context.h"
#include "shaders/main_pass.glsl"
#include "imgui_impl_glfw.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "gui/debug.h"
#include "gui/chat.h"
#include "gui/pause_menu.h"
#include "../common/world.h"
#include "glm/gtc/matrix_transform.hpp"

namespace client::renderer {
    namespace {
        // internal variables
        GLuint main_pass_shader = 0;
        GLuint memory_pool_SSBO = 0;
        GLuint framebuffer = 0, framebuffer_texture = 0;
        glm::mat4 projection_matrix = {};

        // the world view
        struct {
            Region *region = nullptr;
            bool is_dirty = false;
        } WorldView;
        void worldview_update() {
            if (WorldView.is_dirty) {
                WorldView.is_dirty = false;
                glNamedBufferData(memory_pool_SSBO, (long) memory_pool.size(), memory_pool.to_pointer(0), GL_STATIC_COPY);
            }
        }

        // making a few shader parameters dynamic
        int tree_step_limit = 0;
        int dda_step_limit = 0;
        glm::vec3 sun_direction = glm::normalize(glm::vec3(0.3, 0.3, 1.0));

        // keeping track of window size
        int framebuffer_resolution_x = 0, framebuffer_resolution_y = 0;
        void framebuffer_size_callback(int resolution_x, int resolution_y) {
            if (framebuffer_texture) destroy_texture(framebuffer_texture);
            glViewport(0, 0, resolution_x, resolution_y);
            framebuffer_resolution_x = std::max(1, resolution_x);
            framebuffer_resolution_y = std::max(1, resolution_y);
            projection_matrix = glm::perspective(glm::radians(80.0f), (float) framebuffer_resolution_x / float(framebuffer_resolution_y), 0.1f, 100.0f);
            framebuffer_texture = client::util::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_RGBA8, GL_NONE);
            glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, framebuffer_texture, 0);
        }
    }

    void init() {
        // Initializing the renderer shader, SSBO and framebuffer
        char templated_shader_code[sizeof(main_pass_glsl)];
        snprintf(templated_shader_code, sizeof(main_pass_glsl), main_pass_glsl, 0);
        main_pass_shader = client::util::build_program(templated_shader_code, GL_COMPUTE_SHADER);
        glCreateBuffers(1, &memory_pool_SSBO);
        glCreateFramebuffers(1, &framebuffer);
        context::get_window_size(&framebuffer_resolution_x, &framebuffer_resolution_y);
        framebuffer_size_callback(framebuffer_resolution_x, framebuffer_resolution_y);
        context::register_framebuffer_callback(framebuffer_size_callback);

        // Initializing the world view
        info("Initializing world view with a single %ldx%ldx%ld region", IVY_REGION_WIDTH, IVY_REGION_WIDTH, IVY_REGION_WIDTH);
        auto t0 = time_us();
        WorldView.region = world_generator->generate_region(0, 0, 0);
        WorldView.is_dirty = true;
        info("Generated world view in %.2f ms!", double (time_us()-t0)/1e3);
        worldview_update();
    }

    void render() {
        // First, updating the world view
        worldview_update();

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
        gui::pause_menu::render();
        gui::debug::render();
        gui::chat::render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void terminate() {
        glDeleteProgram(main_pass_shader);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteBuffers(1, &memory_pool_SSBO);
    }

    void set_render_type(RenderType type) {
        glDeleteProgram(main_pass_shader);
        char templated_shader_code[sizeof(main_pass_glsl)];
        snprintf(templated_shader_code, sizeof(main_pass_glsl), main_pass_glsl, int(type));
        main_pass_shader = client::util::build_program(templated_shader_code, GL_COMPUTE_SHADER);
    }

    void set_tree_step_limit(int i) {
        tree_step_limit = i;
    }

    void set_dda_step_limit(int i) {
        dda_step_limit = i;
    }

    void set_sun_direction(glm::vec3 v) {
        sun_direction = v;
    }
}