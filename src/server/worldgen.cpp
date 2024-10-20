#include <complex>
#include "worldgen.h"
#include "../common/materials.h"
#include "../common/log.h"
#include "FastNoise/FastNoise.h"

#define PI ((double)3.14159265359)

void server::worldgen::init() {}

void server::worldgen::terminate() {}

static Chunk *generate_chunk(int x, int y, int z) {
    static __thread Chunk chunk;
    int voxel_count = 0;
    for (int dz = 0; dz < IVY_NODE_WIDTH; dz += 1) {
        for (int dy = 0; dy < IVY_NODE_WIDTH; dy += 1) {
            for (int dx = 0; dx < IVY_NODE_WIDTH; dx += 1) {
                int h = MAX(x+dx, y+dy);
                if(z+dz<=h){
                    chunk.set(dx, dy, dz, STONE);
                    voxel_count+=1;
                }else{
                    chunk.set(dx, dy, dz, AIR);
                }
            }
        }
    }
    if(voxel_count==0) return nullptr;
    //if(voxel_count == IVY_NODE_WIDTH_CUBED) return nullptr;
    return &chunk;
}

Region *server::worldgen::generate_region(int rx, int ry, int rz) {
    Region *region = new Region();
    for (int z = rz; z < IVY_REGION_WIDTH; z += IVY_NODE_WIDTH) {
        for (int y = ry; y < IVY_REGION_WIDTH; y += IVY_NODE_WIDTH) {
            for (int x = rx; x < IVY_REGION_WIDTH; x += IVY_NODE_WIDTH) {
                Chunk *chunk = generate_chunk(x, y, z);
                if (chunk != nullptr) region->add_leaf_node(x - rx, y - ry, z - rz, chunk);
            }
        }
    }
    return region;
}