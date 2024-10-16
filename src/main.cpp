#include <iostream>
#include "client/client.h"
#include "server/server.h"

int main() {
    server::start();
    client::start();
    server::stop();
    server::join();
    return 0;
}
