#include "http_server.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "wifi_ap.h"
#include "ws_hub.h"

extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");
extern const uint8_t portal_html_gz_start[] asm("_binary_portal_html_gz_start");
extern const uint8_t portal_html_gz_end[] asm("_binary_portal_html_gz_end");

namespace http_server {
namespace {

constexpr char kTag[] = "http";
httpd_handle_t g_server = nullptr;

// Captive-portal acceptance. A phone is sent to the portal until it taps
// "Oyuna gir"; afterwards its OS connectivity probes get the success answer, so
// the OS closes the captive sheet and keeps the network. Keyed by MAC (phones
// use a stable per-network MAC): a phone that re-joins stays accepted, and a new
// phone that inherits a recycled IP does not. Touched only on the httpd task.
struct Lease {
  uint8_t mac[6];
  uint32_t ip;  // network order; 0 = free slot
  bool accepted;
};
constexpr size_t kMaxLeases = 16;  // = CONFIG_LWIP_DHCPS_MAX_STATION_NUM
Lease g_leases[kMaxLeases];
size_t g_next_lease = 0;

uint32_t peer_ipv4(httpd_req_t* req) {
  sockaddr_storage ss = {};
  socklen_t len = sizeof ss;
  if (getpeername(httpd_req_to_sockfd(req), reinterpret_cast<sockaddr*>(&ss), &len) != 0) return 0;
  if (ss.ss_family == AF_INET) return reinterpret_cast<sockaddr_in*>(&ss)->sin_addr.s_addr;
  if (ss.ss_family == AF_INET6)  // IPv4-mapped IPv6
    return reinterpret_cast<sockaddr_in6*>(&ss)->sin6_addr.un.u32_addr[3];
  return 0;
}

Lease* lease_by_ip(uint32_t ip) {
  if (ip == 0) return nullptr;
  for (Lease& l : g_leases)
    if (l.ip == ip) return &l;
  return nullptr;
}

bool is_accepted(uint32_t ip) {
  const Lease* l = lease_by_ip(ip);
  return l != nullptr && l->accepted;
}

void lease_work(void* arg) {
  const Lease* in = static_cast<Lease*>(arg);
  Lease* slot = nullptr;
  for (Lease& l : g_leases)
    if (l.ip != 0 && memcmp(l.mac, in->mac, sizeof l.mac) == 0) slot = &l;
  if (slot == nullptr) {
    slot = &g_leases[g_next_lease];
    g_next_lease = (g_next_lease + 1) % kMaxLeases;
    *slot = Lease{};
    memcpy(slot->mac, in->mac, sizeof slot->mac);
  }
  for (Lease& l : g_leases)  // the IP moved to this phone
    if (&l != slot && l.ip == in->ip) l = Lease{};
  slot->ip = in->ip;
  delete in;
}

void on_ip_assigned(void*, esp_event_base_t, int32_t, void* data) {
  const auto* e = static_cast<ip_event_ap_staipassigned_t*>(data);
  ESP_LOGI(kTag, "lease " IPSTR " -> " MACSTR, IP2STR(&e->ip), MAC2STR(e->mac));
  auto* lease = new Lease{};
  memcpy(lease->mac, e->mac, sizeof lease->mac);
  lease->ip = e->ip.addr;
  if (httpd_queue_work(g_server, lease_work, lease) != ESP_OK) delete lease;
}

esp_err_t send_gz(httpd_req_t* req, const uint8_t* start, const uint8_t* end) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  return httpd_resp_send(req, reinterpret_cast<const char*>(start), end - start);
}

esp_err_t redirect_to_portal(httpd_req_t* req) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", cfg::kPortalUrl);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t index_get(httpd_req_t* req) { return send_gz(req, index_html_gz_start, index_html_gz_end); }

esp_err_t portal_get(httpd_req_t* req) {
  return send_gz(req, portal_html_gz_start, portal_html_gz_end);
}

esp_err_t accept_post(httpd_req_t* req) {
  if (Lease* l = lease_by_ip(peer_ipv4(req))) l->accepted = true;
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t status_get(httpd_req_t* req) {
  char buf[384];
  const int len = snprintf(
      buf, sizeof buf,
      "{\"ssid\":\"%s\",\"version\":\"%s\",\"uptimeMs\":%lld,\"resetReason\":%d,"
      "\"heapFree\":%u,\"heapMin\":%u,\"psramFree\":%u,\"psramTotal\":%u,"
      "\"stations\":%d,\"wsClients\":%d}",
      wifi_ap::ssid(), esp_app_get_description()->version,
      static_cast<long long>(esp_timer_get_time() / 1000), static_cast<int>(esp_reset_reason()),
      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
      static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
      static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)), wifi_ap::station_count(),
      ws_hub::client_count());
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, buf, len);
}

// OS connectivity probes. Before acceptance they redirect to the portal (the OS
// shows its captive sheet); afterwards they return what each OS expects.
struct Probe {
  const char* uri;
  const char* status;
  const char* type;
  const char* body;
};

constexpr char kAppleSuccess[] = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
constexpr Probe kProbes[] = {
    {"/generate_204", "204 No Content", nullptr, nullptr},  // Android, ChromeOS
    {"/gen_204", "204 No Content", nullptr, nullptr},
    {"/hotspot-detect.html", "200 OK", "text/html", kAppleSuccess},  // iOS, macOS
    {"/library/test/success.html", "200 OK", "text/html", kAppleSuccess},
    {"/connecttest.txt", "200 OK", "text/plain", "Microsoft Connect Test"},  // Windows
    {"/ncsi.txt", "200 OK", "text/plain", "Microsoft NCSI"},
    {"/success.txt", "200 OK", "text/plain", "success\n"},  // Firefox
    {"/canonical.html", "200 OK", "text/html",
     "<meta http-equiv=\"refresh\" content=\"0;url=https://support.mozilla.org/kb/captive-portal\"/>"},
};

esp_err_t probe_get(httpd_req_t* req) {
  if (!is_accepted(peer_ipv4(req))) return redirect_to_portal(req);
  const auto* probe = static_cast<const Probe*>(req->user_ctx);
  httpd_resp_set_status(req, probe->status);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  if (probe->body == nullptr) return httpd_resp_send(req, nullptr, 0);
  httpd_resp_set_type(req, probe->type);
  return httpd_resp_sendstr(req, probe->body);
}

// Unknown path: on our own host it is a real 404; any other host is a phone
// probing the internet through our captive DNS, so it goes to the portal.
esp_err_t not_found(httpd_req_t* req, httpd_err_code_t) {
  char host[64] = {};
  const bool ours = httpd_req_get_hdr_value_str(req, "Host", host, sizeof host) == ESP_OK &&
                    strncmp(host, cfg::kApIp, strlen(cfg::kApIp)) == 0;
  if (!ours) return redirect_to_portal(req);
  httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
  return ESP_OK;
}

void on_close(httpd_handle_t, int fd) {
  ws_hub::on_close(fd);
  close(fd);
}

void reg(const char* uri, httpd_method_t method, esp_err_t (*fn)(httpd_req_t*),
         void* ctx = nullptr, bool websocket = false) {
  httpd_uri_t u = {};
  u.uri = uri;
  u.method = method;
  u.handler = fn;
  u.user_ctx = ctx;
  u.is_websocket = websocket;
  ESP_ERROR_CHECK(httpd_register_uri_handler(g_server, &u));
}

}  // namespace

void start() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  // 15 WebSockets plus page-load churn; must stay <= CONFIG_LWIP_MAX_SOCKETS - 3
  // with one socket left for the DNS task.
  config.max_open_sockets = 24;
  config.lru_purge_enable = true;
  config.max_uri_handlers = 16;
  config.backlog_conn = 8;
  config.stack_size = 8192;
  config.close_fn = on_close;
  ESP_ERROR_CHECK(httpd_start(&g_server, &config));

  reg("/", HTTP_GET, index_get);
  reg("/portal", HTTP_GET, portal_get);
  reg("/api/accept", HTTP_POST, accept_post);
  reg("/api/status", HTTP_GET, status_get);
  reg("/ws", HTTP_GET, ws_hub::handler, nullptr, true);
  for (const Probe& p : kProbes) reg(p.uri, HTTP_GET, probe_get, const_cast<Probe*>(&p));
  ESP_ERROR_CHECK(httpd_register_err_handler(g_server, HTTPD_404_NOT_FOUND, not_found));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED,
                                                      on_ip_assigned, nullptr, nullptr));
  ws_hub::start(g_server);
  ESP_LOGI(kTag, "listening on http://%s", cfg::kApIp);
}

}  // namespace http_server
