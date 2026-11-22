#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Persistent key -> blob storage. The firmware implements it on NVS
// (nvs_store.cpp), the host simulator on files, tests in memory. Keys are at
// most 15 characters (NVS limit).
class Store {
 public:
  virtual ~Store() = default;
  // Copies the blob into buf; returns its size, or 0 if absent or larger than cap.
  virtual size_t load(const char* key, void* buf, size_t cap) = 0;
  virtual void save(const char* key, const void* data, size_t len) = 0;
  virtual void erase(const char* key) = 0;
};

class MemStore : public Store {
 public:
  size_t load(const char* key, void* buf, size_t cap) override {
    auto it = blobs_.find(key);
    if (it == blobs_.end() || it->second.size() > cap) return 0;
    if (!it->second.empty()) memcpy(buf, it->second.data(), it->second.size());
    return it->second.size();
  }
  void save(const char* key, const void* data, size_t len) override {
    const auto* p = static_cast<const uint8_t*>(data);
    blobs_[key].assign(p, p + len);
    ++writes;
  }
  void erase(const char* key) override { blobs_.erase(key); }

  int writes = 0;

 private:
  std::map<std::string, std::vector<uint8_t>> blobs_;
};
