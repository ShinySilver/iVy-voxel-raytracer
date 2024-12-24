#include "context.h"
#include "ivy_log.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#define CLIENT_WIN_WIDTH 900
#define CLIENT_WIN_HEIGHT 600

static GLFWwindow *window;

typedef struct {
    int key_index;
    void (*callback)(int action_index);
    int priority;
} KeyEventEntry;

static KeyEventEntry *key_listeners = nullptr, *mouse_listeners = nullptr;
static void (**framebuffer_listeners)(int, int) = nullptr;
static int key_listeners_count = 0, mouse_listeners_count = 0, framebuffer_listeners_count = 0;
static bool vsync_enabled = false, fullscreen = false;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void framebuffer_size_callback(GLFWwindow *window, int framebuffer_resolution_x, int framebuffer_resolution_y);

static void APIENTRY debug_callback(GLenum source, GLenum type, unsigned int id, GLenum severity,
                                    GLsizei length, const char *message, const void *userParam);

GLFWwindow *client::context::init() {
    /**
     * Ensuring the context has not been already initialized
     */
    if (window != nullptr) fatal("Tried to init an already existing context!");

    /**
     * Initializing GLFW and giving all the window hints
     */
    if (!glfwInit()) {
        fatal("Failed to initialize GLFW")
    } else {
        info("Initialized GLFW %d.%d", GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR)
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    /**
     * Creating the window
     */
    window = glfwCreateWindow(CLIENT_WIN_WIDTH, CLIENT_WIN_HEIGHT, "iVy", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        fatal("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(vsync_enabled ? 1 : 0);

    /**
     * Initializing GLAD
     */
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        fatal("Failed to initialize GLAD");
    } else {
        info("Initialized OpenGL %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    }

    /**
     * Initializing the OpenGL debug context
     */
    int flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        info("Enabled OpenGL debug output.");
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debug_callback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    } else {
        warn("Could not create OpenGL debug context, we are running blind!");
    }

    /**
     * Registering key callbacks
     */
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    /**
     * Renaming the window to include graphics card model, which is too verbose to include in our debug menu
     */
    char win_title[128];
    snprintf(win_title, 128, "iVy - %s", glGetString(GL_RENDERER));
    glfwSetWindowTitle(window, win_title);

    /**
     * Enabling ImGui
     */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
    info("Initialized ImGui %s", IMGUI_VERSION);

    return window;
}

void client::context::terminate() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
}

void client::context::register_key_callback(int key_index, void(*callback)(int action_index), int priority) {
    key_listeners = (KeyEventEntry *) realloc(key_listeners, (++key_listeners_count) * sizeof(KeyEventEntry));
    key_listeners[key_listeners_count - 1] = KeyEventEntry{key_index, callback, priority};
}

void client::context::register_mouse_callback(int button_index, void(*callback)(int action_index), int priority) {
    mouse_listeners = (KeyEventEntry *) realloc(mouse_listeners, (++mouse_listeners_count) * sizeof(KeyEventEntry));
    mouse_listeners[mouse_listeners_count - 1] = KeyEventEntry{button_index, callback, priority};
}
void client::context::register_framebuffer_callback(void (*callback)(int, int)) {
    framebuffer_listeners = (void (**)(int, int)) realloc(framebuffer_listeners, (++framebuffer_listeners_count) * sizeof(void (**)(int, int)));
    framebuffer_listeners[framebuffer_listeners_count - 1] = callback;
}

void client::context::set_vsync_enabled(bool _vsync_enabled) {
    vsync_enabled = _vsync_enabled;
    glfwSwapInterval(_vsync_enabled ? 1 : 0);
}

bool client::context::is_vsync_enabled() {
    return vsync_enabled;
}

void client::context::set_fullscreen(bool _fullscreen) {
    static int prev_win_width, prev_win_height, win_x, win_y;
    if (fullscreen != _fullscreen) {
        fullscreen = _fullscreen;
        if (fullscreen) {
            GLFWmonitor *monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            glfwGetWindowSize(window, &prev_win_width, &prev_win_height);
            glfwGetWindowPos(window, &win_x, &win_y);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                                 mode->refreshRate);
        } else {
            glfwSetWindowMonitor(window, nullptr, 0, 0, prev_win_width,
                                 prev_win_height,
                                 GLFW_DONT_CARE);
            glfwSetWindowPos(window, win_x, win_y);
        }
    }
    glfwSwapInterval(vsync_enabled ? 1 : 0);
}

bool client::context::is_fullscreen() {
    return fullscreen;
}


void client::context::set_cursor_enabled(bool is_enabled) {
    glfwSetInputMode(window, GLFW_CURSOR, is_enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

bool client::context::is_cursor_enabled() {
    return glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL;
}

void client::context::get_window_size(int *width, int *height) {
    glfwGetWindowSize(window, width, height);
}

void client::context::close_window() {
    glfwSetWindowShouldClose(window, true);
}

static void key_callback(GLFWwindow *_window, int key, int scancode, int action, int mods) {
    ImGuiIO &io = ImGui::GetIO();
    for (int i = 0; i < key_listeners_count; i++) {
        if (key == key_listeners[i].key_index && (!io.WantCaptureKeyboard || key_listeners[i].priority > 0)) key_listeners[i].callback(action);
    }
}

static void mouse_button_callback(GLFWwindow *_window, int button, int action, int mods) {
    ImGuiIO &io = ImGui::GetIO();
    for (int i = 0; i < mouse_listeners_count; i++) {
        if (button == mouse_listeners[i].key_index && (!io.WantCaptureMouse || mouse_listeners[i].priority > 0)) mouse_listeners[i].callback(action);
    }
}

static void framebuffer_size_callback(GLFWwindow *_window, int framebuffer_resolution_x, int framebuffer_resolution_y) {
    for (int i = 0; i < framebuffer_listeners_count; i++) {
        framebuffer_listeners[i](framebuffer_resolution_x, framebuffer_resolution_y);
    }
}

static void APIENTRY debug_callback(GLenum source, GLenum type, unsigned int id, GLenum severity,
                                    GLsizei length, const char *message, const void *userParam) {
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;
    char header_str[256];
    snprintf(header_str, 256, "OpenGL debug message (%u): %s", id, message);
    warn("%s", header_str);
}