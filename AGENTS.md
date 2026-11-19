# AGENTS.md

- Read `CONTEXT.md` first; design and rules live in `docs/design.md`, the current phase and its tasks in `docs/roadmap.md`.
- Open source (MIT): no secrets, personal data or machine-specific paths in the repo.
- English first. Code, identifiers, comments, commits, issues and PRs are English.
- Bilingual UI: every player-facing string goes into both `en` and `tr` in `web/src/i18n.ts` (portal: the inline table in `web/public/portal.html`); English is the default and fallback.
- Docs: English is the source of truth; a change to `X.md` updates its `X.tr.md` translation in the same commit.
- Every add-on (speaker, LED ring, OLED, button, battery, NFC) stays optional: the firmware must boot and every game must be playable on a bare board plus phones.
- The box is authoritative: game rules and hidden information live in the firmware; a phone only receives its own view.
- A WebSocket protocol change updates `firmware/src/ws_hub.h` (source of truth) and `web/src/connection.ts` in the same commit.
- Do not lower the capacity settings in `firmware/sdkconfig.defaults` (DHCP leases, sockets, ESP-NOW keys) without re-running the 15-phone test.
