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

