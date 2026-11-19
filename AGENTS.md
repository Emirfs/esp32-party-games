# AGENTS.md

- Read `CONTEXT.md` first; design and rules live in `docs/tasarim.md`.
- Open source (MIT): no secrets, personal data or machine-specific paths in the repo.
- Code, identifiers, comments, commits: English. Player-facing UI text and repo docs: Turkish (README is English).
- Every add-on (speaker, LED ring, OLED, button, battery, NFC) stays optional: the firmware must boot and every game must be playable on a bare board plus phones.
- The box is authoritative: game rules and hidden information live in the firmware; a phone only receives its own view.
- A WebSocket protocol change updates `firmware/src/ws_hub.h` (source of truth) and `web/src/connection.ts` in the same commit.
- Do not lower the capacity settings in `firmware/sdkconfig.defaults` (DHCP leases, sockets, ESP-NOW keys) without re-running the 15-phone test.
