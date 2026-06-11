# Zappy Heuristic AI

Fully heuristic Python AI for the Zappy game. Each player runs as an independent
process connected to the server over TCP. Players coordinate with each other only
through the in-game broadcast mechanism — no shared memory, no external bus.

---

## Quick start

```bash
# from the repo root
PYTHONPATH=Ai/heuristic_ai python3 Ai/heuristic_ai/main.py -p 4242 -n team1 -h localhost
```

The compiled wrapper `Ai/zappy_ai` is a shell script that sets `PYTHONPATH` and calls
`main.py`, so in tournament use you just run `./zappy_ai -p PORT -n TEAM -h HOST`.

---

## File map

```
Ai/heuristic_ai/
├── main.py           entry point — arg parsing, handshake, launch
├── connection.py     non-blocking TCP socket + command pipeline
├── commands.py       thin wrappers for every server command
├── broadcast.py      team broadcast protocol + direction navigation
├── vision.py         look-response parser + tile navigation helpers
├── resources.py      elevation table, deficit helpers, rarity order
├── config.py         all tunable parameters + JSON-override loader
├── state_machine.py  State enum
└── player/
    ├── state.py      PlayerAI class — main loop + state machine
    └── actions.py    PlayerActionsMixin — navigation, events, rituals
```

---

## Architecture overview

### 1. Connection (`connection.py`)

A thin wrapper around a non-blocking TCP socket. Key design points:

- **Pipeline**: the Zappy server accepts up to 10 commands before the first response
  arrives. `Connection.push()` sends immediately and queues the `(command, callback)`
  pair. `pump()` (called every loop tick) reads available bytes, splits on `\n`, and
  dispatches each line to the oldest queued callback in FIFO order.
- **Unsolicited messages**: `dead`, `message K, text`, `eject: K`, and
  `Current level: K` arrive outside the request/response rhythm. `_handle_line`
  catches them before touching the pending queue and routes them to registered
  callback slots (`on_dead`, `on_broadcast`, `on_eject`, `on_level_up`).
- **Incantation edge case**: after `Elevation underway` the server may later send
  `ko` (end-check failure) instead of `Current level: K`. A dedicated
  `_incantation_end_cb` slot intercepts that `ko` so it does not fall through to the
  next pending command's callback and desync the pipeline.
- **Handshake** is the one blocking operation: the socket is temporarily set to
  blocking mode, three lines are read synchronously (WELCOME, slots, dimensions),
  then the socket goes back to non-blocking for the rest of the game.

### 2. Commands (`commands.py`)

Every server command is a one-line wrapper that calls `conn.push()` and converts
the raw response string to a typed value before calling the callback:

| Function        | Server command   | Callback type           |
|-----------------|------------------|-------------------------|
| `forward`       | `Forward`        | `bool` (ok/ko)          |
| `turn_right`    | `Right`          | `bool`                  |
| `turn_left`     | `Left`           | `bool`                  |
| `look`          | `Look`           | `list[list[str]]` tiles |
| `inventory`     | `Inventory`      | `dict[str, int]`        |
| `broadcast`     | `Broadcast TEXT` | `bool`                  |
| `connect_nbr`   | `Connect_nbr`    | `int` (open slots)      |
| `fork`          | `Fork`           | `bool`                  |
| `eject`         | `Eject`          | `bool`                  |
| `take`          | `Take RES`       | `bool`                  |
| `set_down`      | `Set RES`        | `bool`                  |
| `incantation`   | `Incantation`    | `int \| None` new level |

`incantation` is special: it registers `on_level_up` and `set_incantation_end_cb`
on the connection to catch both exit paths (success `Current level: K` and failure
`ko` after underway).

### 3. Broadcast protocol (`broadcast.py`)

All team messages share the prefix `ZAPPY:` so they can be silently ignored by
other teams. Format: `ZAPPY:<TYPE>:<PAYLOAD>`.

| Type       | Constant    | Payload            | Meaning                              |
|------------|-------------|--------------------|--------------------------------------|
| `NEED_INC` | `NEED_INC`  | `"level:uid"`      | Leader calling same-level teammates  |
| `IM_COMING`| `IM_COMING` | `"leader_uid"`     | Follower acknowledging the call      |
| `IM_READY` | `IM_READY`  | `"leader_uid"`     | Follower arrived on leader's tile    |
| `START`    | `START`     | `"leader_uid"`     | Leader firing incantation            |
| `LVL_UP`   | `LVL`       | `"new_level"`      | Ritual finished (success or failure) |
| `FORK`     | `FORK`      | `"team_name"`      | Player announcing a fork             |

**Sound-based homing** (`moves_toward`): The server delivers broadcasts with a
direction integer `k` (0 = same tile, 1–8 = adjacent/diagonal sectors). The
function converts `k` into at most two primitives (a turn + a forward step) so the
follower makes one step of progress per ping and re-aims on the next ping. Pure
rotation without a step causes the follower to orbit the target and never reach it.

```
  k layout (from protocol spec):
    2  1  8
    3  @  7
    4  5  6

  k=0 → []                    (already there)
  k=1 → [Forward]             (straight ahead)
  k=2,3,4 → [Left, Forward]   (leader on the left)
  k=5 → [Left, Left, Forward] (directly behind)
  k=6,7,8 → [Right, Forward]  (leader on the right)
```

### 4. Vision (`vision.py`)

Helpers that work on the `list[list[str]]` produced by `look`:

- **`find_resource(tiles, name)`** — linear scan, returns the index of the nearest
  tile containing the resource, `None` if not visible.
- **`tile_to_moves(index, level)`** — converts a tile index to a move sequence. The
  look grid grows one row per level: row `r` starts at index `r²` and has `2r+1`
  tiles. If a resource is mostly ahead (lateral offset < row depth) it returns up to
  3 `Forward` commands to close distance fast; otherwise it turns once then steps.
- **`count_players_on_tile(tiles, 0)`** — counts `"player"` strings on tile 0 for
  the leader's attendance check.

### 5. Resources (`resources.py`)

Contains the full elevation table copied from the PDF spec:

| Level | Players | linemate | deraumere | sibur | mendiane | phiras | thystame |
|-------|---------|----------|-----------|-------|----------|--------|----------|
| 1→2   | 1       | 1        | –         | –     | –        | –      | –        |
| 2→3   | 2       | 1        | 1         | 1     | –        | –      | –        |
| 3→4   | 2       | 2        | –         | 1     | –        | 2      | –        |
| 4→5   | 4       | 1        | 1         | 2     | –        | 1      | –        |
| 5→6   | 4       | 1        | 2         | 1     | 3        | –      | –        |
| 6→7   | 6       | 1        | 2         | 3     | –        | 1      | –        |
| 7→8   | 6       | 2        | 2         | 2     | 2        | 2      | 1        |

Stone collection priority is rarest-first (`RARITY_ORDER`): thystame → phiras →
mendiane → sibur → deraumere → linemate. This avoids collecting plenty of linemate
while the one thystame on the map is still uncollected.

### 6. State machine (`state_machine.py`, `player/state.py`)

Each player is always in exactly one of these states:

```
SURVIVE       food < FOOD_CRITICAL (5)  — emergency, drop everything
GATHER_FOOD   food < FOOD_SAFE          — top up before doing anything
GATHER_STONES food ok, collecting stones for the current level ritual
SEEK_TEAM     all stones collected, homing toward incantation leader
WAIT_TEAM     on the leader tile, waiting for enough players
INCANTATING   frozen during the ritual, no actions possible
FORKING       executing a fork, brief detour from GATHER_FOOD/STONES
```

Transition rules (checked every tick in `_update_state`):

```
any state       → SURVIVE       if food < FOOD_CRITICAL (except INCANTATING)
SURVIVE         → GATHER_FOOD   if food >= FOOD_CRITICAL + 2
GATHER_FOOD     → GATHER_STONES if food >= FOOD_SAFE
GATHER_STONES   → GATHER_FOOD   if food < FOOD_LOW
GATHER_STONES   → WAIT_TEAM     if has_all_stones AND players_needed == 1
GATHER_STONES   → SEEK_TEAM     if has_all_stones AND players_needed > 1
SEEK_TEAM       → GATHER_STONES if seek_ticks > SEEK_TIMEOUT
WAIT_TEAM       → GATHER_STONES if seek_ticks > WAIT_TIMEOUT
WAIT_TEAM/SEEK  → INCANTATING   on ritual fire
INCANTATING     → GATHER_FOOD   when ritual ends (success or failure)
```

`_coord_cooldown` is applied on timeout to prevent an immediately re-entering the
coordination dance right after a failed attempt, which caused tight loops.

### 7. Food thresholds (dynamic, map-aware)

Fixed thresholds fail on large maps (players burn food just walking between sparse
resources) and hoard more than the map can provide on dense small maps. Instead all
thresholds are computed at startup from map dimensions and estimated player count:

```python
total_food  = 0.5 * world_x * world_y          # approx food on the map
food_pp     = total_food / max(6, slots + 1)   # per-player share

FOOD_SAFE = max(food_safe_min,
                FOOD_CRITICAL + int((world_x + world_y) * 0.8),   # survive-floor
                min(food_safe_max,
                    food_pp * food_safe_mult + (world_x + world_y) * food_travel_factor))

FOOD_LOW  = clamp(food_pp * food_low_mult, food_low_min, food_low_max)
# coherence guard: FOOD_LOW must be < FOOD_SAFE
if FOOD_LOW >= FOOD_SAFE:
    FOOD_LOW = max(FOOD_CRITICAL + 1, FOOD_SAFE - 3)

FOOD_JOIN = max(FOOD_CRITICAL + join_food_buffer,
                FOOD_CRITICAL + int((world_x + world_y) * 0.5))
```

`FOOD_JOIN` (floor to accept a NEED_INC call) is kept well below `FOOD_LOW` on
purpose: joining a nearby leader is cheap (1–4 food measured) so gating it behind
`FOOD_LOW` caused all partners to refuse on food-scarce maps and rituals never fired.

### 8. Coordination protocol (multi-player ritual)

The protocol is asymmetric: one player is the **leader**, the rest are **followers**.

```
Leader flow:
  1. GATHER_STONES → SEEK_TEAM (has all stones, players_needed > 1)
  2. listen LISTEN_BEFORE_LEAD ticks for an existing NEED_INC
  3. no reply → _become_leader: broadcast NEED_INC("level:uid"), move to WAIT_TEAM
  4. in WAIT_TEAM: drop stones, re-broadcast NEED_INC every BCAST_INTERVAL ticks
  5. when (confirmed ≥ needed OR present_ticks ≥ 30) AND stones_ok:
     → broadcast START(uid), send Incantation
  6. ritual ends → broadcast LVL_UP(new_level), go to GATHER_FOOD

Follower flow:
  1. hear NEED_INC at same level, food ≥ FOOD_JOIN, no current leader
     → broadcast IM_COMING(leader_uid), SEEK_TEAM toward leader
  2. homing: on each new ping recalculate one move via moves_toward(k)
  3. k == 0 → arrived → broadcast IM_READY(leader_uid), WAIT_TEAM
  4. in WAIT_TEAM: re-broadcast IM_READY every BCAST_INTERVAL (handles lost acks)
  5. hear START(leader_uid) → INCANTATING (no Incantation sent, server auto-includes)
  6. success: "Current level: K" via _on_level_up → GATHER_FOOD
     failure: LVL_UP broadcast from leader → GATHER_FOOD
```

**Leader election**: If two players both become leaders simultaneously (race on
`LISTEN_BEFORE_LEAD`), the one with the higher UID yields — it switches to follower
mode and homes toward the lower UID leader. Yield only happens when
`_ready_count == 0` (no followers have committed yet); a leader with followers stays
put to avoid leaving committed followers homing to an empty tile.

**Forking**: Done opportunistically during `GATHER_FOOD` and `GATHER_STONES` every
`fork_interval` ticks. `Connect_nbr` is checked first; if slots are already open,
forking is skipped (42/f time units wasted otherwise).

### 9. Navigation

**Known-resource navigation** (`_navigate_to_resource`):
1. If the resource is on tile 0 → `Take` it immediately.
2. If it appears in the look grid → `tile_to_moves` → execute moves.
3. Not visible → `_wander`.

**Coverage wander** (`_wander`): Instead of a pure random walk the player tracks
visited tiles using dead reckoning (private spawn-relative coordinate frame). It
scores all four neighbours by how long ago they were visited (unvisited wins) and
heads toward the least-recently-visited one. This cuts search time on sparse large
maps compared to a random walk that keeps revisiting cleared tiles.

**Dead reckoning** (`_track_move`): Applied at command-issue time (not on response)
because Forward/Left/Right never fail in Zappy (the map is a torus with no walls),
so the state stays in sync with the server without waiting for confirmations. Eject
can desync it — see Known Issues.

---

## Configuration (`config.py`)

All magic numbers live in `DEFAULTS`. Override any subset via a JSON file:

```bash
ZAPPY_AI_CONFIG=my_params.json ./zappy_ai -p 4242 -n team1
```

Key parameters:

| Key                  | Default | Description                                           |
|----------------------|---------|-------------------------------------------------------|
| `food_critical`      | 5       | Drop everything and find food now                     |
| `food_safe_mult`     | 1.2     | Multiplier on food-per-player for FOOD_SAFE           |
| `food_safe_min/max`  | 15/50   | Clamp on computed FOOD_SAFE                           |
| `food_travel_factor` | 0.3     | Extra food reserve per unit of map span (w+h)         |
| `food_low_mult`      | 0.9     | Multiplier for FOOD_LOW (re-enter food gathering)     |
| `join_food_buffer`   | 3       | Extra food above FOOD_CRITICAL required to join call  |
| `seek_timeout`       | 500     | Ticks before giving up homing to a leader             |
| `wait_timeout_mult`  | 2.0     | Leader wait = seek_timeout × this                     |
| `listen_before_lead` | 30      | Ticks to listen for an existing NEED_INC before self-leading |
| `bcast_interval`     | 2       | Ticks between leader re-broadcasts / follower IM_READY |
| `fork_min_food`      | 30      | Minimum food before forking                           |
| `fork_interval`      | 150     | Ticks between fork attempts                           |
| `leader_eat_in_wait` | 1       | Leader/followers eat food on the ritual tile while waiting |

The `seek_timeout` and `wait_timeout` are automatically scaled by map span
(`(world_x + world_y) / 20`) so the same config works on both 10×10 and 20×20 maps.

Loop speed can be tuned without changing the config file:
```bash
ZAPPY_AI_TICK=0.001 ./zappy_ai -p 4242 -n team1   # keep up with high-frequency server
```

---

## Known issues

### 1. Single `on_level_up` callback slot (fragile follower level-up)

`Connection` only has one `on_level_up` slot. The `PlayerAI` constructor registers
`_on_level_up` there, but `cmd.incantation()` overwrites it with its own handler
every time the player fires an incantation, then sets the slot to `None` when the
ritual ends. After the first incantation where this player was the **leader**, the
init-registered `_on_level_up` is dead.

In practice this is okay because:
- Leaders track level via `_on_incantation_result`.
- Followers that weren't the one sending `Incantation` fall back to the `LVL_UP`
  broadcast the leader sends after the ritual.

But if a follower somehow needs a server-sent `Current level: K` and the leader's
`LVL_UP` broadcast is lost, the follower stays frozen in `INCANTATING` until
`WAIT_TIMEOUT` fires. The real fix is a list of `on_level_up` handlers instead of a
single slot.

### 2. `parse_look` in `vision.py` is dead code

`vision.py` exports a `parse_look(raw)` function (lines 9–24) that parses the raw
look string. However `commands.py`'s `look()` wrapper does the same parsing inline
and is the only caller. `parse_look` is never called anywhere. It should either
replace the inline parser in `commands.py` or be deleted.

### 3. `extra_player_wait` and `extra_wait_min_level` are loaded but never used

Both config parameters are read, stored as `self.EXTRA_PLAYER_WAIT` and
`self.EXTRA_WAIT_MIN_LVL`, and commented "unused now" throughout the code. They're
dead config keys that add noise to the tuner's search space. Either wire them back
up or remove them from `DEFAULTS` and the init.

### 4. `_full_wait_ticks` is tracked but never read

`_full_wait_ticks` is reset to `0` in `_transition` and incremented nowhere that
matters. It was presumably used for the "wait extra for more players" branch that was
removed. It should be deleted.

### 5. Dead reckoning desyncs on eject

When `_on_eject` fires, `_pos` and `_facing` are **not** updated. The server pushes
the player to a new tile in a direction the AI didn't control, so the stored
coordinates are now wrong. The wander coverage map will be incorrect until the player
has moved enough to accidentally re-sync. This hurts exploration efficiency (the AI
might think it's visiting a new tile when it's actually revisiting a cleared one) but
never causes incorrect behaviour (the look-based navigation doesn't depend on `_pos`).

### 6. Leader election still has a race window

The `LISTEN_BEFORE_LEAD` guard (30 ticks) reduces — but does not eliminate — the
chance of two players becoming leaders simultaneously. If two players start listening
at the same tick and neither hears the other within the window (possible on large maps
where broadcast propagation delay is longer), both call `_become_leader`. The UID
yield then resolves it, **but only if `_ready_count == 0`**. If a third player
happened to send `IM_READY` to the higher-UID leader before the yield check, the
higher-UID leader will not yield and both rituals stall. This is rare but observable
on large maps with many players.

### 7. `tile_to_moves` returns up to 3 Forward steps

The function docstring says "one step closer" but when a resource is mostly-ahead it
returns `["Forward"] * min(row, 3)` — up to 3 steps. This is intentional (close
distance fast) but inconsistent with the single-step contract implied by the name and
docstring. The follower homing code (`moves_toward`) correctly does one primitive per
ping; only `_navigate_to_resource` calls `tile_to_moves`, so the multi-step return
only applies to resource gathering, not coordination.

### 8. `_visited` coverage dict grows without bound

`_visited: dict[tuple[int,int], int]` accumulates every tile ever stepped on and is
never pruned. On a typical Zappy map (≤ 20×20 = 400 cells) this is negligible, but
on a hypothetical very large map or a very long game the dict would grow
proportionally to `world_x * world_y`.
