#pragma once

#include <cstdint>
#include <mutex>
#include <deque>
#include <vector>
#include <iostream>
#include <stdexcept>

class MemoryPoolClient;

class FastMemoryPool {
private:
    void *base_addr;  // Pointer to the base address of the single large memory block
    void *next;       // Pointer to the next free address in the pool
    size_t max_size;  // Maximum size of the memory pool
    size_t chunk_size;  // Size of each chunk within the pool
    std::mutex guard;  // Mutex to ensure thread-safe operations
    std::deque<void*> free_blocks;  // Deque to store deallocated blocks for reuse

    /**
     * Allocate a new chunk of memory from the pool.
     * @return Pointer to the allocated memory, or nullptr if the pool is exhausted.
     */
    void *allocate();

    /**
     * Deallocate a block of memory and add it to the free_blocks deque for reuse.
     * @param ptr Pointer to the memory block to be deallocated.
     */
    void deallocate(void *ptr);

    friend class MemoryPoolClient;  // Allow MemoryPoolClient to access private members
public:
    /**
     * Constructor to initialize the FastMemoryPool with the given size and chunk size.
     * @param max_size Maximum size of the memory pool.
     * @param chunk_size Size of each chunk within the pool.
     */
    FastMemoryPool(size_t max_size = 2ULL * 1024 * 1024 * 1024, size_t chunk_size = 8 * 1024);

    /**
     * Destructor to free the memory pool.
     */
    ~FastMemoryPool();

    /**
     * Create a new MemoryPoolClient associated with this pool.
     * @return Pointer to the newly created MemoryPoolClient.
     */
    MemoryPoolClient *create_client();

    /**
     * Free a MemoryPoolClient and deallocate all memory blocks associated with it.
     * @param client Pointer to the MemoryPoolClient to be freed.
     */
    void free_client(MemoryPoolClient *client);
};

class MemoryPoolClient {
private:
    struct SubPool {
        void *next_alloc;  // Pointer to the next allocation point within this subpool
        std::deque<void *> holes;  // Deque to store deallocated blocks for reuse
        std::vector<void *> blocks;  // Vector to store allocated blocks

        SubPool() : next_alloc(nullptr) {}
        ~SubPool() {}
    };

    FastMemoryPool *source;  // The source memory pool
    SubPool pools[64];  // Array of subpools for different allocation sizes (1 to 64 bytes)

    /**
     * Constructor for MemoryPoolClient. Only accessible by FastMemoryPool.
     * @param src Pointer to the FastMemoryPool from which this client is created.
     */
    MemoryPoolClient(FastMemoryPool *src);

    /**
     * Destructor for MemoryPoolClient. Frees all allocated blocks.
     */
    ~MemoryPoolClient();

    friend class FastMemoryPool;  // Allow FastMemoryPool to access private members

public:
    /**
     * Allocate a block of memory of the specified size.
     * @param size Size of the memory block to be allocated (1 to 64 bytes).
     * @return Pointer to the allocated memory, or nullptr if allocation fails.
     */
    void *allocate(int size);

    /**
     * Reallocate a block of memory to a new size.
     * @param ptr Pointer to the existing memory block.
     * @param new_size New size for the memory block.
     * @return Pointer to the newly allocated memory, or nullptr if reallocation fails.
     */
    void *reallocate(void *ptr, int new_size);

    /**
     * Deallocate a block of memory and add it to the appropriate subpool's holes.
     * @param ptr Pointer to the memory block to be deallocated.
     * @param size Size of the memory block (1 to 64 bytes).
     */
    void deallocate(void *ptr, int size);

    /**
     * Convert a pointer to an index (offset in bytes from the base address of the memory pool).
     * @param ptr Pointer to convert.
     * @return Index representing the offset from the base address.
     */
    int to_index(void *ptr);

    /**
     * Convert an index (offset in bytes) back to a pointer.
     * @param index Offset in bytes from the base address.
     * @return Pointer corresponding to the given index.
     */
    void *to_pointer(int index);
};

// MemoryPoolClient Implementation

MemoryPoolClient::MemoryPoolClient(FastMemoryPool *src) : source(src) {}

MemoryPoolClient::~MemoryPoolClient() {
    for (int i = 0; i < 64; ++i) {
        for (void *block : pools[i].blocks) {
            source->deallocate(block);
        }
    }
}

void *MemoryPoolClient::allocate(int size) {
    if (size < 1 || size > 64) {
        return nullptr;  // Invalid size, only sizes 1 to 64 bytes are allowed
    }

    int idx = size - 1;  // Map size (1 to 64) to index (0 to 63)
    SubPool &pool = pools[idx];

    // Try to allocate from holes if available
    if (!pool.holes.empty()) {
        void *ptr = pool.holes.front();
        pool.holes.pop_front();
        return ptr;
    }

    // Allocate from FastMemoryPool's pre-allocated memory if no holes are available
    if (pool.next_alloc == nullptr || ((char*)pool.next_alloc - (char*)source->base_addr) >= source->max_size) {
        pool.next_alloc = source->allocate();
        if (!pool.next_alloc) {
            throw std::runtime_error("Out of memory in FastMemoryPool!");
        }
        pool.blocks.push_back(pool.next_alloc);
    }

    // Allocate from the current block in the memory pool
    void *ptr = pool.next_alloc;
    pool.next_alloc = (char*)pool.next_alloc + size;

    return ptr;
}

void *MemoryPoolClient::reallocate(void *ptr, int new_size) {
    deallocate(ptr, new_size);
    return allocate(new_size);
}

void MemoryPoolClient::deallocate(void *ptr, int size) {
    if (size < 1 || size > 64) {
        return;  // Invalid size
    }

    int idx = size - 1;  // Map size (1 to 64) to index (0 to 63)
    pools[idx].holes.push_back(ptr);  // Store the deallocated pointer in the appropriate pool's holes
}

int MemoryPoolClient::to_index(void *ptr) {
    return static_cast<int>((char*)ptr - (char*)source->base_addr);
}

void *MemoryPoolClient::to_pointer(int index) {
    return (char*)source->base_addr + index;
}

// FastMemoryPool Implementation

FastMemoryPool::FastMemoryPool(size_t max_size, size_t chunk_size)
        : max_size(max_size), chunk_size(chunk_size), base_addr(nullptr), next(nullptr) {

    base_addr = malloc(max_size);
    if (!base_addr) {
        throw std::bad_alloc();
    }
    next = base_addr;
}

FastMemoryPool::~FastMemoryPool() {
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

    if (next && ((char*)next - (char*)base_addr) + chunk_size <= max_size) {
        void *allocated = next;
        next = (char*)next + chunk_size;
        return allocated;
    }

    return nullptr;
}

void FastMemoryPool::deallocate(void *ptr) {
    std::lock_guard<std::mutex> lock(guard);
    free_blocks.push_back(ptr);
}

MemoryPoolClient *FastMemoryPool::create_client() {
    return new MemoryPoolClient(this);
}

void FastMemoryPool::free_client(MemoryPoolClient *client) {
    delete client;
}