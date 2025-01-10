#pragma once

#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "glm/vec3.hpp"

namespace client {
    class Renderer {
    public:
        explicit Renderer(const char *name) : name(name) {};
        virtual ~Renderer() = default;
        virtual void render() = 0;
        virtual void resize(int resolution_x, int resolution_y) = 0;
        const char *get_name() { return name; };
    protected:
        const char *name;
    };
}
