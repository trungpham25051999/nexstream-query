#pragma once

#include "nexstream/query/application/query_service.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace nexstream::query::infrastructure {

class HttpServer final {
  public:
    HttpServer(application::QueryService& service, std::uint16_t port, std::size_t max_request_bytes = 8192);
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    void run();
    void stop() noexcept;

  private:
    void handle_client(int fd, std::chrono::steady_clock::time_point accepted);
    void worker();
    application::QueryService& service_;
    std::uint16_t port_;
    std::size_t max_request_bytes_;
    std::atomic<bool> stopped_{false};
    int listener_{-1};
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::pair<int,std::chrono::steady_clock::time_point>> pending_;
    std::vector<std::jthread> workers_;
};

}  // namespace nexstream::query::infrastructure
