// Host simulator: the box's core/ and games/ on a PC, without Wi-Fi or HTTP.
// web/scripts/sim.mjs owns the WebSockets and talks to this process in lines:
//   stdin:  O <conn>          a socket opened
//           T <conn> <json>   a text frame from that socket
//           C <conn>          the socket closed
//   stdout: S <conn> <json>   send a text frame
//           X <conn>          close the socket
//           R                 the admin asked for a restart; this process exits
// State persists in a directory of files (default .sim-store), so killing and
// restarting the process behaves like a power cut on the box.

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <sys/stat.h>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#endif

#include "core/hub.h"
#include "core/room.h"

namespace {

class FileStore : public Store {
 public:
  explicit FileStore(std::string dir) : dir_(std::move(dir)) {
#ifdef _WIN32
    _mkdir(dir_.c_str());
#else
    mkdir(dir_.c_str(), 0755);
#endif
  }

  size_t load(const char* key, void* buf, size_t cap) override {
    std::ifstream f(path(key), std::ios::binary);
    if (!f) return 0;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() > cap) return 0;
    memcpy(buf, data.data(), data.size());
    return data.size();
  }

  void save(const char* key, const void* data, size_t len) override {
    std::ofstream f(path(key), std::ios::binary | std::ios::trunc);
    f.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
  }

  void erase(const char* key) override { std::remove(path(key).c_str()); }

 private:
  std::string path(const char* key) const { return dir_ + "/" + key + ".bin"; }
  std::string dir_;
};

class SimPlatform : public Platform {
 public:
  explicit SimPlatform(Store& store) {
    char pass[kWifiPassMax + 1] = {};
    if (store.load(kKeyWifiPass, pass, sizeof pass) > 0 && pass[kWifiPassMax] == '\0') pass_ = pass;
  }
  const char* ssid() const override { return "PartyGames-SIM"; }
  const char* wifiPassword() const override { return pass_.c_str(); }
  void restart() override {
    std::fputs("R\n", stdout);
    std::fflush(stdout);
    std::exit(0);
  }

 private:
  std::string pass_ = "partygames";
};

class StdoutTransport : public Transport {
 public:
  void send(int conn, const char* text, size_t len) override {
    std::fprintf(stdout, "S %d ", conn);
    std::fwrite(text, 1, len, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
  }
  void close(int conn) override {
    std::fprintf(stdout, "X %d\n", conn);
    std::fflush(stdout);
  }
};

std::mutex g_mutex;
std::condition_variable g_cv;
std::deque<std::string> g_lines;
bool g_eof = false;

void readStdin() {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_lines.push_back(std::move(line));
    g_cv.notify_one();
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  g_eof = true;
  g_cv.notify_one();
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  std::string dir = ".sim-store";
  uint64_t seed = std::random_device{}() | static_cast<uint64_t>(std::random_device{}()) << 32;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (std::strcmp(argv[i], "--store") == 0) dir = argv[i + 1];
    else if (std::strcmp(argv[i], "--seed") == 0) seed = std::strtoull(argv[i + 1], nullptr, 10);
  }

  FileStore store(dir);
  SimPlatform platform(store);
  StdoutTransport transport;
  Room room(store, platform, seed);
  Hub hub(room, transport);

  const auto start = std::chrono::steady_clock::now();
  auto now = [&] {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
  };
  room.begin(now());
  std::fprintf(stderr, "host: store %s, ready\n", dir.c_str());

  std::thread(readStdin).detach();
  uint32_t lastTick = 0;
  for (;;) {
    std::deque<std::string> lines;
    {
      std::unique_lock<std::mutex> lock(g_mutex);
      g_cv.wait_for(lock, std::chrono::milliseconds(100), [] { return !g_lines.empty() || g_eof; });
      lines.swap(g_lines);
      if (lines.empty() && g_eof) return 0;
    }
    for (const std::string& line : lines) {
      if (line.size() < 3) continue;
      char* rest = nullptr;
      const int conn = static_cast<int>(std::strtol(line.c_str() + 2, &rest, 10));
      switch (line[0]) {
        case 'O':
          if (!hub.opened(conn, now())) transport.close(conn);
          break;
        case 'T':
          if (rest != nullptr && *rest == ' ') ++rest;
          if (rest != nullptr) hub.text(conn, rest, std::strlen(rest), now());
          break;
        case 'C':
          hub.closed(conn, now());
          break;
        default:
          break;
      }
    }
    const uint32_t t = now();
    if (t - lastTick >= 250) {
      lastTick = t;
      hub.tick(t);
    }
  }
}
