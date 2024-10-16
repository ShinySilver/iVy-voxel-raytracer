#pragma once

#include <sys/time.h>
#include <cstdint>

inline uint64_t time_us(){
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return 1000000 * tv.tv_sec + tv.tv_usec;
}