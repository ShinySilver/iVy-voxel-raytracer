#include <cassert>
#include <cstring>
#include "client/utils/wide_tree.h"
#include "common/world/voxel.h"
#include "common/world/chunk.h"
#include "server/generators/generator.h"
#include "client/client.h"

namespace client::utils {
    namespace {
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

        void delete_node_recursively(MemoryPoolClient &memory_subpool, Node *node, int depth) {
            depth += 1;
            if (depth != IVY_REGION_TREE_DEPTH) {
                Node *child_array = (Node *) memory_subpool.to_pointer(node->header & ~(0b11u << 30));
                int child_count = __builtin_popcountll(node->bitmap);
                for (int i = 0; i < child_count; i++) {
                    delete_node_recursively(memory_subpool, &child_array[child_count], depth);
                }
            } else {
                auto *child_array = (Voxel *) memory_subpool.to_pointer(node->header & ~(0b11u << 30));
                int child_count = __builtin_popcountll(node->bitmap);
                for (int i = 0; i < child_count; i++) {
                    memory_subpool.deallocate(&child_array[child_count], sizeof(Voxel));
                }
            }
        }
    }

    WideTree::WideTree() : memory_subpool{memory_pool.create_client()} {
        Node *node = (Node *) memory_subpool.allocate(sizeof(Node));
        node->header = 0;
        node->bitmap = 0;
        root_node = memory_subpool.to_index(node);
    }

    WideTree::~WideTree() {
        delete_node_recursively(memory_subpool, (Node *) memory_subpool.to_pointer(root_node), 0);
    }

    uint32_t WideTree::get_root_node() const {
        return root_node;
    }

    void WideTree::add_chunk(int dx, int dy, int dz, Chunk *chunk) {
        Node *node = (Node *) memory_subpool.to_pointer(root_node);
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
            child_xyz = int(child_x + child_y * IVY_NODE_WIDTH + child_z * IVY_NODE_WIDTH * IVY_NODE_WIDTH);
            if ((node->bitmap & (0x1ul << child_xyz)) == 0) {

                //assert((node->header & ~(0b11u << 30)) != 0); //

                // First, we update the current node bitmap to account for the child that will soon be created
                int previous_child_count = __builtin_popcountll(node->bitmap);
                int previous_child_id = __builtin_popcountll(node->bitmap & ~(UINT64_MAX << child_xyz));
                node->bitmap = node->bitmap | (0x1ul << child_xyz);

                // The while loop guarantee the current node is not supposed to be terminal. As such, the current node children are nodes of size 12.

                // We extend the child array to have room for the newly created child
                Node *previous_child_array = (Node *) memory_subpool.to_pointer(node->header & ~(0b11u << 30));
                Node *new_child_array = (Node *) memory_subpool.allocate((previous_child_count + 1) * int(sizeof(Node)));

                // We copy from the previous child array to the new one, while leaving an empty space for the new child.
                // Then, we don't forget to free the old child array
                if (previous_child_count != 0) {
                    memcpy(new_child_array, previous_child_array, previous_child_id * sizeof(Node));
                    memcpy(new_child_array + previous_child_id + 1, previous_child_array + previous_child_id, (previous_child_count - previous_child_id) * sizeof(Node));
                    memory_subpool.deallocate(previous_child_array, int(sizeof(Node)) * previous_child_count);
                }

                // We place the child array in the current node header
                node->header = (node->header & (0b11u << 30)) | memory_subpool.to_index(new_child_array);

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
                Node *child_array = (Node *) memory_subpool.to_pointer(node->header & ~(0b11u << 30));
                node = &child_array[previous_child_id];
            }
        }

        // Once we have reached this point, the "node" variable should be set to the address we want to write to.
        // It's now time to actually copy the chunk in the region memory!

        // If the target node has an allocation (non-empty and non-LOD), we first have to free it
        if (node->bitmap != 0 && (node->header & (0b10u << 30)) == 0) {
            int child_count = __builtin_popcountll(node->bitmap);
            node->bitmap = 0;
            Node *child_array = (Node *) memory_subpool.to_pointer(node->header & ~(0b11u << 30));
            memory_subpool.deallocate(child_array, int(child_count * sizeof(Voxel)));
        }

        // Now that we know for sure that the node has no existing allocation, we can write into it. First, we assemble the bitmask...
        auto uniform_material = Voxel{AIR};
        bool is_uniform = true;
        for (child_z = 0; child_z < IVY_NODE_WIDTH; child_z++) {
            for (child_y = 0; child_y < IVY_NODE_WIDTH; child_y++) {
                for (child_x = 0; child_x < IVY_NODE_WIDTH; child_x++) {
                    Voxel voxel = chunk->get(child_x, child_y, child_z);
                    if (voxel.material != AIR) {
                        if (uniform_material.material == AIR) {
                            uniform_material = voxel;
                        } else if (uniform_material.material != voxel.material) {
                            is_uniform = false;
                        }
                        node->bitmap = node->bitmap | (0x1ul << (child_x + child_y * IVY_NODE_WIDTH + child_z * IVY_NODE_WIDTH * IVY_NODE_WIDTH));
                    }
                }
            }
        }

        if (is_uniform) {
            // If the node is uniform, we apply the lod color & the terminal & lod bits
            node->header = uniform_material.material | (0b11u << 30);
        } else {
            // If it's not, we allocate a voxel array, place it in the header, and set the voxels.
            int child_count = __builtin_popcountll(node->bitmap);
            auto *child_array = (Voxel *) memory_subpool.allocate(int(child_count * sizeof(Voxel)));
            node->header = memory_subpool.to_index(child_array);
            int index = 0;
            for (child_z = 0; child_z < IVY_NODE_WIDTH; child_z++) {
                for (child_y = 0; child_y < IVY_NODE_WIDTH; child_y++) {
                    for (child_x = 0; child_x < IVY_NODE_WIDTH; child_x++) {
                        Voxel voxel = chunk->get(child_x, child_y, child_z);
                        if (voxel.material != AIR) {
                            child_array[index] = voxel;
                            index += 1;
                        }
                    }
                }
            }

            // And again we don't forget to mark the node as terminal
            node->header = node->header | (0b01u << 30);
        }
    }
}