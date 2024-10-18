#include "gtest/gtest.h"
#include "common/utils/memory_pool.h"


TEST(MemoryPoolClientIndividualTest, AllocateAndCheckMemorySize12) {
    FastMemoryPool pool;  // Create a unique FastMemoryPool instance
    MemoryPoolClient *client = pool.create_client();  // Create a unique MemoryPoolClient instance

    // Allocate memory of size 12 bytes
    void *ptr = client->allocate(12);
    ASSERT_NE(ptr, nullptr) << "Failed to allocate memory of size 12";

    // Check that the pool allocated one chunk for this allocation size
    EXPECT_EQ(pool.allocated(), pool.get_chunk_size());  // Full chunk allocated
    EXPECT_EQ(client->get_used_memory(), 12);        // Check client usage

    // Deallocate the memory
    client->deallocate(ptr, 12);
}

TEST(MemoryPoolClientIndividualTest, ReuseDeallocatedMemorySize12) {
    FastMemoryPool pool;  // Create a unique FastMemoryPool instance
    MemoryPoolClient *client = pool.create_client();  // Create a unique MemoryPoolClient instance

    // Allocate memory of size 12 bytes
    void *ptr = client->allocate(12);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(pool.allocated(), pool.get_chunk_size());  // Full chunk allocated
    EXPECT_EQ(client->get_used_memory(), 12);

    // Deallocate memory
    client->deallocate(ptr, 12);

    // Reallocate the same size to check for reuse
    void *reused_ptr = client->allocate(12);
    ASSERT_NE(reused_ptr, nullptr);
    EXPECT_EQ(pool.allocated(), pool.get_chunk_size());  // Full chunk allocated
    EXPECT_EQ(client->get_used_memory(), 12);

    // Deallocate the reused memory
    client->deallocate(reused_ptr, 12);
}

TEST(MemoryPoolClientIndividualTest, CheckMemoryUsageAfterDeallocationSize12) {
    FastMemoryPool pool;  // Create a unique FastMemoryPool instance
    MemoryPoolClient *client = pool.create_client();  // Create a unique MemoryPoolClient instance

    // Allocate memory of size 12 bytes
    void *ptr = client->allocate(12);
    ASSERT_NE(ptr, nullptr) << "Failed to allocate memory of size 12";

    // Check that the pool allocated one chunk for this allocation size
    EXPECT_EQ(pool.allocated(), pool.get_chunk_size());  // Full chunk allocated
    EXPECT_EQ(client->get_used_memory(), 12);        // Check client usage

    // Deallocate memory
    client->deallocate(ptr, 12);

    // Ensure that the memory usage reports zero after deallocation
    EXPECT_EQ(client->get_used_memory(), 0);  // Client should report no used memory
}

TEST(MemoryPoolClientIndividualTest, MemoryPoolClientDestruction) {
    FastMemoryPool pool;  // Create a unique FastMemoryPool instance
    MemoryPoolClient *client = pool.create_client();  // Create a unique MemoryPoolClient instance
    size_t initial_used = pool.used();

    MemoryPoolClient *temp_client = pool.create_client();
    void *ptr = temp_client->allocate(12);  // Allocate size 12
    ASSERT_NE(ptr, nullptr);
    pool.free_client(temp_client);

    // After temp_client is destroyed, there should be no change in the allocated memory
    EXPECT_EQ(pool.used(), initial_used);
}