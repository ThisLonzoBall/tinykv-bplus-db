#include "server/session.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include <array>
#include <string>

#include "protocol/parser.hpp"
#include "protocol/response.hpp"

namespace db::server {

namespace {

// Strips a trailing '\r' so the protocol works whether clients send "\n" or
// "\r\n" line endings.
std::string strip_trailing_cr(std::string line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
}

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

} // namespace

void run_session(int client_fd, storage::KvStore& store) {
    std::string buffer;
    std::array<char, 4096> chunk{};

    while (true) {
        ssize_t n = ::recv(client_fd, chunk.data(), chunk.size(), 0);
        if (n <= 0) {
            break; // client closed the connection or an error occurred
        }
        buffer.append(chunk.data(), static_cast<std::size_t>(n));

        std::size_t newline_pos;
        while ((newline_pos = buffer.find('\n')) != std::string::npos) {
            std::string line = strip_trailing_cr(buffer.substr(0, newline_pos));
            buffer.erase(0, newline_pos + 1);

            if (line.empty()) {
                continue;
            }

            protocol::Command command = protocol::parse_command(line);
            std::string response = protocol::handle_command(command, store);
            if (!send_line(client_fd, response)) {
                ::close(client_fd);
                return;
            }
        }
    }

    ::close(client_fd);
}

} // namespace db::server
