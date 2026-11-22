#pragma once

// Game module interface. Games depend on nothing but core/ (no ESP-IDF), so
// they build and run in the native tests and the bot simulator.
//
// Players are identified by their room player id (pid, 0..kMaxPlayers-1). The
// pid set is fixed for the whole game, so games index their state by pid and
// write pids into their views. Hidden information never leaves privateView():
// each phone receives only its own view (see docs/protocol.md).

#include <cstddef>
#include <cstdint>
#include <string>

#include "json.h"
#include "rng.h"

constexpr int kMaxPlayers = 15;  // ESP32-S3 SoftAP station limit
using PlayerMask = uint16_t;     // bit pid set = player is in

inline bool hasPid(PlayerMask m, int pid) { return pid >= 0 && pid < kMaxPlayers && (m >> pid & 1); }
inline int countPids(PlayerMask m) {
  int n = 0;
  for (; m; m &= m - 1) ++n;
  return n;
}

// Services the room provides to the running game.
class GameHost {
 public:
  virtual ~GameHost() = default;
  virtual Rng& rng() = 0;
  // The game's single phase timer: onTimer() fires once, ms from now.
  // A new call replaces the pending one; 0 cancels it.
  virtual void setTimer(uint32_t ms) = 0;
  // Must be called after every phase change; the room then snapshots the game
  // (save()) to flash so a power cut resumes in the same phase.
  virtual void phaseChanged() = 0;
  // Vibrates every phone in `players` with the same pattern, a JSON array of
  // on/off milliseconds such as "[400,200,400]" (iPhone: screen flash).
  virtual void buzz(PlayerMask players, const char* pattern) = 0;
  // Whether the player's phone is connected right now. Games never wait for
  // offline players; this only feeds views ("disconnected" badges).
  virtual bool online(int pid) const = 0;
};

struct GameInfo {
  const char* id;  // protocol id, e.g. "vampires"
  uint8_t minPlayers;
  uint8_t maxPlayers;
};

class Game {
 public:
  explicit Game(GameHost& host) : host_(host) {}
  virtual ~Game() = default;

  virtual const GameInfo& info() const = 0;

  // Deals roles and enters the first phase. `options` is the start message's
  // options object; missing or out-of-range values are clamped to defaults.
  // Returns nullptr on success, else an error code (docs/protocol.md).
  virtual const char* setup(PlayerMask players, const json::Value& options) = 0;

  // A phone's intent: msg is the whole message ({"t":"act"|"vote"|"ack", ...}).
  // Invalid or late moves are rejected with an error code and change nothing.
  virtual const char* onAction(int pid, const json::Value& msg) = 0;

  virtual void onTimer() = 0;

  // Admin's "continue": ends the current phase early where the rules allow it
  // (role card, dawn, discussion, vote, verdict). Returns an error code if not.
  virtual const char* adminSkip() = 0;

  // Writes one JSON object: everything `pid` may know, nothing more. Called for
  // seated players and for spectators (pids not in the game).
  virtual void privateView(int pid, json::Writer& out) const = 0;
  // Writes one JSON object with public table information only.
  virtual void boardView(json::Writer& out) const = 0;

  virtual bool finished() const = 0;
  // After finished(): did this seated player's side win (for profile stats).
  virtual bool won(int pid) const = 0;

  // Binary snapshot of the whole game state (options included) and its
  // inverse. load() returns false for a blob of another version/size.
  virtual std::string save() const = 0;
  virtual bool load(const uint8_t* data, size_t len) = 0;

 protected:
  GameHost& host_;
};
