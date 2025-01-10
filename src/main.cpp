#include "client/client.h"
#include "server/server.h"

/*
 * I guess my to-do list will go here:
 *
 * High priority:
 *   - Refactored worldgen, client-server communication and world loading
 *   - Inventory system and/or terraforming/building tools
 *
 * Performance:
 *   - Smart memory pool (dirty flag, alloc size)
 *   - Upscaling
 *   - Some manner of morton ordering / sorting ?
 *   - 2-bits and 4-bits palette optimization
 *
 * Art:
 *   - Smart screen-space normal blur for far voxels
 *   - Per-voxel normals
 *   - 16-bits voxels
 *   - Voxel palette (materials)
 *
 * Technical debt:
 *   - World serialization / deserialization
 *   - Enforced client-server separation
 *
 * Feats (high cost, high reward) :
 *   - Networking
 *   - Some kind of dynamic LOD (especially if we end up supporting per-voxel normals...)
 *   - Importing 3d models
 *   - Animated voxel stuffs
 *   - Multi-grid
 *   - Tile-entities
 *   - Prebuilt structure placement. Either grid-snapped (such as furnace & machines), or grid-free (walls)
 *
 * Optional:
 *   - font loading.
 *   - a keybinding abstraction & UI
 *   - a parameter menu for client parameters (including UI size parameter)
 *   - something to save client parameters & keybindings
 *   - a main menu with a blurred background gif of some builds of mine
 */

int main() {
    server::start();
    client::start();
    server::stop();
    server::join();
    return 0;
}
