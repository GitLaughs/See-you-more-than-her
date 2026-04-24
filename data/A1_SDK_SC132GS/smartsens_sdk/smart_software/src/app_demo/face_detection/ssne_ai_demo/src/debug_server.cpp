#include "debug_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

std::string JsonEscape(const std::string& text) {
    std::ostringstream oss;
    for (const unsigned char ch : text) {
        switch (ch) {
            case '\\': oss << "\\\\"; break;
            case '"':  oss << "\\\""; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << ch; break;
        }
    }
    return oss.str();
}

}  // namespace

A1DebugServer::A1DebugServer() = default;

A1DebugServer::~A1DebugServer() {
    Stop();
}

bool A1DebugServer::Start(uint16_t port) {
    if (running_.load()) {
        return true;
    }
    port_ = port;
    running_.store(true);
    worker_ = std::thread(&A1DebugServer::ServerLoop, this);
    return true;
}

void A1DebugServer::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void A1DebugServer::ServerLoop() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::fprintf(stderr, "[WARN] A1 调试测试服务创建 socket 失败: %s\n", std::strerror(errno));
        running_.store(false);
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::fprintf(stderr, "[WARN] A1 调试测试服务绑定端口 %u 失败: %s\n",
                     static_cast<unsigned>(port_), std::strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        running_.store(false);
        return;
    }

    if (listen(server_fd_, 4) < 0) {
        std::fprintf(stderr, "[WARN] A1 调试测试服务 listen 失败: %s\n", std::strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        running_.store(false);
        return;
    }

    std::printf("[INFO] A1 调试测试服务已启动，监听 TCP %u\n", static_cast<unsigned>(port_));

    while (running_.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd_, &readfds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        const int ready = select(server_fd_ + 1, &readfds, nullptr, nullptr, &tv);
        if (!running_.load()) {
            break;
        }
        if (ready <= 0) {
            continue;
        }

        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        const int client_fd = accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && running_.load()) {
                std::fprintf(stderr, "[WARN] A1 调试测试服务 accept 失败: %s\n", std::strerror(errno));
            }
            continue;
        }

        HandleClient(client_fd);
        close(client_fd);
    }

    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    std::printf("[INFO] A1 调试测试服务已停止\n");
}

void A1DebugServer::HandleClient(int client_fd) {
    char buffer[2048];
    std::memset(buffer, 0, sizeof(buffer));
    const ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        return;
    }

    std::string request_text(buffer, static_cast<size_t>(received));
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_request_ = request_text;
    }
    std::printf("[A1_TEST] 收到前端测试指令: %s\n", request_text.c_str());

    const std::string reply = BuildReply(request_text);
    send(client_fd, reply.c_str(), reply.size(), 0);
}

std::string A1DebugServer::BuildReply(const std::string& request_text) const {
    const bool is_test_command =
        request_text.find("test_echo") != std::string::npos ||
        request_text.find("A1_TEST") != std::string::npos ||
        request_text.find("depth_probe_prepare") != std::string::npos;

    std::ostringstream oss;
    if (is_test_command) {
        oss << "{\"success\":true,"
            << "\"message\":\"测试回传成功\","
            << "\"command\":\"test_echo\","
            << "\"echo\":\"" << JsonEscape(request_text) << "\"}\n";
    } else {
        oss << "{\"success\":false,"
            << "\"message\":\"未识别的测试指令\","
            << "\"echo\":\"" << JsonEscape(request_text) << "\"}\n";
    }
    return oss.str();
}
