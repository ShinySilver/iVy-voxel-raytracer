#include "server.h"
#include "features/generators/debug_generator.h"
#include "features/generators/procedural_generator.h"
#include "../common/log.h"
#include "../common/time.h"

// TODO: Everything :)

void server::start() {
    if (world_generator == nullptr) {
        auto t0 = time_us();
        //world_generator = new DebugGenerator();
        world_generator = new ProceduralGenerator();
    }
    info("Server started");
}

void server::stop() {
    delete world_generator;
    info("Server stopped");
}

void server::join() {

}