#include "wifi_ap.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

namespace wifi_ap {
namespace {

constexpr char kTag[] = "wifi_ap";
char g_ssid[33];

void on_wifi_event(void*, esp_event_base_t, int32_t id, void* data) {
  if (id == WIFI_EVENT_AP_STACONNECTED) {
    auto* e = static_cast<wifi_event_ap_staconnected_t*>(data);
    ESP_LOGI(kTag, "join " MACSTR " aid=%d, stations=%d", MAC2STR(e->mac), e->aid, station_count());
  } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
    auto* e = static_cast<wifi_event_ap_stadisconnected_t*>(data);
    ESP_LOGI(kTag, "leave " MACSTR " reason=%d, stations=%d", MAC2STR(e->mac), e->reason,
             station_count());
  }
}

}  // namespace

void start() {
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event,
                                                      nullptr, nullptr));

  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
  // MAC suffix keeps two boxes at the same table apart.
  snprintf(g_ssid, sizeof g_ssid, "%s%02X%02X", cfg::kSsidPrefix, mac[4], mac[5]);

  wifi_config_t wc = {};
  memcpy(wc.ap.ssid, g_ssid, strlen(g_ssid));
  wc.ap.ssid_len = strlen(g_ssid);
  strlcpy(reinterpret_cast<char*>(wc.ap.password), cfg::kApPassword, sizeof wc.ap.password);
  wc.ap.channel = cfg::kApChannel;
  wc.ap.authmode = WIFI_AUTH_WPA2_PSK;
  wc.ap.max_connection = cfg::kMaxStations;
  wc.ap.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(kTag, "SoftAP \"%s\" up on channel %d, max %d stations", g_ssid, cfg::kApChannel,
           cfg::kMaxStations);
}

const char* ssid() { return g_ssid; }

int station_count() {
  wifi_sta_list_t list;
  return esp_wifi_ap_get_sta_list(&list) == ESP_OK ? list.num : -1;
}

}  // namespace wifi_ap
