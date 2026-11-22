// Vampires & Villagers rules, views and snapshots (docs/protocol.md).

#include <unity.h>

#include <cstring>
#include <set>
#include <string>

#include "fake_host.h"
#include "games/registry.h"
#include "games/vampires.h"

using Role = Vampires::Role;
using Phase = Vampires::Phase;

void setUp() {}
void tearDown() {}

namespace {

PlayerMask firstN(int n) { return static_cast<PlayerMask>((1u << n) - 1); }

const char* setupWith(Vampires& g, PlayerMask players, const char* options = "{}") {
  json::Doc doc;
  TEST_ASSERT_TRUE(doc.parse(options, strlen(options)));
  return g.setup(players, doc.root());
}

const char* ack(Vampires& g, int pid) { return sendMsg(g, pid, "ack", g.phaseId(), 0, false); }
const char* act(Vampires& g, int pid, int target) { return sendMsg(g, pid, "act", g.phaseId(), target); }
const char* vote(Vampires& g, int pid, int target) { return sendMsg(g, pid, "vote", g.phaseId(), target); }

int findRole(const Vampires& g, Role r, int nth = 0) {
  for (int p = 0; p < kMaxPlayers; ++p)
    if (hasPid(g.seated(), p) && g.roleOf(p) == r && nth-- == 0) return p;
  return -1;
}

void parse(json::Doc& doc, const std::string& text) {
  TEST_ASSERT_TRUE_MESSAGE(doc.parse(text.c_str(), text.size()), text.c_str());
}

// The role shown for `pid` in a view's players list, or nullptr if hidden.
const char* shownRole(const json::Value& view, int pid) {
  const json::Value players = view["players"];
  for (size_t i = 0; i < players.size(); ++i) {
    const json::Value p = players[i];
    if (p["pid"].integer(-1) == pid) return p["role"].isString() ? p["role"].str() : nullptr;
  }
  return nullptr;
}

bool hasKey(const json::Value& obj, const char* key) { return !obj[key].isNull(); }

bool arrayHas(const json::Value& arr, int v) {
  for (size_t i = 0; i < arr.size(); ++i)
    if (arr[i].integer(-99) == v) return true;
  return false;
}

// Admin skips until the phase is `target` (never through night or end).
void skipTo(Vampires& g, Phase target) {
  for (int i = 0; i < 10 && g.phase() != target; ++i) TEST_ASSERT_NULL(g.adminSkip());
  TEST_ASSERT_EQUAL(static_cast<int>(target), static_cast<int>(g.phase()));
}

// Ends the night by firing the timer (25 s, then 60 s if needed).
void endNight(FakeHost& host, Vampires& g) {
  for (int i = 0; i < 2 && g.phase() == Phase::Night; ++i) TEST_ASSERT_TRUE(host.fire(g));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Dawn), static_cast<int>(g.phase()));
}

}  // namespace

// ---------------------------------------------------------------- setup

void test_registry() {
  TEST_ASSERT_EQUAL(1, static_cast<int>(games::count()));
  TEST_ASSERT_EQUAL_STRING("vampires", games::info(0).id);
  const GameInfo* info = games::find("vampires");
  TEST_ASSERT_NOT_NULL(info);
  TEST_ASSERT_EQUAL(5, info->minPlayers);
  TEST_ASSERT_EQUAL(15, info->maxPlayers);
  TEST_ASSERT_NULL(games::find("chess"));
  FakeHost host;
  TEST_ASSERT_NOT_NULL(games::create("vampires", host).get());
  TEST_ASSERT_NULL(games::create("chess", host).get());
}

void test_role_counts() {
  const int expected[16] = {0, 0, 0, 0, 0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3};
  for (int n = 5; n <= 15; ++n) {
    for (uint64_t seed = 1; seed <= 20; ++seed) {
      FakeHost host(seed);
      Vampires g(host);
      // Contiguous seats for odd seeds, scattered seats for even ones.
      PlayerMask mask = firstN(n);
      if (seed % 2 == 0) {
        Rng seats(seed);
        int order[kMaxPlayers];
        for (int i = 0; i < kMaxPlayers; ++i) order[i] = i;
        seats.shuffle(order, kMaxPlayers);
        mask = 0;
        for (int i = 0; i < n; ++i) mask |= static_cast<PlayerMask>(1u << order[i]);
      }
      TEST_ASSERT_NULL(setupWith(g, mask));
      TEST_ASSERT_EQUAL(mask, g.seated());
      TEST_ASSERT_EQUAL(expected[n], Vampires::vampiresFor(n));
      int counts[4] = {};
      for (int p = 0; p < kMaxPlayers; ++p)
        if (hasPid(mask, p)) ++counts[static_cast<int>(g.roleOf(p))];
      TEST_ASSERT_EQUAL(expected[n], counts[static_cast<int>(Role::Vampire)]);
      TEST_ASSERT_EQUAL(1, counts[static_cast<int>(Role::Doctor)]);
      TEST_ASSERT_EQUAL(1, counts[static_cast<int>(Role::Seer)]);
      TEST_ASSERT_EQUAL(n - expected[n] - 2, counts[static_cast<int>(Role::Villager)]);
      TEST_ASSERT_EQUAL(static_cast<int>(Phase::Role), static_cast<int>(g.phase()));
      TEST_ASSERT_EQUAL(1, g.round());
    }
  }
}

void test_setup_rejects_player_counts() {
  FakeHost host;
  Vampires g(host);
  TEST_ASSERT_EQUAL_STRING("too_few", setupWith(g, firstN(4)));
  TEST_ASSERT_EQUAL_STRING("bad_pid", setupWith(g, 0x801F));
}

void test_options_defaults_and_ranges() {
  {
    FakeHost host;
    Vampires g(host);
    TEST_ASSERT_NULL(setupWith(g, firstN(5), "{\"discussionSec\":45,\"voteSec\":20,\"revealRole\":false}"));
    json::Doc doc;
    parse(doc, privateViewOf(g, 0));
    const json::Value o = doc.root()["options"];
    TEST_ASSERT_FALSE(o["revealRole"].boolean(true));
    TEST_ASSERT_TRUE(o["ghostsSeeRoles"].boolean(false));
    TEST_ASSERT_TRUE(o["suspicion"].boolean(false));
    TEST_ASSERT_EQUAL(45, o["discussionSec"].integer(0));
    TEST_ASSERT_EQUAL(20, o["voteSec"].integer(0));
    TEST_ASSERT_EQUAL_UINT32(Vampires::kRoleMs, host.timer);
    g.adminSkip();
    endNight(host, g);
    TEST_ASSERT_EQUAL_UINT32(Vampires::kDawnMs, host.timer);
    skipTo(g, Phase::Discussion);
    TEST_ASSERT_EQUAL_UINT32(45000, host.timer);
    skipTo(g, Phase::Vote);
    TEST_ASSERT_EQUAL_UINT32(20000, host.timer);
  }
  {
    FakeHost host;
    Vampires g(host);
    TEST_ASSERT_NULL(setupWith(g, firstN(5),
                               "{\"discussionSec\":10,\"voteSec\":181.5,\"suspicion\":\"no\",\"revealRole\":1}"));
    json::Doc doc;
    parse(doc, privateViewOf(g, 0));
    const json::Value o = doc.root()["options"];
    TEST_ASSERT_TRUE(o["revealRole"].boolean(false));
    TEST_ASSERT_TRUE(o["suspicion"].boolean(false));
    TEST_ASSERT_EQUAL(180, o["discussionSec"].integer(0));
    TEST_ASSERT_EQUAL(60, o["voteSec"].integer(0));
  }
  {
    // Options missing entirely.
    FakeHost host;
    Vampires g(host);
    TEST_ASSERT_NULL(g.setup(firstN(5), json::Value()));
    json::Doc doc;
    parse(doc, privateViewOf(g, 0));
    TEST_ASSERT_EQUAL(180, doc.root()["options"]["discussionSec"].integer(0));
  }
}

// ---------------------------------------------------------------- role phase

void test_role_phase_acks() {
  FakeHost host;
  Vampires g(host);
  setupWith(g, firstN(5));
  const uint32_t id = g.phaseId();
  for (int p = 0; p < 4; ++p) TEST_ASSERT_NULL(ack(g, p));
  TEST_ASSERT_NULL(ack(g, 0));  // repeated ack is harmless
  json::Doc doc;
  parse(doc, privateViewOf(g, 0));
  TEST_ASSERT_TRUE(doc.root()["acked"].boolean(false));
  TEST_ASSERT_EQUAL(4, doc.root()["ackCount"].integer(0));
  TEST_ASSERT_EQUAL(5, doc.root()["ackTotal"].integer(0));
  json::Doc board;
  parse(board, boardViewOf(g));
  TEST_ASSERT_EQUAL(4, board.root()["acted"].integer(0));
  TEST_ASSERT_EQUAL(5, board.root()["actors"].integer(0));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Role), static_cast<int>(g.phase()));
  TEST_ASSERT_EQUAL_STRING("not_allowed", ack(g, 7));  // not seated
  const int changes = host.phaseChanges;
  TEST_ASSERT_NULL(ack(g, 4));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  TEST_ASSERT_EQUAL_UINT32(id + 1, g.phaseId());
  TEST_ASSERT_EQUAL(changes + 1, host.phaseChanges);
  TEST_ASSERT_EQUAL_UINT32(Vampires::kNightMinMs, host.timer);
  TEST_ASSERT_EQUAL(1, static_cast<int>(host.buzzes.size()));
  TEST_ASSERT_EQUAL(firstN(5), host.buzzes[0].players);
  TEST_ASSERT_EQUAL_STRING(Vampires::kNightStartBuzz, host.buzzes[0].pattern.c_str());
  TEST_ASSERT_EQUAL_STRING("not_allowed", sendMsg(g, 0, "ack", g.phaseId(), 0, false));
}

void test_role_phase_skips_offline_timeout_and_admin() {
  {
    FakeHost host;
    Vampires g(host);
    setupWith(g, firstN(6));
    host.offline = 1u << 5;
    for (int p = 0; p < 5; ++p) ack(g, p);
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  }
  {
    FakeHost host;
    Vampires g(host);
    setupWith(g, firstN(6));
    TEST_ASSERT_TRUE(host.fire(g));
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  }
  {
    FakeHost host;
    Vampires g(host);
    setupWith(g, firstN(6));
    TEST_ASSERT_NULL(g.adminSkip());
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
    TEST_ASSERT_EQUAL_STRING("not_allowed", g.adminSkip());
  }
}

// ---------------------------------------------------------------- night

void test_night_timing() {
  FakeHost host;
  Vampires g(host);
  setupWith(g, firstN(5));
  g.adminSkip();
  TEST_ASSERT_TRUE(findRole(g, Role::Vampire) >= 0);
  // Everyone picks early: the night still waits for its 25 s minimum.
  for (int p = 0; p < 5; ++p) {
    json::Doc doc;
    parse(doc, privateViewOf(g, p));
    const json::Value targets = doc.root()["task"]["targets"];
    TEST_ASSERT_TRUE(targets.size() > 0);
    TEST_ASSERT_NULL(act(g, p, targets[size_t{0}].integer(-1)));
  }
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  TEST_ASSERT_TRUE(host.fire(g));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Dawn), static_cast<int>(g.phase()));
  TEST_ASSERT_EQUAL_STRING(Vampires::kNightEndBuzz, host.buzzes.back().pattern.c_str());
  TEST_ASSERT_EQUAL(firstN(5), host.buzzes.back().players);

  // Not everyone picked at 25 s: wait for the last pick, or 60 s.
  skipTo(g, Phase::Verdict);
  TEST_ASSERT_TRUE(host.fire(g));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  const uint32_t nightId = g.phaseId();
  TEST_ASSERT_TRUE(host.fire(g));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  TEST_ASSERT_EQUAL_UINT32(nightId, g.phaseId());
  TEST_ASSERT_EQUAL_UINT32(Vampires::kNightMaxMs - Vampires::kNightMinMs, host.timer);
  int lastPicker = -1;
  for (int p = 0; p < 5; ++p) {
    if (!hasPid(g.aliveMask(), p)) continue;
    json::Doc doc;
    parse(doc, privateViewOf(g, p));
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
    TEST_ASSERT_NULL(act(g, p, doc.root()["task"]["targets"][size_t{0}].integer(-1)));
    lastPicker = p;
  }
  TEST_ASSERT_TRUE(lastPicker >= 0);
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Dawn), static_cast<int>(g.phase()));
}

void test_night_kill_protect_and_doctor_ban() {
  FakeHost host;
  Vampires g(host);
  setupWith(g, firstN(5));
  const int v = findRole(g, Role::Vampire);
  const int d = findRole(g, Role::Doctor);
  const int s = findRole(g, Role::Seer);
  const int a = findRole(g, Role::Villager, 0);
  const int b = findRole(g, Role::Villager, 1);
  g.adminSkip();

  // Night 1: the doctor saves the vampires' target.
  TEST_ASSERT_EQUAL_STRING("bad_target", act(g, v, v));
  TEST_ASSERT_EQUAL_STRING("bad_target", act(g, s, s));
  TEST_ASSERT_EQUAL_STRING("bad_target", act(g, a, a));
  TEST_ASSERT_EQUAL_STRING("bad_target", act(g, a, 9));
  TEST_ASSERT_EQUAL_STRING("bad_target", act(g, a, -1));
  TEST_ASSERT_NULL(act(g, v, b));
  TEST_ASSERT_NULL(act(g, v, a));  // picks change until the night ends
  TEST_ASSERT_NULL(act(g, d, a));
  TEST_ASSERT_NULL(act(g, s, v));
  TEST_ASSERT_NULL(act(g, a, b));
  TEST_ASSERT_NULL(act(g, b, a));
  endNight(host, g);
  TEST_ASSERT_EQUAL(-1, g.died());
  TEST_ASSERT_EQUAL(firstN(5), g.aliveMask());
  TEST_ASSERT_EQUAL(a, g.lastProtect());

  json::Doc seer;
  parse(seer, privateViewOf(g, s));
  const json::Value seen = seer.root()["seen"];
  TEST_ASSERT_EQUAL(1, static_cast<int>(seen.size()));
  TEST_ASSERT_EQUAL(v, seen[size_t{0}]["pid"].integer(-1));
  TEST_ASSERT_TRUE(seen[size_t{0}]["vampire"].boolean(false));
  const json::Value dawn = seer.root()["dawn"];
  TEST_ASSERT_EQUAL(-1, dawn["died"].integer(0));
  TEST_ASSERT_EQUAL(2, static_cast<int>(dawn["suspicion"].size()));  // a and b, one each

  // Night 2: the doctor may not protect a again but may protect himself.
  skipTo(g, Phase::Verdict);
  g.adminSkip();
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  TEST_ASSERT_EQUAL(2, g.round());
  json::Doc doc;
  parse(doc, privateViewOf(g, d));
  TEST_ASSERT_EQUAL_STRING("protect", doc.root()["task"]["kind"].str());
  TEST_ASSERT_FALSE(arrayHas(doc.root()["task"]["targets"], a));
  TEST_ASSERT_TRUE(arrayHas(doc.root()["task"]["targets"], d));
  TEST_ASSERT_EQUAL_STRING("bad_target", act(g, d, a));
  TEST_ASSERT_NULL(act(g, d, d));
  TEST_ASSERT_NULL(act(g, v, b));
  endNight(host, g);
  TEST_ASSERT_EQUAL(b, g.died());
  TEST_ASSERT_FALSE(hasPid(g.aliveMask(), b));
  TEST_ASSERT_EQUAL(d, g.lastProtect());

  // Night 3: the ban holds; the doctor does not pick → no ban in night 4.
  skipTo(g, Phase::Verdict);
  g.adminSkip();
  TEST_ASSERT_EQUAL_STRING("bad_target", act(g, d, d));
  endNight(host, g);
  TEST_ASSERT_EQUAL(-1, g.lastProtect());
  TEST_ASSERT_EQUAL(-1, g.died());  // the vampire did not pick: no kill
  skipTo(g, Phase::Verdict);
  g.adminSkip();
  TEST_ASSERT_NULL(act(g, d, d));
  // The dead never act.
  TEST_ASSERT_EQUAL_STRING("not_allowed", act(g, b, a));
  json::Doc dead;
  parse(dead, privateViewOf(g, b));
  TEST_ASSERT_FALSE(hasKey(dead.root(), "task"));
}

void test_vampire_tie_is_random() {
  std::set<int> outcomes;
  for (uint64_t seed = 1; seed <= 60; ++seed) {
    FakeHost host(seed);
    Vampires g(host);
    setupWith(g, firstN(7));
    g.adminSkip();
    const int v1 = findRole(g, Role::Vampire, 0);
    const int v2 = findRole(g, Role::Vampire, 1);
    const int t1 = findRole(g, Role::Villager, 0);
    const int t2 = findRole(g, Role::Villager, 1);
    TEST_ASSERT_NULL(act(g, v1, t1));
    TEST_ASSERT_NULL(act(g, v2, t2));
    endNight(host, g);
    TEST_ASSERT_TRUE(g.died() == t1 || g.died() == t2);
    outcomes.insert(g.died() == t1 ? 1 : 2);
  }
  TEST_ASSERT_EQUAL(2, static_cast<int>(outcomes.size()));

  // Plurality: 3 vampires, two on the same target.
  for (uint64_t seed = 1; seed <= 20; ++seed) {
    FakeHost host(seed);
    Vampires g(host);
    setupWith(g, firstN(10));
    g.adminSkip();
    const int t1 = findRole(g, Role::Villager, 0);
    const int t2 = findRole(g, Role::Villager, 1);
    TEST_ASSERT_NULL(act(g, findRole(g, Role::Vampire, 0), t1));
    TEST_ASSERT_NULL(act(g, findRole(g, Role::Vampire, 1), t2));
    TEST_ASSERT_NULL(act(g, findRole(g, Role::Vampire, 2), t2));
    endNight(host, g);
    TEST_ASSERT_EQUAL(t2, g.died());
  }
}

// ---------------------------------------------------------------- vote

void test_vote_runoff_and_lynch() {
  FakeHost host;
  Vampires g(host);
  setupWith(g, firstN(5));
  const int v = findRole(g, Role::Vampire);
  const int d = findRole(g, Role::Doctor);
  const int s = findRole(g, Role::Seer);
  const int a = findRole(g, Role::Villager, 0);
  const int b = findRole(g, Role::Villager, 1);
  g.adminSkip();
  endNight(host, g);
  skipTo(g, Phase::Vote);

  json::Doc doc;
  parse(doc, privateViewOf(g, a));
  TEST_ASSERT_EQUAL(1, doc.root()["vote"]["round"].integer(0));
  TEST_ASSERT_EQUAL(5, static_cast<int>(doc.root()["vote"]["candidates"].size()));
  TEST_ASSERT_EQUAL(-2, doc.root()["vote"]["mine"].integer(0));

  // Round 1: a and b tied with 2 each, one "nobody".
  TEST_ASSERT_EQUAL_STRING("bad_target", vote(g, v, 12));
  TEST_ASSERT_NULL(vote(g, v, b));
  TEST_ASSERT_NULL(vote(g, v, a));  // changeable
  TEST_ASSERT_NULL(vote(g, d, a));
  TEST_ASSERT_NULL(vote(g, s, b));
  TEST_ASSERT_NULL(vote(g, a, b));
  parse(doc, privateViewOf(g, v));
  TEST_ASSERT_EQUAL(a, doc.root()["vote"]["mine"].integer(0));
  TEST_ASSERT_EQUAL(4, doc.root()["vote"]["cast"].integer(0));
  TEST_ASSERT_EQUAL(5, doc.root()["vote"]["voters"].integer(0));
  json::Doc board;
  parse(board, boardViewOf(g));
  TEST_ASSERT_FALSE(hasKey(board.root()["vote"], "mine"));
  TEST_ASSERT_NULL(vote(g, b, -1));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Verdict), static_cast<int>(g.phase()));
  TEST_ASSERT_TRUE(g.runoff());
  TEST_ASSERT_EQUAL(-1, g.lynched());
  parse(doc, privateViewOf(g, s));
  const json::Value tally = doc.root()["tally"];
  TEST_ASSERT_TRUE(tally["runoff"].boolean(false));
  TEST_ASSERT_EQUAL(-1, tally["lynched"].integer(0));
  TEST_ASSERT_EQUAL(5, static_cast<int>(tally["votes"].size()));
  TEST_ASSERT_FALSE(hasKey(tally, "role"));

  // Round 2 among the tied (plus nobody); still tied → nobody.
  TEST_ASSERT_TRUE(host.fire(g));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Vote), static_cast<int>(g.phase()));
  TEST_ASSERT_EQUAL(2, g.voteRound());
  parse(doc, privateViewOf(g, a));
  TEST_ASSERT_EQUAL(2, static_cast<int>(doc.root()["vote"]["candidates"].size()));
  TEST_ASSERT_EQUAL_STRING("bad_target", vote(g, a, v));
  TEST_ASSERT_NULL(vote(g, v, a));
  TEST_ASSERT_NULL(vote(g, d, a));
  TEST_ASSERT_NULL(vote(g, s, b));
  TEST_ASSERT_NULL(vote(g, a, b));
  TEST_ASSERT_NULL(vote(g, b, -1));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Verdict), static_cast<int>(g.phase()));
  TEST_ASSERT_FALSE(g.runoff());
  TEST_ASSERT_EQUAL(-1, g.lynched());
  TEST_ASSERT_EQUAL(firstN(5), g.aliveMask());

  // Next day: "nobody" as strong as the leader → nobody.
  TEST_ASSERT_TRUE(host.fire(g));
  endNight(host, g);
  skipTo(g, Phase::Vote);
  vote(g, v, a);
  vote(g, d, a);
  vote(g, s, -1);
  vote(g, a, -1);
  TEST_ASSERT_NULL(g.adminSkip());  // b abstains
  TEST_ASSERT_EQUAL(-1, g.lynched());
  TEST_ASSERT_FALSE(g.runoff());
  parse(doc, privateViewOf(g, a));
  TEST_ASSERT_EQUAL(4, static_cast<int>(doc.root()["tally"]["votes"].size()));

  // Next day: the vampire is lynched; his role is revealed and the village wins.
  TEST_ASSERT_TRUE(host.fire(g));
  endNight(host, g);
  skipTo(g, Phase::Vote);
  vote(g, d, v);
  vote(g, s, v);
  vote(g, a, -1);
  TEST_ASSERT_NULL(g.adminSkip());
  TEST_ASSERT_EQUAL(v, g.lynched());
  parse(doc, privateViewOf(g, a));
  TEST_ASSERT_EQUAL_STRING("vampire", doc.root()["tally"]["role"].str());
  TEST_ASSERT_EQUAL_STRING("vampire", shownRole(doc.root(), v));
  parse(board, boardViewOf(g));
  TEST_ASSERT_EQUAL_STRING("vampire", shownRole(board.root(), v));
  TEST_ASSERT_FALSE(g.finished());
  TEST_ASSERT_TRUE(host.fire(g));
  TEST_ASSERT_TRUE(g.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(Vampires::Winner::Village), static_cast<int>(g.winner()));
  TEST_ASSERT_TRUE(g.won(d));
  TEST_ASSERT_TRUE(g.won(a));
  TEST_ASSERT_FALSE(g.won(v));
  TEST_ASSERT_EQUAL_STRING("not_allowed", g.adminSkip());
  TEST_ASSERT_EQUAL_UINT32(0, host.timer);
  parse(doc, privateViewOf(g, a));
  TEST_ASSERT_EQUAL_STRING("village", doc.root()["winner"].str());
  for (int p = 0; p < 5; ++p) TEST_ASSERT_NOT_NULL(shownRole(doc.root(), p));
}

void test_vampires_win() {
  FakeHost host;
  Vampires g(host);
  setupWith(g, firstN(5), "{\"revealRole\":false}");
  const int v = findRole(g, Role::Vampire);
  const int d = findRole(g, Role::Doctor);
  const int s = findRole(g, Role::Seer);
  const int a = findRole(g, Role::Villager, 0);
  g.adminSkip();
  act(g, v, a);
  endNight(host, g);
  TEST_ASSERT_EQUAL(a, g.died());
  skipTo(g, Phase::Vote);
  vote(g, v, s);
  vote(g, d, s);
  g.adminSkip();
  TEST_ASSERT_EQUAL(s, g.lynched());
  json::Doc doc;
  parse(doc, privateViewOf(g, d));
  TEST_ASSERT_FALSE(hasKey(doc.root()["tally"], "role"));
  TEST_ASSERT_NULL(shownRole(doc.root(), s));
  TEST_ASSERT_TRUE(host.fire(g));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  act(g, v, d);
  endNight(host, g);
  TEST_ASSERT_TRUE(host.fire(g));  // dawn → win check
  TEST_ASSERT_TRUE(g.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(Vampires::Winner::Vampires), static_cast<int>(g.winner()));
  TEST_ASSERT_TRUE(g.won(v));
  TEST_ASSERT_FALSE(g.won(d));
  TEST_ASSERT_FALSE(g.won(9));
}

// ---------------------------------------------------------------- views

void test_hidden_information() {
  for (uint64_t seed = 1; seed <= 10; ++seed) {
    FakeHost host(seed);
    Vampires g(host);
    setupWith(g, firstN(9));
    g.adminSkip();
    for (int me = 0; me < 9; ++me) {
      json::Doc doc;
      parse(doc, privateViewOf(g, me));
      const json::Value view = doc.root();
      const bool vampire = g.roleOf(me) == Role::Vampire;
      for (int p = 0; p < 9; ++p) {
        const bool visible = shownRole(view, p) != nullptr;
        const bool allowed = p == me || (vampire && g.roleOf(p) == Role::Vampire);
        TEST_ASSERT_EQUAL(allowed, visible);
      }
      TEST_ASSERT_EQUAL(vampire, hasKey(view, "allies"));
      TEST_ASSERT_EQUAL(g.roleOf(me) == Role::Seer, hasKey(view, "seen"));
      if (vampire) {
        const json::Value allies = view["allies"];
        TEST_ASSERT_EQUAL(1, static_cast<int>(allies.size()));
        TEST_ASSERT_EQUAL(Role::Vampire, g.roleOf(allies[size_t{0}]["pid"].integer(-1)));
      }
    }
    // Spectator: only the public keys.
    json::Doc spec;
    parse(spec, privateViewOf(g, 12));
    const json::Value sv = spec.root();
    TEST_ASSERT_FALSE(sv["seated"].boolean(true));
    TEST_ASSERT_EQUAL(6, static_cast<int>(sv.size()));
    for (const char* k : {"id", "phase", "phaseId", "round", "seated", "players"}) TEST_ASSERT_TRUE(hasKey(sv, k));
    for (int p = 0; p < 9; ++p) TEST_ASSERT_NULL(shownRole(sv, p));
    json::Doc board;
    parse(board, boardViewOf(g));
    for (int p = 0; p < 9; ++p) TEST_ASSERT_NULL(shownRole(board.root(), p));
    TEST_ASSERT_FALSE(hasKey(board.root(), "task"));
    TEST_ASSERT_EQUAL(9, board.root()["actors"].integer(0));
  }
}

void test_ghosts_see_roles() {
  for (int option = 0; option < 2; ++option) {
    FakeHost host;
    Vampires g(host);
    setupWith(g, firstN(6), option ? "{\"ghostsSeeRoles\":false}" : "{}");
    const int v = findRole(g, Role::Vampire);
    const int a = findRole(g, Role::Villager, 0);
    g.adminSkip();
    act(g, v, a);
    endNight(host, g);
    TEST_ASSERT_EQUAL(a, g.died());
    json::Doc doc;
    parse(doc, privateViewOf(g, a));
    TEST_ASSERT_FALSE(doc.root()["alive"].boolean(true));
    TEST_ASSERT_EQUAL(option == 0, shownRole(doc.root(), v) != nullptr);
    // A night kill is never revealed to the living.
    json::Doc living;
    parse(living, privateViewOf(g, findRole(g, Role::Seer)));
    TEST_ASSERT_NULL(shownRole(living.root(), a));
  }
}

void test_late_and_bad_messages() {
  FakeHost host;
  Vampires g(host);
  setupWith(g, firstN(5));
  const uint32_t roleId = g.phaseId();
  g.adminSkip();
  const int v = findRole(g, Role::Vampire);
  const int a = findRole(g, Role::Villager);
  TEST_ASSERT_EQUAL_STRING("late", sendMsg(g, v, "act", roleId, a));
  TEST_ASSERT_EQUAL_STRING("late", sendMsg(g, v, "act", g.phaseId() + 1, a));
  TEST_ASSERT_EQUAL_STRING("bad_msg", sendMsg(g, v, "act", g.phaseId(), 0, false));
  TEST_ASSERT_EQUAL_STRING("bad_msg", sendMsg(g, v, "dance", g.phaseId(), a));
  TEST_ASSERT_EQUAL_STRING("not_allowed", sendMsg(g, v, "vote", g.phaseId(), a));
  TEST_ASSERT_EQUAL_STRING("not_allowed", sendMsg(g, 11, "act", g.phaseId(), a));
  json::Doc doc;
  const char* text = "{\"t\":\"act\",\"target\":1}";
  doc.parse(text, strlen(text));
  TEST_ASSERT_EQUAL_STRING("bad_msg", g.onAction(v, doc.root()));
  TEST_ASSERT_EQUAL(-1, g.pickOf(v));
}

// ---------------------------------------------------------------- snapshot

void test_save_load_round_trip() {
  FakeHost host(7);
  Vampires g(host);
  setupWith(g, firstN(8), "{\"discussionSec\":90}");
  g.adminSkip();
  const int v = findRole(g, Role::Vampire);
  const int d = findRole(g, Role::Doctor);
  const int a = findRole(g, Role::Villager);
  act(g, v, a);
  act(g, d, d);
  TEST_ASSERT_TRUE(host.fire(g));  // 25 s passed, not everyone picked
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g.phase()));
  const std::string blob = g.save();

  FakeHost host2(99);
  Vampires g2(host2);
  TEST_ASSERT_TRUE(g2.load(reinterpret_cast<const uint8_t*>(blob.data()), blob.size()));
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Night), static_cast<int>(g2.phase()));
  TEST_ASSERT_EQUAL_UINT32(g.phaseId(), g2.phaseId());
  TEST_ASSERT_EQUAL_UINT32(Vampires::kNightMaxMs - Vampires::kNightMinMs, host2.timer);
  for (int p = -1; p < kMaxPlayers; ++p)
    TEST_ASSERT_EQUAL_STRING(privateViewOf(g, p).c_str(), privateViewOf(g2, p).c_str());
  TEST_ASSERT_EQUAL_STRING(boardViewOf(g).c_str(), boardViewOf(g2).c_str());
  TEST_ASSERT_EQUAL(static_cast<int>(blob.size()), static_cast<int>(g2.save().size()));
  TEST_ASSERT_TRUE(blob == g2.save());

  // The resumed game goes on: the remaining picks end the night.
  for (int p = 0; p < 8; ++p) {
    if (g2.pickOf(p) >= 0) continue;
    json::Doc doc;
    parse(doc, privateViewOf(g2, p));
    const int target = g2.roleOf(p) == Role::Vampire ? a : doc.root()["task"]["targets"][size_t{0}].integer(-1);
    TEST_ASSERT_NULL(act(g2, p, target));
  }
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Dawn), static_cast<int>(g2.phase()));
  TEST_ASSERT_EQUAL(a, g2.died());
  TEST_ASSERT_EQUAL(d, g2.lastProtect());
  host2.fire(g2);
  TEST_ASSERT_EQUAL_UINT32(90000, host2.timer);

  // Snapshot of the role phase re-arms the role timer.
  FakeHost host3;
  Vampires g3(host3);
  setupWith(g3, firstN(5));
  const std::string roleBlob = g3.save();
  FakeHost host4;
  Vampires g4(host4);
  TEST_ASSERT_TRUE(g4.load(reinterpret_cast<const uint8_t*>(roleBlob.data()), roleBlob.size()));
  TEST_ASSERT_EQUAL_UINT32(Vampires::kRoleMs, host4.timer);
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Role), static_cast<int>(g4.phase()));
}

void test_load_rejects_bad_blobs() {
  FakeHost host;
  Vampires g(host);
  setupWith(g, firstN(5));
  const std::string blob = g.save();
  FakeHost host2;
  Vampires g2(host2);
  const auto* data = reinterpret_cast<const uint8_t*>(blob.data());
  TEST_ASSERT_FALSE(g2.load(data, blob.size() - 1));
  TEST_ASSERT_FALSE(g2.load(data, 0));
  TEST_ASSERT_FALSE(g2.load(nullptr, blob.size()));
  std::string wrongVersion = blob;
  wrongVersion[0] = static_cast<char>(wrongVersion[0] + 1);
  TEST_ASSERT_FALSE(g2.load(reinterpret_cast<const uint8_t*>(wrongVersion.data()), wrongVersion.size()));
  std::string wrongSize = blob;
  wrongSize[4] = static_cast<char>(wrongSize[4] + 1);
  TEST_ASSERT_FALSE(g2.load(reinterpret_cast<const uint8_t*>(wrongSize.data()), wrongSize.size()));
  std::string longer = blob + "x";
  TEST_ASSERT_FALSE(g2.load(reinterpret_cast<const uint8_t*>(longer.data()), longer.size()));
  TEST_ASSERT_EQUAL_UINT32(0, host2.timerSets);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_registry);
  RUN_TEST(test_role_counts);
  RUN_TEST(test_setup_rejects_player_counts);
  RUN_TEST(test_options_defaults_and_ranges);
  RUN_TEST(test_role_phase_acks);
  RUN_TEST(test_role_phase_skips_offline_timeout_and_admin);
  RUN_TEST(test_night_timing);
  RUN_TEST(test_night_kill_protect_and_doctor_ban);
  RUN_TEST(test_vampire_tie_is_random);
  RUN_TEST(test_vote_runoff_and_lynch);
  RUN_TEST(test_vampires_win);
  RUN_TEST(test_hidden_information);
  RUN_TEST(test_ghosts_see_roles);
  RUN_TEST(test_late_and_bad_messages);
  RUN_TEST(test_save_load_round_trip);
  RUN_TEST(test_load_rejects_bad_blobs);
  return UNITY_END();
}
