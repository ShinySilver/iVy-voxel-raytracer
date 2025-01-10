#include <cassert>
#include "chunk.h"

void Chunk::set(int x, int y, int z, Voxel voxel) {
    assert(x >= 0 && x < 4 && y >= 0 && y < 4 && z >= 0 && z < 4);
    voxels[x + y * IVY_NODE_WIDTH + z * IVY_NODE_WIDTH_SQUARED] = voxel;
}

Voxel Chunk::get(int x, int y, int z) const {
    return voxels[x + y * IVY_NODE_WIDTH + z * IVY_NODE_WIDTH_SQUARED];
}