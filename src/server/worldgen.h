#pragma once

#include "../common/world.h"

namespace server::worldgen {
    namespace imp{
        Chunk *generate_chunk(int dx, int dy, int dz, HeightMap *height_map, HeightMap *depth_map);
        HeightMap *generate_heightmap(int dx, int dy, int dz);
        HeightMap *generate_depthmap(int dx, int dy, int dz);
    }
    void init();
    void terminate();
    Region *generate_region(int x, int y, int z);
};
