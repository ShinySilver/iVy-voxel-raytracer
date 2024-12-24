#include "debug_generator.h"
#include "../../common/materials.h"
#include "ivy_log.h"

static Voxel get_voxel(int x, int y, int z);
static Chunk *generate_chunk(int x, int y, int z);

server::DebugGenerator::DebugGenerator() = default;
server::DebugGenerator::~DebugGenerator() = default;

Region *server::DebugGenerator::generate_region(int rx, int ry, int rz) {
    Region *region = new Region();
    for (int z = rz; z < IVY_REGION_WIDTH; z += IVY_NODE_WIDTH) {
        for (int y = ry; y < IVY_REGION_WIDTH; y += IVY_NODE_WIDTH) {
            for (int x = rx; x < IVY_REGION_WIDTH; x += IVY_NODE_WIDTH) {
                Chunk *chunk = generate_chunk(x, y, z);
                if (chunk != nullptr) {
                    region->add_leaf_node(x - rx, y - ry, z - rz, chunk);
                }
            }
        }
    }
    return region;
}

HeightMap *server::DebugGenerator::generate_heightmap(int x, int y, int z, int width) {
    return nullptr;
}

static Voxel get_voxel(int x, int y, int z) {
    if (x < 0 || x >= IVY_REGION_WIDTH || y < 0 || y >= IVY_REGION_WIDTH || z < 0 || z >= IVY_REGION_WIDTH) return AIR;
    return (z <= MAX(x, y)) ? STONE : AIR;
}

static Chunk *generate_chunk(int x, int y, int z) {
    static __thread Chunk chunk;
    int voxel_count = 0;
    for (int dz = 0; dz < IVY_NODE_WIDTH; dz += 1) {
        for (int dy = 0; dy < IVY_NODE_WIDTH; dy += 1) {
            for (int dx = 0; dx < IVY_NODE_WIDTH; dx += 1) {
                Voxel v = get_voxel(x + dx, y + dy, z + dz);
                if (v != AIR) {
                    bool is_surface = get_voxel(x + dx + 1, y + dy, z + dz) == AIR ||
                                      get_voxel(x + dx - 1, y + dy, z + dz) == AIR ||
                                      get_voxel(x + dx, y + dy + 1, z + dz) == AIR ||
                                      get_voxel(x + dx, y + dy - 1, z + dz) == AIR ||
                                      get_voxel(x + dx, y + dy, z + dz + 1) == AIR ||
                                      get_voxel(x + dx, y + dy, z + dz - 1) == AIR;
                    v = is_surface ? v : AIR;
                }
                chunk.set(dx, dy, dz, v);
                if (v != AIR) voxel_count += 1;
            }
        }
    }
    if (voxel_count == 0) return nullptr;
    //if(voxel_count == IVY_NODE_WIDTH_CUBED) return nullptr;
    return &chunk;
}
