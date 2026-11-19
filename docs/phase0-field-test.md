# Phase 0 — Field test

> Türkçe: [phase0-field-test.tr.md](phase0-field-test.tr.md). This English text is the source of truth.

Goal: measure the four assumptions the design rests on with real phones. The
results drive the reconnect, portal and power decisions of phase 1.

## Setup

1. `cd firmware && pio run -t upload && pio device monitor`
2. The log should show `SoftAP "PartyGames-XXXX" up` and `listening on http://192.168.4.1`.
3. Wi-Fi: `PartyGames-XXXX`, password `partygames`.
4. Test page `http://192.168.4.1`: connection state, latency, reconnects,
   "After screen on" time, players at the table, box status, event log.

## Tests

### 1. Portal and staying on the network (per OS)

- Join the network → does the sign-in sheet open by itself?
- "Play" → does the test page open? Does the sheet show "Done"?
- Does the page work **with mobile data on**? Does Android warn "limited connectivity" / "no internet", or leave the network?
- After closing the browser and reopening `http://192.168.4.1`, is the "device" id the same (is the id persistent)?

### 2. Screen lock and background

With the page open, lock, wait, unlock. Write down "After screen on" and the event log.

| Wait | 10 s | 30 s | 2 min | 10 min |
|---|---|---|---|---|
| Result (ms / dropped?) | | | | |

Repeat by switching to another app and coming back after 1 min.

### 3. Capacity

As many phones as possible (at least **9**, to pass the old 8-address limit).
Does every phone get an IP and open the page? Note the Wi-Fi / WebSocket counts and the lowest RAM value on the Box card.

### 4. Power (phone OTG)

Power the board from a phone only and leave it 60 min with at least one phone
connected. Then check "Uptime" and "Last boot reason": if uptime is shorter than
expected and the reason is "power on"/"brownout", the phone cut the power. Note
the phone's OTG auto-off setting and its battery drain.

### 5. Vibration

"Try vibration": Android should vibrate; iPhone should say "not supported".

### 6. Language

With the phone set to Turkish, the page and portal should open in Turkish,
otherwise in English. The EN/TR choice should survive a reload and carry over to the portal.

## Results

| Phone / OS / browser | Portal opened | After "Play" | Mobile data on | Id persistent | Lock 30 s | Lock 2 min | Vibration |
|---|---|---|---|---|---|---|---|
| | | | | | | | |

| Capacity: phones | Got IP | Opened page | Lowest RAM |
|---|---|---|---|
| | | | |

| Power source | Duration | Cut? | Boot reason |
|---|---|---|---|
| | | | |
