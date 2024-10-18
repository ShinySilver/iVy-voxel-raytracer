#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <deque>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <sys/param.h>

class MemoryPoolClient;

class FastMemoryPool {
private:
    void *base_addr;  // Base address of the large memory block
    void *next;       // Pointer to the next free address in the pool
    size_t max_size;  // Maximum size of the memory pool
    size_t chunk_size;
private:
    // Size of each chunk within the pool
    size_t allocated_blocks;  // Total number of allocated blocks
    std::mutex guard;  // Mutex for thread-safe operations
    std::deque<void *> free_blocks;  // Deque to store deallocated blocks for reuse
    std::vector<MemoryPoolClient *> clients;  // List of all created clients

    /**
     * Allocate a chunk of memory from the pool.
     * @return A pointer to the allocated memory or nullptr if out of memory.
     */
    void *allocate();

    /**
     * Deallocate a chunk of memory, adding it back to the pool's free blocks.
     * @param ptr Pointer to the memory to deallocate.
     */
    void deallocate(void *ptr);

    friend class MemoryPoolClient;

public:
    /**
     * Constructor for FastMemoryPool.
     * @param max_size Maximum size of the memory pool.
     * @param chunk_size Size of each chunk to allocate.
     */
    FastMemoryPool(size_t max_size = 2ULL * 1024 * 1024 * 1024, size_t chunk_size = 12 * 1024);

    /**
     * Destructor for FastMemoryPool. Frees all allocated clients and the main memory block.
     */
    ~FastMemoryPool();

    /**
     * Create a new MemoryPoolClient associated with this pool.
     * @return A pointer to the newly created MemoryPoolClient.
     */
    MemoryPoolClient *create_client();

    /**
     * Free a MemoryPoolClient, deallocating its memory.
     * @param client Pointer to the client to free.
     */
    void free_client(MemoryPoolClient *client);

    /**
     * @return The size of the memory pool (in bytes).
     */
    size_t size();

    /**
     * @return The total allocated memory (in bytes, allocations made by clients in the memory pool).
     */
    size_t allocated();

    /**
     * @return The actual used memory (in bytes, excluding holes and unused memory).
     */
    size_t used();

    size_t get_chunk_size() const;

    void *to_pointer(uint32_t index);
    uint32_t to_index(void *ptr);
};

class MemoryPoolClient {
private:
    struct SubPool {
        void *next_alloc, *current_chunk;  // Pointer to the next allocation point within this subpool
        std::deque<void *> holes;  // Deque to store deallocated blocks for reuse
        std::vector<void *> blocks;  // Vector to store allocated blocks
        size_t allocated;

        SubPool() : next_alloc(nullptr), current_chunk(nullptr), holes(), blocks(), allocated() {}
        ~SubPool() = default;
    };

    FastMemoryPool *source;  // Source memory pool
    SubPool pools[64 + 64 - 5];  // Array of subpools for different allocation sizes (1 to 64 bytes or multiples of 12 bytes up to 64*12)

    MemoryPoolClient(FastMemoryPool *src);
    ~MemoryPoolClient();

    friend class FastMemoryPool;

public:
    /**
     * Allocate a memory block of the specified size.
     * @param size Size of the memory block to allocate (1 to 64 bytes or multiples of 12 bytes up to 64*12).
     * @return A pointer to the allocated memory block.
     */
    void *allocate(int size);

    /**
     * Deallocate a memory block of the specified size.
     * @param ptr Pointer to the memory block to deallocate.
     * @param size Size of the memory block being deallocated.
     */
    void deallocate(void *ptr, int size);

    /**
     * Convert a pointer to an index relative to the base address of the pool.
     * @param ptr Pointer to convert.
     * @return Index relative to the base address.
     */
    uint32_t to_index(void *ptr);

    /**
     * Convert an index to a pointer relative to the base address of the pool.
     * @param index Index to convert.
     * @return Pointer relative to the base address.
     */
    void *to_pointer(uint32_t index);

    /**
     * Get the amount of memory currently used by this client. Always inferior or equals to the total memory allocated to this client.
     * @return The size of the used memory in bytes.
     */
    size_t get_used_memory() const;
};