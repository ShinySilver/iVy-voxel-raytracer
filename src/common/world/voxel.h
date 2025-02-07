#pragma once

#include <cstdint>

enum Material : uint8_t {
    AIR,
    DEBUG_RED,
    DEBUG_GREEN,
    DEBUG_BLUE,
    STONE,
    DIRT,
    GRASS
};

// TODO: replace with the weird bitwise thing, with a normal encoded in spherical coordinates, one bit to know if there is a normal at all, and 7 bits for the material
struct Voxel {
    Material material;
};