#include "ws_hub.h"

#include <cstdio>
#include <cstring>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"

// Invariant: the client table is touched only on the httpd task (URI handler,
// close_fn, httpd_queue_work); the esp_timer callback just queues work.
namespace ws_hub {
namespace {

constexpr char kTag[] = "ws";
constexpr int kMaxClients = 24;  // >= httpd max_open_sockets
constexpr size_t kMaxFrame = 512;
constexpr size_t kMaxIdLen = 23;
constexpr int64_t kIdleCloseUs = 15 * 1000 * 1000;
constexpr int64_t kTickUs = 1000 * 1000;
constexpr uint32_t kPresenceEveryTicks = 5;

struct Client {
  int fd = -1;
  char id[kMaxIdLen + 1] = {};
  int rtt_ms = -1;
  int64_t last_seen_us = 0;
};

Client g_clients[kMaxClients];
httpd_handle_t g_server = nullptr;
uint32_t g_ticks = 0;

Client* find(int fd) {
  for (Client& c : g_clients)
    if (c.fd == fd) return &c;
  return nullptr;
}

// Ids are echoed into JSON unescaped, so only [A-Za-z0-9_-] is accepted.
bool valid_id(const char* id) {
  const size_t n = strlen(id);
  if (n == 0 || n > kMaxIdLen) return false;
  for (size_t i = 0; i < n; ++i) {
    const char ch = id[i];
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
    if (!ok) return false;
  }
  return true;
}

void send_text(int fd, const char* text, size_t len) {
  httpd_ws_frame_t frame = {};
  frame.final = true;
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(text));
  frame.len = len;
  const esp_err_t err = httpd_ws_send_frame_async(g_server, fd, &frame);
  if (err != ESP_OK) ESP_LOGW(kTag, "send to fd %d failed: %s", fd, esp_err_to_name(err));
}

void broadcast_presence() {
  char buf[64 + kMaxClients * (kMaxIdLen + 40)];
  const int64_t now = esp_timer_get_time();
  int len = snprintf(buf, sizeof buf, "{\"t\":\"presence\",\"clients\":[");
  bool first = true;
  for (const Client& c : g_clients) {
    if (c.fd < 0 || c.id[0] == '\0') continue;
    len += snprintf(buf + len, sizeof buf - len, "%s{\"id\":\"%s\",\"rtt\":%d,\"age\":%lld}",
                    first ? "" : ",", c.id, c.rtt_ms,
                    static_cast<long long>((now - c.last_seen_us) / 1000));
    first = false;
  }
  len += snprintf(buf + len, sizeof buf - len, "]}");
  for (const Client& c : g_clients)
    if (c.fd >= 0 && c.id[0] != '\0') send_text(c.fd, buf, len);
}

void handle_hello(Client& self, const cJSON* msg) {
  const cJSON* id = cJSON_GetObjectItemCaseSensitive(msg, "id");
  if (!cJSON_IsString(id) || !valid_id(id->valuestring)) return;
  // Same phone reconnecting: the old socket is a half-open leftover.
  for (Client& other : g_clients) {
    if (other.fd >= 0 && other.fd != self.fd && strcmp(other.id, id->valuestring) == 0) {
      ESP_LOGI(kTag, "%s reconnected, closing stale fd %d", other.id, other.fd);
      other.id[0] = '\0';
      httpd_sess_trigger_close(g_server, other.fd);
    }
  }
  strlcpy(self.id, id->valuestring, sizeof self.id);
  char buf[32];
  const int len = snprintf(buf, sizeof buf, "{\"t\":\"welcome\",\"fd\":%d}", self.fd);
  send_text(self.fd, buf, len);
  broadcast_presence();
}

void handle_ping(Client& self, const cJSON* msg) {
  const cJSON* c = cJSON_GetObjectItemCaseSensitive(msg, "c");
  const cJSON* rtt = cJSON_GetObjectItemCaseSensitive(msg, "rtt");
  if (cJSON_IsNumber(rtt)) self.rtt_ms = rtt->valueint;
  if (!cJSON_IsNumber(c)) return;
  char buf[48];
  const int len = snprintf(buf, sizeof buf, "{\"t\":\"pong\",\"c\":%.0f}", c->valuedouble);
  send_text(self.fd, buf, len);
}

void handle_message(Client& self, const char* text, size_t len) {
  cJSON* msg = cJSON_ParseWithLength(text, len);
  if (msg == nullptr) return;
  const cJSON* type = cJSON_GetObjectItemCaseSensitive(msg, "t");
  if (cJSON_IsString(type)) {
    if (strcmp(type->valuestring, "hello") == 0) handle_hello(self, msg);
    else if (strcmp(type->valuestring, "ping") == 0) handle_ping(self, msg);
  }
  cJSON_Delete(msg);
}

void tick(void*) {
  const int64_t now = esp_timer_get_time();
  for (Client& c : g_clients) {
    if (c.fd >= 0 && now - c.last_seen_us > kIdleCloseUs) {
      ESP_LOGI(kTag, "fd %d (%s) idle, closing", c.fd, c.id);
      c.last_seen_us = now;  // trigger_close is async; do not re-trigger every tick
      httpd_sess_trigger_close(g_server, c.fd);
    }
  }
  if (++g_ticks % kPresenceEveryTicks == 0) broadcast_presence();
}

void on_timer(void*) { httpd_queue_work(g_server, tick, nullptr); }

void presence_work(void*) { broadcast_presence(); }

}  // namespace

void start(httpd_handle_t server) {
  g_server = server;
  const esp_timer_create_args_t args = {
      .callback = on_timer,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "ws_tick",
      .skip_unhandled_events = true,
  };
  esp_timer_handle_t timer;
  ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, kTickUs));
}

esp_err_t handler(httpd_req_t* req) {
  const int fd = httpd_req_to_sockfd(req);
  if (req->method == HTTP_GET) {  // handshake completed
    Client* slot = find(-1);
    if (slot == nullptr) return ESP_FAIL;
    *slot = Client{};
    slot->fd = fd;
    slot->last_seen_us = esp_timer_get_time();
    return ESP_OK;
  }

  Client* self = find(fd);
  if (self == nullptr) return ESP_FAIL;

  httpd_ws_frame_t frame = {};
  if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK) return ESP_FAIL;
  if (frame.len > kMaxFrame) return ESP_FAIL;
  char buf[kMaxFrame + 1];
  frame.payload = reinterpret_cast<uint8_t*>(buf);
  if (frame.len > 0 && httpd_ws_recv_frame(req, &frame, frame.len) != ESP_OK) return ESP_FAIL;
  buf[frame.len] = '\0';

  self->last_seen_us = esp_timer_get_time();
  if (frame.type == HTTPD_WS_TYPE_TEXT) handle_message(*self, buf, frame.len);
  return ESP_OK;
}

void on_close(int fd) {
  Client* c = find(fd);
  if (c == nullptr) return;
  const bool announced = c->id[0] != '\0';
  *c = Client{};
  if (announced) httpd_queue_work(g_server, presence_work, nullptr);
}

int client_count() {
  int n = 0;
  for (const Client& c : g_clients)
    if (c.fd >= 0) ++n;
  return n;
}

}  // namespace ws_hub
