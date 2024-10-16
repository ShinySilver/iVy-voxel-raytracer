#include <chrono>
#include "camera.h"
#include "glm/gtc/matrix_transform.hpp"

namespace client::camera {
    glm::vec3 position{0.5, 0.5, 0.5}, direction{1.0f, 0, 0};
    glm::mat4 view_matrix{};
}

#define CAMERA_BASE_SPEED (50.f)
#define CAMERA_FAST_SPEED (250.f)
#define CAMERA_MOUSE_SENSITIVITY (3)

void client::camera::update(GLFWwindow *window) {
    static auto t0 = std::chrono::steady_clock::now();
    auto t1 = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(t1 - t0).count();
    t0 = t1;

    float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? CAMERA_FAST_SPEED : CAMERA_BASE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += direction * delta_time * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= direction * delta_time * speed;

    glm::vec3 right = normalize(cross(direction, glm::vec3{0, 1, 0}));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position += right * delta_time * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position -= right * delta_time * speed;

    glm::vec3 up = normalize(cross(direction, right));
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position += up * delta_time * speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) position -= up * delta_time * speed;

    static double oldPosX = 0.0, oldPosY = 0.0;
    double posX, posY;
    glfwGetCursorPos(window, &posX, &posY);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) || glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
        direction = normalize(direction - right * float((posX - oldPosX) * CAMERA_MOUSE_SENSITIVITY / 1000.0f));
        direction = normalize(direction - glm::vec3{0, 1, 0} * float(-(posY - oldPosY) * CAMERA_MOUSE_SENSITIVITY * 2 / 1000.0f));
    }
    oldPosX = posX, oldPosY = posY;

    view_matrix = glm::lookAt(camera::position, camera::position + camera::direction, up);
}
