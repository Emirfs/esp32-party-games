# Roadmap

> Türkçe: [roadmap.tr.md](roadmap.tr.md). This English text is the source of truth.

The single source for phases, their tasks and exit criteria. Design and rules:
[design.md](design.md). Launch: [launch-plan.md](launch-plan.md).

**Start here:** phase 0 field test → phase 1. Phases 1, 2 and 3 must be done
before the launch (phase 4); add-ons and new games come after it.

| Phase | Goal | Status |
|---|---|---|
| 0 | Connectivity: AP, captive portal, WebSocket test page | Code done, field test open |
| 1 | Lobby, profiles, admin, game engine skeleton | Not started |
| 2 | Vampires & Villagers | Not started |
| 3 | Distribution: browser flasher, releases, README | Not started |
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
- [ ] Protocol v1 in `ws_hub.h`: `hello{id}`, `profile{name, avatar}`, `ready`, `start{game, options}`, `view{seq, view}` (each phone gets only its own full view), `err{code}`
- [ ] Session: player id ↔ socket; a new socket with the same id replaces the old one
- [ ] Profiles in NVS: name, avatar emoji, colour, stats
- [ ] Lobby: player list, ready state, kick
- [ ] Admin: first "Become admin" wins, transfer to another phone, reset by holding BOOT (GPIO0) for 5 s
- [ ] Admin settings: Wi-Fi password, game options, timers
- [ ] Engine skeleton: `Game` interface (design §5), phase ids, timers, state snapshot to NVS at every phase change, resume after a power cut
- [ ] PlatformIO `native` env with unit tests for `core/`

Web:
- [ ] Screens chosen by `view.type`: profile, lobby, admin tab, table view
- [ ] Wi-Fi QR (`WIFI:T:WPA;S:…;P:…;;`) and URL QR on the admin phone
- [ ] Keep the screen on (silent looping video; Wake Lock needs HTTPS)
- [ ] Vibration helper for `buzz{pattern}` (Android only; screen flash on iPhone)
- [ ] Every new string in both `en` and `tr`

**Exit:** 6+ phones join and set profiles, the admin starts a placeholder game;
lock/unlock keeps each player's identity; a power cut returns everyone to the same lobby.

## Phase 2 — Vampires & Villagers

- [ ] Rules from design §8: Vampire, Villager, Doctor, Seer; role counts per player count
- [ ] Simultaneous night with a task for everyone; night lasts 25–60 s regardless of speed
- [ ] Vampires see each other's picks live; ties broken randomly
- [ ] Resolution order: protect → kill → seer result; doctor cannot repeat a target
- [ ] Dawn, discussion (admin can end early), hidden-then-revealed vote, one tie-break round, verdict, win check
- [ ] Options: reveal role on lynch, ghosts see roles, discussion length, suspicion stats
- [ ] UI: hold-to-reveal role card, night task screens, vote, results; table view for the admin
- [ ] Bot simulator in the `native` env: thousands of random games with 5–15 bots; every game ends, no rule is broken

**Exit:** simulator passes; a full real game with friends.

## Phase 3 — Distribution (before launch)

- [ ] GitHub Actions: build web + firmware; on a tag publish a merged `.bin` (bootloader + partitions + app) to Releases
- [ ] GitHub Pages flasher with ESP Web Tools; "Flash from browser" button in the README
- [ ] README: hero GIF/video at the top, 2-minute quick start, social preview image
- [ ] `CONTRIBUTING.md`, "Add your own game" guide, a few `good first issue`s
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
