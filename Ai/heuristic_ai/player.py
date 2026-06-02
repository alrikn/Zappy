##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## main player ai 
##

import random
import time
from connection import Connection
from state_machine import State
from resources import (
    get_deficit, has_all_stones, next_stone_to_collect, players_needed, STONES
)
import commands as cmd
import broadcast as bcast
from vision import parse_look, find_resource, count_players_on_tile, tile_to_moves

# food thresholds
FOOD_CRITICAL = 5   # below this we go into emergency mode
FOOD_SAFE     = 20  # above this we can focus on stones
FOOD_FORK_MIN = 30  # need at least this much food before we fork

# how many loop ticks we wait in SEEK_TEAM before giving up and restarting
# avoids getting stuck forever if the leader dies mid coordination
SEEK_TIMEOUT = 200


def _uid() -> str:
    # short random hex id to identify this player instance in broadcasts
    return f"{random.randint(0, 0xFFFF):04x}"


class PlayerAI:
    def __init__(self, conn: Connection, team_name: str, world_x: int, world_y: int):
        self.conn      = conn
        self.team_name = team_name
        self.world_x   = world_x
        self.world_y   = world_y

        self.level     = 1
        self.inventory: dict[str, int] = {"food": 10}
        self.state     = State.GATHER_FOOD
        self.uid       = _uid()

        # latest look result, updated evry loop
        self._tiles: list[list[str]] = []

        # these flags track wether we have pending async calls in flight
        self._inventory_pending = False
        self._look_pending      = False
        self._action_pending    = False  # waiting for a move/turn/take response

        # coordination state
        self._leader_uid: str | None = None  # uid of whoever called NEED_INC
        self._leader_k:   int | None = None  # most recent direction to leader
        self._ready_count: int = 0           # followers that said IM_READY to us
        self._seek_ticks:  int = 0           # anti stall counter

        # hook up the unsolicited event handlers
        self.conn.on_broadcast(self._on_broadcast)
        self.conn.on_eject(self._on_eject)
        self.conn.on_level_up(self._on_level_up)

        # get initial state before the loop starts
        self._request_inventory()
        self._request_look()

    def run(self):
        """main loop, runs forever until the player dies or the game ends"""
        while True:
            self.conn.pump()       # read socket, dispatch callbacks
            self._update_state()   # check if we need to transition
            self._act()            # do one action based on current state
            time.sleep(0.01)       # small sleep to avoid busy spinning

    # state transition logic

    def _update_state(self):
        food = self.inventory.get("food", 0)

        # food check always wins regardless of what state we are in
        # (except during incantation where we're frozen and cant do anything anyway)
        if food < FOOD_CRITICAL and self.state not in (State.SURVIVE, State.INCANTATING):
            self._transition(State.SURVIVE)
            return

        if self.state == State.SURVIVE and food >= FOOD_CRITICAL + 5:
            self._transition(State.GATHER_FOOD)

        elif self.state == State.GATHER_FOOD and food >= FOOD_SAFE:
            self._transition(State.GATHER_STONES)

        elif self.state == State.GATHER_STONES:
            if has_all_stones(self.inventory, self.level):
                self._transition(State.SEEK_TEAM)
            elif food < FOOD_SAFE:
                # food droped while gathering, go refill
                self._transition(State.GATHER_FOOD)

        elif self.state == State.SEEK_TEAM:
            self._seek_ticks += 1
            if self._seek_ticks > SEEK_TIMEOUT:
                # leader probably died, give up and start over
                self._leader_uid  = None
                self._leader_k    = None
                self._seek_ticks  = 0
                self._transition(State.GATHER_STONES)

    def _transition(self, new_state: State):
        print(f"[{self.uid}] {self.state} -> {new_state}")
        self.state           = new_state
        self._action_pending = False

    # per state action handlers

    def _act(self):
        if self._action_pending:
            return
        # always keep inventory and vision fresh
        if not self._inventory_pending:
            self._request_inventory()
        if not self._look_pending:
            self._request_look()

        if self.state == State.SURVIVE:
            self._act_survive()
        elif self.state == State.GATHER_FOOD:
            self._act_gather_food()
        elif self.state == State.GATHER_STONES:
            self._act_gather_stones()
        elif self.state == State.SEEK_TEAM:
            self._act_seek_team()
        elif self.state == State.WAIT_TEAM:
            self._act_wait_team()
        elif self.state == State.FORKING:
            pass  # handled by the fork callback
        elif self.state == State.INCANTATING:
            pass  # frozen, nothing we can do

    def _act_survive(self):
        self._navigate_to_resource("food")

    def _act_gather_food(self):
        self._navigate_to_resource("food")

    def _act_gather_stones(self):
        stone = next_stone_to_collect(self.inventory, self.level)
        if stone is None:
            return
        self._navigate_to_resource(stone)

    def _act_seek_team(self):
        if self._leader_uid is None:
            # nobody called for an incantation yet, so we become the leader
            self._become_leader()
            return

        if self._leader_k is not None:
            if self._leader_k == 0:
                # we are already on the leaders tile
                self._leader_k = None
                cmd.broadcast(self.conn, bcast.encode(bcast.IM_READY, self._leader_uid),
                               lambda ok: None)
                self._transition(State.WAIT_TEAM)
            else:
                # take one step toward the leader based on bcast dir
                moves = bcast.moves_toward(self._leader_k)
                self._leader_k = None
                self._execute_moves(moves)

    def _act_wait_team(self):
        # drop anything we dont need so the tile has the right stones
        self._drop_excess()

        if self._leader_uid == self.uid:
            # we're the leader, check if enought players showed up
            needed  = players_needed(self.level)
            on_tile = count_players_on_tile(self._tiles, 0)
            if on_tile >= needed:
                self._fire_incantation()
        # followers just sit here and wait for the START broadcast

    # leader side of the incantation coordination

    def _become_leader(self):
        """takes on the leader role and starts broadcasting to gather teammates"""
        self._leader_uid  = self.uid
        self._ready_count = 0
        text = bcast.encode(bcast.NEED_INC, f"{self.level}:{self.uid}")
        cmd.broadcast(self.conn, text, lambda ok: None)

    def _fire_incantation(self):
        """all players are here, send the START signal and kick off the ritual"""
        text = bcast.encode(bcast.START, self.uid)
        cmd.broadcast(self.conn, text, lambda ok: None)
        self._transition(State.INCANTATING)
        self._action_pending = True
        cmd.incantation(self.conn, self._on_incantation_result)

    def _on_incantation_result(self, new_level: int | None):
        self._action_pending = False
        self._leader_uid     = None
        if new_level is not None:
            self.level = new_level
            print(f"[{self.uid}] leveled up to {self.level}!")
            cmd.broadcast(self.conn, bcast.encode(bcast.LVL_UP, str(self.level)),
                          lambda ok: None)
        # whether it worked or not, go back to gathering food
        self._transition(State.GATHER_FOOD)

    # navigation helpers below 

    def _navigate_to_resource(self, resource: str):
        """
        tries to move toward the given resource using look data,
        if the resource is on our current tile we just take it,
        if its visible in a nearbly tile we move one step toward it,
        if its not visible at all we just wander and hope to find it
        """
        tiles = self._tiles
        if not tiles:
            self._wander()
            return

        # take from current tile immediately if its there
        if resource in tiles[0]:
            self._action_pending = True
            cmd.take(self.conn, resource, self._on_take_done)
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
            cmd.turn_right(self.conn, lambda ok: self._clear_action())
        else:
            cmd.forward(self.conn, lambda ok: self._clear_action())

    def _execute_moves(self, moves: list[str]):
        """executes the first move from a list, one step at a time"""
        if not moves:
            return
        self._action_pending = True
        move = moves[0]
        if move == "Forward":
            cmd.forward(self.conn, lambda ok: self._clear_action())
        elif move == "Right":
            cmd.turn_right(self.conn, lambda ok: self._clear_action())
        elif move == "Left":
            cmd.turn_left(self.conn, lambda ok: self._clear_action())

    def _drop_excess(self):
        """
        drops any stones we have more of than needed for the incantation,
        we do this before the ritual so the tile ends up with the right amounts,
        food is never dropped
        """
        from resources import ELEVATION
        if self.level not in ELEVATION:
            return
        needed = ELEVATION[self.level]
        for stone in STONES:
            have = self.inventory.get(stone, 0)
            need = needed.get(stone, 0)
            for _ in range(have - need):
                cmd.set_down(self.conn, stone, lambda ok: None)

    # forking

    def _try_fork(self):
        """
        spawns a new player if we have enough food and the team needs more slots,
        fork takes a long time (42/f) so we only do it when we can afford it
        """
        if self.inventory.get("food", 0) < FOOD_FORK_MIN:
            return
        prev_state = self.state
        self._transition(State.FORKING)
        self._action_pending = True

        def on_fork(ok: bool):
            self._action_pending = False
            cmd.broadcast(self.conn, bcast.encode(bcast.FORKING, self.team_name),
                          lambda ok2: None)
            self._transition(prev_state)

        cmd.fork(self.conn, on_fork)

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

    # event callbacks

    def _on_take_done(self, ok: bool):
        self._action_pending = False

    def _clear_action(self, *_):
        self._action_pending = False

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
                               lambda ok: None)
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
