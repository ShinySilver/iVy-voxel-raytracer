#pragma once

#include <functional>
#include "common/world/chunk.h"

#define IVY_REGION_TREE_DEPTH (4)
#define IVY_REGION_WIDTH (0x1l<<(IVY_NODE_WIDTH_SQRT*IVY_REGION_TREE_DEPTH))

namespace server{
    class Generator {
    public:
        virtual ~Generator() = default;
        virtual const char *get_name() = 0;
        virtual void generate_view(int rx, int ry, int rz, ChunkStore& view) = 0;
    };
}