# AGENTS.md

- Read `CONTEXT.md` first; design and rules live in `docs/design.md`, the current phase and its tasks in `docs/roadmap.md`.
- Open source (MIT): no secrets, personal data or machine-specific paths in the repo.
- English first. Code, identifiers, comments, commits, issues and PRs are English.
- Bilingual UI: every player-facing string goes into both `en` and `tr` in `web/src/i18n.ts` (portal: the inline table in `web/public/portal.html`); English is the default and fallback.
- Docs: English is the source of truth; a change to `X.md` updates its `X.tr.md` translation in the same commit.
- Every add-on (speaker, LED ring, OLED, button, battery, NFC) stays optional: the firmware must boot and every game must be playable on a bare board plus phones.
- The box is authoritative: game rules and hidden information live in the firmware; a phone only receives its own view.
- Game logic (`firmware/src/core/`, `firmware/src/games/`) stays free of ESP-IDF so `pio test -e native` and the host simulator run it on a PC; every rule change keeps those tests green.
- A WebSocket protocol change updates `docs/protocol.md` (source of truth, and its `.tr.md`), the firmware (`core/`, `games/`, the summary in `ws_hub.h`) and `web/` in the same commit.
- Do not lower the capacity settings in `firmware/sdkconfig.defaults` (DHCP leases, sockets, ESP-NOW keys) without re-running the 15-phone test.
