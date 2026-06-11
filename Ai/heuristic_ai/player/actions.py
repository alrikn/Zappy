##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## action, navigation and event handler methods for the player ai
##

import random
import time
from state_machine import State
from resources import STONES, ELEVATION, players_needed, next_stone_to_collect
import commands as cmd
import broadcast as bcast
from vision import find_resource, tile_to_moves, count_players_on_tile

# dead reckoning: facing index to (dx, dy) applied on a forward step, in the player
# private spawn relative frame, order is an arbitrary but consistent set of cardinals,
# right rotates +1, left rotates +3 (ie -1) thru it
_FACING_DELTA = [(0, 1), (1, 0), (0, -1), (-1, 0)]


class PlayerActionsMixin:
    """
    mixin that holds all the action/navigation/event methods for PlayerAI,
    split out so player/state.py stays focused on the state machine and loop
    """

    # leader side of the incantation coordination

    def _become_leader(self):
        """takes on the leader role, broadcasts need_inc and moves to wait state"""
        self._leader_uid  = self.uid
        self._ready_count = 0
        # only recruit for real team rituals, a solo ritual (needed==1) must stay silent
        # so it doesnt lure same level players into a pointless chase
        if players_needed(self.level) > 1:
            text = bcast.encode(bcast.NEED_INC, f"{self.level}:{self.uid}")
            cmd.broadcast(self.conn, text, self._noop)
        # leader is already at the meeting point so go straight to waiting
        self._transition(State.WAIT_TEAM)

    def _fire_incantation(self):
        """all players are here, send the start signal and kick off the ritual"""
        self._log_incant_tile("LEADER fire")
        self._incant_t0 = time.time()
        text = bcast.encode(bcast.START, self.uid)
        cmd.broadcast(self.conn, text, self._noop)
        self._transition(State.INCANTATING)
        self._action_pending = True
        cmd.incantation(self.conn, self._on_incantation_result)

    def _home_log(self):
        """throttled diagnostic so a test run shows wether homing converges (k to 0)"""
        self._home_ticks += 1
        if self._home_ticks % 15 == 1:
            print(f"[{self.uid}] homing -> {self._leader_uid} k={self._leader_k} "
                  f"seek_ticks={self._seek_ticks}/{self.SEEK_TIMEOUT}")

    def _log_incant_tile(self, who: str):
        """diagnostic: dump the tile contents we think we re incanting on"""
        tile     = self._tiles[0] if self._tiles else []
        req      = ELEVATION.get(self.level, {})
        have     = {s: tile.count(s) for s in STONES if req.get(s, 0) > 0}
        need     = {s: req.get(s, 0) for s in have}
        print(f"[{self.uid}] {who} lvl{self.level} players_on_tile={tile.count('player')} "
              f"need={players_needed(self.level)} ready_cnt={self._ready_count} "
              f"stones_have={have} stones_need={need}")

    def _on_incantation_result(self, new_level: int | None):
        self._action_pending = False
        self._stones_dropped = False
        food = self.inventory.get("food", 0)
        if new_level is not None:
            self.level = new_level
            print(f"[{self.uid}] leveled up to {self.level}!  food={food}")
        else:
            elapsed = time.time() - getattr(self, "_incant_t0", time.time())
            phase = "start-check (immediate)" if elapsed < 0.5 else "end-check (after underway)"
            print(f"[{self.uid}] incantation ko [{phase}, {elapsed:.2f}s]  food={food}")
        # always signal followers that the ritual ended (success OR failure) so they exit
        # incantating, followers dont send incantation so they only get the server
        # "current level: x" on success, this broadcast is their exit on failure
        cmd.broadcast(self.conn, bcast.encode(bcast.LVL_UP, str(self.level)), self._noop)
        self._transition(State.GATHER_FOOD)

    # navigation helpers

    def _navigate_to_resource(self, resource: str):
        """
        tries to move toward the given resource using look data,
        if the resource is on our current tile we just take it,
        if its visible in a nearby tile we move one step toward it,
        if its not visible at all we just wander and hope to find it
        """
        tiles = self._tiles
        if not tiles:
            # tiles were cleared after a move or failed take, wait for fresh look
            # rather than wandering blindly with stale positional data
            return

        # take from current tile immediately if its there
        if resource in tiles[0]:
            self._action_pending = True
            def on_taken(ok: bool, _res=resource):
                if ok:
                    # item taken: remove one instance from tile0 so we can
                    # immediately take another if more are on the same tile
                    try:
                        self._tiles[0].remove(_res)
                    except (ValueError, IndexError):
                        pass
                else:
                    # tile doesn't have it anymore: stale tiles, force fresh look
                    self._tiles = []
                self._action_pending = False
            cmd.take(self.conn, resource, on_taken)
            return

        idx = find_resource(tiles, resource)
        if idx is None:
            self._wander()
            return

        moves = tile_to_moves(idx, self.level)
        if moves:
            self._execute_moves(moves)
        else:
            self._wander()

    def _wander(self):
        """
        explore toward the least recently visited neighbour instead of moving at random,
        systematic coverage finds sparse resources on a big map way faster than a random
        walk that keeps re treading tiles it already cleared, this is the sound single
        player version of the shared map idea (we cant merge maps across players without
        a common coord origin which zappy doesnt give us, but each player remembering its
        own coverage still cuts search time)
        """
        # score our four neighbours by how long ago we stepped on them, unvisited (-1)
        # wins, shuffle so ties dont make evry player sweep in lockstep
        dirs = [0, 1, 2, 3]
        random.shuffle(dirs)
        best_dir, best_age = self._facing, None
        for d in dirs:
            dx, dy = _FACING_DELTA[d]
            nb = ((self._pos[0] + dx) % self.world_x,
                  (self._pos[1] + dy) % self.world_y)
            age = self._visited.get(nb, -1)
            if best_age is None or age < best_age:
                best_age, best_dir = age, d
        # turn from our current facing toward the chosen direction, then step
        diff = (best_dir - self._facing) % 4
        if diff == 1:
            moves = ["Right", "Forward"]
        elif diff == 2:
            moves = ["Right", "Right", "Forward"]
        elif diff == 3:
            moves = ["Left", "Forward"]
        else:
            moves = ["Forward"]
        self._execute_moves(moves)

    def _track_move(self, move: str):
        """
        update our private dead reckoned facing/pos as each move is issued, forward and
        turns never fail in zappy (the torus has no walls) so applying the update at
        issue time stays in sync w/ the server without waiting on the response
        """
        if move == "Right":
            self._facing = (self._facing + 1) % 4
        elif move == "Left":
            self._facing = (self._facing + 3) % 4
        elif move == "Forward":
            dx, dy = _FACING_DELTA[self._facing]
            self._pos[0] = (self._pos[0] + dx) % self.world_x
            self._pos[1] = (self._pos[1] + dy) % self.world_y
            self._move_tick += 1
            self._visited[(self._pos[0], self._pos[1])] = self._move_tick

    def _execute_moves(self, moves: list[str]):
        """
        sends all moves in the list to the server using the pipeline,
        _action_pending clears only after the last one completes,
        for straight ahead paths this pipelines up to 3 forwards at once
        """
        if not moves:
            return
        self._action_pending = True
        pending = [len(moves)]

        def done(_):
            pending[0] -= 1
            if pending[0] <= 0:
                # position changed: old tiles are now wrong, force fresh look
                self._tiles = []
                self._clear_action()

        for move in moves:
            # keep dead reckoning in sync (additive, doesnt change what we send)
            self._track_move(move)
            if move == "Forward":
                cmd.forward(self.conn, done)
            elif move == "Right":
                cmd.turn_right(self.conn, done)
            elif move == "Left":
                cmd.turn_left(self.conn, done)

    def _drop_for_ritual(self):
        """
        drops the required stones onto the current tile before incantating,
        each player contributes up to what the ritual needs (min of have vs need),
        the server checks the tile so stones must be on the ground not in inventory,
        food is never dropped
        """
        if self.level not in ELEVATION:
            return
        needed  = ELEVATION[self.level]
        dropped = False
        for stone in STONES:
            have = self.inventory.get(stone, 0)
            need = needed.get(stone, 0)
            for _ in range(min(have, need)):
                cmd.set_down(self.conn, stone, self._noop)
                dropped = True
        if dropped:
            # clear stale tile data so the stones_ok check waits for a fresh look that
            # reflects the newly dropped stones on the ground
            self._tiles = []

    # forking

    def _try_fork(self):
        """
        spawns a new player only if slots are actually needed and we have enuf food,
        we check connect_nbr first bc forking when slots are already open is a waste
        of 42/f time units
        """
        if self.inventory.get("food", 0) < self.cfg.i("fork_min_food"):
            return

        prev_state = self.state

        def on_nbr(slots: int):
            if slots > 0:
                # slots still available, no need to fork rn
                return
            self._transition(State.FORKING)
            self._action_pending = True

            def on_fork(_: bool):
                self._action_pending = False
                cmd.broadcast(self.conn, bcast.encode(bcast.FORKING, self.team_name),
                              self._noop)
                self._transition(prev_state)

            cmd.fork(self.conn, on_fork)

        cmd.connect_nbr(self.conn, on_nbr)

    # inventory and look refresh

    def _request_inventory(self):
        self._inventory_pending = True

        def on_inv(inv: dict):
            self._inventory_pending = False
            if inv:
                self.inventory = inv

        cmd.inventory(self.conn, on_inv)

    def _request_look(self):
        self._look_pending = True

        def on_look(tiles: list):
            self._look_pending = False
            if tiles:
                self._tiles = tiles

        cmd.look(self.conn, on_look)

    # callbacks

    def _on_take_done(self, ok: bool):
        self._action_pending = False

    def _clear_action(self, *_):
        self._action_pending = False

    def _noop(self, *_):
        # discard callback for fire and forget cmds where we dont care about the resp
        pass

    def _on_broadcast(self, k: int, text: str):
        """
        handles incoming broadcast messages,
        we only care about messages from our own team (prefixed with zappy:)
        """
        parsed = bcast.decode(text)
        if parsed is None:
            return
        msg_type, payload = parsed

        if msg_type == bcast.NEED_INC:
            parts = payload.split(":")
            if len(parts) != 2:
                return
            level_str, leader_uid = parts
            try:
                level = int(level_str)
            except ValueError:
                return
            # if we re already following this leader, refresh the bearing, _k_fresh tells
            # the homing loop to act on this new direction next tick
            if self._leader_uid == leader_uid and self.state == State.SEEK_TEAM:
                self._leader_k = k
                self._k_fresh  = True
                return

            # leader election: yield to the player w/ the lower uid so only one leader
            # exists per level group, this MUST also fire while we re already waiting
            # (wait_team), else two players both self elect, both sit in wait_team
            # broadcasting and neither ever joins the other, they just burn their whole
            # timeout (the "two leaders home to each other" thrash we saw on 20x20), the
            # higher uid leader yields and walks over to the lower one, merges the two
            # stalled groups into a single firing ritual
            if (level == self.level
                    and self._leader_uid == self.uid
                    and self.state in (State.SEEK_TEAM, State.WAIT_TEAM)
                    and leader_uid < self.uid
                    and self._ready_count == 0):
                # only yield if NOBODY is counting on us yet, a leader that already has
                # committed followers must stay put, abandoning to chase a lower uid
                # leader leaves its followers homing to an empty tile (the cascade that
                # had everyone leading AND following at once so no ritual fired)
                print(f"[{self.uid}] yielding leadership to {leader_uid} (from {self.state})")
                self._leader_uid     = leader_uid
                self._leader_k       = k
                self._k_fresh        = True
                self._stones_dropped = False  # we ll re drop at the new leader tile
                cmd.broadcast(self.conn, bcast.encode(bcast.IM_COMING, leader_uid),
                               self._noop)
                self._transition(State.SEEK_TEAM)  # go home to the elected leader
                return

            # join if we re the right level, not already committed, and can survive the
            # meet+ritual, the leader is a known nearby target so this is cheap (like 1
            # to 4 food measured), we gate on food_join (survival floor) NOT food_low,
            # gating on food_low made evry partner reject on food scarce maps so 2 player
            # rituals never fired and the team capped at level 2
            food = self.inventory.get("food", 0)
            if (level == self.level
                    and self._leader_uid is None
                    and self.state in (State.GATHER_STONES, State.GATHER_FOOD)
                    and food >= self.FOOD_JOIN):
                print(f"[{self.uid}] joining {leader_uid} for lvl{level} ritual  food={food}")
                self._leader_uid = leader_uid
                self._leader_k   = k
                self._k_fresh    = True
                self._seek_ticks = 0
                cmd.broadcast(self.conn, bcast.encode(bcast.IM_COMING, leader_uid),
                               self._noop)
                self._transition(State.SEEK_TEAM)
            elif level == self.level and self._leader_uid is None and self.state in (State.GATHER_STONES, State.GATHER_FOOD):
                self._reject_count += 1
                if self._reject_count % 10 == 1:
                    print(f"[{self.uid}] rejected NEED_INC x{self._reject_count}: food={food} < {self.FOOD_JOIN}")

        elif msg_type == bcast.IM_READY:
            # a follower arrived at our tile (only relevant if we are the leader)
            if payload == self.uid:
                self._ready_count += 1

        elif msg_type == bcast.LVL_UP:
            # leader finished the ritual (success or failure), if we re a follower still
            # frozen in incantating (either the ritual failed so the server never sent us
            # "current level: x", or _on_level_up was cleared bc we were the leader in a
            # previous ritual) use this as the exit signal, payload is the leader new
            # level on success, old level on failure
            if (self.state == State.INCANTATING
                    and self._leader_uid is not None
                    and self._leader_uid != self.uid):
                food = self.inventory.get("food", 0)
                try:
                    broadcast_level = int(payload)
                except ValueError:
                    broadcast_level = None
                if broadcast_level is not None and broadcast_level > self.level:
                    print(f"[{self.uid}] FOLLOWER lvl-up {self.level} -> {broadcast_level} via LVL_UP  food={food}")
                    self.level = broadcast_level
                else:
                    print(f"[{self.uid}] FOLLOWER exiting INCANTATING (ko) via LVL_UP  food={food}")
                self._action_pending = False
                self._stones_dropped = False
                self._transition(State.GATHER_FOOD)

        elif msg_type == bcast.START:
            # leader says ritual starts, stay frozen on the tile without sending our own
            # incantation, the server auto includes all players on the tile and sends
            # "current level: x" to all of them on success (_on_level_up handles that),
            # if the ritual fails the leader broadcasts lvl_up above so we still exit clean
            if payload == self._leader_uid and self.state == State.WAIT_TEAM:
                self._log_incant_tile("FOLLOWER")
                self._incant_t0 = time.time()
                self._transition(State.INCANTATING)
                self._action_pending = True

    def _on_eject(self, k: int):
        """
        we got pushed off our tile by another player,
        if we were waiting for the ritual we need to go back to the leader
        """
        self._tiles = []
        if self.state == State.WAIT_TEAM:
            self._leader_k = k
            self._transition(State.SEEK_TEAM)

    def _on_level_up(self, level: int):
        # fires when the server sends "current level: x" to a follower that was on the
        # tile during the ritual (didnt send incantation so cmd.incantation on_level
        # handler was never registered, this one stays active instead)
        prev = self.level
        self.level = level
        food = self.inventory.get("food", 0)
        print(f"[{self.uid}] passive lvl-up {prev} -> {self.level}  food={food}")
        if self.state == State.INCANTATING:
            self._action_pending = False
            self._stones_dropped = False
            self._transition(State.GATHER_FOOD)
