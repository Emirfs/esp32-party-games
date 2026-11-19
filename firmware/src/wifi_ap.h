#pragma once

namespace wifi_ap {

// Starts the WPA2 SoftAP at 192.168.4.1. Needs esp_netif and the default event loop.
void start();
const char* ssid();
int station_count();

}  // namespace wifi_ap
