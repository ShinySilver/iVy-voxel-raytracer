#pragma once

#include <cstdint>
#include "../../../../libraries/ivy_util/include/ivy_log.h"

// TODO: Multithreaded access through the MemoryAccess class
// TODO: Multiple size support in ... VoxelPool class? And a NodePool specialization?

template<typename T>
class MemoryPool {
private:
    T *data, *next_available;
    size_t max_size;
public:
    MemoryPool();
    MemoryPool(T *allocation, size_t max_size);
    ~MemoryPool();
    T *allocate();
    T *allocate(T &);
    T *allocate(T);
    void deallocate(T *p);
    size_t size();
    uint32_t to_index(T *p);
    T *to_pointer(uint32_t p);
};

template<typename T>
MemoryPool<T>::MemoryPool(): data{(T *) std::aligned_alloc(32, 1073741824)},
                             next_available{data},
                             max_size{1073741824} {}

template<typename T>
MemoryPool<T>::MemoryPool(T *allocation, size_t max_size): data{(T *) allocation},
                                                           next_available{data},
                                                           max_size{max_size} {}

template<typename T>
MemoryPool<T>::~MemoryPool() { free(data); }

template<typename T>
T *MemoryPool<T>::allocate() {
    return next_available++;
}

template<typename T>
T *MemoryPool<T>::allocate(T &value) {
    *next_available = value;
    return next_available++;
}

template<typename T>
T *MemoryPool<T>::allocate(T value) {
    *next_available = value;
    return next_available++;
}

template<typename T>
void MemoryPool<T>::deallocate(T *p) {
    fatal("Function not implemented.");
}

template<typename T>
uint32_t MemoryPool<T>::to_index(T *p) {
    return p - data;
}

template<typename T>
T *MemoryPool<T>::to_pointer(uint32_t p) {
    return data + p;
}

template<typename T>
size_t MemoryPool<T>::size() {
    return next_available - data;
}

