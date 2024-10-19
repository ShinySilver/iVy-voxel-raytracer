#include "world_view.h"
#include "gl_util.h"
#include "../server/worldgen.h"
#include "../common/log.h"
#include "../common/time.h"
#include <cmath>

struct Node;

/**
 * An acceleration structure to render a 3D view of the world.
 * The chosen acceleration structure is a sparse voxel octree.
 * The data of the octree is split into two pools, with the first pool containing 96 bits nodes and the second one 512 bits chunks.
 */
struct {
    Region *region = 0;

    // is set to true when the terrain has changed so its GPU-memory copy is updated.
    bool dirty = false;
} WorldView;

static uint32_t memory_pool_SSBO = 0;

// Easiest:
// Finish the region class
// Finish the generate_region function. iterate the heightmap, ask the server for chunks, and add them to the region
// Add uniform chunk material support

void client::world_view::init() {
    // Initializing the WorldView structure, with a single dummy node for now
    info("Initializing world view with a single %dx%dx%d region", IVY_REGION_WIDTH, IVY_REGION_WIDTH, IVY_REGION_WIDTH);
    auto t0 = time_us();
    WorldView.region = server::worldgen::generate_region(0, 0, 0);
    WorldView.dirty = true;
    info("Done generating after %.2f ms!", double (time_us()-t0)/1e3);
    debug("Root node has bitmask 0x%lx and header 0x%x",
          *((uint64_t *) memory_pool.to_pointer(WorldView.region->get_root_node())),
          *(((uint32_t *) memory_pool.to_pointer(WorldView.region->get_root_node())) + 2));
    debug("For reference, node 0x%x has bitmask 0x%lx and header 0x%x",
          *(((uint32_t *) memory_pool.to_pointer(WorldView.region->get_root_node())) + 2),
          *((uint64_t *) memory_pool.to_pointer(*(((uint32_t *) memory_pool.to_pointer(WorldView.region->get_root_node())) + 2))),
          *(((uint32_t *) memory_pool.to_pointer(*(((uint32_t *) memory_pool.to_pointer(WorldView.region->get_root_node())) + 2))) + 2));


    // Creating the buffer that will receive the WorldView data
    glCreateBuffers(1, &memory_pool_SSBO);

    // Send the whole buffer to the graphic card.
    update();
}

void client::world_view::update() {
    // TODO: Have a dirty flag per chunk & per region
    if (WorldView.dirty) {
        WorldView.dirty = false;
        glNamedBufferData(memory_pool_SSBO, (long) memory_pool.size(), memory_pool.to_pointer(0), GL_STATIC_COPY);
    }
}

void client::world_view::bind(int memory_pool_binding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, memory_pool_binding, memory_pool_SSBO);
}

void client::world_view::terminate() {
    glDeleteBuffers(1, &memory_pool_SSBO);
}