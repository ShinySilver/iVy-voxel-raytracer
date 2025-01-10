#include "ivy_log.h"
#include "server/server.h"
#include "server/generators/procedural_generator.h"

namespace server {
    namespace {
        // TODO: Everything :)
    }

    Generator *world_generator;

    void start() {
        if (world_generator == nullptr) world_generator = new ProceduralGenerator();
        info("Server started");
    }

    void stop() {
        delete world_generator;
        info("Server stopped");
    }

    void join() {

    }
}