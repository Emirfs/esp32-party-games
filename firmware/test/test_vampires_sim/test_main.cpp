// Bot simulator: thousands of seeded random Vampires games with valid and
// invalid moves, random timers, admin skips, disconnects and snapshot/restore
// into a fresh instance. Every move's outcome is checked against an
// independent model of the rules, and every view must parse and leak nothing.

#include <unity.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "../test_vampires/fake_host.h"
#include "games/vampires.h"

using Role = Vampires::Role;
using Phase = Vampires::Phase;
using Winner = Vampires::Winner;

void setUp() {}
void tearDown() {}

namespace {

constexpr int kGames = 5000;
constexpr int kMaxSteps = 20000;

const char* roleName(Role r) {
  switch (r) {
    case Role::Vampire: return "vampire";
    case Role::Doctor: return "doctor";
    case Role::Seer: return "seer";
    case Role::Villager: break;
  }
  return "villager";
}

struct Options {
  bool revealRole = true;
  bool ghostsSeeRoles = true;
  bool suspicion = true;
  int discussionSec = 180;
  int voteSec = 60;
};

// Random start options, some invalid; `eff` gets the values the game must use.
std::string randomOptions(Rng& r, Options& eff) {
  std::string s = "{";
  auto sep = [&] {
    if (s.size() > 1) s += ',';
  };
  auto boolOpt = [&](const char* name, bool& out) {
    switch (r.below(4)) {
      case 0: break;  // absent → default
      case 1: sep(), s += "\"" + std::string(name) + "\":true", out = true; break;
      case 2: sep(), s += "\"" + std::string(name) + "\":false", out = false; break;
      default: sep(), s += "\"" + std::string(name) + "\":1"; break;  // wrong type → default
    }
  };
  auto secOpt = [&](const char* name, int lo, int hi, int& out) {
    if (r.below(3) == 0) return;
    const int v = static_cast<int>(r.below(static_cast<uint32_t>(hi + 60))) - 20;
    sep();
    s += "\"" + std::string(name) + "\":" + std::to_string(v);
    if (v >= lo && v <= hi) out = v;
  };
  boolOpt("revealRole", eff.revealRole);
  boolOpt("ghostsSeeRoles", eff.ghostsSeeRoles);
  boolOpt("suspicion", eff.suspicion);
  secOpt("discussionSec", 30, 600, eff.discussionSec);
  secOpt("voteSec", 20, 180, eff.voteSec);
  s += "}";
  return s;
}

struct Match {
  FakeHost host;
  std::unique_ptr<Vampires> g;
  Rng bot;
  Options opt;
  PlayerMask seats = 0;
  PlayerMask lynchedEver = 0;
  int lastProtect = -1;  // model of the doctor's ban
  int doctor = -1;
  int steps = 0;
  int restores = 0;
  int accepted = 0;
  int rejected = 0;

  explicit Match(uint64_t seed) : host(seed), g(new Vampires(host)), bot(seed * 0x9E3779B97F4A7C15ull + 1) {}

  bool alive(int p) const { return hasPid(g->aliveMask(), p); }
  int aliveRole(Role r) const {
    int n = 0;
    for (int p = 0; p < kMaxPlayers; ++p)
      if (alive(p) && g->roleOf(p) == r) ++n;
    return n;
  }
  int randomSeat() {
    int order[kMaxPlayers];
    int n = 0;
    for (int p = 0; p < kMaxPlayers; ++p)
      if (hasPid(seats, p)) order[n++] = p;
    return order[bot.below(static_cast<uint32_t>(n))];
  }
  int randomPid() { return static_cast<int>(bot.below(19)) - 2; }  // -2..16

  bool validNightTarget(int actor, int t) const {
    if (!alive(t)) return false;
    switch (g->roleOf(actor)) {
      case Role::Vampire: return g->roleOf(t) != Role::Vampire;
      case Role::Doctor: return t != lastProtect;
      case Role::Seer:
      case Role::Villager: break;
    }
    return t != actor;
  }

  // ------------------------------------------------------------ views

  bool roleAllowed(int viewer, int p) const {
    if (g->phase() == Phase::End) return true;
    if (opt.revealRole && hasPid(lynchedEver, p)) return true;
    if (!hasPid(seats, viewer)) return false;
    if (viewer == p) return true;
    if (g->roleOf(viewer) == Role::Vampire && g->roleOf(p) == Role::Vampire) return true;
    return !alive(viewer) && opt.ghostsSeeRoles;
  }

  void checkPlayers(const json::Value& view, int viewer) const {
    const json::Value players = view["players"];
    TEST_ASSERT_TRUE(players.isArray());
    TEST_ASSERT_EQUAL(countPids(seats), static_cast<int>(players.size()));
    for (size_t i = 0; i < players.size(); ++i) {
      const json::Value e = players[i];
      const int p = e["pid"].integer(-1);
      TEST_ASSERT_TRUE(hasPid(seats, p));
      TEST_ASSERT_EQUAL(alive(p), e["alive"].boolean(!alive(p)));
      if (e["role"].isNull()) {
        TEST_ASSERT_FALSE(roleAllowed(viewer, p));
      } else {
        TEST_ASSERT_TRUE_MESSAGE(roleAllowed(viewer, p), "role leaked");
        TEST_ASSERT_EQUAL_STRING(roleName(g->roleOf(p)), e["role"].str());
      }
    }
  }

  void checkCommon(const json::Value& v) const {
    TEST_ASSERT_TRUE(v.isObject());
    TEST_ASSERT_EQUAL_STRING("vampires", v["id"].str());
    TEST_ASSERT_EQUAL(static_cast<int>(g->phaseId()), v["phaseId"].integer(-1));
    TEST_ASSERT_EQUAL(g->round(), v["round"].integer(-1));
    TEST_ASSERT_EQUAL(g->phase() == Phase::End, !v["winner"].isNull());
  }

  void checkView(int viewer) const {
    const std::string text = privateViewOf(*g, viewer);
    json::Doc doc;
    TEST_ASSERT_TRUE_MESSAGE(doc.parse(text.c_str(), text.size()), text.c_str());
    const json::Value v = doc.root();
    checkCommon(v);
    checkPlayers(v, viewer);
    const bool seated = hasPid(seats, viewer);
    TEST_ASSERT_EQUAL(seated, v["seated"].boolean(!seated));
    if (!seated) {
      static const char* const kAllowed[] = {"id", "phase", "phaseId", "round", "seated", "players", "winner"};
      for (size_t i = 0; i < v.size(); ++i) {
        bool ok = false;
        for (const char* k : kAllowed) ok = ok || strcmp(k, v.keyAt(i)) == 0;
        TEST_ASSERT_TRUE_MESSAGE(ok, v.keyAt(i));
      }
      return;
    }
    const Role role = g->roleOf(viewer);
    const bool night = g->phase() == Phase::Night && alive(viewer);
    TEST_ASSERT_EQUAL_STRING(roleName(role), v["role"].str());
    TEST_ASSERT_EQUAL(alive(viewer), v["alive"].boolean(!alive(viewer)));
    TEST_ASSERT_EQUAL(opt.revealRole, v["options"]["revealRole"].boolean(!opt.revealRole));
    TEST_ASSERT_EQUAL(opt.ghostsSeeRoles, v["options"]["ghostsSeeRoles"].boolean(!opt.ghostsSeeRoles));
    TEST_ASSERT_EQUAL(opt.suspicion, v["options"]["suspicion"].boolean(!opt.suspicion));
    TEST_ASSERT_EQUAL(opt.discussionSec, v["options"]["discussionSec"].integer(-1));
    TEST_ASSERT_EQUAL(opt.voteSec, v["options"]["voteSec"].integer(-1));
    TEST_ASSERT_EQUAL(night, !v["task"].isNull());
    TEST_ASSERT_EQUAL(night && role == Role::Vampire, !v["allies"].isNull());
    TEST_ASSERT_EQUAL(role == Role::Seer, !v["seen"].isNull());
    if (night) {
      const json::Value targets = v["task"]["targets"];
      for (int t = 0; t < kMaxPlayers; ++t) {
        bool listed = false;
        for (size_t i = 0; i < targets.size(); ++i) listed = listed || targets[i].integer(-1) == t;
        TEST_ASSERT_EQUAL(validNightTarget(viewer, t), listed);
      }
      const json::Value allies = v["allies"];
      for (size_t i = 0; i < allies.size(); ++i)
        TEST_ASSERT_EQUAL(Role::Vampire, g->roleOf(allies[i]["pid"].integer(-1)));
    }
    const json::Value seen = v["seen"];
    for (size_t i = 0; i < seen.size(); ++i) {
      const int p = seen[i]["pid"].integer(-1);
      TEST_ASSERT_TRUE(hasPid(seats, p));
      TEST_ASSERT_EQUAL(g->roleOf(p) == Role::Vampire, seen[i]["vampire"].boolean(false));
    }
    if (g->phase() == Phase::Vote) TEST_ASSERT_EQUAL(g->voteOf(viewer), v["vote"]["mine"].integer(-9));
  }

  void checkBoard() const {
    const std::string text = boardViewOf(*g);
    json::Doc doc;
    TEST_ASSERT_TRUE_MESSAGE(doc.parse(text.c_str(), text.size()), text.c_str());
    const json::Value v = doc.root();
    checkCommon(v);
    checkPlayers(v, -1);
    for (const char* k : {"task", "allies", "seen", "role", "options", "seated"}) TEST_ASSERT_TRUE(v[k].isNull());
    TEST_ASSERT_TRUE(v["vote"]["mine"].isNull());
  }

  void checkAllViews() const {
    for (int p = -1; p < kMaxPlayers; ++p) checkView(p);
    checkBoard();
  }

  // ------------------------------------------------------------ moves

  void sendAction() {
    const uint32_t id = g->phaseId();
    const Phase phase = g->phase();
    const int actor = bot.below(10) == 0 ? randomPid() : randomSeat();
    const bool stale = bot.below(20) == 0;
    const long long phaseId = stale ? static_cast<long long>(id) - 1 - bot.below(3) : id;

    // Mostly the phase's own move, sometimes another one.
    static const char* const kTypes[] = {"ack", "act", "vote"};
    const char* type = phase == Phase::Role ? "ack" : phase == Phase::Night ? "act" : "vote";
    if (bot.below(7) == 0) type = kTypes[bot.below(3)];

    int target = randomPid();
    if (bot.below(5) != 0) {
      // A plausible target: one the view offers.
      const std::string text = privateViewOf(*g, actor);
      json::Doc doc;
      TEST_ASSERT_TRUE(doc.parse(text.c_str(), text.size()));
      const json::Value list = strcmp(type, "act") == 0 ? doc.root()["task"]["targets"] : doc.root()["vote"]["candidates"];
      if (list.size() > 0 && bot.below(6) != 0) target = list[bot.below(static_cast<uint32_t>(list.size()))].integer(-1);
      else if (strcmp(type, "vote") == 0) target = -1;
    }

    // Expected outcome from the model.
    const char* expected = nullptr;
    const bool isAck = strcmp(type, "ack") == 0;
    if (phaseId < 0) expected = "bad_msg";
    else if (stale) expected = "late";
    else if (!hasPid(seats, actor)) expected = "not_allowed";
    else if (isAck) expected = phase == Phase::Role ? nullptr : "not_allowed";
    else if (strcmp(type, "act") == 0) {
      if (phase != Phase::Night || !alive(actor)) expected = "not_allowed";
      else if (!validNightTarget(actor, target)) expected = "bad_target";
    } else {
      if (phase != Phase::Vote || !alive(actor)) expected = "not_allowed";
      else if (target != -1 && !hasPid(g->candidates(), target)) expected = "bad_target";
    }

    const std::string before = g->save();
    const PlayerMask aliveBefore = g->aliveMask();
    const char* rc = sendMsg(*g, actor, type, phaseId, target, !isAck);
    if (expected == nullptr) {
      TEST_ASSERT_NULL_MESSAGE(rc, rc);
      ++accepted;
      // Dead players never act; a valid move only records itself (or ends the phase).
      TEST_ASSERT_TRUE(hasPid(aliveBefore, actor));
      if (g->phaseId() == id) {
        if (strcmp(type, "act") == 0) TEST_ASSERT_EQUAL(target, g->pickOf(actor));
        if (strcmp(type, "vote") == 0) TEST_ASSERT_EQUAL(target, g->voteOf(actor));
      }
    } else {
      TEST_ASSERT_NOT_NULL(rc);
      TEST_ASSERT_EQUAL_STRING(expected, rc);
      TEST_ASSERT_TRUE_MESSAGE(before == g->save(), "rejected move changed the state");
      ++rejected;
    }
  }

  void sendGarbage() {
    char buf[128];
    const unsigned id = g->phaseId();
    switch (bot.below(7)) {
      case 0: snprintf(buf, sizeof buf, "{\"t\":\"act\",\"target\":1}"); break;
      case 1: snprintf(buf, sizeof buf, "{\"t\":\"act\",\"phaseId\":\"%u\",\"target\":1}", id); break;
      case 2: snprintf(buf, sizeof buf, "{\"t\":\"vote\",\"phaseId\":%u,\"target\":\"2\"}", id); break;
      case 3: snprintf(buf, sizeof buf, "{\"t\":\"vote\",\"phaseId\":%u,\"target\":1.5}", id); break;
      case 4: snprintf(buf, sizeof buf, "{\"phaseId\":%u,\"target\":1}", id); break;
      case 5: snprintf(buf, sizeof buf, "{\"t\":\"act\",\"phaseId\":%u.5,\"target\":1}", id); break;
      default: snprintf(buf, sizeof buf, "[1,2,3]"); break;
    }
    json::Doc doc;
    TEST_ASSERT_TRUE(doc.parse(buf, strlen(buf)));
    const std::string before = g->save();
    TEST_ASSERT_NOT_NULL_MESSAGE(g->onAction(randomSeat(), doc.root()), buf);
    TEST_ASSERT_TRUE(before == g->save());
  }

  void restore() {
    const std::string blob = g->save();
    std::unique_ptr<Vampires> fresh(new Vampires(host));
    // A damaged blob is refused.
    TEST_ASSERT_FALSE(fresh->load(reinterpret_cast<const uint8_t*>(blob.data()), blob.size() - 1 - bot.below(4)));
    TEST_ASSERT_TRUE(fresh->load(reinterpret_cast<const uint8_t*>(blob.data()), blob.size()));
    TEST_ASSERT_TRUE(blob == fresh->save());
    TEST_ASSERT_TRUE(host.timer > 0 || fresh->finished());
    const int viewer = randomPid();
    TEST_ASSERT_EQUAL_STRING(privateViewOf(*g, viewer).c_str(), privateViewOf(*fresh, viewer).c_str());
    TEST_ASSERT_EQUAL_STRING(boardViewOf(*g).c_str(), boardViewOf(*fresh).c_str());
    g = std::move(fresh);
    ++restores;
  }

  // ------------------------------------------------------------ transitions

  void checkNoWinner() const {
    const int v = aliveRole(Role::Vampire);
    TEST_ASSERT_TRUE(v > 0);
    TEST_ASSERT_TRUE(v < countPids(g->aliveMask()) - v);
  }

  void observe(Phase phase, uint32_t id, PlayerMask aliveBefore, size_t buzzesBefore) {
    TEST_ASSERT_EQUAL(aliveBefore, static_cast<PlayerMask>(aliveBefore | g->aliveMask()));  // nobody revives
    if (g->phaseId() == id) {
      TEST_ASSERT_EQUAL(static_cast<int>(phase), static_cast<int>(g->phase()));
      TEST_ASSERT_EQUAL(aliveBefore, g->aliveMask());
      TEST_ASSERT_EQUAL(static_cast<int>(buzzesBefore), static_cast<int>(host.buzzes.size()));
      return;
    }
    TEST_ASSERT_EQUAL_UINT32(id + 1, g->phaseId());
    const Phase now = g->phase();
    TEST_ASSERT_TRUE(now != phase);

    if (phase == Phase::Night) {
      TEST_ASSERT_EQUAL(static_cast<int>(Phase::Dawn), static_cast<int>(now));
      TEST_ASSERT_EQUAL_STRING(Vampires::kNightEndBuzz, host.buzzes.back().pattern.c_str());
      TEST_ASSERT_EQUAL(seats, host.buzzes.back().players);
      const int died = g->died();
      const int protect = hasPid(aliveBefore, doctor) ? g->pickOf(doctor) : -1;
      if (died >= 0) {
        TEST_ASSERT_TRUE(hasPid(aliveBefore, died));
        TEST_ASSERT_TRUE_MESSAGE(g->roleOf(died) != Role::Vampire, "vampires killed a vampire");
        TEST_ASSERT_TRUE(died != protect);
        // The victim is the pick of at least one living vampire.
        bool picked = false;
        for (int p = 0; p < kMaxPlayers; ++p)
          picked = picked || (hasPid(aliveBefore, p) && g->roleOf(p) == Role::Vampire && g->pickOf(p) == died);
        TEST_ASSERT_TRUE(picked);
        TEST_ASSERT_EQUAL(static_cast<PlayerMask>(aliveBefore & ~(1u << died)), g->aliveMask());
      } else {
        TEST_ASSERT_EQUAL(aliveBefore, g->aliveMask());
      }
      TEST_ASSERT_EQUAL(protect, g->lastProtect());
      lastProtect = protect;
    } else {
      TEST_ASSERT_EQUAL(static_cast<int>(buzzesBefore) + (now == Phase::Night ? 1 : 0),
                        static_cast<int>(host.buzzes.size()));
    }

    switch (now) {
      case Phase::Night:
        TEST_ASSERT_TRUE(phase == Phase::Role || phase == Phase::Verdict);
        TEST_ASSERT_EQUAL_STRING(Vampires::kNightStartBuzz, host.buzzes.back().pattern.c_str());
        TEST_ASSERT_EQUAL_UINT32(Vampires::kNightMinMs, host.timer);
        for (int p = 0; p < kMaxPlayers; ++p) TEST_ASSERT_EQUAL(-1, g->pickOf(p));
        checkNoWinner();
        break;
      case Phase::Discussion:
        TEST_ASSERT_EQUAL(static_cast<int>(Phase::Dawn), static_cast<int>(phase));
        TEST_ASSERT_EQUAL_UINT32(opt.discussionSec * 1000u, host.timer);
        checkNoWinner();
        break;
      case Phase::Vote:
        TEST_ASSERT_TRUE(phase == Phase::Discussion || phase == Phase::Verdict);
        TEST_ASSERT_EQUAL_UINT32(opt.voteSec * 1000u, host.timer);
        TEST_ASSERT_EQUAL(phase == Phase::Discussion ? 1 : 2, g->voteRound());
        if (g->voteRound() == 1) TEST_ASSERT_EQUAL(g->aliveMask(), g->candidates());
        else TEST_ASSERT_TRUE(countPids(g->candidates()) >= 2);
        break;
      case Phase::Verdict: {
        TEST_ASSERT_EQUAL(static_cast<int>(Phase::Vote), static_cast<int>(phase));
        const int l = g->lynched();
        if (l >= 0) {
          TEST_ASSERT_FALSE(g->runoff());
          TEST_ASSERT_TRUE(hasPid(g->candidates(), l) || g->voteRound() == 1);
          TEST_ASSERT_EQUAL(static_cast<PlayerMask>(aliveBefore & ~(1u << l)), g->aliveMask());
          lynchedEver |= static_cast<PlayerMask>(1u << l);
        } else {
          TEST_ASSERT_EQUAL(aliveBefore, g->aliveMask());
          if (g->runoff()) TEST_ASSERT_EQUAL(1, g->voteRound());
        }
        break;
      }
      case Phase::End: {
        TEST_ASSERT_TRUE(phase == Phase::Dawn || phase == Phase::Verdict);
        TEST_ASSERT_EQUAL_UINT32(0, host.timer);
        const int v = aliveRole(Role::Vampire);
        const int others = countPids(g->aliveMask()) - v;
        if (g->winner() == Winner::Village) TEST_ASSERT_EQUAL(0, v);
        else if (g->winner() == Winner::Vampires) TEST_ASSERT_TRUE(v > 0 && v >= others);
        else TEST_FAIL_MESSAGE("end without winner");
        break;
      }
      case Phase::Dawn:
      case Phase::Role: break;
    }
    checkAllViews();
  }

  // ------------------------------------------------------------ game

  void play() {
    const int n = 5 + static_cast<int>(bot.below(11));
    int order[kMaxPlayers];
    for (int i = 0; i < kMaxPlayers; ++i) order[i] = i;
    bot.shuffle(order, kMaxPlayers);
    for (int i = 0; i < n; ++i) seats |= static_cast<PlayerMask>(1u << order[i]);

    const std::string options = randomOptions(bot, opt);
    json::Doc doc;
    TEST_ASSERT_TRUE(doc.parse(options.c_str(), options.size()));
    TEST_ASSERT_NULL(g->setup(seats, doc.root()));
    int counts[4] = {};
    for (int p = 0; p < kMaxPlayers; ++p)
      if (hasPid(seats, p)) ++counts[static_cast<int>(g->roleOf(p))];
    TEST_ASSERT_EQUAL(Vampires::vampiresFor(n), counts[static_cast<int>(Role::Vampire)]);
    TEST_ASSERT_EQUAL(1, counts[static_cast<int>(Role::Doctor)]);
    TEST_ASSERT_EQUAL(1, counts[static_cast<int>(Role::Seer)]);
    for (int p = 0; p < kMaxPlayers; ++p)
      if (hasPid(seats, p) && g->roleOf(p) == Role::Doctor) doctor = p;
    checkAllViews();

    for (; steps < kMaxSteps && !g->finished(); ++steps) {
      const Phase phase = g->phase();
      const uint32_t id = g->phaseId();
      const PlayerMask aliveBefore = g->aliveMask();
      const size_t buzzesBefore = host.buzzes.size();
      const uint32_t r = bot.below(100);
      if (r < 60) {
        sendAction();
      } else if (r < 75) {
        TEST_ASSERT_TRUE(host.fire(*g));
      } else if (r < 80) {
        const bool allowed = phase != Phase::Night && phase != Phase::End;
        const char* rc = g->adminSkip();
        if (allowed) TEST_ASSERT_NULL(rc);
        else TEST_ASSERT_EQUAL_STRING("not_allowed", rc);
      } else if (r < 83) {
        restore();
      } else if (r < 87) {
        host.offline ^= static_cast<PlayerMask>(1u << randomSeat());
      } else if (r < 91) {
        sendGarbage();
      } else {
        checkView(randomPid());
        checkBoard();
      }
      observe(phase, id, aliveBefore, buzzesBefore);
    }
    TEST_ASSERT_TRUE_MESSAGE(g->finished(), "game did not end within the step bound");
    TEST_ASSERT_FALSE(host.fire(*g));
    TEST_ASSERT_EQUAL_STRING("not_allowed", g->adminSkip());
    for (int p = 0; p < kMaxPlayers; ++p) {
      const bool vampire = g->roleOf(p) == Role::Vampire;
      const bool expected = hasPid(seats, p) && vampire == (g->winner() == Winner::Vampires);
      TEST_ASSERT_EQUAL(expected, g->won(p));
    }
  }
};

}  // namespace

void test_random_games() {
  long long steps = 0, accepted = 0, rejected = 0, restores = 0;
  int maxSteps = 0, villageWins = 0;
  for (int seed = 1; seed <= kGames; ++seed) {
    Match game(static_cast<uint64_t>(seed));
    game.play();
    steps += game.steps;
    accepted += game.accepted;
    rejected += game.rejected;
    restores += game.restores;
    if (game.steps > maxSteps) maxSteps = game.steps;
    if (game.g->winner() == Winner::Village) ++villageWins;
  }
  char summary[200];
  snprintf(summary, sizeof summary,
           "%d games, %lld steps (max %d), %lld accepted / %lld rejected moves, %lld restores, village won %d",
           kGames, steps, maxSteps, accepted, rejected, restores, villageWins);
  TEST_MESSAGE(summary);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_random_games);
  return UNITY_END();
}
