#include <algorithm>
#include "renderer.h"
#include "world_view.h"
#include "gl_util.h"
#include "camera.h"
#include "glm/gtc/matrix_transform.hpp"
#include "shaders/main_pass.glsl"
#include "../common/log.h"

// internal variables
static GLFWwindow *window = nullptr;
static GLuint main_pass_shader = 0;
static GLuint framebuffer = 0, framebuffer_texture = 0;
static glm::mat4 projection_matrix = {};

// keeping track of window size
static int framebuffer_resolution_x = 0, framebuffer_resolution_y = 0;
static void render_framebuffer_size_callback(GLFWwindow *_window, int width, int height);

// and making a few shader parameters dynamic
static int tree_step_limit = 0;
static int dda_step_limit = 0;
static glm::vec3 sun_direction = {};

void client::renderer::init(GLFWwindow *glfwWindow) {
    window = glfwWindow;
    char templated_shader_code[sizeof(main_pass_glsl)];
    snprintf(templated_shader_code, sizeof(main_pass_glsl), main_pass_glsl, 0);
    main_pass_shader = build_program(templated_shader_code, GL_COMPUTE_SHADER);
    glCreateFramebuffers(1, &framebuffer);
    glfwGetWindowSize(window, &framebuffer_resolution_x, &framebuffer_resolution_y);
    render_framebuffer_size_callback(window, framebuffer_resolution_x, framebuffer_resolution_y);
    glfwSetFramebufferSizeCallback(window, render_framebuffer_size_callback);
    sun_direction = glm::normalize(glm::vec3(0.3, 0.3, 1.0));
}

void client::renderer::render() {
    glUseProgram(main_pass_shader);
    world_view::bind(0);
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
}

void client::renderer::terminate() {
    glDeleteProgram(main_pass_shader);
    glDeleteFramebuffers(1, &framebuffer);
}

void client::renderer::set_render_type(RenderType type){
    glDeleteProgram(main_pass_shader);
    char templated_shader_code[sizeof(main_pass_glsl)];
    snprintf(templated_shader_code, sizeof(main_pass_glsl), main_pass_glsl, int(type));
    main_pass_shader = build_program(templated_shader_code, GL_COMPUTE_SHADER);
}

void client::renderer::set_tree_step_limit(int i) {
    tree_step_limit = i;
}

void client::renderer::set_dda_step_limit(int i) {
    dda_step_limit = i;
}

void client::renderer::set_sun_direction(glm::vec3 v) {
    sun_direction = v;
}

static void render_framebuffer_size_callback(GLFWwindow *_window, int width, int height) {
    if (framebuffer_texture) destroy_texture(framebuffer_texture);
    glViewport(0, 0, width, height);
    framebuffer_resolution_x = std::max(1, width);
    framebuffer_resolution_y = std::max(1, height);
    projection_matrix = glm::perspective(glm::radians(80.0f), (float) framebuffer_resolution_x / float(framebuffer_resolution_y), 0.1f, 100.0f);
    framebuffer_texture = client::create_texture(framebuffer_resolution_x, framebuffer_resolution_y, GL_RGBA8, GL_NONE);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, framebuffer_texture, 0);
}