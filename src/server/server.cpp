#include "server/server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>

#include "server/session.hpp"

namespace db::server {

int run_server(std::uint16_t port, storage::KvStore& store) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "failed to create socket: " << std::strerror(errno) << "\n";
        return 1;
    }

    int reuse = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "failed to bind port " << port << ": " << std::strerror(errno) << "\n";
        ::close(listen_fd);
        return 1;
    }

    if (::listen(listen_fd, /*backlog=*/64) < 0) {
        std::cerr << "failed to listen: " << std::strerror(errno) << "\n";
        ::close(listen_fd);
        return 1;
    }

    std::cout << "db_server listening on port " << port << "\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << "\n";
            continue;
        }

        std::thread(&run_session, client_fd, std::ref(store)).detach();
    }
}

} // namespace db::server
