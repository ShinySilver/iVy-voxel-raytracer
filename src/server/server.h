#pragma once

#include "server/generators/generator.h"

namespace server {
    extern Generator *world_generator;
    void start();
    void stop();
    void join();
}
