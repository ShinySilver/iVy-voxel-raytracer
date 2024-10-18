#include <cstring>
#include "memory_pool.h"
#include "../log.h"

MemoryPoolClient::MemoryPoolClient(FastMemoryPool *src) : source(src), pools() {}

MemoryPoolClient::~MemoryPoolClient() {
    for (int i = 0; i < 64; ++i) {
        for (void *block: pools[i].blocks) {
            source->deallocate(block);
        }
    }
}

void *MemoryPoolClient::allocate(int size) {
    if (size < 1 || (size > 64 && size % 12 != 0) || size > 64 * 12) {
        fatal("Invalid size: %d", size);
    }

    int idx = size > 64 ? int(size / 12) - 6 + 64 : size - 1;
    SubPool &pool = pools[idx];
    pool.allocated += size;

    if (!pool.holes.empty()) {
        void *ptr = pool.holes.front();
        pool.holes.pop_front();
        return ptr;
    }

    if (pool.next_alloc == nullptr || uint32_t((char *) pool.next_alloc - (char *) pool.current_chunk + size) > source->chunk_size) {
        pool.next_alloc = source->allocate();
        // debug("Obtained an allocation with index %u (Ox%x) for allocations of size %d",
        // source->to_index(pool.next_alloc), source->to_index(pool.next_alloc), size);
        pool.current_chunk = pool.next_alloc;
        if (!pool.next_alloc) {
            fatal("Out of memory");
        }
        pool.blocks.push_back(pool.next_alloc);
    }

    void *ptr = pool.next_alloc;
    pool.next_alloc = (char *) pool.next_alloc + size;

    return ptr;
}

void MemoryPoolClient::deallocate(void *ptr, int size) {
    if (size < 1 || (size > 64 && size % 12 != 0) || size > 64 * 12) {
        fatal("Invalid size");
    }

    if (ptr == 0) {
        fatal("Invalid pointer");
    }

    int idx = size > 64 ? int(size / 12) - 6 + 64 : size - 1;
    pools[idx].allocated -= size;
    pools[idx].holes.push_back(ptr);
}

uint32_t MemoryPoolClient::to_index(void *ptr) {
    return uint32_t((char *) ptr - (char *) source->base_addr);
}

void *MemoryPoolClient::to_pointer(uint32_t index) {
    return (char *) source->base_addr + index;
}

size_t MemoryPoolClient::get_used_memory() const {
    size_t used_memory = 0;
    for (const auto &pool: pools) {
        used_memory += pool.allocated;
    }
    return used_memory;
}

// FastMemoryPool Implementation

FastMemoryPool::FastMemoryPool(size_t max_size, size_t chunk_size)
        : base_addr(nullptr), next(nullptr), max_size(max_size), chunk_size(chunk_size), allocated_blocks(0) {

    base_addr = std::aligned_alloc(32, max_size);
    memset(base_addr, 0, max_size);
    if (!base_addr) {
        throw std::bad_alloc();
    }
    next = base_addr;
}

FastMemoryPool::~FastMemoryPool() {
    for (MemoryPoolClient *client: clients) {
        delete client;
    }
    if (base_addr) {
        free(base_addr);
    }
}

void *FastMemoryPool::allocate() {
    std::lock_guard<std::mutex> lock(guard);

    if (!free_blocks.empty()) {
        void *reused_block = free_blocks.front();
        free_blocks.pop_front();
        return reused_block;
    }

    if (next && ((char *) next - (char *) base_addr) + chunk_size <= max_size) {
        void *allocated = next;
        next = (char *) next + chunk_size;
        ++allocated_blocks;  // Increment the number of allocated blocks
        return allocated;
    }

    return nullptr;
}

void FastMemoryPool::deallocate(void *ptr) {
    std::lock_guard<std::mutex> lock(guard);
    free_blocks.push_back(ptr);
}

MemoryPoolClient *FastMemoryPool::create_client() {
    MemoryPoolClient *client = new MemoryPoolClient(this);
    clients.push_back(client);
    return client;
}

void FastMemoryPool::free_client(MemoryPoolClient *client) {
    auto it = std::remove(clients.begin(), clients.end(), client);
    if (it != clients.end()) {
        clients.erase(it);
    }
    delete client;
}

size_t FastMemoryPool::size() {
    return max_size;
}

size_t FastMemoryPool::allocated() {
    return allocated_blocks * chunk_size;
}

size_t FastMemoryPool::used() {
    size_t used_memory = 0;
    for (const auto & client: clients) {
        used_memory += client->get_used_memory();
    }
    return used_memory;
}

uint32_t FastMemoryPool::to_index(void *ptr) {
    return uint32_t((char *) ptr - (char *) base_addr);
}

void *FastMemoryPool::to_pointer(uint32_t index) {
    return (char *) base_addr + index;
}
size_t FastMemoryPool::get_chunk_size() const {
    return chunk_size;
}
