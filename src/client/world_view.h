#pragma once

#include "../common/world.h"

namespace client::world_view {
    void init();
    void update();
    void bind(int memory_pool_binding);
    void terminate();
}
