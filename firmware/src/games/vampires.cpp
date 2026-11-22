#include "vampires.h"

#include <cstring>

namespace {

const char* roleName(Vampires::Role r) {
  switch (r) {
    case Vampires::Role::Vampire: return "vampire";
    case Vampires::Role::Doctor: return "doctor";
    case Vampires::Role::Seer: return "seer";
    case Vampires::Role::Villager: break;
  }
  return "villager";
}

const char* taskName(Vampires::Role r) {
  switch (r) {
    case Vampires::Role::Vampire: return "kill";
    case Vampires::Role::Doctor: return "protect";
    case Vampires::Role::Seer: return "inspect";
    case Vampires::Role::Villager: break;
  }
  return "suspect";
}

const char* phaseName(Vampires::Phase p) {
  switch (p) {
    case Vampires::Phase::Role: return "role";
    case Vampires::Phase::Night: return "night";
    case Vampires::Phase::Dawn: return "dawn";
    case Vampires::Phase::Discussion: return "discussion";
    case Vampires::Phase::Vote: return "vote";
    case Vampires::Phase::Verdict: return "verdict";
    case Vampires::Phase::End: break;
  }
  return "end";
}

bool boolOption(const json::Value& v, bool fallback) { return v.isBool() ? v.boolean() : fallback; }

uint16_t secOption(const json::Value& v, int lo, int hi, int fallback) {
  const int n = v.integer(fallback);
  return static_cast<uint16_t>(n >= lo && n <= hi ? n : fallback);
}

constexpr int8_t kNoVote = -2;
constexpr int8_t kNobody = -1;

}  // namespace

int Vampires::vampiresFor(int players) {
  if (players <= 6) return 1;
  if (players <= 9) return 2;
  return 3;
}

Vampires::Vampires(GameHost& host) : Game(host) {
  memset(&s_, 0, sizeof s_);
  s_.phase = Phase::End;  // no game until setup()
}

const char* Vampires::setup(PlayerMask players, const json::Value& options) {
  if (players >> kMaxPlayers) return "bad_pid";
  const int n = countPids(players);
  if (n < kInfo.minPlayers) return "too_few";
  if (n > kInfo.maxPlayers) return "too_many";

  memset(&s_, 0, sizeof s_);
  s_.opt.revealRole = boolOption(options["revealRole"], true);
  s_.opt.ghostsSeeRoles = boolOption(options["ghostsSeeRoles"], true);
  s_.opt.suspicion = boolOption(options["suspicion"], true);
  s_.opt.discussionSec = secOption(options["discussionSec"], 30, 600, 180);
  s_.opt.voteSec = secOption(options["voteSec"], 20, 180, 60);

  Role deck[kMaxPlayers];
  const int vampires = vampiresFor(n);
  for (int i = 0; i < n; ++i) deck[i] = Role::Villager;
  for (int i = 0; i < vampires; ++i) deck[i] = Role::Vampire;
  deck[vampires] = Role::Doctor;
  deck[vampires + 1] = Role::Seer;
  host_.rng().shuffle(deck, static_cast<size_t>(n));

  int next = 0;
  for (int p = 0; p < kMaxPlayers; ++p) {
    s_.role[p] = hasPid(players, p) ? deck[next++] : Role::Villager;
    s_.pick[p] = kNobody;
    s_.vote[p] = kNoVote;
    s_.seen[p] = kNobody;
  }
  s_.seated = s_.alive = players;
  s_.winner = Winner::None;
  s_.round = 1;
  s_.voteRound = 1;
  s_.lastProtect = kNobody;
  s_.died = kNobody;
  s_.lynchedNow = kNobody;
  enter(Phase::Role);
  return nullptr;
}

// ---------------------------------------------------------------- phases

uint32_t Vampires::phaseMs() const {
  switch (s_.phase) {
    case Phase::Role: return kRoleMs;
    case Phase::Night: return s_.nightMinReached ? kNightMaxMs - kNightMinMs : kNightMinMs;
    case Phase::Dawn: return kDawnMs;
    case Phase::Discussion: return s_.opt.discussionSec * 1000u;
    case Phase::Vote: return s_.opt.voteSec * 1000u;
    case Phase::Verdict: return kVerdictMs;
    case Phase::End: break;
  }
  return 0;
}

void Vampires::enter(Phase phase) {
  s_.phase = phase;
  ++s_.phaseId;
  host_.setTimer(phaseMs());
  host_.phaseChanged();
}

void Vampires::startNight() {
  for (int p = 0; p < kMaxPlayers; ++p) s_.pick[p] = kNobody;
  s_.nightMinReached = false;
  enter(Phase::Night);
  host_.buzz(s_.seated, kNightStartBuzz);
}

void Vampires::endNight() {
  host_.buzz(s_.seated, kNightEndBuzz);

  // Vampires' target: plurality of their picks, ties broken randomly.
  int votes[kMaxPlayers] = {};
  int best = 0;
  for (int p = 0; p < kMaxPlayers; ++p) {
    if (isAlive(p) && s_.role[p] == Role::Vampire && s_.pick[p] >= 0) {
      const int n = ++votes[s_.pick[p]];
      if (n > best) best = n;
    }
  }
  int target = kNobody;
  if (best > 0) {
    int tied[kMaxPlayers];
    int nTied = 0;
    for (int p = 0; p < kMaxPlayers; ++p)
      if (votes[p] == best) tied[nTied++] = p;
    target = tied[host_.rng().below(static_cast<uint32_t>(nTied))];
  }

  // Doctor's protection, seer's result and villagers' suspicion all come from
  // players alive during the night, so they are read before the kill.
  int protect = kNobody;
  memset(s_.suspicion, 0, sizeof s_.suspicion);
  for (int p = 0; p < kMaxPlayers; ++p) {
    const int pick = s_.pick[p];
    if (!isAlive(p) || pick < 0) continue;
    switch (s_.role[p]) {
      case Role::Doctor: protect = pick; break;
      case Role::Seer: {
        bool known = false;
        for (int i = 0; i < s_.seenCount; ++i) known = known || s_.seen[i] == pick;
        if (!known) s_.seen[s_.seenCount++] = static_cast<int8_t>(pick);
        break;
      }
      case Role::Villager: ++s_.suspicion[pick]; break;
      case Role::Vampire: break;
    }
  }
  s_.lastProtect = static_cast<int8_t>(protect);

  s_.died = kNobody;
  if (target >= 0 && target != protect) {
    s_.died = static_cast<int8_t>(target);
    s_.alive &= static_cast<PlayerMask>(~(1u << target));
  }
  enter(Phase::Dawn);
}

void Vampires::startVote(uint8_t voteRound) {
  s_.voteRound = voteRound;
  if (voteRound == 1) s_.candidates = s_.alive;
  for (int p = 0; p < kMaxPlayers; ++p) s_.vote[p] = kNoVote;
  s_.runoff = false;
  s_.lynchedNow = kNobody;
  enter(Phase::Vote);
}

void Vampires::tally() {
  int votes[kMaxPlayers] = {};
  int nobody = 0;
  for (int p = 0; p < kMaxPlayers; ++p) {
    if (!hasPid(s_.seated, p)) continue;
    if (s_.vote[p] == kNobody) ++nobody;
    else if (s_.vote[p] >= 0) ++votes[s_.vote[p]];
  }
  int best = 0;
  for (int p = 0; p < kMaxPlayers; ++p)
    if (votes[p] > best) best = votes[p];

  s_.lynchedNow = kNobody;
  s_.runoff = false;
  if (best > 0 && nobody < best) {
    PlayerMask tied = 0;
    for (int p = 0; p < kMaxPlayers; ++p)
      if (votes[p] == best) tied |= static_cast<PlayerMask>(1u << p);
    if (countPids(tied) == 1) {
      for (int p = 0; p < kMaxPlayers; ++p)
        if (hasPid(tied, p)) s_.lynchedNow = static_cast<int8_t>(p);
      s_.alive &= static_cast<PlayerMask>(~tied);
      s_.lynched |= tied;
    } else if (s_.voteRound == 1) {
      s_.runoff = true;
      s_.candidates = tied;
    }
    // Still tied in round 2: nobody is lynched.
  }
  enter(Phase::Verdict);
}

int Vampires::countRole(PlayerMask m, Role r) const {
  int n = 0;
  for (int p = 0; p < kMaxPlayers; ++p)
    if (hasPid(m, p) && s_.role[p] == r) ++n;
  return n;
}

bool Vampires::checkWin() {
  const int vampires = countRole(s_.alive, Role::Vampire);
  const int others = countPids(s_.alive) - vampires;
  if (vampires == 0) s_.winner = Winner::Village;
  else if (vampires >= others) s_.winner = Winner::Vampires;
  else return false;
  enter(Phase::End);
  return true;
}

void Vampires::advance() {
  switch (s_.phase) {
    case Phase::Role: startNight(); break;
    case Phase::Night: endNight(); break;
    case Phase::Dawn:
      if (!checkWin()) enter(Phase::Discussion);
      break;
    case Phase::Discussion: startVote(1); break;
    case Phase::Vote: tally(); break;
    case Phase::Verdict:
      if (s_.runoff) {
        startVote(2);
      } else if (!checkWin()) {
        ++s_.round;
        startNight();
      }
      break;
    case Phase::End: break;
  }
}

void Vampires::onTimer() {
  if (s_.phase == Phase::Night && !s_.nightMinReached) {
    s_.nightMinReached = true;
    if (allPicked()) {
      endNight();
    } else {
      // Not a new phase (same phaseId), but the snapshot keeps the flag.
      host_.setTimer(phaseMs());
      host_.phaseChanged();
    }
    return;
  }
  advance();
}

const char* Vampires::adminSkip() {
  if (s_.phase == Phase::Night || s_.phase == Phase::End) return "not_allowed";
  advance();
  return nullptr;
}

// ---------------------------------------------------------------- actions

bool Vampires::allAcked() const {
  for (int p = 0; p < kMaxPlayers; ++p)
    if (hasPid(s_.seated, p) && !hasPid(s_.acked, p) && host_.online(p)) return false;
  return true;
}

bool Vampires::allPicked() const {
  for (int p = 0; p < kMaxPlayers; ++p)
    if (isAlive(p) && s_.pick[p] < 0 && host_.online(p)) return false;
  return true;
}

bool Vampires::allVoted() const {
  for (int p = 0; p < kMaxPlayers; ++p)
    if (isAlive(p) && s_.vote[p] == kNoVote && host_.online(p)) return false;
  return true;
}

bool Vampires::validNightTarget(int pid, int target) const {
  if (!isAlive(target)) return false;
  switch (s_.role[pid]) {
    case Role::Vampire: return s_.role[target] != Role::Vampire;
    case Role::Doctor: return target != s_.lastProtect;
    case Role::Seer:
    case Role::Villager: break;
  }
  return target != pid;
}

const char* Vampires::onAction(int pid, const json::Value& msg) {
  const char* t = msg["t"].str();
  const bool ack = strcmp(t, "ack") == 0;
  const bool act = strcmp(t, "act") == 0;
  const bool vote = strcmp(t, "vote") == 0;
  if (!ack && !act && !vote) return "bad_msg";
  const int phaseId = msg["phaseId"].integer(-1);
  if (phaseId < 0) return "bad_msg";
  if (static_cast<uint32_t>(phaseId) != s_.phaseId) return "late";
  if (!hasPid(s_.seated, pid)) return "not_allowed";
  if (ack) return onAck(pid);

  const json::Value target = msg["target"];
  if (!target.isNumber()) return "bad_msg";
  const int to = target.integer(-99);
  return act ? onAct(pid, to) : onVote(pid, to);
}

const char* Vampires::onAck(int pid) {
  if (s_.phase != Phase::Role) return "not_allowed";
  s_.acked |= static_cast<PlayerMask>(1u << pid);
  if (allAcked()) startNight();
  return nullptr;
}

const char* Vampires::onAct(int pid, int target) {
  if (s_.phase != Phase::Night || !isAlive(pid)) return "not_allowed";
  if (!validNightTarget(pid, target)) return "bad_target";
  s_.pick[pid] = static_cast<int8_t>(target);
  if (s_.nightMinReached && allPicked()) endNight();
  return nullptr;
}

const char* Vampires::onVote(int pid, int target) {
  if (s_.phase != Phase::Vote || !isAlive(pid)) return "not_allowed";
  if (target != kNobody && !hasPid(s_.candidates, target)) return "bad_target";
  s_.vote[pid] = static_cast<int8_t>(target);
  if (allVoted()) tally();
  return nullptr;
}

bool Vampires::won(int pid) const {
  if (s_.phase != Phase::End || !hasPid(s_.seated, pid) || s_.winner == Winner::None) return false;
  return (s_.role[pid] == Role::Vampire) == (s_.winner == Winner::Vampires);
}

// ---------------------------------------------------------------- views

bool Vampires::roleVisible(int viewer, int p) const {
  if (s_.phase == Phase::End) return true;
  if (s_.opt.revealRole && hasPid(s_.lynched, p)) return true;
  if (!hasPid(s_.seated, viewer)) return false;
  if (viewer == p) return true;
  if (s_.role[viewer] == Role::Vampire && s_.role[p] == Role::Vampire) return true;
  return !isAlive(viewer) && s_.opt.ghostsSeeRoles;
}

void Vampires::writePlayers(int viewer, json::Writer& w) const {
  w.key("players").beginArray();
  for (int p = 0; p < kMaxPlayers; ++p) {
    if (!hasPid(s_.seated, p)) continue;
    w.beginObject().field("pid", p).field("alive", isAlive(p));
    if (roleVisible(viewer, p)) w.field("role", roleName(s_.role[p]));
    w.endObject();
  }
  w.endArray();
}

void Vampires::writeDawn(json::Writer& w) const {
  w.key("dawn").beginObject().field("died", static_cast<int>(s_.died));
  if (s_.opt.suspicion) {
    w.key("suspicion").beginArray();
    for (int p = 0; p < kMaxPlayers; ++p)
      if (s_.suspicion[p] > 0)
        w.beginObject().field("pid", p).field("n", static_cast<int>(s_.suspicion[p])).endObject();
    w.endArray();
  }
  w.endObject();
}

void Vampires::writeVote(int viewer, json::Writer& w) const {
  w.key("vote").beginObject().field("round", static_cast<int>(s_.voteRound));
  w.key("candidates").beginArray();
  for (int p = 0; p < kMaxPlayers; ++p)
    if (hasPid(s_.candidates, p)) w.value(p);
  w.endArray();
  if (viewer >= 0) w.field("mine", static_cast<int>(s_.vote[viewer]));
  int cast = 0;
  for (int p = 0; p < kMaxPlayers; ++p)
    if (isAlive(p) && s_.vote[p] != kNoVote) ++cast;
  w.field("cast", cast).field("voters", countPids(s_.alive)).endObject();
}

void Vampires::writeTally(json::Writer& w) const {
  w.key("tally").beginObject().field("round", static_cast<int>(s_.voteRound));
  w.key("votes").beginArray();
  for (int p = 0; p < kMaxPlayers; ++p)
    if (hasPid(s_.seated, p) && s_.vote[p] != kNoVote)
      w.beginObject().field("voter", p).field("target", static_cast<int>(s_.vote[p])).endObject();
  w.endArray();
  w.field("lynched", static_cast<int>(s_.lynchedNow));
  if (s_.opt.revealRole && s_.lynchedNow >= 0) w.field("role", roleName(s_.role[s_.lynchedNow]));
  w.field("runoff", s_.runoff).endObject();
}

void Vampires::writeWinner(json::Writer& w) const {
  if (s_.phase == Phase::End)
    w.field("winner", s_.winner == Winner::Vampires ? "vampires" : "village");
}

void Vampires::privateView(int pid, json::Writer& w) const {
  const bool seated = hasPid(s_.seated, pid);
  w.beginObject()
      .field("id", kInfo.id)
      .field("phase", phaseName(s_.phase))
      .field("phaseId", static_cast<long long>(s_.phaseId))
      .field("round", static_cast<int>(s_.round))
      .field("seated", seated);
  if (!seated) {
    writePlayers(-1, w);
    writeWinner(w);
    w.endObject();
    return;
  }

  w.key("options")
      .beginObject()
      .field("revealRole", s_.opt.revealRole)
      .field("ghostsSeeRoles", s_.opt.ghostsSeeRoles)
      .field("suspicion", s_.opt.suspicion)
      .field("discussionSec", static_cast<int>(s_.opt.discussionSec))
      .field("voteSec", static_cast<int>(s_.opt.voteSec))
      .endObject();
  writePlayers(pid, w);
  const Role role = s_.role[pid];
  const bool alive = isAlive(pid);
  w.field("role", roleName(role)).field("alive", alive);

  switch (s_.phase) {
    case Phase::Role:
      w.field("acked", hasPid(s_.acked, pid))
          .field("ackCount", countPids(s_.acked))
          .field("ackTotal", countPids(s_.seated));
      break;
    case Phase::Night:
      if (!alive) break;
      w.key("task").beginObject().field("kind", taskName(role));
      w.key("targets").beginArray();
      for (int p = 0; p < kMaxPlayers; ++p)
        if (validNightTarget(pid, p)) w.value(p);
      w.endArray();
      w.field("pick", static_cast<int>(s_.pick[pid])).endObject();
      if (role == Role::Vampire) {
        w.key("allies").beginArray();
        for (int p = 0; p < kMaxPlayers; ++p)
          if (p != pid && isAlive(p) && s_.role[p] == Role::Vampire)
            w.beginObject().field("pid", p).field("pick", static_cast<int>(s_.pick[p])).endObject();
        w.endArray();
      }
      break;
    case Phase::Dawn:
    case Phase::Discussion: writeDawn(w); break;
    case Phase::Vote: writeVote(pid, w); break;
    case Phase::Verdict: writeTally(w); break;
    case Phase::End: break;
  }

  if (role == Role::Seer) {
    w.key("seen").beginArray();
    for (int i = 0; i < s_.seenCount; ++i)
      w.beginObject()
          .field("pid", static_cast<int>(s_.seen[i]))
          .field("vampire", s_.role[s_.seen[i]] == Role::Vampire)
          .endObject();
    w.endArray();
  }
  writeWinner(w);
  w.endObject();
}

void Vampires::boardView(json::Writer& w) const {
  w.beginObject()
      .field("id", kInfo.id)
      .field("phase", phaseName(s_.phase))
      .field("phaseId", static_cast<long long>(s_.phaseId))
      .field("round", static_cast<int>(s_.round));
  writePlayers(-1, w);
  switch (s_.phase) {
    case Phase::Role:
      w.field("acted", countPids(s_.acked)).field("actors", countPids(s_.seated));
      break;
    case Phase::Night: {
      int acted = 0;
      for (int p = 0; p < kMaxPlayers; ++p)
        if (isAlive(p) && s_.pick[p] >= 0) ++acted;
      w.field("acted", acted).field("actors", countPids(s_.alive));
      break;
    }
    case Phase::Dawn:
    case Phase::Discussion: writeDawn(w); break;
    case Phase::Vote: writeVote(-1, w); break;
    case Phase::Verdict: writeTally(w); break;
    case Phase::End: break;
  }
  writeWinner(w);
  w.endObject();
}

// ---------------------------------------------------------------- snapshot

std::string Vampires::save() const {
  const uint32_t header[2] = {kSaveVersion, static_cast<uint32_t>(sizeof s_)};
  std::string blob(sizeof header + sizeof s_, '\0');
  memcpy(&blob[0], header, sizeof header);
  memcpy(&blob[sizeof header], &s_, sizeof s_);
  return blob;
}

bool Vampires::load(const uint8_t* data, size_t len) {
  uint32_t header[2];
  if (data == nullptr || len != sizeof header + sizeof(State)) return false;
  memcpy(header, data, sizeof header);
  if (header[0] != kSaveVersion || header[1] != sizeof(State)) return false;
  State s;
  memcpy(&s, data + sizeof header, sizeof s);
  if (s.phase > Phase::End || s.winner > Winner::Vampires || s.seenCount > kMaxPlayers) return false;
  if (s.lastProtect < kNobody || s.lastProtect >= kMaxPlayers || s.died < kNobody ||
      s.died >= kMaxPlayers || s.lynchedNow < kNobody || s.lynchedNow >= kMaxPlayers)
    return false;
  for (int p = 0; p < kMaxPlayers; ++p) {
    if (s.role[p] > Role::Seer || s.pick[p] < kNobody || s.pick[p] >= kMaxPlayers ||
        s.vote[p] < kNoVote || s.vote[p] >= kMaxPlayers)
      return false;
  }
  for (int i = 0; i < s.seenCount; ++i)
    if (s.seen[i] < 0 || s.seen[i] >= kMaxPlayers) return false;
  s_ = s;
  // The phase clock restarts: the state holds no absolute times.
  host_.setTimer(phaseMs());
  return true;
}
