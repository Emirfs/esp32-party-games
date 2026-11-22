#pragma once

// GameHost for tests: records the timer, phase changes and buzzes; the test
// fires the timer by hand. Shared by test_vampires and test_vampires_sim.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/game.h"
#include "core/json.h"

struct FakeHost : GameHost {
  explicit FakeHost(uint64_t seed = 1) : rng_(seed) {}

  Rng& rng() override { return rng_; }
  void setTimer(uint32_t ms) override {
    timer = ms;
    ++timerSets;
  }
  void phaseChanged() override { ++phaseChanges; }
  void buzz(PlayerMask players, const char* pattern) override { buzzes.push_back({players, pattern}); }
  bool online(int pid) const override { return !hasPid(offline, pid); }

  // Fires the pending timer the way the room does; false if none is pending.
  bool fire(Game& game) {
    if (timer == 0) return false;
    timer = 0;
    game.onTimer();
    return true;
  }

  struct Buzz {
    PlayerMask players;
    std::string pattern;
  };

  Rng rng_;
  uint32_t timer = 0;  // pending timer (ms), 0 = none
  int timerSets = 0;
  int phaseChanges = 0;
  PlayerMask offline = 0;
  std::vector<Buzz> buzzes;
};

// Sends {"t":t,"phaseId":phaseId[,"target":target]} as player pid.
inline const char* sendMsg(Game& game, int pid, const char* t, long long phaseId, int target, bool withTarget = true) {
  char buf[96];
  if (withTarget)
    snprintf(buf, sizeof buf, "{\"t\":\"%s\",\"phaseId\":%lld,\"target\":%d}", t, phaseId, target);
  else
    snprintf(buf, sizeof buf, "{\"t\":\"%s\",\"phaseId\":%lld}", t, phaseId);
  json::Doc doc;
  if (!doc.parse(buf, strlen(buf))) return "test: bad json";
  return game.onAction(pid, doc.root());
}

inline std::string privateViewOf(const Game& game, int pid) {
  std::string out;
  json::Writer w(out);
  game.privateView(pid, w);
  return out;
}

inline std::string boardViewOf(const Game& game) {
  std::string out;
  json::Writer w(out);
  game.boardView(w);
  return out;
}
