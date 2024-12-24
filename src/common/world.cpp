#include "world.h"
#include "ivy_log.h"
#include "materials.h"
#include <cmath>
#include <cassert>

FastMemoryPool memory_pool = FastMemoryPool();
Generator *world_generator = nullptr;

Region::Region() {
    memory_subpool = memory_pool.create_client();
    Node *node = (Node *) memory_subpool->allocate(sizeof(Node));
    node->header = 0;
    node->bitmap = 0;
    root_node = memory_subpool->to_index(node);
}

static void delete_node_recursively(MemoryPoolClient *memory_subpool, Node *node, int depth) {
    depth += 1;
    if (depth != IVY_REGION_TREE_DEPTH) {
        Node *child_array = (Node *) memory_subpool->to_pointer(node->header & ~(0b11u << 30));
        int child_count = __builtin_popcountll(node->bitmap);
        for (int i = 0; i < child_count; i++) {
            delete_node_recursively(memory_subpool, &child_array[child_count], depth);
        }
    } else {
        Voxel *child_array = (Voxel *) memory_subpool->to_pointer(node->header & ~(0b11u << 30));
        int child_count = __builtin_popcountll(node->bitmap);
        for (int i = 0; i < child_count; i++) {
            memory_subpool->deallocate(&child_array[child_count], sizeof(Voxel));
        }
    }
}

Region::~Region() {
    delete_node_recursively(memory_subpool, (Node *) memory_subpool->to_pointer(root_node), 0);
}

uint32_t Region::get_root_node() const {
    return root_node;
}

/**
 * A region can be seen as a 256^3 grid of 4^3 chunks.
 * This function takes a chunk and its coordinates, and place it in the region's 64-tree, eventually creating or modifying multiple nodes.
 * @param dx, dy, dz Chunk coordinates
 * @param chunk The chunk itself
 * @return the index of the node that host the chunk data
 */
uint32_t Region::add_leaf_node(int dx, int dy, int dz, Chunk *chunk) {
    Node *node = (Node *) memory_subpool->to_pointer(root_node);
    int child_x, child_y, child_z, depth = 0, node_width = IVY_REGION_WIDTH, child_xyz;

    // While we have not reached the target bottom level node, we go down the tree
    while (++depth != IVY_REGION_TREE_DEPTH) {

        // The node we traverse is not supposed to be terminal, or even weirder a LOD node
        assert((node->header & (0b01u << 30)) == 0);
        assert((node->header & (0b10u << 30)) == 0);

        // We update node_width to be the width of a child of the current node
        node_width /= IVY_NODE_WIDTH;
        child_x = dx / node_width;
        child_y = dy / node_width;
        child_z = dz / node_width;
        dx -= child_x * node_width;
        dy -= child_y * node_width;
        dz -= child_z * node_width;

        // If the current node has no child where we want to go, we have to create it
        child_xyz = child_x + child_y * IVY_NODE_WIDTH + child_z * IVY_NODE_WIDTH * IVY_NODE_WIDTH;
        if ((node->bitmap & (0x1ul << child_xyz)) == 0) {

            //assert((node->header & ~(0b11u << 30)) != 0); //

            // First, we update the current node bitmap to account for the child that will soon be created
            int previous_child_count = __builtin_popcountll(node->bitmap);
            int previous_child_id = __builtin_popcountll(node->bitmap & ~(UINT64_MAX << child_xyz));
            node->bitmap = node->bitmap | (0x1ul << child_xyz);

            // The while loop guarantee the current node is not supposed to be terminal. As such, the current node children are nodes of size 12.

            // We extend the child array to have room for the newly created child
            Node *previous_child_array = (Node *) memory_subpool->to_pointer(node->header & ~(0b11u << 30));
            Node *new_child_array = (Node *) memory_subpool->allocate((previous_child_count + 1) * sizeof(Node));

            // We copy from the previous child array to the new one, while leaving an empty space for the new child.
            // Then, we don't forget to free the old child array
            if (previous_child_count != 0) {
                memcpy(new_child_array, previous_child_array, previous_child_id * sizeof(Node));
                memcpy(new_child_array + previous_child_id + 1, previous_child_array + previous_child_id, (previous_child_count - previous_child_id) * sizeof(Node));
                memory_subpool->deallocate(previous_child_array, sizeof(Node) * previous_child_count);
            }

            // We place the child array in the current node header
            node->header = (node->header & (0b11u << 30)) | memory_subpool->to_index(new_child_array);

            // And at last, we use placement new to create the new subnode that is the new "current" node
            Node *child = new_child_array + previous_child_id;
            child->header = 0;
            child->bitmap = 0;
            node = child;

            // If this new node happens to be terminal, we mark it as such
            if (depth + 1 == IVY_REGION_TREE_DEPTH) child->header = child->header | (0x1u << 30);
        } else {
            // If a child exist for the volume we want to write to, we just enter it
            int previous_child_id = __builtin_popcountll(node->bitmap & ~((~0x0ul) << child_xyz));
            Node *child_array = (Node *) memory_subpool->to_pointer(node->header & ~(0b11u << 30));
            node = &child_array[previous_child_id];
        }
    }

    // Once we have reached this point, the "node" variable should be set to the address we want to write to.
    // It's now time to actually copy the chunk in the region memory!

    // If the target node has an allocation (non-empty and non-LOD), we first have to free it
    if (node->bitmap != 0 && (node->header & (0b10u << 30)) == 0) {
        int child_count = __builtin_popcountll(node->bitmap);
        node->bitmap = 0;
        Node *child_array = (Node *) memory_subpool->to_pointer(node->header & ~(0b11u << 30));
        memory_subpool->deallocate(child_array, int(child_count * sizeof(Voxel)));
    }

    // Now that we know for sure that the node has no existing allocation, we can write into it. First, we assemble the bitmask...
    Voxel uniform_material = AIR;
    bool is_uniform = true;
    for (child_z = 0; child_z < IVY_NODE_WIDTH; child_z++) {
        for (child_y = 0; child_y < IVY_NODE_WIDTH; child_y++) {
            for (child_x = 0; child_x < IVY_NODE_WIDTH; child_x++) {
                Voxel voxel = chunk->get(child_x, child_y, child_z);
                if (voxel != AIR) {
                    if (uniform_material == AIR) {
                        uniform_material = voxel;
                    } else if (uniform_material != voxel) {
                        is_uniform = false;
                    }
                    node->bitmap = node->bitmap | (0x1ul << (child_x + child_y * IVY_NODE_WIDTH + child_z * IVY_NODE_WIDTH * IVY_NODE_WIDTH));
                }
            }
        }
    }

    if (is_uniform) {
        // If the node is uniform, we apply the lod color & the terminal & lod bits
        node->header = uniform_material | (0b11u << 30);
    } else {
        // If it's not, we allocate a voxel array, place it in the header, and set the voxels.
        int child_count = __builtin_popcountll(node->bitmap);
        Voxel *child_array = (Voxel *) memory_subpool->allocate(int(child_count * sizeof(Voxel)));
        node->header = memory_subpool->to_index(child_array);
        int index = 0;
        for (child_z = 0; child_z < IVY_NODE_WIDTH; child_z++) {
            for (child_y = 0; child_y < IVY_NODE_WIDTH; child_y++) {
                for (child_x = 0; child_x < IVY_NODE_WIDTH; child_x++) {
                    Voxel voxel = chunk->get(child_x, child_y, child_z);
                    if (voxel != AIR) {
                        child_array[index] = voxel;
                        index += 1;
                    }
                }
            }
        }

        // And again we don't forget to mark the node as terminal
        node->header = node->header | (0b01u << 30);
    }
    return memory_subpool->to_index(node);
}

void Chunk::set(int x, int y, int z, Voxel voxel) {
    assert(x >= 0 && x < 4 && y >= 0 && y < 4 && z >= 0 && z < 4);
    voxels[x + y * IVY_NODE_WIDTH + z * IVY_NODE_WIDTH * IVY_NODE_WIDTH] = voxel;
}

Voxel Chunk::get(int x, int y, int z) const {
    return voxels[x + y * IVY_NODE_WIDTH + z * IVY_NODE_WIDTH * IVY_NODE_WIDTH];
}

HeightMap::HeightMap(int resolution_x, int resolution_y) :
        data{(int *) std::malloc(sizeof(int) * resolution_x * resolution_y)}, resolution_x{resolution_x}, resolution_y{resolution_y}
        { std::memset(data, 0, resolution_x * resolution_y * sizeof(int)); }

HeightMap::~HeightMap() { free(data); }

int HeightMap::get_value(int x, int y) {
    assert(x >= 0 && y >= 0 && x < resolution_x && y < resolution_y);
    return data[x + y * resolution_x];
}

void HeightMap::set_value(int x, int y, int value) {
    assert(x >= 0 && y >= 0 && x < resolution_x && y < resolution_y);
    data[x + y * resolution_x] = value;
}

int HeightMap::get_resolution_x() const {
    return resolution_x;
}

int HeightMap::get_resolution_y() const {
    return resolution_y;
}