#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "utils/memory_pool.h"

#define IVY_NODE_WIDTH (4)
#define IVY_NODE_WIDTH_SQUARE (2)
#define IVY_NODE_WIDTH_CUBED (IVY_NODE_WIDTH*IVY_NODE_WIDTH*IVY_NODE_WIDTH)
#define IVY_NODE_SIZE (8+4)

#define IVY_REGION_TREE_DEPTH (5)
#define IVY_REGION_WIDTH (0x1<<(IVY_NODE_WIDTH_SQUARE*IVY_REGION_TREE_DEPTH))

typedef uint8_t Voxel;

extern FastMemoryPool memory_pool;

class Chunk {
private:
    Voxel voxels[IVY_NODE_WIDTH_CUBED];
public:
    void set(int dx, int dy, int dz, Voxel voxel);
    Voxel get(int dx, int dy, int dz) const;
};

class Region {
private:
    // TODO: add coordinates to Region struct & support having multiple Regions in a world
    MemoryPoolClient *memory_subpool;
    uint32_t root_node = 0;
public:
    Region();
    ~Region();
    uint32_t add_leaf_node(int dx, int dy, int dz, Chunk *chunk);
    uint32_t get_root_node() const;
};

class HeightMap {
private:
    int *data;
    int resolution_x, resolution_y, scaling_factor;
public:
    HeightMap(int resolution_x, int resolution_y, int scaling_factor);
    ~HeightMap();
    int get_value(int x, int y);
    void set_value(int x, int y, int value);
    int get_resolution_x() const;
    int get_resolution_y() const;
    int get_scaling_factor() const;
};