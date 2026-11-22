#pragma once

// Connection layer between sockets and the room: hello/ping, one socket per
// phone, idle close, and view fan-out. Transport-agnostic: the firmware drives
// it from the httpd WebSocket handler (ws_hub.cpp), the host simulator from
// stdin/stdout. Single-threaded like Room.

#include <cstddef>
#include <cstdint>
#include <string>

#include "room.h"

class Transport {
 public:
  virtual ~Transport() = default;
  virtual void send(int conn, const char* text, size_t len) = 0;
  // Asks the transport to close the socket; it later reports Hub::closed().
  virtual void close(int conn) = 0;
};

class Hub {
 public:
  static constexpr int kMaxConns = 24;  // >= httpd max_open_sockets
  static constexpr size_t kMaxFrame = 1024;
  static constexpr uint32_t kIdleCloseMs = 15000;

  Hub(Room& room, Transport& transport) : room_(room), transport_(transport) {}

  // A WebSocket finished its handshake. False if the table is full.
  bool opened(int conn, uint32_t now);
  void text(int conn, const char* data, size_t len, uint32_t now);
  void closed(int conn, uint32_t now);
  // Call about every 250 ms: idle close, game timers.
  void tick(uint32_t now);

  int connCount() const;
  int boundCount() const;  // connections that said hello

 private:
  struct Conn {
    int fd = -1;
    char id[kClientIdMax + 1] = {};
    uint32_t lastSeen = 0;
    uint32_t sentVersion = 0;
  };

  Conn* find(int fd);
  void hello(Conn& c, const json::Value& msg, uint32_t now);
  void flush(uint32_t now);

  Room& room_;
  Transport& transport_;
  Conn conns_[kMaxConns];
  std::string buf_;
};
