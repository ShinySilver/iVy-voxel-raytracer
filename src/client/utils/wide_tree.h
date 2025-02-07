#pragma once

#include "client/utils/memory_pool.h"
#include "common/world/chunk.h"

namespace client::utils {
    class WideTree : public ChunkStore {
        MemoryPoolClient &memory_subpool;
        uint32_t root_node = 0;
    public:
        WideTree();
        ~WideTree() override;
        void add_chunk(int dx, int dy, int dz, Chunk *chunk) override;
        uint32_t get_root_node() const;
    };
}
