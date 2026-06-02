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
