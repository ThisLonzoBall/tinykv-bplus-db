#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

bool send_line(int fd, const std::string& line) {
    std::string out = line + "\n";
    std::size_t sent = 0;
    while (sent < out.size()) {
        ssize_t n = ::send(fd, out.data() + sent, out.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

// Reads exactly one newline-terminated response line from the server.
// Safe to call once per request: the protocol is strictly request/response,
// so the server never sends more than one line before the next command.
bool recv_line(int fd, std::string& out_line) {
    std::array<char, 4096> chunk{};
    out_line.clear();
    while (true) {
        auto newline_pos = out_line.find('\n');
        if (newline_pos != std::string::npos) {
            out_line.erase(newline_pos); // keep only up to the newline
            return true;
        }
        ssize_t n = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (n <= 0) {
            return false;
        }
        out_line.append(chunk.data(), static_cast<std::size_t>(n));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    const char* host = argv[1];
    int port = std::atoi(argv[2]);

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "failed to create socket: " << std::strerror(errno) << "\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        std::cerr << "invalid host: " << host << "\n";
        return 1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "failed to connect to " << host << ":" << port << ": " << std::strerror(errno) << "\n";
        return 1;
    }

    std::cout << "connected to " << host << ":" << port << " (Ctrl-D to quit)\n";

    std::string command;
    std::string response;
    while (std::getline(std::cin, command)) {
        if (command.empty()) {
            continue;
        }
        if (!send_line(fd, command)) {
            std::cerr << "connection closed\n";
            break;
        }
        if (!recv_line(fd, response)) {
            std::cerr << "connection closed\n";
            break;
        }
        std::cout << response << "\n";
    }

    ::close(fd);
    return 0;
}
