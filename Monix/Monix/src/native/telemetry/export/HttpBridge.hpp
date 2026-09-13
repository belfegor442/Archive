#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace monix {

class HttpBridge {
 public:
  using StatusProvider = std::function<std::string()>;
  using HistoryProvider = std::function<std::string()>;

  struct Config {
    int port = 8422;
  };

  explicit HttpBridge(Config config = {}) : config_(config) {}
  ~HttpBridge() { Stop(); }

  bool Start() {
    if (running_.load()) return true;
    listenFd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenFd_ == INVALID_SOCKET) return false;

    int yes = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(config_.port));

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
      closesocket(listenFd_);
      listenFd_ = INVALID_SOCKET;
      return false;
    }
    if (listen(listenFd_, 4) == SOCKET_ERROR) {
      closesocket(listenFd_);
      listenFd_ = INVALID_SOCKET;
      return false;
    }

    running_.store(true);
    acceptThread_ = std::thread(&HttpBridge::AcceptLoop, this);
    return true;
  }

  void Stop() {
    running_.store(false);
    if (listenFd_ != INVALID_SOCKET) {
      closesocket(listenFd_);
      listenFd_ = INVALID_SOCKET;
    }
    if (acceptThread_.joinable()) acceptThread_.join();
  }

  bool IsRunning() const { return running_.load(); }
  int Port() const { return config_.port; }

  void SetStatusProvider(StatusProvider provider) { statusProvider_ = std::move(provider); }
  void SetHistoryProvider(HistoryProvider provider) { historyProvider_ = std::move(provider); }

 private:
  void AcceptLoop() {
    while (running_.load()) {
      fd_set readSet;
      FD_ZERO(&readSet);
      FD_SET(listenFd_, &readSet);

      timeval tv{0, 100000};
      int sel = select(0, &readSet, nullptr, nullptr, &tv);
      if (sel <= 0) continue;

      sockaddr_in clientAddr{};
      int addrLen = sizeof(clientAddr);
      SOCKET clientFd = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
      if (clientFd == INVALID_SOCKET) continue;

      std::thread(&HttpBridge::HandleRequest, this, clientFd).detach();
    }
  }

  void HandleRequest(SOCKET fd) {
    char buf[4096]{};
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { closesocket(fd); return; }

    std::string request(buf, n);
    std::string path = ExtractPath(request);
    std::string body;
    int statusCode = 200;
    std::string contentType = "application/json";

    if (path == "/status" || path == "/") {
      body = statusProvider_ ? statusProvider_() : R"({"status":"ok"})";
    } else if (path == "/history") {
      body = historyProvider_ ? historyProvider_() : R"({"events":[]})";
    } else if (path == "/health") {
      body = R"({"status":"healthy","uptime":"ok"})";
    } else {
      statusCode = 404;
      body = R"({"error":"not_found"})";
    }

    std::string statusText = (statusCode == 200) ? "OK" : "Not Found";
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    std::string resp = response.str();
    send(fd, resp.c_str(), static_cast<int>(resp.size()), 0);
    closesocket(fd);
  }

  static std::string ExtractPath(const std::string& request) {
    auto methodEnd = request.find(' ');
    if (methodEnd == std::string::npos) return "/";
    auto pathEnd = request.find(' ', methodEnd + 1);
    if (pathEnd == std::string::npos) return "/";
    return request.substr(methodEnd + 1, pathEnd - methodEnd - 1);
  }

  Config config_;
  std::atomic<bool> running_{false};
  SOCKET listenFd_ = INVALID_SOCKET;
  std::thread acceptThread_;
  StatusProvider statusProvider_;
  HistoryProvider historyProvider_;
};

} // namespace monix
