# CONTEXT — esp32-party-games

Status: ACTIVE

Offline, app-free party game box on an ESP32-S3 (open source, MIT). The board
runs a SoftAP, sends phones to `http://192.168.4.1` through a captive portal,
serves the UI as one HTML file embedded in the firmware and runs the game over a
WebSocket. Reference board: ESP32-S3 N16R8 (DevKitC-1); any S3 with ≥ 8 MB flash
works. Firmware: PlatformIO + ESP-IDF 5.5 (no Arduino). Web: Vite + Preact +
TypeScript, UI in English (default) and Turkish. Design and rules:
`docs/design.md` (Turkish: `docs/design.tr.md`).

## File map

| File | Role | When it changes |
|---|---|---|
| `firmware/platformio.ini` | Env `esp32-s3`, platform `espressif32@6.13.0` (ESP-IDF 5.5.3), 8 MB flash, pre-script, `board_build.embed_files` | Embed list must match `src/CMakeLists.txt` |
| `firmware/sdkconfig.defaults` | Capacity limits (16 DHCP leases, 0 ESP-NOW keys, 32 sockets), PSRAM, HTTP/WS | `max_open_sockets` in `http_server.cpp` must stay ≤ sockets − 4 |
| `firmware/partitions.csv` | nvs, phy, 4 MB factory app | New partitions go into the flash plan in the design doc |
| `firmware/CMakeLists.txt` | Project, `PROJECT_VER` (version on the status page) | — |
| `firmware/src/CMakeLists.txt` | Sources, `EMBED_FILES` (`web/dist/*.html.gz`) | `_binary_*` symbols in `http_server.cpp`, `platformio.ini` embed list |
| `firmware/src/config.h` | SSID prefix, Wi-Fi password, channel, 15 stations, origin address | Address in `web/public/portal.html`, docs |
| `firmware/src/wifi_ap.*` | SoftAP, SSID = prefix + last 2 MAC bytes, station count | — |
| `firmware/src/dns_server.*` | Captive DNS: every A query → `192.168.4.1`, TTL 0 | — |
| `firmware/src/http_server.*` | Pages, `/api/accept`, `/api/status`, OS connectivity probes, 404 → portal, MAC-keyed portal acceptance | New probe URL → `kProbes` |
| `firmware/src/ws_hub.*` | `/ws`: hello/ping/presence, closes sockets silent for 15 s | `web/src/connection.ts` (same commit) |
| `firmware/tools/build_web.py` | PlatformIO pre-script: builds `web/` on every firmware build | — |
| `web/src/i18n.ts` | UI strings `en`/`tr`, language choice (`pg.lang`) | Portal's inline strings |
| `web/src/connection.ts` | WebSocket client, device id (`pg.clientId`), pong timeout, timings | `ws_hub.h` |
| `web/src/app.tsx` | Phase 0 connection test page | — |
| `web/public/portal.html` | Captive portal page, bilingual, copied verbatim by Vite | `config.h` address, `i18n.ts` language rule |
| `web/scripts/gzip.mjs` | `dist/*.html` → `.html.gz` | `EMBED_FILES` list |
| `docs/design.md` (+ `.tr.md`) | Design, add-on/pin plan, game rules | Translation in the same commit |
| `docs/phase0-field-test.md` (+ `.tr.md`) | Phone field test steps and result tables | Translation in the same commit |
| `docs/roadmap.md` (+ `.tr.md`) | Single source for phases, tasks, status and exit criteria | Tick tasks in the same commit as the work; translation too |
| `docs/launch-plan.md` (+ `.tr.md`) | Launch prerequisites, channels and order | Translation in the same commit |

## Contracts (single source)

- **Origin:** `http://192.168.4.1` — `firmware/src/config.h`. Never a hostname (`localStorage` is per origin).
- **WebSocket protocol:** `firmware/src/ws_hub.h`.
- **Capacity settings:** `firmware/sdkconfig.defaults`.
- **Add-on pin plan:** `docs/design.md` section 4.
- **Embedded files:** `firmware/src/CMakeLists.txt` `EMBED_FILES` ↔ `firmware/platformio.ini` `board_build.embed_files` ↔ `web/scripts/gzip.mjs`.
- **localStorage keys:** `pg.clientId`, `pg.lang` (shared by app and portal).

## Commands

```sh
cd firmware
pio run                 # builds web/, embeds it, builds the firmware
pio run -t upload       # flash
pio device monitor      # 115200
cd ../web && npm run dev   # UI dev while this PC is on the box's Wi-Fi (proxy → 192.168.4.1)
```

## Watch out

- PlatformIO's ESP-IDF builder cannot install on Python 3.14 (no 3.14 wheel for `pydantic<2.12`). Run PlatformIO on Python ≤ 3.13; after a broken install delete `~/.platformio/penv/.espidf-5.5.3` and rebuild.
- Moving the repo folder breaks the CMake cache: delete `firmware/.pio` and rebuild.
- `CONFIG_LWIP_DHCPS_MAX_STATION_NUM` defaults to 8; lower it and the 9th phone gets no IP.
- `CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM` > 2 and the SoftAP can no longer accept 15 stations (17 hardware keys in total).
- All HTTP server state (client table, portal acceptance table) lives on the httpd task; other tasks touch it only through `httpd_queue_work`.
- Add-ons are optional: the firmware must boot without them and every game must be playable without them.
