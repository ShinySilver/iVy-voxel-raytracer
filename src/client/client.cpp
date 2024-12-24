#include "client.h"
#include "camera.h"
#include "renderer.h"
#include "gui/debug.h"
#include "context.h"
#include "ivy_log.h"
#include "gui/chat.h"
#include "gui/pause_menu.h"

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
    renderer::init();
    glfwShowWindow(window);
    info("Client started")

    /**
     * Very basic keybindings
     */
    context::register_key_callback(GLFW_KEY_ESCAPE, [](int action) {
        if (action == GLFW_PRESS) {
            if (!gui::chat::is_enabled) {
                gui::pause_menu::is_enabled = !gui::pause_menu::is_enabled;
                context::set_cursor_enabled(gui::pause_menu::is_enabled);
            } else {
                gui::chat::is_enabled = false;
                context::set_cursor_enabled(false);
            }
        }
    }, 1);
    context::register_key_callback(GLFW_KEY_ENTER, [](int action) {
        if (action == GLFW_PRESS && !gui::chat::is_enabled) {
            gui::chat::is_enabled = true;
            context::set_cursor_enabled(true);
        }
    }, 1);
    context::register_key_callback(GLFW_KEY_F11, [](int action) { if (action == GLFW_PRESS) context::set_fullscreen(!context::is_fullscreen()); });
    context::register_key_callback(GLFW_KEY_F3, [](int action) { if (action == GLFW_PRESS) gui::debug::is_enabled = !gui::debug::is_enabled; });
    context::register_mouse_callback(GLFW_MOUSE_BUTTON_LEFT, [](int) { if (!gui::chat::is_enabled && !gui::pause_menu::is_enabled) context::set_cursor_enabled(false); });

    /**
     * Rendering loop!
     */
    while (!glfwWindowShouldClose(window)) {
        // Handling inputs
        glfwPollEvents();

        // Skipping frames when the window is hidden
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) continue;

        camera::update(window); // Updating the camera location
        renderer::render(); // Then doing the primary render

        glfwSwapBuffers(window);
    }

    renderer::terminate();
    context::terminate();
    info("Client stopped")
}

void client::terminate() {
    glfwSetWindowShouldClose(window, true);
}