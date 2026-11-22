#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

// Small deterministic PRNG (xorshift64*). The firmware seeds it from the
// hardware RNG; tests and the bot simulator seed it with fixed values.
class Rng {
 public:
  explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) { reseed(seed); }

  void reseed(uint64_t seed) { s_ = seed != 0 ? seed : 0x9E3779B97F4A7C15ull; }

  uint32_t next() {
    s_ ^= s_ >> 12;
    s_ ^= s_ << 25;
    s_ ^= s_ >> 27;
    return static_cast<uint32_t>((s_ * 0x2545F4914F6CDD1Dull) >> 32);
  }

  // Uniform in [0, n) without modulo bias; 0 when n == 0.
  uint32_t below(uint32_t n) {
    if (n == 0) return 0;
    const uint32_t limit = UINT32_MAX - UINT32_MAX % n;
    uint32_t r;
    do r = next();
    while (r >= limit);
    return r % n;
  }

  template <typename T>
  void shuffle(T* items, size_t n) {
    for (size_t i = n; i > 1; --i) std::swap(items[i - 1], items[below(static_cast<uint32_t>(i))]);
  }

 private:
  uint64_t s_;
};
