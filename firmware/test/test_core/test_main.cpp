// Unit tests for core/: JSON, Hub (sessions) and Room (profiles, lobby, admin,
// snapshots). Run: cd firmware && pio test -e native

#include <unity.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/hub.h"
#include "core/json.h"
#include "core/room.h"

namespace {

class FakePlatform : public Platform {
 public:
  const char* ssid() const override { return "PartyGames-TEST"; }
  const char* wifiPassword() const override { return "partygames"; }
  void restart() override { ++restarts; }
  int restarts = 0;
};

class FakeTransport : public Transport {
 public:
  void send(int conn, const char* text, size_t len) override { sent[conn].emplace_back(text, len); }
  void close(int conn) override { closed.push_back(conn); }

  std::string last(int conn, const char* type) const {
    auto it = sent.find(conn);
    if (it == sent.end()) return "";
    const std::string needle = std::string("{\"t\":\"") + type + "\"";
    for (auto m = it->second.rbegin(); m != it->second.rend(); ++m)
      if (m->rfind(needle, 0) == 0) return *m;
    return "";
  }

  std::map<int, std::vector<std::string>> sent;
  std::vector<int> closed;
};

// A box with a hub, one socket per phone (conn = phone index + 1).
struct Box {
  MemStore store;
  FakePlatform platform;
  FakeTransport transport;
  Room room{store, platform, 42};
  Hub hub{room, transport};
  uint32_t now = 1000;

  Box() { room.begin(now); }

  void connect(int conn, const std::string& id) {
    TEST_ASSERT_TRUE(hub.opened(conn, now));
    say(conn, "{\"t\":\"hello\",\"id\":\"" + id + "\"}");
  }
  void say(int conn, const std::string& text) { hub.text(conn, text.data(), text.size(), now); }
  void profile(int conn, const char* name) {
    say(conn, std::string("{\"t\":\"profile\",\"name\":\"") + name + "\",\"avatar\":\"🦊\",\"color\":1}");
  }

  // Parsed "view" object of the last view message sent to conn.
  json::Value view(int conn) {
    const std::string text = transport.last(conn, "view");
    TEST_ASSERT_TRUE_MESSAGE(!text.empty(), "no view sent");
    TEST_ASSERT_TRUE(doc.parse(text.data(), text.size()));
    return doc.root()["view"];
  }
  std::string lastErr(int conn) {
    const std::string text = transport.last(conn, "err");
    json::Doc d;
    if (text.empty() || !d.parse(text.data(), text.size())) return "";
    return d.root()["code"].str();
  }

  json::Doc doc;
};

std::string phoneId(int i) { return "phone" + std::to_string(i) + "abcdef"; }

}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- json

void test_json_parse_and_unescape() {
  json::Doc d;
  const char* text = R"({"t":"x","n":-12.5e1,"b":true,"z":null,"s":"a\"b\\c\u00e7\ud83e\udd8a","a":[1,{"k":2}]})";
  TEST_ASSERT_TRUE(d.parse(text, strlen(text)));
  const json::Value r = d.root();
  TEST_ASSERT_EQUAL_STRING("x", r["t"].str());
  TEST_ASSERT_TRUE(r["n"].num() == -125.0);
  TEST_ASSERT_TRUE(r["b"].boolean());
  TEST_ASSERT_TRUE(r["z"].isNull());
  TEST_ASSERT_EQUAL_STRING("a\"b\\c\xC3\xA7\xF0\x9F\xA6\x8A", r["s"].str());
  TEST_ASSERT_EQUAL(2, r["a"].size());
  TEST_ASSERT_EQUAL(2, r["a"][1]["k"].integer(-1));
  TEST_ASSERT_TRUE(r["missing"]["deeper"].isNull());
  TEST_ASSERT_EQUAL(-125, r["n"].integer(0));  // -12.5e1 is integral
  TEST_ASSERT_EQUAL(7, r["s"].integer(7));     // wrong type -> fallback
}

void test_json_rejects_malformed() {
  const char* bad[] = {"", "{", "{\"a\":}", "[1,]", "{\"a\":1}x", "\"\\u0000\"", "01", "{'a':1}",
                       "[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]", "\"\x01\""};
  for (const char* text : bad) {
    json::Doc d;
    TEST_ASSERT_FALSE_MESSAGE(d.parse(text, strlen(text)), text);
  }
}

void test_json_writer_escapes_and_separates() {
  std::string out;
  json::Writer w(out);
  w.beginObject().field("a", "q\"\n\x01").key("b").beginArray().value(1).value(true).null().endArray();
  w.key("c").beginObject().endObject().key("d").raw("[1,2]").endObject();
  TEST_ASSERT_EQUAL_STRING("{\"a\":\"q\\\"\\n\\u0001\",\"b\":[1,true,null],\"c\":{},\"d\":[1,2]}", out.c_str());
  json::Doc d;
  TEST_ASSERT_TRUE(d.parse(out.data(), out.size()));
}

// ---------------------------------------------------------------- hub

void test_hub_requires_hello_and_answers_ping() {
  Box b;
  TEST_ASSERT_TRUE(b.hub.opened(1, b.now));
  b.say(1, "{\"t\":\"ready\",\"on\":true}");
  TEST_ASSERT_EQUAL_STRING("hello_first", b.lastErr(1).c_str());
  // A ping without hello too, so a phone whose hello was lost says it again.
  b.transport.sent.clear();
  b.say(1, "{\"t\":\"ping\",\"c\":1}");
  TEST_ASSERT_TRUE(b.transport.last(1, "pong").empty());
  TEST_ASSERT_EQUAL_STRING("hello_first", b.lastErr(1).c_str());
  b.say(1, "{\"t\":\"hello\",\"id\":\"bad id!\"}");
  TEST_ASSERT_EQUAL_STRING("bad_msg", b.lastErr(1).c_str());
  b.say(1, "not json");
  TEST_ASSERT_EQUAL_STRING("bad_msg", b.lastErr(1).c_str());
  b.say(1, "{\"t\":\"hello\",\"id\":\"abc\"}");
  TEST_ASSERT_FALSE(b.transport.last(1, "welcome").empty());
  TEST_ASSERT_EQUAL(-1, b.view(1)["me"].integer(0));  // no profile yet
  b.say(1, "{\"t\":\"ping\",\"c\":123456}");
  TEST_ASSERT_EQUAL_STRING("{\"t\":\"pong\",\"c\":123456}", b.transport.last(1, "pong").c_str());
}

void test_hub_new_socket_replaces_old_one() {
  Box b;
  b.connect(1, "same");
  b.connect(2, "same");
  TEST_ASSERT_EQUAL(1, static_cast<int>(b.transport.closed.size()));
  TEST_ASSERT_EQUAL(1, b.transport.closed[0]);
  b.hub.closed(1, b.now);
  TEST_ASSERT_EQUAL(1, b.hub.boundCount());
}

void test_hub_closes_idle_sockets() {
  Box b;
  b.connect(1, "a");
  b.now += Hub::kIdleCloseMs - 1;
  b.hub.tick(b.now);
  TEST_ASSERT_TRUE(b.transport.closed.empty());
  b.now += 2;
  b.hub.tick(b.now);
  TEST_ASSERT_EQUAL(1, static_cast<int>(b.transport.closed.size()));
}

// ---------------------------------------------------------------- room

void test_profile_validation() {
  Box b;
  b.connect(1, "a");
  b.say(1, R"({"t":"profile","name":"   ","avatar":"x","color":1})");
  TEST_ASSERT_EQUAL_STRING("bad_name", b.lastErr(1).c_str());
  b.say(1, R"({"t":"profile","name":"abcdefghijklmnopq","avatar":"x","color":1})");
  TEST_ASSERT_EQUAL_STRING("bad_name", b.lastErr(1).c_str());
  b.say(1, R"({"t":"profile","name":"a\u0007b","avatar":"x","color":1})");
  TEST_ASSERT_EQUAL_STRING("bad_name", b.lastErr(1).c_str());
  b.say(1, R"({"t":"profile","name":"Ada","avatar":"","color":1})");
  TEST_ASSERT_EQUAL_STRING("bad_avatar", b.lastErr(1).c_str());
  b.say(1, R"({"t":"profile","name":"Ada","avatar":"x","color":12})");
  TEST_ASSERT_EQUAL_STRING("bad_color", b.lastErr(1).c_str());
  b.say(1, R"({"t":"ready","on":true})");
  TEST_ASSERT_EQUAL_STRING("no_profile", b.lastErr(1).c_str());

  // 16 characters of Turkish text (multi-byte) is fine; surrounding spaces are trimmed.
  b.say(1, R"({"t":"profile","name":"  Şükrü Çağlayanğ  ","avatar":"🦊","color":11})");
  json::Value v = b.view(1);
  TEST_ASSERT_EQUAL(0, v["me"].integer(-1));
  TEST_ASSERT_EQUAL_STRING("Şükrü Çağlayanğ", v["profile"]["name"].str());
  TEST_ASSERT_EQUAL(1, v["players"].size());
}

void test_views_never_contain_client_ids() {
  Box b;
  b.connect(1, phoneId(1));
  b.profile(1, "Ada");
  b.connect(2, phoneId(2));
  b.profile(2, "Bob");
  const std::string text = b.transport.last(1, "view");
  TEST_ASSERT_EQUAL(std::string::npos, text.find(phoneId(2)));
  TEST_ASSERT_EQUAL(2, b.view(1)["players"].size());
}

void test_admin_claim_transfer_and_reset() {
  Box b;
  b.connect(1, "a");
  b.profile(1, "Ada");
  b.connect(2, "b");
  b.profile(2, "Bob");
  TEST_ASSERT_EQUAL(-1, b.view(1)["admin"].integer(0));
  TEST_ASSERT_TRUE(b.view(1)["box"].isNull());

  b.say(2, R"({"t":"start","game":"vampires"})");
  TEST_ASSERT_EQUAL_STRING("not_admin", b.lastErr(2).c_str());
  b.say(1, R"({"t":"claimAdmin"})");
  b.say(2, R"({"t":"claimAdmin"})");
  TEST_ASSERT_EQUAL_STRING("admin_taken", b.lastErr(2).c_str());
  TEST_ASSERT_TRUE(b.view(1)["isAdmin"].boolean());
  TEST_ASSERT_EQUAL_STRING("PartyGames-TEST", b.view(1)["box"]["ssid"].str());
  TEST_ASSERT_TRUE(b.view(2)["box"].isNull());  // only the admin sees the password

  b.say(1, R"({"t":"transfer","pid":1})");
  TEST_ASSERT_EQUAL(1, b.view(2)["admin"].integer(-1));
  TEST_ASSERT_TRUE(b.view(2)["isAdmin"].boolean());
  TEST_ASSERT_FALSE(b.view(1)["isAdmin"].boolean());

  b.room.resetAdmin();
  b.hub.tick(b.now);
  TEST_ASSERT_EQUAL(-1, b.view(1)["admin"].integer(0));
  b.say(1, R"({"t":"claimAdmin"})");
  TEST_ASSERT_EQUAL(0, b.view(2)["admin"].integer(-1));
}

void test_kick_and_settings() {
  Box b;
  b.connect(1, "a");
  b.profile(1, "Ada");
  b.connect(2, "b");
  b.profile(2, "Bob");
  b.say(1, R"({"t":"claimAdmin"})");
  b.say(1, R"({"t":"kick","pid":0})");
  TEST_ASSERT_EQUAL_STRING("bad_pid", b.lastErr(1).c_str());  // not yourself
  b.say(1, R"({"t":"kick","pid":1})");
  TEST_ASSERT_EQUAL(-1, b.view(2)["me"].integer(0));
  TEST_ASSERT_EQUAL(1, b.view(1)["players"].size());
  // The kicked phone keeps its profile and may join again.
  TEST_ASSERT_EQUAL_STRING("Bob", b.view(2)["profile"]["name"].str());

  b.say(1, R"({"t":"settings","wifiPass":"short"})");
  TEST_ASSERT_EQUAL_STRING("bad_pass", b.lastErr(1).c_str());
  b.say(1, R"({"t":"settings","wifiPass":"long enough pass"})");
  char pass[64] = {};
  TEST_ASSERT_TRUE(b.store.load(kKeyWifiPass, pass, sizeof pass) > 0);
  TEST_ASSERT_EQUAL_STRING("long enough pass", pass);
  b.say(1, R"({"t":"restart"})");
  TEST_ASSERT_EQUAL(1, b.platform.restarts);
}

void test_start_needs_ready_players() {
  Box b;
  for (int i = 1; i <= 5; ++i) {
    b.connect(i, phoneId(i));
    b.profile(i, ("P" + std::to_string(i)).c_str());
  }
  b.say(1, R"({"t":"claimAdmin"})");
  b.say(1, R"({"t":"start","game":"chess"})");
  TEST_ASSERT_EQUAL_STRING("bad_game", b.lastErr(1).c_str());
  for (int i = 1; i <= 4; ++i) b.say(i, R"({"t":"ready","on":true})");
  b.say(1, R"({"t":"start","game":"vampires"})");
  TEST_ASSERT_EQUAL_STRING("too_few", b.lastErr(1).c_str());
  b.say(5, R"({"t":"ready","on":true})");
  b.say(1, R"({"t":"start","game":"vampires","options":{"discussionSec":30}})");
  json::Value v = b.view(3);
  TEST_ASSERT_TRUE(v["game"]["seated"].boolean());
  TEST_ASSERT_EQUAL_STRING("role", v["game"]["phase"].str());
  TEST_ASSERT_TRUE(v["endsIn"].integer(-1) > 0);
  TEST_ASSERT_TRUE(v["board"].isNull());       // seated non-admin: no board
  TEST_ASSERT_FALSE(b.view(1)["board"].isNull());  // admin gets the table view
  b.say(2, R"({"t":"ready","on":false})");
  TEST_ASSERT_EQUAL_STRING("in_game", b.lastErr(2).c_str());
  b.say(1, R"({"t":"kick","pid":4})");
  TEST_ASSERT_EQUAL_STRING("in_game", b.lastErr(1).c_str());

  b.say(1, R"({"t":"end"})");
  v = b.view(3);
  TEST_ASSERT_TRUE(v["game"].isNull());
  TEST_ASSERT_FALSE(v["players"][0]["ready"].boolean());
}

void test_power_cut_resumes_lobby_and_game() {
  MemStore store;
  FakePlatform platform;
  std::string beforePhase;
  int beforePhaseId = -1;
  {
    FakeTransport transport;
    Room room(store, platform, 7);
    Hub hub(room, transport);
    room.begin(0);
    for (int i = 1; i <= 6; ++i) {
      hub.opened(i, 0);
      const std::string hello = "{\"t\":\"hello\",\"id\":\"" + phoneId(i) + "\"}";
      hub.text(i, hello.data(), hello.size(), 0);
      const std::string prof = "{\"t\":\"profile\",\"name\":\"P" + std::to_string(i) + "\",\"avatar\":\"x\",\"color\":2}";
      hub.text(i, prof.data(), prof.size(), 0);
      hub.text(i, "{\"t\":\"ready\"}", strlen("{\"t\":\"ready\"}"), 0);
    }
    hub.text(1, "{\"t\":\"claimAdmin\"}", strlen("{\"t\":\"claimAdmin\"}"), 0);
    const char start[] = "{\"t\":\"start\",\"game\":\"vampires\"}";
    hub.text(1, start, sizeof start - 1, 0);
    hub.tick(91000);  // role card timeout -> night
    json::Doc d;
    const std::string text = transport.last(2, "view");
    TEST_ASSERT_TRUE(d.parse(text.data(), text.size()));
    beforePhase = d.root()["view"]["game"]["phase"].str();
    beforePhaseId = d.root()["view"]["game"]["phaseId"].integer(-1);
    TEST_ASSERT_EQUAL_STRING("night", beforePhase.c_str());
  }
  // Power cut: a new Room on the same flash; the clock restarts at 0.
  FakeTransport transport;
  Room room(store, platform, 8);
  Hub hub(room, transport);
  room.begin(0);
  hub.opened(9, 0);
  const std::string hello = "{\"t\":\"hello\",\"id\":\"" + phoneId(2) + "\"}";
  hub.text(9, hello.data(), hello.size(), 0);
  json::Doc d;
  const std::string text = transport.last(9, "view");
  TEST_ASSERT_TRUE(d.parse(text.data(), text.size()));
  const json::Value v = d.root()["view"];
  TEST_ASSERT_EQUAL(1, v["me"].integer(-1));
  TEST_ASSERT_EQUAL_STRING("P2", v["profile"]["name"].str());
  TEST_ASSERT_EQUAL(0, v["admin"].integer(-1));
  TEST_ASSERT_EQUAL(6, v["players"].size());
  TEST_ASSERT_EQUAL_STRING(beforePhase.c_str(), v["game"]["phase"].str());
  TEST_ASSERT_EQUAL(beforePhaseId, v["game"]["phaseId"].integer(-2));
  TEST_ASSERT_TRUE(v["endsIn"].integer(-1) > 0);  // the phase timer came back too
  TEST_ASSERT_FALSE(v["players"][0]["online"].boolean());
}

void test_full_room_evicts_offline_members_only() {
  Box b;
  for (int i = 1; i <= kMaxPlayers; ++i) {
    b.connect(i, phoneId(i));
    b.profile(i, "P");
  }
  b.connect(100, "newcomer");
  TEST_ASSERT_TRUE(b.view(100)["full"].boolean());
  b.profile(100, "New");
  TEST_ASSERT_EQUAL_STRING("room_full", b.lastErr(100).c_str());

  b.hub.closed(7, b.now);  // a phone leaves
  b.profile(100, "New");
  json::Value v = b.view(100);
  TEST_ASSERT_EQUAL(6, v["me"].integer(-1));  // took the offline player's seat
  TEST_ASSERT_EQUAL(kMaxPlayers, static_cast<int>(v["players"].size()));
}

void test_stats_after_finished_game() {
  Box b;
  for (int i = 1; i <= 5; ++i) {
    b.connect(i, phoneId(i));
    b.profile(i, "P");
    b.say(i, R"({"t":"ready"})");
  }
  b.say(1, R"({"t":"claimAdmin"})");
  b.say(1, R"({"t":"start","game":"vampires","options":{"discussionSec":30,"voteSec":20}})");
  // Nobody acts: timers alone must drive the game to its end (no lynch, one
  // kill per night only if vampires pick, so admin skips keep it moving).
  for (int step = 0; step < 2000 && b.view(1)["game"]["winner"].isNull(); ++step) {
    const json::Value g = b.view(1)["game"];
    const std::string phase = g["phase"].str();
    const int phaseId = g["phaseId"].integer(-1);
    if (phase == "night") {
      // Every alive vampire picks the first target it is offered.
      for (int i = 1; i <= 5; ++i) {
        const json::Value pg = b.view(i)["game"];
        if (std::string(pg["role"].str()) == "vampire" && pg["alive"].boolean() && pg["task"]["targets"].size() > 0) {
          const int target = pg["task"]["targets"][0].integer(-1);
          b.say(i, "{\"t\":\"act\",\"phaseId\":" + std::to_string(phaseId) + ",\"target\":" + std::to_string(target) + "}");
        }
      }
    }
    b.now += 1000;
    b.hub.tick(b.now);
  }
  TEST_ASSERT_FALSE(b.view(1)["game"]["winner"].isNull());
  int played = 0;
  for (int i = 1; i <= 5; ++i) played += b.view(i)["profile"]["played"].integer(0);
  TEST_ASSERT_EQUAL(5, played);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_json_parse_and_unescape);
  RUN_TEST(test_json_rejects_malformed);
  RUN_TEST(test_json_writer_escapes_and_separates);
  RUN_TEST(test_hub_requires_hello_and_answers_ping);
  RUN_TEST(test_hub_new_socket_replaces_old_one);
  RUN_TEST(test_hub_closes_idle_sockets);
  RUN_TEST(test_profile_validation);
  RUN_TEST(test_views_never_contain_client_ids);
  RUN_TEST(test_admin_claim_transfer_and_reset);
  RUN_TEST(test_kick_and_settings);
  RUN_TEST(test_start_needs_ready_players);
  RUN_TEST(test_power_cut_resumes_lobby_and_game);
  RUN_TEST(test_full_room_evicts_offline_members_only);
  RUN_TEST(test_stats_after_finished_game);
  return UNITY_END();
}
