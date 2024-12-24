#pragma once

#include "../../common/world.h"

namespace server {
    class DebugGenerator : public Generator {
    public:
        DebugGenerator();
        ~DebugGenerator() override;
        const char *get_name() override { return "Debug Generator \"z<max(x,y)\""; };
        Region *generate_region(int x, int y, int z) override;
        HeightMap *generate_heightmap(int x, int y, int z, int width) override;
    };
}