#include "ivy_log.h"
#include "client/client.h"
#include "client/camera.h"
#include "client/context.h"
#include "client/gui/debug.h"
#include "client/gui/chat.h"
#include "client/renderers/renderer.h"
#include "client/renderers/experimental_renderer.h"
#include "client/renderers/wide_tree_renderer.h"

namespace client {

    Renderer *active_renderer;
    FastMemoryPool memory_pool = FastMemoryPool();

    namespace {
        GLFWwindow *window;
        void resize_view(int resolution_x, int resolution_y){
            if(active_renderer) active_renderer->resize(resolution_x, resolution_y);
        }
    }

    void start() {
        /**
         * Ensuring the client has not been already initialized
         */
        if (window != nullptr) fatal("Tried to init an already existing context!");

        /**
         * Initializing the client
         */
        window = context::init();
        active_renderer = new renderers::ExperimentalRenderer();
        //active_renderer = new renderers::WideTreeRenderer();
        context::register_framebuffer_callback(resize_view);
        glfwShowWindow(window);
        info("Client started")

        /**
         * Forcing a camera pos for benchmarking
         */
        camera::position = {450, 450, 450};
        camera::direction = {0.5, -0.75, 0.5};

        /**
         * Very basic keybindings
         */
        context::register_key_callback(GLFW_KEY_F11, [](int action) { if (action == GLFW_PRESS) context::set_fullscreen(!context::is_fullscreen()); });
        context::register_key_callback(GLFW_KEY_F3, [](int action) { if (action == GLFW_PRESS) gui::debug::is_enabled = !gui::debug::is_enabled; });
        context::register_key_callback(GLFW_KEY_F2, [](int action) { if (action == GLFW_PRESS) context::set_vsync_enabled(!context::is_vsync_enabled()); });
        context::register_key_callback(GLFW_KEY_LEFT_ALT, [](int action) { if (action == GLFW_PRESS) context::set_cursor_enabled(true); });
        context::register_mouse_callback(GLFW_MOUSE_BUTTON_LEFT, [](int) { if (!gui::chat::is_enabled) context::set_cursor_enabled(false); });

        /**
         * Some more complicated keybindings for the chat & game exit
         */
        context::register_key_callback(GLFW_KEY_ESCAPE, [](int action) {
            if (action != GLFW_PRESS) return;
            if (gui::chat::is_enabled) {
                gui::chat::is_enabled = false;
                context::set_cursor_enabled(false);
            } else {
                client::terminate();
            }
        }, 1);
        context::register_key_callback(GLFW_KEY_ENTER, [](int action) {
            if (action == GLFW_PRESS && !gui::chat::is_enabled) {
                gui::chat::is_enabled = true;
                context::set_cursor_enabled(true);
            }
        }, 1);

        /**
         * Rendering loop!
         */
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents(); // Handling inputs
            if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) continue; // Skipping frames when the window is hidden
            camera::update(window); // Updating the camera location
            active_renderer->render(); // Then doing the primary render
            glfwSwapBuffers(window); // Swapping buffer as OpenGL is by design asynchronous :)
        }

        /**
         * Freeing resources before exiting
         */
        delete active_renderer;
        context::terminate();
        info("Client stopped")
    }

    void terminate() {
        glfwSetWindowShouldClose(window, true);
    }
}