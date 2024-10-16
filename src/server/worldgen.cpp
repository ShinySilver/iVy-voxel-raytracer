#include <complex>
#include "worldgen.h"
#include "../common/materials.h"
#include "../common/log.h"
#include "FastNoise/FastNoise.h"

#define PI ((double)3.14159265359)

void server::worldgen::init() {}

void server::worldgen::terminate() {}

/**
 * @param dx absolute x-coordinate of the region
 * @param dy absolute y-coordinate of the region
 * @param dz absolute z-coordinate of the region
 * @return a new heightmap
 */
HeightMap *server::worldgen::imp::generate_heightmap(int dx, int dy, int dz) {
    HeightMap *height_map = new HeightMap(IVY_REGION_WIDTH, IVY_REGION_WIDTH, 1);
    for (int y = 0; y < IVY_REGION_WIDTH; y++) {
        for (int x = 0; x < IVY_REGION_WIDTH; x++) {
            int h = int(IVY_REGION_WIDTH / 2 + IVY_REGION_WIDTH / 4 * MIN(sin(20 * PI * x / IVY_REGION_WIDTH), sin(20 * PI * y / IVY_REGION_WIDTH)));
            height_map->set_value(x, y, h);
        }
    }
    return height_map;
}

/**
 * @param dx absolute x-coordinate of the region
 * @param dy absolute y-coordinate of the region
 * @param dz absolute z-coordinate of the region
 * @return a new depthmap
 */
HeightMap *server::worldgen::imp::generate_depthmap(int x, int y, int z) {
    HeightMap *depth_map = new HeightMap(IVY_REGION_WIDTH, IVY_REGION_WIDTH, 1);
    for (int dy = 0; dy < IVY_REGION_WIDTH; dy++) {
        for (int dx = 0; dx < IVY_REGION_WIDTH; dx++) {
            int h = int(IVY_REGION_WIDTH / 8 + IVY_REGION_WIDTH / 8 * MIN(sin(20 * PI * x / IVY_REGION_WIDTH), sin(20 * PI * y / IVY_REGION_WIDTH)));
            depth_map->set_value(dx, dy, h);
        }
    }
    return depth_map;
}

/**
 * @param dx x-coordinate relative to the region
 * @param dy y-coordinate relative to the region
 * @param dz z-coordinate relative to the region
 * @param height_map map to be used to generate the surface envelope of the world column
 * @param depth_map map to be used to generate the bottom envelope of the world column
 * @return
 */
Chunk *server::worldgen::imp::generate_chunk(int dx, int dy, int dz, HeightMap *height_map, HeightMap *depth_map) {
    assert(dx >= 0 && dx <= IVY_REGION_WIDTH - IVY_NODE_WIDTH);
    assert(dy >= 0 && dy <= IVY_REGION_WIDTH - IVY_NODE_WIDTH);
    assert(dz >= 0 && dz <= IVY_REGION_WIDTH - IVY_NODE_WIDTH);
    static __thread Chunk *chunk = 0;
    if (chunk == 0) [[unlikely]] chunk = (Chunk *) malloc(sizeof(Chunk));
    for (int z = 0; z < IVY_NODE_WIDTH; z++) {
        for (int y = 0; y < IVY_NODE_WIDTH; y++) {
            for (int x = 0; x < IVY_NODE_WIDTH; x++) {
                bool is_air = z + dz > height_map->get_value(x + dx, y + dy) || z + dz < depth_map->get_value(x + dx, y + dy);
                chunk->set(x, y, z, is_air ? AIR : STONE);
            }
        }
    }
    return chunk;
}

// TODO: Add a function to iterate on every node of a region, and a function to check if a node exist at a given position.
// TODO: Then, as a second pass of this function, for all nodes, check for non-solid sides recursively & generate them as long as they are in the volume
Region *server::worldgen::generate_region(int rx, int ry, int rz) {
    // Step 1: Generate heightmap
    Region *region = new Region();
    HeightMap *height_map = imp::generate_heightmap(rx, ry, rz);
    HeightMap *depth_map = imp::generate_depthmap(rx, ry, rz);

    // Step 2: For each node column, get the min/max height & depth.
    for (int y = 0; y < IVY_REGION_WIDTH; y += IVY_NODE_WIDTH) {
        for (int x = 0; x < IVY_REGION_WIDTH; x += IVY_NODE_WIDTH) {
            int min_height = INT32_MAX, max_height = 0;
            int min_depth = INT32_MAX, max_depth = 0;
            for (int dy = 0; dy < IVY_NODE_WIDTH; dy += 1) {
                for (int dx = 0; dx < IVY_NODE_WIDTH; dx += 1) {
                    int height = height_map->get_value(x + dx, y + dy);
                    min_height = MIN(min_height, height);
                    max_height = MAX(max_height, height);

                    int depth = depth_map->get_value(x + dx, y + dy);
                    min_depth = MIN(min_depth, depth);
                    max_depth = MAX(max_depth, depth);
                }
            }
            min_height = MAX(min_height, rz);
            max_depth = MIN(max_depth, rz + IVY_REGION_WIDTH - 1);

            // Step 3: Then for all non-empty node column, generate nodes between the min and the max
            int z;
            if (min_height <= max_height) {
                for (z = (int) floor((double) min_height / IVY_NODE_SIZE) * IVY_NODE_SIZE; z <= ceil((double) max_height / IVY_NODE_SIZE) * IVY_NODE_SIZE; z += IVY_NODE_WIDTH) {
                    region->add_leaf_node(x, y, z, server::worldgen::imp::generate_chunk(x, y, z, height_map, depth_map));
                }
            }
            if (min_depth <= max_depth) {
                for (z = (int) floor((double) min_depth / IVY_NODE_SIZE) * IVY_NODE_SIZE; z <= ceil((double) max_depth / IVY_NODE_SIZE) * IVY_NODE_SIZE; z += IVY_NODE_WIDTH) {
                    region->add_leaf_node(x, y, z, server::worldgen::imp::generate_chunk(x, y, z, height_map, depth_map));
                }
            }
        }
    }
    delete (height_map);
    delete (depth_map);
    return region;
}