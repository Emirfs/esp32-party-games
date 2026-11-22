#include "hub.h"

#include <cstring>

namespace {

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace

Hub::Conn* Hub::find(int fd) {
  for (Conn& c : conns_)
    if (c.fd == fd) return &c;
  return nullptr;
}

bool Hub::opened(int conn, uint32_t now) {
  Conn* slot = find(-1);
  if (slot == nullptr) return false;
  *slot = Conn{};
  slot->fd = conn;
  slot->lastSeen = now;
  return true;
}

void Hub::text(int conn, const char* data, size_t len, uint32_t now) {
  Conn* c = find(conn);
  if (c == nullptr) return;
  c->lastSeen = now;

  json::Doc doc;
  if (len > kMaxFrame || !doc.parse(data, len) || !doc.root().isObject()) {
    static constexpr char kBad[] = "{\"t\":\"err\",\"code\":\"bad_msg\"}";
    transport_.send(conn, kBad, sizeof kBad - 1);
    return;
  }
  const json::Value msg = doc.root();
  const char* t = msg["t"].str();

  if (strcmp(t, "hello") == 0) {
    hello(*c, msg, now);
  } else if (c->id[0] == '\0') {
    // Also for pings: a phone whose hello got lost learns it must say it again.
    static constexpr char kFirst[] = "{\"t\":\"err\",\"code\":\"hello_first\"}";
    transport_.send(conn, kFirst, sizeof kFirst - 1);
    return;
  } else if (strcmp(t, "ping") == 0) {
    // Echo the phone's clock; it measures the round trip itself.
    buf_.clear();
    json::Writer(buf_).beginObject().field("t", "pong").field("c", static_cast<long long>(msg["c"].num(0))).endObject();
    transport_.send(conn, buf_.data(), buf_.size());
    return;
  } else {
    room_.handle(c->id, msg, now);
  }
  flush(now);
}

void Hub::hello(Conn& c, const json::Value& msg, uint32_t now) {
  const char* id = msg["id"].str();
  if (!validClientId(id)) {
    static constexpr char kBad[] = "{\"t\":\"err\",\"code\":\"bad_msg\"}";
    transport_.send(c.fd, kBad, sizeof kBad - 1);
    return;
  }
  if (c.id[0] != '\0' && strcmp(c.id, id) != 0) {
    // Same socket switching identity: the old one leaves.
    char old[kClientIdMax + 1];
    memcpy(old, c.id, sizeof old);
    c.id[0] = '\0';
    room_.disconnected(old, now);
  }
  // Same phone on a new socket: the old one is a half-open leftover.
  for (Conn& other : conns_) {
    if (&other == &c || other.fd < 0 || strcmp(other.id, id) != 0) continue;
    other.id[0] = '\0';
    transport_.close(other.fd);
  }
  strncpy(c.id, id, kClientIdMax);
  c.id[kClientIdMax] = '\0';
  c.sentVersion = 0;
  static constexpr char kWelcome[] = "{\"t\":\"welcome\",\"proto\":1}";
  transport_.send(c.fd, kWelcome, sizeof kWelcome - 1);
  room_.connected(c.id, now);
}

void Hub::closed(int conn, uint32_t now) {
  Conn* c = find(conn);
  if (c == nullptr) return;
  char id[kClientIdMax + 1];
  memcpy(id, c->id, sizeof id);
  *c = Conn{};
  if (id[0] == '\0') return;
  for (const Conn& other : conns_)
    if (other.fd >= 0 && strcmp(other.id, id) == 0) return;
  room_.disconnected(id, now);
  flush(now);
}

void Hub::tick(uint32_t now) {
  for (Conn& c : conns_) {
    if (c.fd >= 0 && timeReached(now, c.lastSeen + kIdleCloseMs)) {
      c.lastSeen = now;  // closing is asynchronous; do not re-trigger every tick
      transport_.close(c.fd);
    }
  }
  room_.tick(now);
  flush(now);
}

void Hub::flush(uint32_t now) {
  const uint32_t version = room_.version();
  for (Conn& c : conns_) {
    if (c.fd < 0 || c.id[0] == '\0' || c.sentVersion == version) continue;
    room_.renderView(c.id, now, buf_);
    transport_.send(c.fd, buf_.data(), buf_.size());
    c.sentVersion = version;
  }
  for (const Room::Outgoing& m : room_.takeOutbox())
    for (const Conn& c : conns_)
      if (c.fd >= 0 && m.clientId == c.id) transport_.send(c.fd, m.text.data(), m.text.size());
}

int Hub::connCount() const {
  int n = 0;
  for (const Conn& c : conns_)
    if (c.fd >= 0) ++n;
  return n;
}

int Hub::boundCount() const {
  int n = 0;
  for (const Conn& c : conns_)
    if (c.fd >= 0 && c.id[0] != '\0') ++n;
  return n;
}
