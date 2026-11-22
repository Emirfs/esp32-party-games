# Contributing

**English** · [Türkçe](CONTRIBUTING.tr.md)

Thanks for helping! New games, translations, bug reports from real game nights
and hardware notes are all welcome. Start with [CONTEXT.md](CONTEXT.md) (file
map and contracts), [docs/design.md](docs/design.md) and
[docs/roadmap.md](docs/roadmap.md). Issues labelled `good first issue` are a
good way in; want to add a game? Read [docs/add-a-game.md](docs/add-a-game.md).

## Setup

| Tool | Version | Used for |
|---|---|---|
| [PlatformIO Core](https://platformio.org/install/cli) | on Python 3.13 or older | firmware, native tests, host simulator |
| Node.js | 20+ | web UI (Vite + Preact + TypeScript) |
| Host C++ compiler | g++ with C++17 | `native` and `host` envs (Linux/macOS: system g++ or clang; Windows: MinGW-w64, e.g. WinLibs or MSYS2, on `PATH`) |
| ESP32-S3 board | ≥ 8 MB flash | optional: everything except Wi-Fi runs on a PC |

```sh
git clone https://github.com/Emirfs/esp32-party-games
cd esp32-party-games/web && npm ci
cd ../firmware
pio test -e native     # unit tests + bot simulator
pio run -e host        # host simulator for UI work
pio run                # firmware (builds and embeds web/ first)
pio run -t upload      # flash a board
```

UI work without a board: see
[README → UI development without a board](README.md#ui-development-without-a-board).
After a broken PlatformIO install on the wrong Python, delete
`~/.platformio/penv/.espidf-5.5.3` and rebuild; after moving the repo folder,
delete `firmware/.pio`.

## Repo rules

- **English first.** Code, identifiers, comments, commits, issues and PRs are in English.
- **Bilingual UI.** Every player-facing string goes into both `en` and `tr` in
  `web/src/i18n.ts` (the captive portal has its own inline table in
  `web/public/portal.html`). English is the default and the fallback. Don't speak
  Turkish? Add your best guess and say so in the PR; a reviewer will fix it.
- **Bilingual docs.** English `X.md` is the source of truth; a change to it
  updates `X.tr.md` in the same commit (or ask for help with the translation in the PR).
- **The box is authoritative.** Game rules and hidden information live in the
  firmware. A phone sends intents and receives only its own view; never send a
  phone something its player may not know.
- **Add-ons are optional.** Speaker, LED ring, OLED, button, battery and NFC are
  extras: the firmware must boot and every game must be playable on a bare board plus phones.
- **Protocol changes land in one commit.** [docs/protocol.md](docs/protocol.md)
  (and its translation), the firmware (`firmware/src/core/`, the summary in
  `firmware/src/ws_hub.h`) and the client (`web/src/connection.ts`) change together.
- **Don't lower the capacity settings** in `firmware/sdkconfig.defaults` (DHCP
  leases, sockets, ESP-NOW keys) without re-running the 15-phone test.
- **Game logic stays portable.** `firmware/src/core/` and `firmware/src/games/`
  depend on nothing but the C++ standard library and each other — no ESP-IDF —
  so they run in the native tests and the host simulator.
- Open source (MIT): no secrets, personal data or machine-specific paths in the repo.
- LF line endings.

## Tests

- `cd firmware && pio test -e native` — Unity tests for `core/` and every game,
  plus the bot simulator (thousands of random games with 5–15 bots: every game
  ends and no rule is broken). Tests live in `firmware/test/<test_name>/`.
- `cd web && npm run build` — type check and production build of the UI.
- `cd firmware && pio run` — the firmware still builds for the ESP32-S3.
- Changed something phones see? Try it in the host simulator with one tab per
  player (`?dev=ada`, `?dev=bob`, …), and on real phones if you have a board.

CI runs all of the above on every push and pull request.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/), as in `git log`:

```
feat(firmware): add SoftAP, captive portal server and WebSocket hub

- Plain ESP-IDF 5.5 so sdkconfig can raise DHCP leases (8 -> 16), sockets and SoftAP slots.
- /ws hello/ping/presence hub that closes sockets silent for 15 s.
```

- Types: `feat`, `fix`, `docs`, `refactor`, `test`, `build`, `ci`, `chore`.
- Scopes when useful: `firmware`, `web`, a game id such as `vampires`.
- Subject in the imperative, lower case, no trailing period; body as `- ` bullets saying what and why.
- Breaking changes: `type!:` plus a `BREAKING CHANGE:` footer (for example a changed Wi-Fi default or protocol version).
- One logical change per commit; a doc and its translation go in the same commit.

## Pull request checklist

- [ ] `pio test -e native` passes; new game logic has tests.
- [ ] `npm run build` in `web/` passes.
- [ ] Every new player-facing string is in both `en` and `tr`.
- [ ] Docs changed together with their `.tr.md` translation.
- [ ] Protocol changes: `docs/protocol.md`, firmware and `web/src/connection.ts` in one commit.
- [ ] Hidden information stays on the box; views send each phone only what its player may know.
- [ ] Works on a bare board: no add-on is required.
- [ ] Capacity settings in `sdkconfig.defaults` are not lowered.
- [ ] Tried on phones or in the host simulator (say which in the PR).

## Releases (maintainers)

1. Push a tag `vX.Y.Z` (`-rc.N` suffixes become pre-releases). CI passes the
   version to the firmware (`PG_VERSION` → `PROJECT_VER` in
   `firmware/CMakeLists.txt`), builds everything, publishes
   `esp32-party-games-vX.Y.Z.bin` (bootloader + partition table + app, flash at
   offset 0) and the app-only `firmware.bin` to GitHub Releases, and deploys the
   browser flasher (`flasher/`) with a generated `manifest.json` to GitHub Pages.
2. One-time repository setup: Settings → Pages → Source: **GitHub Actions**; and
   Settings → Environments → `github-pages` → deployment branches and tags: add the tag rule `v*`.
