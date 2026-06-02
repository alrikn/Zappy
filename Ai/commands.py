##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## wrappers for all server commands, callback based no blocking
##

from connection import Connection
from typing import Callable

# all these just push a command and call the callback with a bool when the response comes back
# true = ok, false = ko or something unexpected


def forward(conn: Connection, cb: Callable[[bool], None]):
    conn.push("Forward", lambda r: cb(r == "ok"))


def turn_right(conn: Connection, cb: Callable[[bool], None]):
    conn.push("Right", lambda r: cb(r == "ok"))


def turn_left(conn: Connection, cb: Callable[[bool], None]):
    conn.push("Left", lambda r: cb(r == "ok"))


def look(conn: Connection, cb: Callable[[list[list[str]]], None]):
    """
    sends look and parses the resp into a list of tiles,
    each tile is a list of strings like ["food", "linemate"] or ["player"],
    tile 0 is alwys the players current tile,
    response format from server: [player, food linemate, , thystame, ...]
    """
    def parse(raw: str):
        if raw == "ko":
            cb([])
            return
        raw = raw.strip()[1:-1]  # strip the [ and ]
        tiles = raw.split(",")
        cb([t.split() for t in tiles])
    conn.push("Look", parse)


def inventory(conn: Connection, cb: Callable[[dict[str, int]], None]):
    """
    sends Inventory and parses the response into a dict,
    response looks like: [food 10, linemate 2, sibur 0, ...]
    """
    def parse(raw: str):
        if raw == "ko":
            cb({})
            return
        raw = raw.strip()[1:-1]  # strip [ ]
        result = {}
        for item in raw.split(","):
            parts = item.strip().split()
            if len(parts) == 2:
                try:
                    result[parts[0]] = int(parts[1])
                except ValueError:
                    pass
        cb(result)
    conn.push("Inventory", parse)


