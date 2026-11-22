# ESP32 Party Games — Design

> Türkçe: [design.tr.md](design.tr.md). This English text is the source of truth.

An offline, app-free party game box on an ESP32-S3. Open source (MIT): one
firmware for every board, every add-on optional, so anyone can build it with
the board they have. The UI is English and Turkish: it opens in Turkish when the
phone's language is Turkish, otherwise in English, and players can switch with
the EN/TR buttons.

## 1. Two setups, one firmware

| | **Bare** | **Box** |
|---|---|---|
| Hardware | Any ESP32-S3 (≥ 8 MB flash) + USB-C cable to a phone | Bare + optional add-ons (section 4) |
| Who runs the game | **Admin phone**: picks the game, sets options, starts it, manages timers; shows the public table info (phase, timer, votes) and plays the narration | Same admin phone; the box also shows phase/timer/QR itself and plays narration through its own speaker |
| Players | Their own phones: the secret card (role, night action, vote) | Same |

Rule: the firmware boots without any add-on and every game is playable without
one. Add-ons only enrich the experience; each has a phone fallback.

**Capacity:** 15 Wi-Fi stations (ESP32-S3 SoftAP limit) → 15 phones including the
admin. Three settings make this possible (`firmware/sdkconfig.defaults`):

- The DHCP server hands out 8 addresses by default; the 9th phone joins Wi-Fi but
  gets no IP → `CONFIG_LWIP_DHCPS_MAX_STATION_NUM=16`.
- SoftAP and ESP-NOW share 17 hardware keys; the ESP-NOW default reserves 7 and
  caps SoftAP at 10 → `CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=0`.
- One WebSocket per phone plus page-load connections → socket limit 32.

## 2. Admin

- The box starts without an admin. The first phone that taps **"Become admin"**
  in the lobby becomes the admin; the right is bound to that phone's token and
  kept in persistent storage (NVS).
- "The phone the ESP is plugged into" cannot be detected over USB: a phone
  browser cannot talk to the board over USB. In practice the person who plugs
  the cable in is the first to join and becomes admin.
- The admin can also play. Admin controls never reveal hidden information; the
  table view (public information) is a separate tab on the admin's phone.
- Admin rights can be handed to another phone. If the admin phone is lost or
  locked, holding the board's **BOOT button for 5 s** resets the admin (no extra hardware).
- Admin settings: game and rule options, timers, kicking players, Wi-Fi
  password, enabling add-ons and changing their pins.

## 3. Joining

- The box opens the `PartyGames-XXXX` network (WPA2, default password
  `partygames`; the admin can change it). The admin phone (or the OLED) shows a
  Wi-Fi QR code.
- A captive DNS resolves every name to `192.168.4.1`; the phone's "is there
  internet" probe lands on the portal page and the OS opens its sign-in sheet.
- **"Play"** on the portal → the box marks that phone (by MAC) as accepted and from
  then on answers its probes with "online". Goal: the phone stays on the network
  instead of falling back to mobile data. Android's HTTPS probe still fails; the
  resulting "limited connectivity" behaviour is measured in the phase 0 field test.
- One origin: **`http://192.168.4.1`**. Player tokens live in `localStorage`,
  which is per origin, so every link, QR code and redirect uses this address;
  never a hostname.
- No HTTPS → no secure context: Wake Lock, service workers, push and
  `crypto.randomUUID` are unavailable. A silent looping video keeps the screen
  on; randomness comes from `crypto.getRandomValues`.

## 4. Optional add-ons

All off by default; enabled from the admin settings, where their pins can also
be changed. The pins below are defaults for the N16R8 DevKitC-1.

| Add-on | In the game | Without it | Default pins |
|---|---|---|---|
| I2S amp (MAX98357A) + speaker | Narration from the box; recordings in flash | Admin phone plays it | BCLK 4, LRCK 5, DIN 6 |
| WS2812B LED ring (≤ 16 LEDs) | Phase colour, countdown, one LED per player | Admin phone table view | DATA 14 |
| 1.3" OLED SH1106 (I2C) | Wi-Fi QR and address on the box | Admin phone shows the QR | SDA 8, SCL 9 |
| Arcade button | Continue / go to vote; buzzer in quiz games | Admin phone buttons | 10 (to GND, internal pull-up) |
| LiPo + charger/boost board | Power independent of phone OTG; battery level | USB power from the phone | ADC 1 (100k/100k divider) |
| NFC sticker (NTAG215) | Tap to open `http://192.168.4.1` | QR / typed address | — (passive) |

Pins to avoid: 0/3/45/46 (strapping; 0 = BOOT button, read-only use),
19/20 (USB), 26–32 (flash), 33–37 (octal PSRAM), 43/44 (UART log).

Power: the LED ring can draw 1 A at full brightness, the speaker 0.6 A peaks.
LED brightness is capped at ~20 % in software; with LEDs + speaker a battery is
recommended.

## 5. Architecture

```
firmware/   PlatformIO + ESP-IDF 5.5 (no Arduino, so sdkconfig limits can change)
  src/      wifi_ap, dns_server, http_server, ws_hub (httpd adapter), nvs_store, boot_button
  src/core/ json, hub (sockets ↔ players), room (profiles, lobby, admin, game host), game interface
  src/games/ registry, vampires
  src/host/ host simulator: core/ + games/ over stdin/stdout (UI work without a board)
  test/     native unit tests and bot simulators (`pio test -e native`)
web/        Vite + Preact + TS → one HTML file, gzipped → embedded in the firmware
docs/       design, protocol, field tests, roadmap
```

- **The box is always right.** A phone states an intent (`act`, `vote`); the
  firmware decides validity and outcome. After every state change each phone
  gets only its own view, in full (no diffs; ~1 KB per message for 15 players).
  Nobody can cheat, and a reconnecting phone recovers with one message.
- **Single-file web:** JS and CSS are inlined; a phone loads the UI over one TCP
  connection (the socket pool is small). The UI is embedded in the firmware, so
  one `.bin` installs everything, also straight from the browser (ESP Web Tools).
- **Game module interface** (`firmware/src/core/game.h`; game logic depends on
  neither Arduino nor ESP-IDF and is tested on a PC). Players are room seats
  (`pid` 0–14), fixed for a whole game, so a game keeps plain pid-indexed state
  that it snapshots with one `memcpy`:

```cpp
class Game {
  virtual const char* setup(PlayerMask players, const json::Value& options) = 0;
  virtual const char* onAction(int pid, const json::Value& msg) = 0;  // error code or nullptr
  virtual void onTimer() = 0;                                          // one phase timer
  virtual const char* adminSkip() = 0;
  virtual void privateView(int pid, json::Writer& out) const = 0;
  virtual void boardView(json::Writer& out) const = 0;                 // table / admin tab
  virtual bool finished() const = 0;
  virtual bool won(int pid) const = 0;
  virtual std::string save() const = 0;                                // snapshot after every phase
  virtual bool load(const uint8_t* data, size_t len) = 0;
};
```

  The room gives the game a `GameHost`: `rng()`, `setTimer(ms)`,
  `phaseChanged()` (snapshot to NVS), `buzz(players, pattern)`, `online(pid)`.
  Adding a game: [add-a-game.md](add-a-game.md).

## 6. Identity and dropouts

- On first visit the phone generates a random id (`localStorage`); the profile
  (name, avatar, colour, stats) is stored on the box in NVS.
- Every connection starts with `hello{id}`; a new socket with the same id closes the old one.
- A socket silent for 15 s is closed (a locked phone stops pinging); the table
  view shows "disconnected". The game does not wait: when time runs out the vote
  counts as "abstain" and the night action as "none".
- Game state is written to persistent storage at every phase change; after a power cut the game resumes.

## 7. WebSocket messages

The single source is [protocol.md](protocol.md): every message, error code and
the full view schema. In short:

| Direction | Messages |
|---|---|
| Phone → box | `hello{id}` · `ping{c}` · `profile{name, avatar, color}` · `ready{on}` · `claimAdmin` · `transfer{pid}` · `kick{pid}` · `settings{wifiPass}` · `restart` · `start{game, options}` · `skip` · `end` · `ack{phaseId}` · `act{phaseId, target}` · `vote{phaseId, target}` |
| Box → phone | `welcome{proto}` · `pong{c}` · `view{seq, view}` · `buzz{p}` · `err{code}` |

`phaseId` rejects late moves; `seq` lets a phone drop stale views. Narration
on the admin phone uses the browser's speech synthesis; the box speaker
(`sound{id}`) comes with the optional add-on in phase 5.

## 8. Vampires & Villagers rules

(Turkish: *Vampir Köylü*; a Werewolf/Mafia-style game.)

**Roles:** Vampire, Villager, Doctor, Seer. Later: Hunter, Jester, Lovers.

| Players | 5–6 | 7–9 | 10–12 | 13–15 |
|---|---|---|---|---|
| Vampires | 1 | 2 | 3 | 3 |
| Doctor + Seer | 1 + 1 | 1 + 1 | 1 + 1 | 1 + 1 |

**Flow:** `LOBBY → ROLE CARD → NIGHT → DAWN → DISCUSSION → VOTE → VERDICT → (NIGHT | RESULT)`

- **Role card:** visible while held down; night starts when everyone taps "Got it".
- **Night:** everyone acts at the same time and **everyone has a task on screen**
  (vampire: victim, doctor: protect, seer: inspect, villager: mark a suspect),
  so touching the screen gives nobody away. Vampires see each other and each
  other's picks live; ties are broken randomly among the tied. A night lasts
  **at least 25 s and at most 60 s**: it waits even if everyone finishes early,
  otherwise its length would be a clue. Every phone vibrates with the same
  pattern at the start and end (on iPhone: screen colour/sound).
- **Resolution order:** doctor's protection → vampires' target (dies unless
  protected) → the seer's private result. The doctor cannot protect the same
  person two nights in a row but may protect themselves.
- **Dawn:** "X died" / "nobody died"; optionally the villagers' suspicion
  distribution (who marked whom is never shown).
- **Discussion:** 3 min by default; the admin can end it early.
- **Vote:** every living player votes for someone or for "nobody"; votes stay
  hidden until everyone has voted, then are shown openly. The player with the
  most votes is lynched; on a tie, one more round among the tied only, and if it
  is still tied nobody is lynched.
- **Verdict:** the lynched player's role is revealed (option, on by default).
- **Win:** the village wins when every vampire is dead; the vampires win when
  living vampires ≥ everyone else alive.
- **The dead** become ghosts; with the option on (default) they see every role and cannot interfere.

## 9. Phases

Phases, tasks and exit criteria live in [roadmap.md](roadmap.md); the launch in [launch-plan.md](launch-plan.md).
