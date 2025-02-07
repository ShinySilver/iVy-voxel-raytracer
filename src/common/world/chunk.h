#pragma once

#include "voxel.h"

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

class ChunkStore {
public:
    virtual ~ChunkStore() = default;
    virtual void add_chunk(int dx, int dy, int dz, Chunk *chunk) = 0;
};