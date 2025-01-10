#pragma once

#include "chunk.h"

class WorldView {
public:
    virtual ~WorldView() = default;
    virtual void add_chunk(int dx, int dy, int dz, Chunk *chunk) = 0;
};