#pragma once

namespace wifi_ap {

// Starts the WPA2 SoftAP at 192.168.4.1. Needs NVS, esp_netif and the default
// event loop. The password is the admin's (NVS "wifi_pass") or cfg::kApPassword.
void start();
const char* ssid();
const char* password();
int station_count();

}  // namespace wifi_ap
