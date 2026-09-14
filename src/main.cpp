#include <cstdlib>
#include <iostream>

#include "server/server.hpp"
#include "storage/kv_store.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "invalid port: " << argv[1] << "\n";
        return 1;
    }

    db::storage::KvStore store;
    return db::server::run_server(static_cast<std::uint16_t>(port), store);
}
