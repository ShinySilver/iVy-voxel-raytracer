#include "server.h"
#include "generators/debug_generator.h"
#include "generators/procedural_generator.h"
#include "ivy_log.h"

// TODO: Everything :)

void server::start() {
    if (world_generator == nullptr) {
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