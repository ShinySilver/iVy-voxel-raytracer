#pragma once

#include "../../common/world.h"

namespace server {
    class ProceduralGenerator : public Generator {
    private:
        const char *name = "Procedural";
    public:
        ProceduralGenerator();
        ~ProceduralGenerator() override;
        const char *get_name() override { return "Procedural"; };
        Region *generate_region(int x, int y, int z) override;
        HeightMap *generate_heightmap(int x, int y, int z, int width) override;
    };
}