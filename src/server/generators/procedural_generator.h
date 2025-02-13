#pragma once

#include "generator.h"
#include "common/world/heightmap.h"

namespace server {
    class ProceduralGenerator : public Generator {
    private:
        const char *name = "Procedural";
    public:
        ProceduralGenerator();
        ~ProceduralGenerator() override;
        const char *get_name() override { return "Procedural"; };
        void generate_view(int rx, int ry, int rz, ChunkStore& view) override;
    };
}