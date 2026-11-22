#include "room.h"

#include <cstring>

#include "games/registry.h"

namespace {

constexpr char kKeyProfiles[] = "profiles";
constexpr char kKeyAdmin[] = "admin";
constexpr char kKeyRoom[] = "room";
constexpr char kKeyGame[] = "game";
constexpr uint32_t kProfilesMagic = 0x50470101;  // "PG", blob 1, version 1
constexpr uint32_t kRoomMagic = 0x50470201;
constexpr size_t kGameIdMax = 15;
constexpr size_t kGameBlobMax = 4096;

struct ProfilesBlob {
  uint32_t magic;
  uint32_t lruClock;
  Profile profiles[kKnownProfiles];
};

struct RoomBlob {
  uint32_t magic;
  struct {
    uint8_t used;
    uint8_t ready;
    char id[kClientIdMax + 1];
  } members[kMaxPlayers];
  char gameId[kGameIdMax + 1];  // "" = lobby
  PlayerMask seated;
  uint8_t statsDone;
  uint32_t timerLeft;  // ms; 0 = none
};

// Valid UTF-8 without control characters; returns the code point count or -1.
int utf8Length(const char* s) {
  int n = 0;
  for (const auto* p = reinterpret_cast<const unsigned char*>(s); *p;) {
    int extra;
    if (*p < 0x80) {
      if (*p < 0x20 || *p == 0x7F) return -1;
      extra = 0;
    } else if ((*p & 0xE0) == 0xC0 && *p >= 0xC2) {
      extra = 1;
    } else if ((*p & 0xF0) == 0xE0) {
      extra = 2;
    } else if ((*p & 0xF8) == 0xF0 && *p <= 0xF4) {
      extra = 3;
    } else {
      return -1;
    }
    ++p;
    for (int i = 0; i < extra; ++i, ++p)
      if ((*p & 0xC0) != 0x80) return -1;
    ++n;
  }
  return n;
}

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace

bool validClientId(const char* id) {
  const size_t n = strlen(id);
  if (n == 0 || n > kClientIdMax) return false;
  for (size_t i = 0; i < n; ++i) {
    const char ch = id[i];
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
    if (!ok) return false;
  }
  return true;
}

Room::Room(Store& store, Platform& platform, uint64_t seed)
    : store_(store), platform_(platform), rng_(seed) {}

Room::~Room() = default;

// ---------------------------------------------------------------- persistence

void Room::begin(uint32_t now) {
  now_ = now;

  auto profiles = std::make_unique<ProfilesBlob>();
  if (store_.load(kKeyProfiles, profiles.get(), sizeof *profiles) == sizeof *profiles &&
      profiles->magic == kProfilesMagic) {
    memcpy(profiles_, profiles->profiles, sizeof profiles_);
    lruClock_ = profiles->lruClock;
    for (Profile& p : profiles_) {  // never trust flash contents as C strings
      p.id[kClientIdMax] = p.name[kNameMax] = p.avatar[kAvatarMax] = '\0';
      if (p.id[0] != '\0' && !validClientId(p.id)) p = Profile{};
    }
  }

  char admin[kClientIdMax + 1] = {};
  if (store_.load(kKeyAdmin, admin, sizeof admin) == sizeof admin) {
    admin[kClientIdMax] = '\0';
    if (validClientId(admin)) memcpy(admin_, admin, sizeof admin_);
  }

  RoomBlob room = {};
  if (store_.load(kKeyRoom, &room, sizeof room) != sizeof room || room.magic != kRoomMagic) return;
  for (int pid = 0; pid < kMaxPlayers; ++pid) {
    room.members[pid].id[kClientIdMax] = '\0';
    if (!room.members[pid].used || !validClientId(room.members[pid].id)) continue;
    if (profileOf(room.members[pid].id) == nullptr) continue;
    Member& m = members_[pid];
    m.used = true;
    m.ready = room.members[pid].ready != 0;
    memcpy(m.id, room.members[pid].id, sizeof m.id);
  }

  room.gameId[kGameIdMax] = '\0';
  if (room.gameId[0] == '\0') return;
  std::string blob(kGameBlobMax, '\0');
  const size_t len = store_.load(kKeyGame, &blob[0], blob.size());
  std::unique_ptr<Game> game = games::create(room.gameId, *this);
  bool seatsOk = true;
  for (int pid = 0; pid < kMaxPlayers; ++pid)
    if (hasPid(room.seated, pid) && !members_[pid].used) seatsOk = false;
  if (game == nullptr || len == 0 || !seatsOk ||
      !game->load(reinterpret_cast<const uint8_t*>(blob.data()), len)) {
    store_.erase(kKeyGame);
    return;
  }
  game_ = std::move(game);
  seated_ = room.seated;
  statsDone_ = room.statsDone != 0;
  // load() re-armed the phase timer with the full phase length; the snapshot
  // knows the time that was actually left.
  if (room.timerLeft > 0) setTimer(room.timerLeft);
}

void Room::saveProfiles() {
  auto blob = std::make_unique<ProfilesBlob>();
  blob->magic = kProfilesMagic;
  blob->lruClock = lruClock_;
  memcpy(blob->profiles, profiles_, sizeof profiles_);
  store_.save(kKeyProfiles, blob.get(), sizeof *blob);
}

void Room::saveRoom() {
  RoomBlob room = {};
  room.magic = kRoomMagic;
  for (int pid = 0; pid < kMaxPlayers; ++pid) {
    room.members[pid].used = members_[pid].used;
    room.members[pid].ready = members_[pid].ready;
    memcpy(room.members[pid].id, members_[pid].id, sizeof room.members[pid].id);
  }
  if (game_) strncpy(room.gameId, game_->info().id, kGameIdMax);
  room.seated = seated_;
  room.statsDone = statsDone_;
  room.timerLeft = deadline_ != 0 && !timeReached(now_, deadline_) ? deadline_ - now_ : 0;
  store_.save(kKeyRoom, &room, sizeof room);
}

void Room::saveGame() {
  if (!game_) {
    store_.erase(kKeyGame);
    return;
  }
  const std::string blob = game_->save();
  if (blob.size() <= kGameBlobMax) store_.save(kKeyGame, blob.data(), blob.size());
}

void Room::snapshot() {
  if (!snapshotDue_) return;
  snapshotDue_ = false;
  saveGame();
  saveRoom();
}

// ---------------------------------------------------------------- lookups

int Room::pidOf(const char* clientId) const {
  for (int pid = 0; pid < kMaxPlayers; ++pid)
    if (members_[pid].used && strcmp(members_[pid].id, clientId) == 0) return pid;
  return -1;
}

Profile* Room::profileOf(const char* clientId) {
  for (Profile& p : profiles_)
    if (p.id[0] != '\0' && strcmp(p.id, clientId) == 0) return &p;
  return nullptr;
}

const Profile* Room::profileOf(const char* clientId) const {
  return const_cast<Room*>(this)->profileOf(clientId);
}

bool Room::isAdmin(const char* clientId) const {
  return admin_[0] != '\0' && strcmp(admin_, clientId) == 0;
}

int Room::adminPid() const { return admin_[0] != '\0' ? pidOf(admin_) : -1; }

// A member slot for a new player: a free one, else an offline member who is
// neither the admin nor in the running game.
int Room::freeSlot() const {
  for (int pid = 0; pid < kMaxPlayers; ++pid)
    if (!members_[pid].used) return pid;
  for (int pid = 0; pid < kMaxPlayers; ++pid) {
    const Member& m = members_[pid];
    if (!m.online && !isAdmin(m.id) && !(game_ && hasPid(seated_, pid))) return pid;
  }
  return -1;
}

bool Room::join(const char* clientId) {
  if (pidOf(clientId) >= 0) return true;
  const int pid = freeSlot();
  if (pid < 0) return false;
  Member& m = members_[pid];
  m = Member{};
  m.used = true;
  m.online = true;
  strncpy(m.id, clientId, kClientIdMax);
  snapshotDue_ = true;
  return true;
}

void Room::error(const char* clientId, const char* code) {
  std::string text;
  json::Writer(text).beginObject().field("t", "err").field("code", code).endObject();
  outbox_.push_back({clientId, std::move(text)});
}

// ---------------------------------------------------------------- hub events

void Room::connected(const char* clientId, uint32_t now) {
  now_ = now;
  const int pid = pidOf(clientId);
  if (pid >= 0) members_[pid].online = true;
  else if (profileOf(clientId) != nullptr) join(clientId);
  changed();
  snapshot();
}

void Room::disconnected(const char* clientId, uint32_t now) {
  now_ = now;
  const int pid = pidOf(clientId);
  if (pid < 0) return;
  members_[pid].online = false;
  changed();
}

void Room::handle(const char* clientId, const json::Value& msg, uint32_t now) {
  now_ = now;
  const char* t = msg["t"].str();
  const char* err = nullptr;
  if (strcmp(t, "profile") == 0) {
    err = onProfile(clientId, msg);
  } else if (pidOf(clientId) < 0) {
    err = "no_profile";
  } else if (strcmp(t, "ready") == 0) {
    err = onReady(clientId, msg);
  } else if (strcmp(t, "claimAdmin") == 0) {
    err = onClaimAdmin(clientId);
  } else if (strcmp(t, "transfer") == 0) {
    err = onTransfer(clientId, msg);
  } else if (strcmp(t, "kick") == 0) {
    err = onKick(clientId, msg);
  } else if (strcmp(t, "settings") == 0) {
    err = onSettings(clientId, msg);
  } else if (strcmp(t, "restart") == 0) {
    if (!isAdmin(clientId)) {
      err = "not_admin";
    } else {
      snapshotDue_ = true;
      snapshot();
      platform_.restart();
    }
  } else if (strcmp(t, "start") == 0) {
    err = onStart(clientId, msg);
  } else if (strcmp(t, "skip") == 0) {
    if (!isAdmin(clientId)) err = "not_admin";
    else if (!game_) err = "no_game";
    else err = game_->adminSkip();
  } else if (strcmp(t, "end") == 0) {
    err = onEnd(clientId);
  } else if (strcmp(t, "ack") == 0 || strcmp(t, "act") == 0 || strcmp(t, "vote") == 0) {
    const int pid = pidOf(clientId);
    if (!game_) err = "no_game";
    else if (!hasPid(seated_, pid)) err = "not_allowed";
    else err = game_->onAction(pid, msg);
  } else {
    err = "bad_msg";
  }

  if (err != nullptr) {
    error(clientId, err);
  } else {
    changed();
    afterGameStep();
  }
  snapshot();
}

void Room::tick(uint32_t now) {
  now_ = now;
  if (game_ && deadline_ != 0 && timeReached(now, deadline_)) {
    deadline_ = 0;
    game_->onTimer();
    changed();
    afterGameStep();
  }
  snapshot();
}

void Room::resetAdmin() {
  admin_[0] = '\0';
  store_.erase(kKeyAdmin);
  changed();
}

// Records the result once, the moment the game reports it is over.
void Room::afterGameStep() {
  if (!game_ || statsDone_ || !game_->finished()) return;
  statsDone_ = true;
  deadline_ = 0;
  for (int pid = 0; pid < kMaxPlayers; ++pid) {
    if (!hasPid(seated_, pid)) continue;
    Profile* p = profileOf(members_[pid].id);
    if (p == nullptr) continue;
    if (p->played < UINT16_MAX) ++p->played;
    if (game_->won(pid) && p->won < UINT16_MAX) ++p->won;
  }
  saveProfiles();
  snapshotDue_ = true;
}

// ---------------------------------------------------------------- requests

const char* Room::onProfile(const char* clientId, const json::Value& msg) {
  // Name: trimmed, 1-16 characters, no control characters.
  const char* raw = msg["name"].str();
  while (*raw == ' ') ++raw;
  std::string name(raw);
  while (!name.empty() && name.back() == ' ') name.pop_back();
  const int chars = utf8Length(name.c_str());
  if (chars < 1 || chars > 16 || name.size() > kNameMax) return "bad_name";

  const char* avatar = msg["avatar"].str();
  const size_t avatarLen = strlen(avatar);
  if (avatarLen == 0 || avatarLen > kAvatarMax || utf8Length(avatar) < 0) return "bad_avatar";

  const int color = msg["color"].integer(-1);
  if (color < 0 || color >= kColors) return "bad_color";

  Profile* p = profileOf(clientId);
  if (p == nullptr) {
    if (pidOf(clientId) < 0 && freeSlot() < 0) return "room_full";
    // An empty record, else the least recently used one outside the room.
    for (Profile& candidate : profiles_) {
      if (candidate.id[0] == '\0') {
        p = &candidate;
        break;
      }
      if (pidOf(candidate.id) >= 0 || isAdmin(candidate.id)) continue;
      if (p == nullptr || candidate.lastUse < p->lastUse) p = &candidate;
    }
    if (p == nullptr) return "room_full";
    *p = Profile{};
    strncpy(p->id, clientId, kClientIdMax);
  }
  strncpy(p->name, name.c_str(), kNameMax);
  p->name[kNameMax] = '\0';
  strncpy(p->avatar, avatar, kAvatarMax);
  p->avatar[kAvatarMax] = '\0';
  p->color = static_cast<uint8_t>(color);
  p->lastUse = ++lruClock_;
  if (!join(clientId)) return "room_full";
  saveProfiles();
  return nullptr;
}

const char* Room::onReady(const char* clientId, const json::Value& msg) {
  if (game_) return "in_game";
  Member& m = members_[pidOf(clientId)];
  const bool on = msg["on"].boolean(true);
  if (m.ready != on) {
    m.ready = on;
    snapshotDue_ = true;
  }
  return nullptr;
}

const char* Room::onClaimAdmin(const char* clientId) {
  if (adminPid() >= 0) return "admin_taken";
  strncpy(admin_, clientId, kClientIdMax);
  admin_[kClientIdMax] = '\0';
  store_.save(kKeyAdmin, admin_, sizeof admin_);
  return nullptr;
}

const char* Room::onTransfer(const char* clientId, const json::Value& msg) {
  if (!isAdmin(clientId)) return "not_admin";
  const int pid = msg["pid"].integer(-1);
  if (pid < 0 || pid >= kMaxPlayers || !members_[pid].used) return "bad_pid";
  memcpy(admin_, members_[pid].id, sizeof admin_);
  store_.save(kKeyAdmin, admin_, sizeof admin_);
  return nullptr;
}

const char* Room::onKick(const char* clientId, const json::Value& msg) {
  if (!isAdmin(clientId)) return "not_admin";
  if (game_) return "in_game";
  const int pid = msg["pid"].integer(-1);
  if (pid < 0 || pid >= kMaxPlayers || !members_[pid].used || pid == pidOf(clientId))
    return "bad_pid";
  members_[pid] = Member{};
  snapshotDue_ = true;
  return nullptr;
}

const char* Room::onSettings(const char* clientId, const json::Value& msg) {
  if (!isAdmin(clientId)) return "not_admin";
  const json::Value pass = msg["wifiPass"];
  if (pass.isString()) {
    const char* s = pass.str();
    const size_t n = strlen(s);
    if (n < kWifiPassMin || n > kWifiPassMax) return "bad_pass";
    for (size_t i = 0; i < n; ++i)
      if (s[i] < 0x20 || s[i] > 0x7E) return "bad_pass";
    store_.save(kKeyWifiPass, s, n + 1);
  }
  return nullptr;
}

const char* Room::onStart(const char* clientId, const json::Value& msg) {
  if (!isAdmin(clientId)) return "not_admin";
  if (game_) return "in_game";
  const GameInfo* info = games::find(msg["game"].str());
  if (info == nullptr) return "bad_game";
  PlayerMask players = 0;
  for (int pid = 0; pid < kMaxPlayers; ++pid)
    if (members_[pid].used && members_[pid].ready) players |= static_cast<PlayerMask>(1u << pid);
  const int n = countPids(players);
  if (n < info->minPlayers) return "too_few";
  if (n > info->maxPlayers) return "too_many";

  std::unique_ptr<Game> game = games::create(info->id, *this);
  game_ = std::move(game);  // setup() may already call setTimer/phaseChanged/buzz
  seated_ = players;
  statsDone_ = false;
  deadline_ = 0;
  if (const char* err = game_->setup(players, msg["options"])) {
    game_.reset();
    seated_ = 0;
    deadline_ = 0;
    outbox_.clear();
    return err;
  }
  snapshotDue_ = true;
  return nullptr;
}

const char* Room::onEnd(const char* clientId) {
  if (!isAdmin(clientId)) return "not_admin";
  if (!game_) return "no_game";
  game_.reset();
  seated_ = 0;
  deadline_ = 0;
  statsDone_ = false;
  for (Member& m : members_) m.ready = false;
  snapshotDue_ = true;
  return nullptr;
}

// ---------------------------------------------------------------- GameHost

void Room::setTimer(uint32_t ms) {
  deadline_ = ms == 0 ? 0 : now_ + ms;
  if (ms != 0 && deadline_ == 0) deadline_ = 1;
}

void Room::phaseChanged() {
  snapshotDue_ = true;
  changed();
}

void Room::buzz(PlayerMask players, const char* pattern) {
  std::string text;
  json::Writer(text).beginObject().field("t", "buzz").key("p").raw(pattern).endObject();
  for (int pid = 0; pid < kMaxPlayers; ++pid)
    if (hasPid(players, pid) && members_[pid].used) outbox_.push_back({members_[pid].id, text});
}

bool Room::online(int pid) const {
  return pid >= 0 && pid < kMaxPlayers && members_[pid].used && members_[pid].online;
}

// ---------------------------------------------------------------- views

std::vector<Room::Outgoing> Room::takeOutbox() {
  std::vector<Outgoing> out;
  out.swap(outbox_);
  return out;
}

void Room::renderPlayers(json::Writer& w) const {
  w.beginArray();
  for (int pid = 0; pid < kMaxPlayers; ++pid) {
    const Member& m = members_[pid];
    if (!m.used) continue;
    const Profile* p = profileOf(m.id);
    w.beginObject()
        .field("pid", pid)
        .field("name", p ? p->name : "?")
        .field("avatar", p ? p->avatar : "?")
        .field("color", p ? static_cast<int>(p->color) : 0)
        .field("online", m.online)
        .field("ready", m.ready)
        .field("inGame", game_ != nullptr && hasPid(seated_, pid))
        .endObject();
  }
  w.endArray();
}

void Room::renderView(const char* clientId, uint32_t now, std::string& out) const {
  out.clear();
  json::Writer w(out);
  const int me = pidOf(clientId);
  const bool admin = me >= 0 && isAdmin(clientId);

  w.beginObject().field("t", "view").field("seq", static_cast<long long>(version_)).key("view");
  w.beginObject();
  w.field("me", me);

  w.key("profile");
  if (const Profile* p = profileOf(clientId)) {
    w.beginObject()
        .field("name", p->name)
        .field("avatar", p->avatar)
        .field("color", static_cast<int>(p->color))
        .field("played", static_cast<int>(p->played))
        .field("won", static_cast<int>(p->won))
        .endObject();
  } else {
    w.null();
  }

  w.field("admin", adminPid());
  w.field("isAdmin", admin);
  w.field("full", me < 0 && freeSlot() < 0);
  w.key("players");
  renderPlayers(w);

  w.key("games").beginArray();
  for (size_t i = 0; i < games::count(); ++i) {
    const GameInfo& g = games::info(i);
    w.beginObject()
        .field("id", g.id)
        .field("min", static_cast<int>(g.minPlayers))
        .field("max", static_cast<int>(g.maxPlayers))
        .endObject();
  }
  w.endArray();

  long long endsIn = -1;
  if (game_ && deadline_ != 0) endsIn = timeReached(now, deadline_) ? 0 : deadline_ - now;
  w.field("endsIn", endsIn);

  if (admin) {
    w.key("box")
        .beginObject()
        .field("ssid", platform_.ssid())
        .field("wifiPass", platform_.wifiPassword())
        .endObject();
  }
  if (game_ && me >= 0) {
    w.key("game");
    game_->privateView(me, w);
    if (admin || !hasPid(seated_, me)) {
      w.key("board");
      game_->boardView(w);
    }
  }
  w.endObject().endObject();
}
