# Oyun Kutusu

Offline party games on an ESP32-S3. The board opens its own Wi-Fi; everyone at
the table joins it and plays in the phone browser — no app, no internet.
First game: *Vampir Köylü* (Werewolf/Mafia), with roles, night actions and votes
handled by the box so nobody can peek.

> Türkçe: ESP32-S3 kendi Wi-Fi ağını açar, masadaki herkes telefon tarayıcısından
> bağlanıp oynar. Uygulama ve internet gerekmez. Tasarım: [docs/tasarim.md](docs/tasarim.md).

## Two ways to build it

| | **Bare** | **Box** |
|---|---|---|
| Hardware | Any ESP32-S3 board (≥ 8 MB flash), USB-C cable to a phone | Bare + any of: speaker, LED ring, OLED, button, LiPo battery, NFC sticker |
| Game host | The admin phone (usually the one powering the board) runs the game: start, timers, settings | Same, plus the box shows phase/timer/QR and plays narration itself |
| Firmware | Same binary | Same binary; each add-on is switched on in the admin settings |

Every add-on is optional and has a phone fallback; the game never requires one.

## Status

Phase 0 — connectivity. The firmware runs the access point, captive portal and a
WebSocket test page that measures what the rest of the design depends on:
reconnect time after a phone screen lock, 15-phone capacity, phone USB power.
Field-test checklist: [docs/faz0-saha-testi.md](docs/faz0-saha-testi.md).

## Build and flash

Requirements: [PlatformIO Core](https://platformio.org/install/cli) running on
Python 3.13 or older (its ESP-IDF builder cannot install its Python packages on
3.14 yet), Node.js 20+.

```sh
cd firmware
pio run -t upload      # builds web/ first, embeds it, flashes the board
pio device monitor
```

Then join Wi-Fi `OyunKutusu-XXXX` (password `oyunkutusu`) and open `http://192.168.4.1`.

UI development against a running board: join its Wi-Fi from your PC, then
`cd web && npm install && npm run dev`.

## License

MIT — see [LICENSE](LICENSE).
