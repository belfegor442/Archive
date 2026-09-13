#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace monix {

struct WsConfig {
  int port = 8421;
  int maxClients = 16;
};

class WebSocketServer {
 public:
  using MessageHandler = std::function<void(const std::string& message)>;

  explicit WebSocketServer(WsConfig config = {}) : config_(config) {}
  ~WebSocketServer() { Stop(); }

  bool Start() {
    if (running_.load()) return true;
    listenFd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenFd_ == INVALID_SOCKET) return false;

    int yes = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

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
    acceptThread_ = std::thread(&WebSocketServer::AcceptLoop, this);
    return true;
  }

  void Stop() {
    running_.store(false);
    if (listenFd_ != INVALID_SOCKET) {
      closesocket(listenFd_);
      listenFd_ = INVALID_SOCKET;
    }
    {
      std::lock_guard lock(clientsMutex_);
      for (auto& c : clients_) closesocket(c.fd);
      clients_.clear();
    }
    if (acceptThread_.joinable()) acceptThread_.join();
  }

  void Broadcast(const std::string& json) {
    std::lock_guard lock(clientsMutex_);
    auto frame = EncodeTextFrame(json);
    for (auto it = clients_.begin(); it != clients_.end();) {
      if (send(it->fd, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0) == SOCKET_ERROR) {
        closesocket(it->fd);
        it = clients_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void BroadcastBinary(const uint8_t* data, size_t len) {
    std::lock_guard lock(clientsMutex_);
    auto frame = EncodeBinaryFrame(data, len);
    for (auto it = clients_.begin(); it != clients_.end();) {
      if (send(it->fd, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0) == SOCKET_ERROR) {
        closesocket(it->fd);
        it = clients_.erase(it);
      } else {
        ++it;
      }
    }
  }

  int ClientCount() const {
    std::lock_guard lock(clientsMutex_);
    return static_cast<int>(clients_.size());
  }

  bool IsRunning() const { return running_.load(); }
  int Port() const { return config_.port; }

  void SetOnMessage(MessageHandler handler) { onMessage_ = std::move(handler); }

 private:
  struct Client {
    SOCKET fd;
    std::string pendingBuffer;
  };

  static constexpr const char* kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

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

      {
        std::lock_guard lock(clientsMutex_);
        if (static_cast<int>(clients_.size()) >= config_.maxClients) {
          closesocket(clientFd);
          continue;
        }
      }

      if (!PerformHandshake(clientFd)) {
        closesocket(clientFd);
        continue;
      }

      {
        std::lock_guard lock(clientsMutex_);
        clients_.push_back({clientFd, {}});
      }

      std::thread(&WebSocketServer::ReadLoop, this, clientFd).detach();
    }
  }

  bool PerformHandshake(SOCKET fd) {
    char buf[4096]{};
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;

    std::string request(buf, n);
    auto keyPos = request.find("Sec-WebSocket-Key: ");
    if (keyPos == std::string::npos) return false;
    keyPos += 19;
    auto keyEnd = request.find("\r\n", keyPos);
    if (keyEnd == std::string::npos) return false;
    std::string key = request.substr(keyPos, keyEnd - keyPos);

    std::string acceptKey = ComputeAcceptKey(key);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
        "\r\n";
    return send(fd, response.c_str(), static_cast<int>(response.size()), 0) != SOCKET_ERROR;
  }

  void ReadLoop(SOCKET fd) {
    while (running_.load()) {
      fd_set readSet;
      FD_ZERO(&readSet);
      FD_SET(fd, &readSet);

      timeval tv{1, 0};
      int sel = select(0, &readSet, nullptr, nullptr, &tv);
      if (sel <= 0) continue;

      uint8_t header[2]{};
      int n = recv(fd, reinterpret_cast<char*>(header), 2, 0);
      if (n <= 0) break;

      bool fin = (header[0] & 0x80) != 0;
      uint8_t opcode = header[0] & 0x0F;
      bool masked = (header[1] & 0x80) != 0;
      uint64_t payloadLen = header[1] & 0x7F;

      if (payloadLen == 126) {
        uint8_t ext[2]{};
        if (recv(fd, reinterpret_cast<char*>(ext), 2, MSG_WAITALL) != 2) break;
        payloadLen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
      } else if (payloadLen == 127) {
        uint8_t ext[8]{};
        if (recv(fd, reinterpret_cast<char*>(ext), 8, MSG_WAITALL) != 8) break;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i)
          payloadLen = (payloadLen << 8) | ext[i];
      }

      uint8_t maskKey[4]{};
      if (masked) {
        if (recv(fd, reinterpret_cast<char*>(maskKey), 4, MSG_WAITALL) != 4) break;
      }

      if (payloadLen > 1048576) break;

      std::vector<uint8_t> payload(payloadLen);
      if (payloadLen > 0) {
        if (recv(fd, reinterpret_cast<char*>(payload.data()),
                 static_cast<int>(payloadLen), MSG_WAITALL) !=
            static_cast<int>(payloadLen))
          break;
      }

      if (masked) {
        for (uint64_t i = 0; i < payloadLen; ++i)
          payload[i] ^= maskKey[i % 4];
      }

      if (opcode == 0x01) {
        std::string msg(reinterpret_cast<char*>(payload.data()), payload.size());
        if (onMessage_) onMessage_(msg);
      } else if (opcode == 0x08) {
        uint8_t closeFrame[2] = {0x88, 0x00};
        send(fd, reinterpret_cast<char*>(closeFrame), 2, 0);
        break;
      } else if (opcode == 0x09) {
        std::vector<uint8_t> pong(payloadLen + 2);
        pong[0] = 0x8A;
        pong[1] = static_cast<uint8_t>(payloadLen);
        if (payloadLen > 0) std::memcpy(pong.data() + 2, payload.data(), payloadLen);
        send(fd, reinterpret_cast<const char*>(pong.data()), static_cast<int>(pong.size()), 0);
      }
    }

    closesocket(fd);
    std::lock_guard lock(clientsMutex_);
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
      if (it->fd == fd) { clients_.erase(it); break; }
    }
  }

  static std::string ComputeAcceptKey(const std::string& key) {
    std::string concat = key + kWsGuid;
    uint8_t hash[20]{};
    Sha1(reinterpret_cast<const uint8_t*>(concat.data()), concat.size(), hash);
    return Base64Encode(hash, 20);
  }

  static void Sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE,
             h3 = 0x10325476, h4 = 0xC3D2E1F0;

    auto rl32 = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };

    std::vector<uint8_t> msg(data, data + len);
    uint64_t bitLen = static_cast<uint64_t>(len) * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i) msg.push_back(static_cast<uint8_t>(bitLen >> (i * 8)));

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
      uint32_t w[80]{};
      for (int i = 0; i < 16; ++i)
        w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
               (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
               static_cast<uint32_t>(msg[chunk + i * 4 + 3]);
      for (int i = 16; i < 80; ++i)
        w[i] = rl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

      uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
      for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        uint32_t temp = rl32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rl32(b, 30); b = a; a = temp;
      }
      h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    auto put32 = [&](int off, uint32_t v) {
      out[off]     = static_cast<uint8_t>(v >> 24);
      out[off + 1] = static_cast<uint8_t>(v >> 16);
      out[off + 2] = static_cast<uint8_t>(v >> 8);
      out[off + 3] = static_cast<uint8_t>(v);
    };
    put32(0, h0); put32(4, h1); put32(8, h2); put32(12, h3); put32(16, h4);
  }

  static std::string Base64Encode(const uint8_t* data, size_t len) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
      uint32_t n = static_cast<uint32_t>(data[i]) << 16;
      if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
      if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);
      out.push_back(table[(n >> 18) & 0x3F]);
      out.push_back(table[(n >> 12) & 0x3F]);
      out.push_back((i + 1 < len) ? table[(n >> 6) & 0x3F] : '=');
      out.push_back((i + 2 < len) ? table[n & 0x3F] : '=');
    }
    return out;
  }

  static std::string EncodeTextFrame(const std::string& payload) {
    std::string frame;
    frame.push_back(0x81);
    EncodePayloadLength(frame, payload.size());
    frame.append(payload);
    return frame;
  }

  static std::string EncodeBinaryFrame(const uint8_t* data, size_t len) {
    std::string frame;
    frame.push_back(0x82);
    EncodePayloadLength(frame, len);
    frame.append(reinterpret_cast<const char*>(data), len);
    return frame;
  }

  static void EncodePayloadLength(std::string& frame, size_t len) {
    if (len < 126) {
      frame.push_back(static_cast<char>(len));
    } else if (len <= 0xFFFF) {
      frame.push_back(126);
      frame.push_back(static_cast<char>((len >> 8) & 0xFF));
      frame.push_back(static_cast<char>(len & 0xFF));
    } else {
      frame.push_back(127);
      for (int i = 7; i >= 0; --i)
        frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
    }
  }

  WsConfig config_;
  std::atomic<bool> running_{false};
  SOCKET listenFd_ = INVALID_SOCKET;
  std::thread acceptThread_;
  mutable std::mutex clientsMutex_;
  std::vector<Client> clients_;
  MessageHandler onMessage_;
};

} // namespace monix
