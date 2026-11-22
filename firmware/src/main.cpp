#include "boot_button.h"
#include "dns_server.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "http_server.h"
#include "nvs_flash.h"
#include "wifi_ap.h"

extern "C" void app_main() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Reset reason tells a power cut (phone OTG dropped) from a crash in field tests.
  ESP_LOGI("main", "reset reason %d, PSRAM %u bytes", static_cast<int>(esp_reset_reason()),
           static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)));

  wifi_ap::start();
  dns_server::start();
  boot_button::start();
  http_server::start();
}
