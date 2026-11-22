idf_component_register(
  SRCS "main.cpp" "wifi_ap.cpp" "dns_server.cpp" "http_server.cpp" "ws_hub.cpp"
       "nvs_store.cpp" "boot_button.cpp"
       # Portable game logic (also built by the native/host envs in platformio.ini).
       "core/json.cpp" "core/room.cpp" "core/hub.cpp"
       "games/registry.cpp" "games/vampires.cpp"
  INCLUDE_DIRS "."
  PRIV_REQUIRES esp_wifi esp_netif esp_event esp_http_server esp_timer esp_app_format
                esp_driver_gpio nvs_flash lwip
  # Built by tools/build_web.py (web/ -> web/dist) before every firmware build.
  EMBED_FILES "../../web/dist/index.html.gz" "../../web/dist/portal.html.gz")
