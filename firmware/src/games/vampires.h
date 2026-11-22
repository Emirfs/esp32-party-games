#pragma once

// Vampires & Villagers: rules in docs/design.md section 8, messages and views
// in docs/protocol.md. All state is indexed by pid and kept in one trivially
// copyable struct, so a snapshot is a version tag, a size and a memcpy. The
// state holds no absolute times: after a power cut the phase clock restarts.

#include <cstdint>
#include <type_traits>

#include "core/game.h"

class Vampires : public Game {
 public:
  enum class Role : uint8_t { Villager, Vampire, Doctor, Seer };
  enum class Phase : uint8_t { Role, Night, Dawn, Discussion, Vote, Verdict, End };
  enum class Winner : uint8_t { None, Village, Vampires };

  static constexpr GameInfo kInfo{"vampires", 5, 15};

  // Phase timers (ms). Discussion and vote come from the options.
  static constexpr uint32_t kRoleMs = 90000;
  static constexpr uint32_t kNightMinMs = 25000;
  static constexpr uint32_t kNightMaxMs = 60000;
  static constexpr uint32_t kDawnMs = 8000;
  static constexpr uint32_t kVerdictMs = 8000;
  // Same pattern on every phone so the vibration gives no role away.
  static constexpr const char* kNightStartBuzz = "[400]";
  static constexpr const char* kNightEndBuzz = "[200,100,200]";

  // Vampires per player count (doctor and seer are always one each).
  static int vampiresFor(int players);

  explicit Vampires(GameHost& host);

  const GameInfo& info() const override { return kInfo; }
  const char* setup(PlayerMask players, const json::Value& options) override;
  const char* onAction(int pid, const json::Value& msg) override;
  void onTimer() override;
  const char* adminSkip() override;
  void privateView(int pid, json::Writer& out) const override;
  void boardView(json::Writer& out) const override;
  bool finished() const override { return s_.phase == Phase::End; }
  bool won(int pid) const override;
  std::string save() const override;
  bool load(const uint8_t* data, size_t len) override;

  // Read-only state for tests and the bot simulator.
  Phase phase() const { return s_.phase; }
  uint32_t phaseId() const { return s_.phaseId; }
  int round() const { return s_.round; }
  Winner winner() const { return s_.winner; }
  PlayerMask seated() const { return s_.seated; }
  PlayerMask aliveMask() const { return s_.alive; }
  Role roleOf(int pid) const { return s_.role[pid]; }
  int pickOf(int pid) const { return s_.pick[pid]; }
  int voteOf(int pid) const { return s_.vote[pid]; }
  int lastProtect() const { return s_.lastProtect; }
  int died() const { return s_.died; }
  int lynched() const { return s_.lynchedNow; }
  bool runoff() const { return s_.runoff; }
  int voteRound() const { return s_.voteRound; }
  PlayerMask candidates() const { return s_.candidates; }

 private:
  struct Options {
    bool revealRole;
    bool ghostsSeeRoles;
    bool suspicion;
    uint16_t discussionSec;
    uint16_t voteSec;
  };

  struct State {
    Options opt;
    Phase phase;
    Winner winner;
    bool nightMinReached;  // night: the 25 s minimum has passed
    bool runoff;           // verdict: round 1 was tied, round 2 follows
    uint8_t voteRound;     // 1, or 2 for the runoff among the tied
    uint16_t round;        // night number, 1-based
    uint32_t phaseId;
    PlayerMask seated;
    PlayerMask alive;
    PlayerMask lynched;     // every player lynched so far
    PlayerMask acked;       // role phase
    PlayerMask candidates;  // vote
    Role role[kMaxPlayers];
    int8_t pick[kMaxPlayers];  // night pick, -1 = none
    int8_t vote[kMaxPlayers];  // -2 = not voted, -1 = nobody
    int8_t lastProtect;        // doctor's protect of the previous night, -1 = none
    int8_t died;               // last night's victim, -1 = nobody
    int8_t lynchedNow;         // this verdict's lynched player, -1 = nobody
    uint8_t suspicion[kMaxPlayers];  // last night's villager picks per target
    uint8_t seenCount;
    int8_t seen[kMaxPlayers];  // seer's inspected pids, in order
  };
  static_assert(std::is_trivially_copyable<State>::value, "save() copies State as bytes");

  static constexpr uint32_t kSaveVersion = 1;

  void enter(Phase phase);
  uint32_t phaseMs() const;
  void startNight();
  void endNight();
  void startVote(uint8_t voteRound);
  void tally();
  // Ends the game if a side has won; returns whether it did.
  bool checkWin();
  void advance();  // the current phase is over (timer or admin skip)

  const char* onAck(int pid);
  const char* onAct(int pid, int target);
  const char* onVote(int pid, int target);

  bool isAlive(int pid) const { return hasPid(s_.alive, pid); }
  bool validNightTarget(int pid, int target) const;
  bool allAcked() const;
  bool allPicked() const;
  bool allVoted() const;
  int countRole(PlayerMask m, Role r) const;
  // Whether `viewer` (-1 or a non-seated pid = spectator/board) may see p's role.
  bool roleVisible(int viewer, int p) const;

  void writePlayers(int viewer, json::Writer& w) const;
  void writeDawn(json::Writer& w) const;
  void writeVote(int viewer, json::Writer& w) const;
  void writeTally(json::Writer& w) const;
  void writeWinner(json::Writer& w) const;

  State s_;
};
