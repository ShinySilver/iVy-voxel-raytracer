#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "utils/memory_pool.h"

#define IVY_NODE_WIDTH (4l)
#define IVY_NODE_WIDTH_SQRT (2)
#define IVY_NODE_WIDTH_CUBED (IVY_NODE_WIDTH*IVY_NODE_WIDTH*IVY_NODE_WIDTH)

#define IVY_REGION_TREE_DEPTH (7)
#define IVY_REGION_WIDTH (0x1l<<(IVY_NODE_WIDTH_SQRT*IVY_REGION_TREE_DEPTH))

class Generator;

extern FastMemoryPool memory_pool;
extern Generator *world_generator;

typedef uint8_t Voxel;

struct __attribute__((packed)) Node {
    /**
     * 4x4x4 bit, one per subnode. Can be used for faster traversal than a list of u32.
     * An empty bitmap with value 0x0000 means that the node data has not been generated yet.
     */
    uint64_t bitmap = 0;

    /**
     * The header starts with two bits encoding the LOD status:
     * - If the first bits are 0b00, the last 30 bits are the address of the first non-empty subnode.
     * - If the first bits are 0b01, the node is terminal and the last 30 bits are the address of the first non-empty voxel.
     * - If the first bits are Ob11, the node is terminal and the last 8 bits are the LOD color of the non-empty subnodes.
     */
    uint32_t header = 0;
};

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
    int resolution_x, resolution_y;
public:
    HeightMap(int resolution_x, int resolution_y);
    ~HeightMap();
    int get_value(int x, int y);
    void set_value(int x, int y, int value);
    int get_resolution_x() const;
    int get_resolution_y() const;
};

class Generator {
public:
    virtual ~Generator() = default;
    virtual const char *get_name() = 0;
    virtual HeightMap *generate_heightmap(int x, int y, int z, int width) = 0;
    virtual Region *generate_region(int x, int y, int z) = 0;
};