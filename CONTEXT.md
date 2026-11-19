# CONTEXT — oyun-kutusu

Durum: AKTIF

ESP32-S3 üzerinde internetsiz, uygulamasız masa oyunu kutusu (açık kaynak, MIT).
Kart kendi Wi-Fi ağını (SoftAP) açar, captive portal ile telefonları
`http://192.168.4.1`'e yönlendirir, arayüzü firmware'e gömülü tek HTML olarak
sunar, oyunu WebSocket üzerinden yönetir. Referans kart ESP32-S3 N16R8
(DevKitC-1); ≥ 8 MB flash'lı her S3 çalışır. Firmware: PlatformIO + ESP-IDF 5.5
(Arduino yok). Web: Vite + Preact + TypeScript. Tasarım ve kurallar:
`docs/tasarim.md`.

## Dosya haritası

| Dosya | Görev | Değişince |
|---|---|---|
| `firmware/platformio.ini` | Ortam `esp32-s3`, platform `espressif32@6.13.0` (ESP-IDF 5.5.3), 8 MB flash, ön betik | — |
| `firmware/sdkconfig.defaults` | Kapasite sınırları (DHCP 16 adres, ESP-NOW anahtarı 0, 32 soket), PSRAM, HTTP/WS | `http_server.cpp` `max_open_sockets` ≤ soket − 4 kalmalı |
| `firmware/partitions.csv` | nvs, phy, 4 MB factory uygulama | Eklenen bölüm tasarımdaki flash planına işlenir |
| `firmware/CMakeLists.txt` | Proje, `PROJECT_VER` (durum sayfasındaki sürüm) | — |
| `firmware/src/CMakeLists.txt` | Kaynaklar, `EMBED_FILES` (`web/dist/*.html.gz`) | `http_server.cpp` içindeki `_binary_*` sembolleri |
| `firmware/src/config.h` | SSID öneki, Wi-Fi şifresi, kanal, 15 istasyon, köken adresi | Portal sayfasındaki adres (`web/public/portal.html`) |
| `firmware/src/wifi_ap.*` | SoftAP, SSID = önek + MAC son 2 bayt, istasyon sayısı | — |
| `firmware/src/dns_server.*` | Sahte DNS: her A sorgusuna `192.168.4.1`, TTL 0 | — |
| `firmware/src/http_server.*` | Sayfalar, `/api/accept`, `/api/status`, OS bağlantı yoklamaları, 404 → portal, MAC bazlı portal kabulü | Yeni yoklama adresi → `kProbes` |
| `firmware/src/ws_hub.*` | `/ws`: hello/ping/presence, 15 sn sessiz soketi kapatma | `web/src/connection.ts` (aynı commit) |
| `firmware/tools/build_web.py` | PlatformIO ön betiği: her derlemede `web/` derler | — |
| `web/src/connection.ts` | WebSocket istemcisi, cihaz kimliği, pong zaman aşımı, ölçümler | `ws_hub.h` |
| `web/src/app.tsx` | Faz 0 bağlantı test sayfası | — |
| `web/public/portal.html` | Captive portal sayfası (Vite işlemeden kopyalar) | `config.h` adresi |
| `web/scripts/gzip.mjs` | `dist/*.html` → `.html.gz` | `EMBED_FILES` listesi |
| `docs/tasarim.md` | Tasarım, eklenti/pin planı, oyun kuralları, fazlar | — |
| `docs/faz0-saha-testi.md` | Telefonla saha testi adımları ve sonuç tablosu | — |

## Sözleşmeler (tek kaynak)

- **Köken:** `http://192.168.4.1` — `firmware/src/config.h`. Ana makine adı kullanılmaz (`localStorage` köken başına).
- **WebSocket protokolü:** `firmware/src/ws_hub.h`.
- **Kapasite ayarları:** `firmware/sdkconfig.defaults`.
- **Eklenti pin planı:** `docs/tasarim.md` bölüm 4.
- **Gömülü dosyalar:** `firmware/src/CMakeLists.txt` `EMBED_FILES` ↔ `web/scripts/gzip.mjs`.

## Komutlar

```sh
cd firmware
pio run                 # web/ derlenir, gömülür, firmware derlenir
pio run -t upload       # karta yükle
pio device monitor      # 115200
cd ../web && npm run dev   # PC kutunun ağındayken arayüz geliştirme (proxy → 192.168.4.1)
```

## Dikkat

- PlatformIO'nun ESP-IDF derleyicisi Python 3.14 ile kurulamaz (`pydantic<2.12` için 3.14 wheel'i yok). PlatformIO'yu Python ≤ 3.13 ile çalıştır; bozuk kurulumda `~/.platformio/penv/.espidf-5.5.3` silinip yeniden derlenir.
- `CONFIG_LWIP_DHCPS_MAX_STATION_NUM` varsayılanı 8'dir; düşürülürse 9. telefon IP alamaz.
- `CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM` > 2 olursa SoftAP 15 istasyonu kabul edemez (toplam 17 donanım anahtarı).
- HTTP sunucusunun tüm erişimi (istemci tablosu, portal kabul tablosu) httpd görevinde; başka görevden yalnızca `httpd_queue_work` ile dokunulur.
- Boş depoda `git describe` çalışmaz; sürüm `PROJECT_VER` ile sabit.
- Eklentiler isteğe bağlıdır: firmware eklentisiz açılmalı, her oyun eklentisiz oynanmalı.
