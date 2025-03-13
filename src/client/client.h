#pragma once

#include "client/renderers/renderer.h"
#include "client/utils/memory_pool.h"

namespace client {
    extern Renderer *active_renderer;
    extern FastMemoryPool *memory_pool;
    void start();
    void terminate();
}
