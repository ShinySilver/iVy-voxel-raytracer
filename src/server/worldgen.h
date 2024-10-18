#pragma once

#include "../common/world.h"

namespace server::worldgen {
    void init();
    void terminate();
    Region *generate_region(int x, int y, int z);
};
