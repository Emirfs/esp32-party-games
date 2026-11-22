#pragma once

#include "core/store.h"

// core Store on the NVS namespace "pg" (nvs partition, see partitions.csv).
class NvsStore : public Store {
 public:
  size_t load(const char* key, void* buf, size_t cap) override;
  void save(const char* key, const void* data, size_t len) override;
  void erase(const char* key) override;
};
