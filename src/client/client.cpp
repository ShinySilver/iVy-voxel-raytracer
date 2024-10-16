#include "client.h"
#include "camera.h"
#include "renderer.h"
#include "world_view.h"
#include "gui.h"
#include "context.h"
#include "../common/log.h"

static GLFWwindow *window;

void client::start() {
    /**
     * Ensuring the client has not been already initialized
     */
    if (window != NULL) fatal("Tried to init an already existing context!");

    /**
     * Initializing the client
     */
    window = context::init();
    world_view::init();
    renderer::init(window);
    gui::init(window);
    glfwShowWindow(window); // the window is initially hidden. We show it only after creating doing the lengthy initialisation.
    info("Client started")

    /**
     * Very basic keybindings:
     *  - Closing when pressing escape
     *  - Mouse capturing / releasing on left click / left alt
     */
    context::register_key_callback(GLFW_KEY_ESCAPE, [](int) { glfwSetWindowShouldClose(window, true); });
    context::register_key_callback(GLFW_KEY_F11, [](int action) { if (action == GLFW_PRESS) context::toggle_fullscreen(); });
    context::register_key_callback(GLFW_KEY_LEFT_ALT, [](int) { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); });
    context::register_mouse_callback(GLFW_MOUSE_BUTTON_LEFT, [](int) { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); });

    /**
     * Rendering loop!
     */
    while (!glfwWindowShouldClose(window)) {
        // Handling inputs
        glfwPollEvents();

        // Skipping frames when the window is hidden
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) continue;

        // Rendering! First the main renderer, then the UI
        camera::update(window);
        world_view::update();
        renderer::render();
        gui::render();
        glfwSwapBuffers(window);
    }

    gui::terminate();
    renderer::terminate();
    world_view::terminate();
    context::terminate();
    info("Client stopped")
}

void client::terminate() {
    glfwSetWindowShouldClose(window, 1);
}