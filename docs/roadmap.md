# Roadmap

> Türkçe: [roadmap.tr.md](roadmap.tr.md). This English text is the source of truth.

The single source for phases, their tasks and exit criteria. Design and rules:
[design.md](design.md). Launch: [launch-plan.md](launch-plan.md).

**Start here:** phase 0 field test → phase 1. Phases 1, 2 and 3 must be done
before the launch (phase 4); add-ons and new games come after it.

| Phase | Goal | Status |
|---|---|---|
| 0 | Connectivity: AP, captive portal, WebSocket test page (now `#diag`) | Code done, field test open |
| 1 | Lobby, profiles, admin, game engine skeleton | Code done; exit check needs real phones |
| 2 | Vampires & Villagers | Code done, simulator passes; real game open |
| 3 | Distribution: browser flasher, releases, README | Code done; issues, `main` and first tag open |
| 4 | Launch | Not started |
| 5 | Optional add-ons | Not started |
| 6 | More games | Not started |

## Phase 0 — Connectivity

- [x] SoftAP for 15 phones (16 DHCP leases, 0 ESP-NOW keys, 32 sockets)
- [x] Captive DNS, OS connectivity probes, MAC-keyed portal acceptance
- [x] UI embedded as one gzipped HTML file; pre-build script rebuilds `web/`
- [x] `/ws` hello/ping/presence; sockets silent for 15 s are closed
- [x] English/Turkish UI and portal
- [ ] Field test with real phones: [phase0-field-test.md](phase0-field-test.md) (Android + iPhone, lock/unlock, ≥ 9 phones, 60 min OTG power)
- [ ] Write the results into the field test tables and decide for phase 1: idle timeout, portal strategy, whether keep-screen-on is needed, battery vs phone power

**Exit:** result tables filled, phase 1 decisions written down.

## Phase 1 — Lobby, profiles, admin

Firmware (`firmware/src/core/`, game logic free of ESP-IDF so it runs on a PC):
- [x] Protocol v1 ([protocol.md](protocol.md), summary in `ws_hub.h`): `hello{id}`, `profile{name, avatar, color}`, `ready`, `start{game, options}`, `view{seq, view}` (each phone gets only its own full view), `err{code}`, admin requests
- [x] Session: player id ↔ socket; a new socket with the same id replaces the old one (`core/hub.*`)
- [x] Profiles in NVS: name, avatar emoji, colour, stats (32 phones remembered, LRU)
- [x] Lobby: player list, ready state, kick; a full room reuses an offline member's seat
- [x] Admin: first "Become admin" wins, transfer to another phone, reset by holding BOOT (GPIO0) for 5 s
- [x] Admin settings: Wi-Fi password (applied after restart), game options, timers
- [x] Engine skeleton: `Game` interface (design §5), phase ids, timers, state snapshot to NVS at every phase change, resume after a power cut
- [x] PlatformIO `native` env with unit tests for `core/`; `host` env + `npm run sim` to run the UI without a board

Web:
- [x] Screens chosen from the view: profile, lobby, admin tab, table view
- [x] Wi-Fi QR (`WIFI:T:WPA;S:…;P:…;;`) and URL QR on the admin phone
- [x] Keep the screen on (silent looping video; Wake Lock needs HTTPS)
- [x] Vibration helper for `buzz{p}` (Android; screen flash on iPhone)
- [x] Every new string in both `en` and `tr`

**Exit:** 6+ phones join and set profiles, the admin starts a placeholder game;
lock/unlock keeps each player's identity; a power cut returns everyone to the same lobby.

## Phase 2 — Vampires & Villagers

- [x] Rules from design §8: Vampire, Villager, Doctor, Seer; role counts per player count
- [x] Simultaneous night with a task for everyone; night lasts 25–60 s regardless of speed
- [x] Vampires see each other's picks live; ties broken randomly
- [x] Resolution order: protect → kill → seer result; doctor cannot repeat a target
- [x] Dawn, discussion (admin can end early), hidden-then-revealed vote, one tie-break round, verdict, win check
- [x] Options: reveal role on lynch, ghosts see roles, discussion length, vote length, suspicion stats
- [x] UI: hold-to-reveal role card, night task screens, vote, results; table view (with optional spoken narration) for the admin
- [x] Bot simulator in the `native` env: 5000 random games with 5–15 bots; every game ends, no rule is broken

**Exit:** simulator passes; a full real game with friends.

## Phase 3 — Distribution (before launch)

- [x] GitHub Actions: build web + firmware + native tests; on a tag publish a merged `.bin` (bootloader + partitions + app) to Releases
- [x] GitHub Pages flasher with ESP Web Tools; "Flash from browser" button in the README
- [x] README: hero image at the top (`docs/media/hero.png`), 2-minute quick start, social preview image (`docs/media/social-preview.png`, set it in the repo settings); the real-table video comes with the launch filming
- [x] `CONTRIBUTING.md`, "Add your own game" guide ([add-a-game.md](add-a-game.md))
- [ ] A few `good first issue`s on GitHub
- [ ] Merge the feature branch into `main` and make `main` the default branch

**Exit:** a blank board is set up with nothing but a browser.

## Phase 4 — Launch

Follow [launch-plan.md](launch-plan.md).

**Exit:** posted on every channel in the plan within one week; first-week issues answered.

## Phase 5 — Optional add-ons

Each add-on is off by default, enabled and re-pinned from admin settings, and has a phone fallback (design §4).
- [ ] I2S speaker + narration recordings in a flash partition
- [ ] WS2812B LED ring: phase colour, countdown, one LED per player (brightness capped)
- [ ] SH1106 OLED: Wi-Fi QR and address on the box
- [ ] Arcade button: continue / go to vote
- [ ] Battery level via ADC
- [ ] NFC sticker how-to (no firmware)
- [ ] Build guide with wiring and an enclosure

**Exit:** each add-on works on its own; a full game runs with none of them.

## Phase 6 — More games

- [ ] Who's the Spy (proves the engine interface is general)
- Backlog: quiz with buzzer, Who's most likely, Two truths and a lie, Draw & guess, Imposter word game
