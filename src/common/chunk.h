#pragma once

#include "common/voxel.h"

#define IVY_NODE_WIDTH (4l)
#define IVY_NODE_WIDTH_SQRT (2)
#define IVY_NODE_WIDTH_SQUARED (IVY_NODE_WIDTH*IVY_NODE_WIDTH)
#define IVY_NODE_WIDTH_CUBED (IVY_NODE_WIDTH*IVY_NODE_WIDTH*IVY_NODE_WIDTH)

class Chunk {
    Voxel voxels[IVY_NODE_WIDTH_CUBED];
public:
    void set(int dx, int dy, int dz, Voxel voxel);
    Voxel get(int dx, int dy, int dz) const;
};
