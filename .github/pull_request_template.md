## What and why

<!-- What changes, why, and the issue it closes (e.g. "Closes #12"). -->

## How it was tested

<!-- pio test -e native, npm run build, host simulator with N browser windows, real phones (models/OS), board model. -->

## Checklist

- [ ] `cd firmware && pio test -e native` passes; new game logic has tests
- [ ] `cd web && npm run build` passes
- [ ] Every new player-facing string is in both `en` and `tr` (`web/src/i18n.ts`, portal table)
- [ ] Changed docs updated together with their `.tr.md` translation
- [ ] Protocol change? `docs/protocol.md`, firmware and `web/src/connection.ts` in this one commit
- [ ] Hidden information stays on the box; each phone gets only what its player may know
- [ ] Works on a bare board: no add-on is required
- [ ] Capacity settings in `firmware/sdkconfig.defaults` are not lowered
- [ ] Commits follow Conventional Commits (`feat(web): ...`, `fix(firmware): ...`)
