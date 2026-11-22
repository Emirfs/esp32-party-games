#include "ws_hub.h"

#include "core/hub.h"
#include "core/room.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_store.h"
#include "wifi_ap.h"

// Invariant: Hub and Room are touched only on the httpd task (URI handler,
// close_fn, httpd_queue_work); timers and other tasks only queue work.
namespace ws_hub {
namespace {

constexpr char kTag[] = "ws";
constexpr uint64_t kTickUs = 250 * 1000;

httpd_handle_t g_server = nullptr;

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

class HttpdTransport : public Transport {
 public:
  void send(int conn, const char* text, size_t len) override {
    httpd_ws_frame_t frame = {};
    frame.final = true;
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(text));
    frame.len = len;
    const esp_err_t err = httpd_ws_send_frame_async(g_server, conn, &frame);
    if (err != ESP_OK) ESP_LOGW(kTag, "send to fd %d failed: %s", conn, esp_err_to_name(err));
  }
  void close(int conn) override { httpd_sess_trigger_close(g_server, conn); }
};

class BoxPlatform : public Platform {
 public:
  const char* ssid() const override { return wifi_ap::ssid(); }
  const char* wifiPassword() const override { return wifi_ap::password(); }
  void restart() override {
    ESP_LOGI(kTag, "restart requested by the admin");
    esp_restart();
  }
};

NvsStore* g_store = nullptr;
BoxPlatform g_platform;
HttpdTransport g_transport;
Room* g_room = nullptr;
Hub* g_hub = nullptr;

void tick(void*) { g_hub->tick(now_ms()); }
void on_timer(void*) { httpd_queue_work(g_server, tick, nullptr); }

void reset_admin_work(void*) {
  ESP_LOGI(kTag, "admin reset (BOOT button)");
  g_room->resetAdmin();
  g_hub->tick(now_ms());  // pushes the new views
}

}  // namespace

void start(httpd_handle_t server) {
  g_server = server;
  g_store = new NvsStore();
  const uint64_t seed = static_cast<uint64_t>(esp_random()) << 32 | esp_random();
  g_room = new Room(*g_store, g_platform, seed);
  g_room->begin(now_ms());
  g_hub = new Hub(*g_room, g_transport);

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
    return g_hub->opened(fd, now_ms()) ? ESP_OK : ESP_FAIL;
  }

  httpd_ws_frame_t frame = {};
  if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK) return ESP_FAIL;
  if (frame.len > Hub::kMaxFrame) return ESP_FAIL;
  char buf[Hub::kMaxFrame + 1];
  frame.payload = reinterpret_cast<uint8_t*>(buf);
  if (frame.len > 0 && httpd_ws_recv_frame(req, &frame, frame.len) != ESP_OK) return ESP_FAIL;
  buf[frame.len] = '\0';
  if (frame.type == HTTPD_WS_TYPE_TEXT) g_hub->text(fd, buf, frame.len, now_ms());
  return ESP_OK;
}

void on_close(int fd) {
  if (g_hub != nullptr) g_hub->closed(fd, now_ms());
}

int client_count() { return g_hub != nullptr ? g_hub->boundCount() : 0; }

void reset_admin() { httpd_queue_work(g_server, reset_admin_work, nullptr); }

}  // namespace ws_hub
