#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class A1DebugServer {
  public:
    A1DebugServer();
    ~A1DebugServer();

    bool Start(uint16_t port);
    void Stop();

    bool running() const { return running_.load(); }
    uint16_t port() const { return port_; }

  private:
    void ServerLoop();
    void HandleClient(int client_fd);
    std::string BuildReply(const std::string& request_text) const;

    std::atomic<bool> running_{false};
    std::thread worker_;
    mutable std::mutex state_mutex_;
    int server_fd_ = -1;
    uint16_t port_ = 0;
    std::string last_request_;
};
