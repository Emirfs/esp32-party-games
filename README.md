# ESP32 Party Games

**English** · [Türkçe](README.tr.md)

Offline party games on an ESP32-S3. The board opens its own Wi-Fi; everyone at
the table joins it and plays in the phone browser — no app, no internet.
First game: *Vampires & Villagers* (Werewolf/Mafia-style), with roles, night
actions and votes handled by the box so nobody can peek. The UI speaks English
and Turkish. Design: [docs/design.md](docs/design.md).

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
Field-test checklist: [docs/phase0-field-test.md](docs/phase0-field-test.md).

## Build and flash

Requirements: [PlatformIO Core](https://platformio.org/install/cli) running on
Python 3.13 or older (its ESP-IDF builder cannot install its Python packages on
3.14 yet), Node.js 20+.

```sh
cd firmware
pio run -t upload      # builds web/ first, embeds it, flashes the board
pio device monitor
```

Then join Wi-Fi `PartyGames-XXXX` (password `partygames`) and open `http://192.168.4.1`.

UI development against a running board: join its Wi-Fi from your PC, then
`cd web && npm install && npm run dev`.

## Languages

The UI opens in Turkish when the phone's language is Turkish and in English
otherwise; the EN/TR buttons switch it. Strings live in `web/src/i18n.ts`.
Docs are written in English with Turkish translations next to them (`*.tr.md`).

## License

MIT — see [LICENSE](LICENSE).
