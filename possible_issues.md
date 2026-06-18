# Possible Issues: Server / Reference GUI Integration

Verified against reference server output (`graphical_test_output.txt`).
Each issue notes whether it is **confirmed** by the reference output, **inferred** from code review, or **not observable** in the passive-only test.

---

## 1. Missing `#` prefix on all player and egg IDs
**Status: Confirmed** (output lines 104-116, 123, 132, etc.)

The reference server prefixes every player ID and egg ID with `#`. Your server sends bare integers.

**Expected formats (from output):**
```
pnw #0 6 6 4 1 steve
pin #0 6 6 9 0 0 0 0 0 0
ebo #0
pgt #0 0
ppo #1 4 1 1
enw #0 #-1 6 6
```

**Required edits:**

`GuiPassive.cpp`:
- `pnw` line 21: `" #" + std::to_string(player->getId())`
- `pex` line 36: `" #" + std::to_string(player->getId())`
- `pbc` line 48: `" #" + std::to_string(player->getId())`
- `pic` line 69 (player IDs in loop): `" #" + std::to_string(player->getId())`
- `pfk` line 93: `" #" + std::to_string(player->getId())`
- `pdr` line 102: `" #" + std::to_string(player->getId())`
- `pgt` line 114: `" #" + std::to_string(player->getId())`
- `pdi` line 127: `" #" + std::to_string(player->getId())`
- `enw` lines 140-141: `" #" + std::to_string(egg_id)` and `" #" + std::to_string(player_id)` (see also issue 2)
- `ebo` line 152: `" #" + std::to_string(egg_id)`
- `edi` line 162: `" #" + std::to_string(egg_id)`

`GuiCommands.cpp`:
- `ppo` line 79: `"ppo #" + std::to_string(player_id) + ...`
- `plv` line 100: `"plv #" + std::to_string(player_id) + ...`
- `pin` line 121: `"pin #" + std::to_string(player_id) + ...`

---

## 2. `enw` argument order is swapped + wrong signature
**Status: Confirmed** (output lines 104-110: `enw #0 #-1 6 6`)

Reference format: `enw #<egg_id> #<player_id> <x> <y>`
Current code (`GuiPassive.cpp:136-145`) sends: `enw <player_id> <egg_id> <x> <y>`

Two problems: egg_id and player_id are swapped, and both lack `#`.

Additionally, initial eggs (created at server start, not by any player) use player_id `= -1`.
The function signature must change to support this.

**Required edit — replace the function:**
```cpp
// GuiPassive.cpp — replace enw()
void Gui::enw(int egg_id, int player_id, int x, int y)
{
    std::string result = "enw";
    result += " #" + std::to_string(egg_id);
    result += " #" + std::to_string(player_id);
    result += " " + std::to_string(x);
    result += " " + std::to_string(y);
    result += "\n";
    send_message(result);
}
```

Update `Gui.hpp` declaration accordingly.
All call sites must be updated to pass `(egg_id, player_id, x, y)` in that order.

---

## 3. GUI incoming `ppo`/`plv`/`pin` requests with `#` prefix cause `std::stoi` to throw
**Status: Not observable** in this passive test, but logically certain.

When the reference GUI queries a player's position it sends `ppo #3`. Your handlers call `std::stoi(args[0])` at `GuiCommands.cpp:73`, `94`, `115`. `std::stoi("#3")` throws `std::invalid_argument`, which is caught silently in `game_loop.cpp:run()` and the GUI receives no reply.

**Required edit — strip `#` before parsing, in all three handlers:**
```cpp
// apply at GuiCommands.cpp:73, 94, 115
std::string id_str = args[0];
if (!id_str.empty() && id_str[0] == '#') id_str = id_str.substr(1);
int player_id = std::stoi(id_str);
```

---

## 4. `sgt` returns milliseconds instead of `f`, and `sst` corrupts game speed
**Status: Confirmed** (output line 2: `sgt 130` when launched with `-f 130`)

`Polling.cpp:22` converts: `time_unit = (7.0 / f) * 1000` (stores 53 ms for `f=130`).
`GuiCommands.cpp:136` returns `time_unit` directly, so the GUI sees `53` instead of `130`.
`sst` then writes the incoming value directly into `time_unit`, treating a frequency as milliseconds.

**Required edits:**

Add to `Server.hpp`:
```cpp
long long _freq = 100;
long long getFreq() const { return _freq; }
```

In `Polling.cpp` (Server constructor):
```cpp
this->_freq = trantorian_time_unit;
this->time_unit = (7.0 / trantorian_time_unit) * 1000;
```

In `GuiCommands.cpp::sgt`:
```cpp
std::string result = "sgt " + std::to_string(server.getFreq()) + "\n";
```

In `GuiCommands.cpp::sst`:
```cpp
server._freq = time_unit;
server.time_unit = (7.0 / time_unit) * 1000;
std::string result = "sst " + std::to_string(server.getFreq()) + "\n";
```

---

## 5. Orientation encoding is 0-based; protocol requires 1-based
**Status: Confirmed** (output line 111: `pnw #0 6 6 4 1 steve` — orientation `4` = WEST; our enum has `WEST=3`)

Reference: `1=NORTH, 2=EAST, 3=SOUTH, 4=WEST`. Your enum: `NORTH=0, EAST=1, SOUTH=2, WEST=3`.
Every `pnw` and `ppo` message sends a value one less than expected.

**Required edits:**

`GuiPassive.cpp::pnw` line 25:
```cpp
result += " " + std::to_string(player->getOrientation() + 1);
```

`GuiCommands.cpp::ppo` line 79:
```cpp
result += " " + std::to_string(static_cast<int>(player->orientation) + 1) + "\n";
```

All new passive `ppo` calls (added for issue 6) must also apply `+ 1`.

---

## 6. No passive `ppo` sent when a player moves or turns
**Status: Confirmed** (output shows dozens of `ppo` updates interleaved with action events)

Your `move_forward`, `turn_right`, `turn_left` (`Movement.cpp:14-49`) never notify the GUI.
The reference server sends `ppo #<id> <x> <y> <orientation>` after every `Forward`, `Right`, and `Left`.

**Required edits:**

Add a passive overload to `GuiPassive.cpp` (and declare it in `Gui.hpp`):
```cpp
void Gui::ppo(std::shared_ptr<Player> player)
{
    std::string result = "ppo #" + std::to_string(player->getId());
    result += " " + std::to_string(player->getX());
    result += " " + std::to_string(player->getY());
    result += " " + std::to_string(player->getOrientation() + 1);
    result += "\n";
    send_message(result);
}
```

In `Movement.cpp`, after every position or orientation change, notify the GUI:
```cpp
// move_forward — after server.move_player(...)
auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
server._gui_subject.Notify([self](Client* c) {
    static_cast<Gui*>(c)->ppo(self);
});

// turn_right and turn_left — after updating orientation
server._gui_subject.Notify([self](Client* c) {
    static_cast<Gui*>(c)->ppo(self);
});
```

Also add a `ppo` notification in `Actions.cpp::eject()` for each ejected player after `server.move_player(*p, nx, ny)`.

---

## 7. After `pgt`/`pdr`, passive `pin` and `bct` must also be sent
**Status: Confirmed** (output lines 123-125 and every subsequent pgt event)

Reference pattern after every take/drop:
```
pgt #0 0
pin #0 6 6 10 0 0 0 0 0 0
bct 6 6 0 0 0 0 0 0 0
```

Your `take_resource` and `set_down_resource` (`Actions.cpp:70-96`, `41-68`) only send `pgt`/`pdr`.

**Required edit — in `take_resource`, after the `pgt` Notify:**
```cpp
server._gui_subject.Notify([self, &server](Client* c) {
    auto gui = static_cast<Gui*>(c);
    gui->pin(server, {std::to_string(self->getId())});
    gui->bct(server, {std::to_string(self->getX()), std::to_string(self->getY())});
});
```
Apply the same pattern in `set_down_resource` after the `pdr` Notify.

---

## 8. Food drain does not send a passive `pin` update
**Status: Confirmed** (output shows inventory counts dropping without any preceding pgt/pdr event, e.g. lines 702-703)

`game_loop.cpp::drain_food` removes food from a player's inventory but never notifies the GUI.

**Required edit — in `drain_food`, after decrementing food:**
```cpp
server._gui_subject.Notify([player](Client* c) {
    static_cast<Gui*>(c)->pin(*server_ref, {std::to_string(player->getId())});
});
```
(Pass `server` by reference into the lambda or refactor `pin` to accept a `Player` directly.)

---

## 9. `gui_start` is incomplete and in the wrong order
**Status: Confirmed** (output lines 1-116 show the full reference init sequence)

Reference `gui_start` order:
1. `msz`
2. `sgt`
3. `bct` for every tile (mct)
4. `tna`
5. `enw` for every existing egg (initial eggs use `player_id = -1`)
6. For each connected player: `pnw`, `pin`, `ebo`

Your `Gui.cpp:14-27` does: `msz` → `mct` → `tna` → `pnw`+`pin` per player → `sgt`.
Missing: `sgt` is in the wrong position, no `enw` for existing eggs, no `ebo` for connected players.

**Required edits in `Gui.cpp::gui_start`:**
```cpp
void Gui::gui_start(Server &server)
{
    msz(server);
    sgt(server);
    mct(server);
    tna(server);
    // send enw for all existing eggs (player_id = -1 for initial/unhatched eggs)
    for (const auto& team : server.getTeams())
        for (const auto& egg : team->eggs)
            enw(egg->getId(), -1, egg->position[0], egg->position[1]);
    // send pnw + pin + ebo for already-connected players
    for (const auto& [fd, client] : server._clients) {
        if (client->get_type() == client_type::PLAYER) {
            auto player = std::dynamic_pointer_cast<Player>(client);
            pnw(player);
            pin(server, {std::to_string(player->getId())});
            ebo(player->egg_id);  // the egg this player hatched from
        }
    }
}
```

This also requires:
- `Egg` to have a unique ID (add a static counter to `Egg` similar to `Player::player_num`)
- `Player` to store the `egg_id` it hatched from (set in `Server::create_player`)
- At game start, initial team slots must be created as actual `Egg` objects (not just a `spots_left` counter), so `gui_start` and `enw` can reference them

---

## 10. `enw` is never called after a player forks
**Status: Inferred** from code review (`Actions.cpp:196-215`)

`fork()` calls `pfk` (fork started) and queues `ok`, but never calls `enw` (egg laid).
The reference protocol sends `pfk` when the action starts and `enw` when it completes (after 42 ticks).

**Required edit:**

Add a fork-completion callback similar to `incantation_end`. When the fork action's `action_done_at` tick fires in `step_player_action`, call `enw` for the new egg:
```cpp
server._gui_subject.Notify([egg_id, player](Client* c) {
    static_cast<Gui*>(c)->enw(egg_id, player->getId(),
                              player->getX(), player->getY());
});
```
The egg should be created at fork start (for the slot) but `enw` sent only at completion.

---

## 11. `ebo` is never sent when a player connects and hatches an egg
**Status: Confirmed** (output lines 113, 116, 119, etc.: `ebo #N` follows every `pnw`+`pin`)

`Server::create_player` (`server_helper.cpp:105-147`) pops an egg but never calls `ebo`.

**Required edit — in `create_player`, after popping the egg:**
```cpp
int used_egg_id = egg->getId();
server._gui_subject.Notify([used_egg_id](Client* c) {
    static_cast<Gui*>(c)->ebo(used_egg_id);
});
```
(The `gui_subject` and GUI must exist at this point; defer if needed until `add_client` is called.)

---

## 12. `edi` is never sent when an egg is destroyed by eject
**Status: Not observable** in this test, but inferred from code review

`Actions.cpp::eject()` moves players but does not destroy eggs on the tile, even though the spec requires it. `edi` is never called anywhere.

**Required edit — in `eject()`, destroy eggs on the tile and notify:**
```cpp
auto& tile = server._map[position[1]][position[0]];
for (const auto& team : server.teams) {
    team->eggs.erase(std::remove_if(team->eggs.begin(), team->eggs.end(),
        [&](const auto& egg) {
            if (egg->position[0] == position[0] && egg->position[1] == position[1]) {
                int eid = egg->getId();
                server._gui_subject.Notify([eid](Client* c) {
                    static_cast<Gui*>(c)->edi(eid);
                });
                team->spots_left--;
                return true;
            }
            return false;
        }), team->eggs.end());
}
```

---

## 13. Eject `K` direction is computed incorrectly
**Status: Not observable** in this test, but inferred from code review

`Actions.cpp:107`: `int k = (static_cast<int>(orientation) + 2) % 4 + 1;`

This gives K in range 1–4 and ignores the ejected player's own orientation. K must be computed per ejected player in the same frame as the `broadcast_dir` logic (1=ahead, 3=right, 5=behind, 7=left from the ejected player's perspective).

**Required edit — compute K per ejected player inside the eject loop:**
```cpp
for (const auto &p : to_eject) {
    int world_from  = (static_cast<int>(orientation) + 2) % 4; // direction they came from
    int local_from  = (world_from - static_cast<int>(p->orientation) + 4) % 4;
    int k           = local_from * 2 + 1; // maps 0→1, 1→3, 2→5, 3→7
    ...
    p->send_message("eject " + std::to_string(k) + "\n");
}
```

---

## 14. `bct` is not sent after resource respawn
**Status: Inferred** from reference behavior (bct is always sent when tile contents change)

`game_loop.cpp::respawn_resources` calls `populate_map_resources()` but sends no `bct` updates. The GUI map will silently go stale every 20 ticks.

**Required edit — after `populate_map_resources()` in `respawn_resources()`:**
```cpp
server._gui_subject.Notify([&server](Client* c) {
    static_cast<Gui*>(c)->mct(server);  // resend all tile contents
});
```
Or send individual `bct` only for tiles that actually changed.

---

## 15. `send_message` is an unbuffered raw `write()`
**Status: Not observable** in this test, but a latent reliability issue

`Client.cpp:13-16` calls `write()` directly with no retry or partial-write check. If the TCP send buffer fills mid-game, data is silently dropped without error.

**Required edit:** Either use a per-client send buffer with `POLLOUT`-gated flushing, or at minimum wrap the `write()` call to retry on partial writes.

---

## Priority Order

| # | Issue | Status | Impact |
|---|---|---|---|
| 1 | Missing `#` prefix everywhere | Confirmed | GUI parser breaks on every message |
| 5 | Orientation 0-based (should be 1-based) | Confirmed | All `pnw`/`ppo` orientations wrong |
| 2 | `enw` argument order swapped | Confirmed | Eggs assigned wrong IDs/positions |
| 4 | `sgt`/`sst` use ms not `f` | Confirmed | Wrong speed shown and set by GUI |
| 6 | No passive `ppo` on movement/turn | Confirmed | GUI player positions go stale immediately |
| 7 | No passive `pin`+`bct` after pgt/pdr | Confirmed | GUI inventory and tile state drift |
| 8 | No passive `pin` on food drain | Confirmed | GUI inventory drift over time |
| 9 | `gui_start` incomplete and wrong order | Confirmed | GUI starts with no eggs, wrong sgt position |
| 3 | `stoi` crash on `#` prefix in GUI requests | Inferred | GUI queries return no response |
| 10 | `enw` never called after fork | Inferred | GUI never sees eggs laid mid-game |
| 11 | `ebo` never called on player connect | Confirmed | GUI never sees egg hatch events |
| 12 | `edi` never called on eject | Inferred | GUI never sees egg death events |
| 13 | Eject K direction wrong | Inferred | Wrong direction reported to ejected players |
| 14 | No `bct` after respawn | Inferred | GUI map drifts every 20 ticks |
| 15 | Unbuffered `write()` | Inferred | Silent data loss under load |
