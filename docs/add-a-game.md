# Add your own game

> Türkçe: [add-a-game.tr.md](add-a-game.tr.md). This English text is the source of truth.

A game is one C++ class on the box plus a few screens in the web UI. The box
owns the rules and every secret; phones only send intents and draw the view they
are given. Vampires & Villagers (`firmware/src/games/vampires.*`, its tests in
`firmware/test/`, its screens in `web/src/`) is the reference implementation:
read it alongside this guide.

The running example below is a made-up game `spy` ("Who's the Spy": everyone
but the spy gets the same secret word). Replace it with your own.

## What you will touch

| Where | What |
|---|---|
| `docs/design.md` (+ `.tr.md`) | Rules: roles, phases, timers, win condition |
| `firmware/src/games/<id>.h`, `<id>.cpp` | The `Game` implementation |
| `firmware/src/games/registry.cpp` | Register the game |
| `firmware/src/CMakeLists.txt` | Add `games/<id>.cpp` to `SRCS` |
| `firmware/test/test_<id>/` | Unity tests and a bot simulator |
| `web/src/` | Screens for your game's views; strings in `web/src/i18n.ts` (`en` and `tr`) |
| `docs/protocol.md` (+ `protocol.tr.md`) | Options, phases, private view and board view |

## 1. Write the rules down first

Decide phases, timers, what each player knows in each phase and how the game
ends. Ask for each phase: *could someone learn a secret from what they see, from
how long others take, or from who is tapping?* Vampires answers this by giving
every player a task at night (so tapping gives nothing away), keeping the night
at least 25 s long and vibrating every phone with the same pattern. Add the
rules as a new section of `docs/design.md` and its translation.

## 2. The `Game` interface

`firmware/src/core/game.h` is the whole contract. Games depend only on `core/`
and the C++ standard library (no ESP-IDF, no Arduino), so they compile for the
ESP32-S3, the native tests and the host simulator alike.

Players are identified by **pid** (room seat 0–14). The set of seated pids is a
`PlayerMask` bitmask fixed for the whole game: index your state by pid and use
`hasPid()` / `countPids()`.

| Method | What it must do |
|---|---|
| `info()` | Return a static `GameInfo{id, minPlayers, maxPlayers}`. `id` is the protocol id (`"spy"`); the lobby lists it with min/max. |
| `setup(players, options)` | Check the player count (`"too_few"` / `"too_many"`), read the options from the start message, **clamping** missing or out-of-range values to defaults (never fail on options), deal roles with `host_.rng()` and enter the first phase. Return `nullptr` on success. |
| `onAction(pid, msg)` | A phone's intent; `msg` is the whole message (`{"t":"act"\|"vote"\|"ack", "phaseId":n, ...}`). Reject anything stale or illegal with an error code from [protocol.md](protocol.md#error-codes) (`late`, `bad_msg`, `bad_target`, `not_allowed`) **without changing state**. On success record it and return `nullptr`; advance early only when that leaks nothing. |
| `onTimer()` | The phase timer fired: end the current phase. |
| `adminSkip()` | The admin's "continue": end the phase early where the rules allow it, else return `"not_allowed"`. |
| `privateView(pid, out)` | Write one JSON object with everything this pid may know and nothing more. Also called for spectators (pids not in the game). |
| `boardView(out)` | Write one JSON object with public table information only. |
| `finished()` / `won(pid)` | Whether the game is over; whether this seated player's side won (feeds the profile stats). |
| `save()` / `load(data, len)` | Binary snapshot of the whole state (options included) and its inverse; `load()` returns `false` for a blob of another version or size. |

The room re-renders every phone's view after each accepted action, timer and
skip; you never push views yourself.

### State: one plain struct, indexed by pid

Keep all state in a single trivially copyable struct: fixed arrays of size
`kMaxPlayers`, small integer types, enums with an explicit underlying type, no
pointers, `std::string` or `std::vector`. Then `save()` is a version tag, a size
and a `memcpy`, and `load()` checks both before copying back:

```cpp
struct State {
  Phase phase;
  uint32_t phaseId;          // bumped on every phase change
  PlayerMask seated, alive;
  int8_t spy;                // pid
  int8_t vote[kMaxPlayers];  // -2 = not voted, -1 = nobody
  // ...
};
static_assert(std::is_trivially_copyable<State>::value, "save() copies State as bytes");
```

Bump your save version whenever the struct changes. Store **no absolute times**:
the room saves the pending timer's remaining time with the snapshot and re-arms
it after `load()`. Keep the struct small (a few hundred bytes); it is written
to flash at every phase change.

### Phases and `phaseId`

Every phase change goes through one helper that bumps `phaseId`, sets the
phase timer and tells the host:

```cpp
void Spy::enter(Phase p) {
  s_.phase = p;
  ++s_.phaseId;                 // phones echo it; older ids get "late"
  host_.setTimer(phaseMs(p));   // 0 = no timer
  host_.phaseChanged();         // the room snapshots the game to flash
}
```

### `GameHost` services

- `rng()` — the only randomness source (`next()`, `below(n)`, `shuffle()`). Never
  use `rand()` or the clock: tests and the bot simulator seed it for reproducible games.
- `setTimer(ms)` — a single phase timer; `onTimer()` fires once. A new call
  replaces the pending one, `0` cancels it. Phones see the remaining time as `endsIn`.
- `phaseChanged()` — call after every phase change so a power cut resumes in the same phase.
- `buzz(players, pattern)` — vibrate phones with a JSON array of on/off
  milliseconds such as `"[200,100,200]"` (iPhones flash the screen). If a
  vibration could reveal a role, buzz everyone with the same pattern.
- `online(pid)` — for "disconnected" badges only. **Never wait for offline
  players**: when time runs out a missing vote is an abstention, a missing night
  pick is "none".

### The hidden-information rule

- Everything secret is decided and stored on the box.
- `privateView(pid)` contains only what that player may know: their own role,
  allies the rules reveal, their own picks and results. Others' secrets are
  simply absent (not `null`, not obfuscated).
- `boardView()` is strictly public: the admin sees it **while playing**, and so
  do members who sit out. Counts of "who has acted" are fine only if everyone
  has a task.
- Spectators (`seated: false`) get the public subset.
- At `end`, reveal everything in every view.
- Write a test that renders every pid's view and checks no secret appears (see step 4).

## 3. Register the game

- `firmware/src/games/registry.cpp`: include your header, add `&Spy::kInfo` to
  the game list and a branch in `create()` that returns `new Spy(host)`.
- `firmware/src/CMakeLists.txt`: add `"games/spy.cpp"` to `SRCS` (the ESP-IDF
  build lists sources explicitly; the PlatformIO `native` and `host` envs pick up
  everything under `src/games/` by themselves).

The lobby now offers the game to the admin (`view.games`) and `start` with
`"game":"spy"` creates it.

## 4. Tests

Tests are Unity programs under `firmware/test/test_<id>/` and run with
`cd firmware && pio test -e native` (add `-f test_spy` to run only yours). Give
the game a small stub `GameHost` with a fixed-seed `Rng` that records the timer,
`phaseChanged()` calls and buzzes, then drive it through `setup()`,
`onAction()` (parse messages with `json::Doc`), `onTimer()` and `adminSkip()`.

Cover at least:
- player-count limits and option clamping;
- every rule in your design section, including ties and edge cases;
- stale `phaseId` → `late`, illegal targets → `bad_target`, wrong phase → `not_allowed`, and that rejected actions change nothing;
- `save()` → `load()` into a fresh instance gives identical views; a truncated or wrong-version blob is refused;
- **no leaks**: for every pid, parse `privateView()` and `boardView()` and check that secrets appear only where allowed.

**Bot simulator.** Add a test that plays thousands of games with random player
counts between `min` and `max` and random seeds. Each bot reads its own private
view, picks a random legal move (and sometimes an illegal one, which must be
rejected), the test fires timers and admin skips at random, and occasionally
round-trips the state through `save()`/`load()`. Assert after every step that
the invariants hold (e.g. dead players do not act, counts add up) and that every
game reaches `finished()` within a step limit. Vampires' simulator is a good template.

## 5. Web screens

The UI picks a screen from the view: a present `game` with `game.seated` shows
your game's screens, chosen by `game.id` and `game.phase`; members who sit out
and the admin's table tab get `board`.

- Add components for each phase next to the Vampires screens; draw only what
  the view says, keep no game logic on the phone, and send intents with the
  current `phaseId`.
- Add the start options to the admin's game picker.
- Every player-facing string goes into both `en` and `tr` in `web/src/i18n.ts`.
- Try it without a board: `cd firmware && pio run -e host`, then `cd ../web && npm run sim`
  and `PG_BOX=http://localhost:7420 npm run dev`, with one tab per player (`?dev=ada`, `?dev=bob`, …).

## 6. Document the protocol

Add a section for your game to [protocol.md](protocol.md), shaped like the
Vampires one: id and player range, the start options table (default and range),
phases and timers, which phases `skip` works in, the private view (`jsonc`
with comments) and the board view. Update `protocol.tr.md` in the same commit
(translate the prose, keep code and JSON identical), and add the game to the
README.

## Checklist

- [ ] Rules in `docs/design.md` and `docs/design.tr.md`
- [ ] `Game` implemented; state is one trivially copyable struct; `save`/`load` versioned
- [ ] Randomness only from `host_.rng()`; nothing waits for offline players
- [ ] Views pass the no-leak test; board view is public
- [ ] Registered in `registry.cpp` and `src/CMakeLists.txt`
- [ ] Unity tests and bot simulator pass in `pio test -e native`
- [ ] Screens and `en`/`tr` strings; playable on a bare board with no add-ons
- [ ] `docs/protocol.md` and `docs/protocol.tr.md` updated
