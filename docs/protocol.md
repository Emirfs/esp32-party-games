# WebSocket protocol v1

> Türkçe: [protocol.tr.md](protocol.tr.md). This English text is the source of truth.

One WebSocket per phone at `ws://192.168.4.1/ws`, JSON text frames, at most
1024 bytes from a phone. Every message has a type field `t`. The firmware side
lives in `firmware/src/core/` (`hub.cpp`, `room.cpp`, `games/*`); the summary in
`firmware/src/ws_hub.h` and the client in `web/src/connection.ts` change in the
same commit as this file.

**The box is authoritative.** A phone only states an intent; the box checks it
and, after every state change, sends each phone its own complete `view`. There
are no diffs: a reconnecting phone recovers with one message.

**Player ids.** The phone's `clientId` (random, `[A-Za-z0-9_-]{1,23}`, kept in
`localStorage` as `pg.clientId`) is a secret: whoever knows it is that player.
It is never sent to other phones. Everyone else sees the player's public
`pid` (room seat 0–14).

## Connection

| Phone → box | Meaning |
|---|---|
| `{"t":"hello","id":"<clientId>"}` | First message on every socket. A second socket with the same id closes the older one. |
| `{"t":"ping","c":<ms>}` | Every 3 s. A socket silent for 15 s is closed by the box. |

| Box → phone | Meaning |
|---|---|
| `{"t":"welcome","proto":1}` | Reply to hello; a `view` follows. |
| `{"t":"pong","c":<ms>}` | Echo of `c`. |
| `{"t":"view","seq":<n>,"view":{…}}` | The phone's full state. `seq` grows; drop a view with a smaller `seq` than the last one. |
| `{"t":"err","code":"<code>"}` | The last request was rejected; nothing changed. |
| `{"t":"buzz","p":[<ms on>,<ms off>,…]}` | Vibrate now (iPhone: flash the screen). |

Messages before `hello`, pings included, get `err` `hello_first`; the phone
then sends `hello` again (its hello was lost, e.g. the box restarted).

## Lobby and admin requests

| Message | Who | Effect |
|---|---|---|
| `{"t":"profile","name":"Ada","avatar":"🦊","color":3}` | anyone | Creates or edits the profile and joins the room. Name 1–16 characters, avatar one emoji (≤ 16 bytes), color 0–11. |
| `{"t":"ready","on":true}` | member, lobby | "I'm playing the next game". |
| `{"t":"claimAdmin"}` | member | Only while the box has no admin. |
| `{"t":"transfer","pid":3}` | admin | Hands admin rights to member 3. |
| `{"t":"kick","pid":3}` | admin, lobby | Removes member 3 from the room (they can join again). |
| `{"t":"settings","wifiPass":"…"}` | admin | New Wi-Fi password (8–63 printable ASCII), applied after a restart. |
| `{"t":"restart"}` | admin | Reboots the box. |
| `{"t":"start","game":"vampires","options":{…}}` | admin, lobby | Starts a game with every ready member. |
| `{"t":"skip"}` | admin, game | Ends the current phase early where the rules allow it. |
| `{"t":"end"}` | admin, game | Back to the lobby. A finished game counts in the stats, an aborted one does not. |

## Game requests

`phaseId` comes from the current view; a stale one gets `err` `late`.

| Message | Effect |
|---|---|
| `{"t":"ack","phaseId":n}` | "Got it" on the role card. |
| `{"t":"act","phaseId":n,"target":pid}` | Night pick (the task kind is implied by the role). May be changed until the night ends. |
| `{"t":"vote","phaseId":n,"target":pid}` | Vote; `-1` = nobody. May be changed until the votes are revealed. |

## Error codes

`hello_first`, `bad_msg`, `no_profile`, `bad_name`, `bad_avatar`,
`bad_color`, `room_full`, `not_admin`, `admin_taken`, `bad_pid`, `in_game`,
`no_game`, `bad_game`, `too_few`, `too_many`, `bad_pass`, `late`,
`bad_target`, `not_allowed`.

## View

Every key is always present unless marked *optional*.

```jsonc
{
  "me": 2,                 // my pid, -1 = not a member yet (show the profile screen)
  "profile": {"name": "Ada", "avatar": "🦊", "color": 3, "played": 4, "won": 2},  // or null
  "admin": 0,              // admin's pid, -1 = nobody (offline admins keep it)
  "isAdmin": false,
  "full": false,           // room has 15 members; a new profile is refused
  "players": [             // room members, by pid
    {"pid": 0, "name": "Ada", "avatar": "🦊", "color": 3, "online": true, "ready": true, "inGame": false}
  ],
  "games": [{"id": "vampires", "min": 5, "max": 15}],
  "endsIn": 23000,         // ms left on the game's phase timer, -1 = none
  "box": {"ssid": "PartyGames-1A2B", "wifiPass": "partygames"},  // optional: admin only
  "game": {…},             // optional: a game exists (running or finished); the game's private view
  "board": {…}             // optional: admin, or a member who is not in the game; public table view
}
```

The screen follows from the view: `me == -1` → profile; `game` present and
`game.seated` → game screens; otherwise the lobby (with `board` while others play).

## Vampires & Villagers (`"vampires"`)

Rules: [design.md §8](design.md#8-vampires--villagers-rules). 5–15 players.

Start options (missing or out of range → default):

| Option | Default | Range |
|---|---|---|
| `revealRole` (lynched player's role is shown) | `true` | bool |
| `ghostsSeeRoles` | `true` | bool |
| `suspicion` (dawn shows villagers' suspicion counts) | `true` | bool |
| `discussionSec` | `180` | 30–600 |
| `voteSec` | `60` | 20–180 |

Phases: `role` → `night` → `dawn` → `discussion` → `vote` → `verdict` → `night`
… → `end`. A tied vote goes `vote` → `verdict` (with `runoff: true`) → `vote`
(round 2) → `verdict`. Roles: `vampire`, `villager`, `doctor`, `seer`. Timers:
role card ≤ 90 s, night 25–60 s, dawn 8 s, discussion and vote from options,
verdict 8 s. The admin's `skip` works in every phase except `night` and `end`.

Private view (`game`):

```jsonc
{
  "id": "vampires",
  "phase": "night",        // role | night | dawn | discussion | vote | verdict | end
  "phaseId": 7,
  "round": 2,              // night number, 1-based
  "seated": true,          // false: spectator (only id/phase/phaseId/round/seated/players/winner)
  "options": {"revealRole": true, "ghostsSeeRoles": true, "suspicion": true, "discussionSec": 180, "voteSec": 60},
  "players": [             // everyone in the game
    {"pid": 0, "alive": true, "role": "vampire"}   // "role" only when this phone may know it
  ],
  "role": "seer",          // seated only: my role
  "alive": true,           // seated only
  "acked": false,          // role phase: did I tap "Got it"
  "ackCount": 3, "ackTotal": 7,  // role phase
  "task": {"kind": "inspect", "targets": [0, 1, 4], "pick": -1},  // night, alive: kill | protect | inspect | suspect
  "allies": [{"pid": 4, "pick": 1}],             // night, vampire: the other vampires and their picks
  "seen": [{"pid": 1, "vampire": false}],        // seer: every result so far
  "dawn": {"died": 3, "suspicion": [{"pid": 1, "n": 2}]},  // dawn and discussion; died -1 = nobody; suspicion only with the option
  "vote": {"round": 1, "candidates": [0, 1, 4], "mine": -2, "cast": 2, "voters": 6},  // vote; mine -2 = not voted, -1 = nobody
  "tally": {"round": 1, "votes": [{"voter": 0, "target": 4}], "lynched": 4, "role": "vampire", "runoff": false},  // verdict; abstainers are absent; role only with revealRole
  "winner": "village"      // end only: village | vampires; every role is then in players
}
```

Role visibility in `players`: my own; fellow vampires if I am a vampire;
lynched players' roles if `revealRole`; every role if I am dead and
`ghostsSeeRoles`, and for everyone at `end`. The seer's results are only in `seen`.

Board view (`board`, public): `id`, `phase`, `phaseId`, `round`, `players`
(roles only where public: lynched with `revealRole`, all at `end`), `acted`
and `actors` (night: how many alive players picked, never what; role phase:
acks), `dawn`, `vote` (without `mine`), `tally`, `winner`.
