##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## action, navigation and event handler methods for the player ai
##

import random
from state_machine import State
from resources import STONES, ELEVATION, players_needed, next_stone_to_collect
import commands as cmd
import broadcast as bcast
from vision import find_resource, tile_to_moves, count_players_on_tile

FOOD_FORK_MIN = 30  # need at least this much food before we fork


class PlayerActionsMixin:
    """
    mixin that holds all the action/navigation/event methods for PlayerAI,
    split out so player/state.py stays focused on the state machine and loop
    """

    # leader side of the incantation coordination

    def _become_leader(self):
        """takes on the leader role, broadcasts NEED_INC and moves to wait state"""
        self._leader_uid  = self.uid
        self._ready_count = 0
        text = bcast.encode(bcast.NEED_INC, f"{self.level}:{self.uid}")
        cmd.broadcast(self.conn, text, self._noop)
        # leader is already at the meeting point so go straight to waiting
        self._transition(State.WAIT_TEAM)

    def _fire_incantation(self):
        """all players are here, send the START signal and kick off the ritual"""
        text = bcast.encode(bcast.START, self.uid)
        cmd.broadcast(self.conn, text, self._noop)
        self._transition(State.INCANTATING)
        self._action_pending = True
        cmd.incantation(self.conn, self._on_incantation_result)

    def _on_incantation_result(self, new_level: int | None):
        self._action_pending = False
        self._stones_dropped = False
        if new_level is not None:
            self.level = new_level
            print(f"[{self.uid}] leveled up to {self.level}!")
            cmd.broadcast(self.conn, bcast.encode(bcast.LVL_UP, str(self.level)),
                          self._noop)
        # whether it worked or not, go back to gathering food
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
            self._wander()
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
        """random movement when we dont know where to go"""
        self._action_pending = True
        if random.random() < 0.3:
            cmd.turn_right(self.conn, self._clear_action)
        else:
            cmd.forward(self.conn, self._clear_action)

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
                self._clear_action()

        for move in moves:
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
        needed = ELEVATION[self.level]
        for stone in STONES:
            have = self.inventory.get(stone, 0)
            need = needed.get(stone, 0)
            # drop up to what's required, not excess
            for _ in range(min(have, need)):
                cmd.set_down(self.conn, stone, self._noop)

    # forking

    def _try_fork(self):
        """
        spawns a new player only if slots are actually needed and we have enuf food,
        we check connect_nbr first bc forking when slots are already open is a waste
        of 42/f time units
        """
        if self.inventory.get("food", 0) < FOOD_FORK_MIN:
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
        we only care about messages from our own team (prefixed with ZAPPY:)
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
            # if we're already folowing this leader, just update the direction
            if self._leader_uid == leader_uid and self.state == State.SEEK_TEAM:
                self._leader_k = k
                return
            # join if we're the right level and not already committed to someone else
            if (level == self.level
                    and self._leader_uid is None
                    and self.state in (State.GATHER_STONES, State.GATHER_FOOD)):
                self._leader_uid = leader_uid
                self._leader_k   = k
                self._seek_ticks = 0
                cmd.broadcast(self.conn, bcast.encode(bcast.IM_COMING, leader_uid),
                               self._noop)
                self._transition(State.SEEK_TEAM)

        elif msg_type == bcast.IM_READY:
            # a follower arrived at our tile (only relevant if we are the leader)
            if payload == self.uid:
                self._ready_count += 1

        elif msg_type == bcast.START:
            # leader says start, we join the incant
            if payload == self._leader_uid and self.state == State.WAIT_TEAM:
                self._transition(State.INCANTATING)
                self._action_pending = True
                cmd.incantation(self.conn, self._on_incantation_result)

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
        # this fires if the lvl up comes from an external incantation we joined
        self.level = level
