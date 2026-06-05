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


def broadcast(conn: Connection, text: str, cb: Callable[[bool], None]):
    conn.push(f"Broadcast {text}", lambda r: cb(r == "ok"))


def connect_nbr(conn: Connection, cb: Callable[[int], None]):
    """
    asks how many open slots the team still has,
    usefull to know if we should fork or not
    """
    def parse(raw: str):
        try:
            cb(int(raw.strip()))
        except ValueError:
            cb(0)
    conn.push("Connect_nbr", parse)


def fork(conn: Connection, cb: Callable[[bool], None]):
    # takes 42/f time units, pretty slow but necessary to grow the team
    conn.push("Fork", lambda r: cb(r == "ok"))


def eject(conn: Connection, cb: Callable[[bool], None]):
    conn.push("Eject", lambda r: cb(r == "ok"))


def take(conn: Connection, resource: str, cb: Callable[[bool], None]):
    conn.push(f"Take {resource}", lambda r: cb(r == "ok"))


def set_down(conn: Connection, resource: str, cb: Callable[[bool], None]):
    conn.push(f"Set {resource}", lambda r: cb(r == "ok"))


def incantation(conn: Connection, cb: Callable[[int | None], None]):
    """
    starts an incantation, its a bit tricky bc the server sends two seperate lines:
      1. "Elevation underway"  -> immediate response to the command
      2. "Current level: k"   -> arrives after the ritual finishes (300/f later)
    so we rgst  an on_level_up handler in the connexion to catch the second one,
    cb gets the new level on success, or none if it failed (ko)
    """
    # EDGE CASE TODO REVIEW PLESE we dont fully handle: the subject says prerequisites are checked at
    # the start and the end of the ritual, the ko for a start failure arrives instantly
    # and is caught below, but if the ritual fails the end check (ex: another player left
    # the tile mid ritual) the server could send a ko after "Elevation underway" instead
    # of "Current level: k", we only listen for "Current level:" as unsolicited so that
    # late ko would fall throguh to _handle_line and get matched to the wrong pending
    # cmd callback, desyning the whole pipeline, depends on the real server behavior
    # so leaving it for now, proper fix would track an "incantation in progress" flag in
    # the connection and treat a bare ko during that window as the incantation result
    def on_first_response(raw: str):
        if raw == "ko":
            conn.on_level_up(None)
            cb(None)
            return
        if raw.startswith("Elevation underway"):
            # nice, ritual started, now wait for lvl up event
            def on_level(level: int):
                conn.on_level_up(None)  # deregister so it doesnt fire again
                cb(level)
            conn.on_level_up(on_level)
        else:
            conn.on_level_up(None)
            cb(None)

    conn.push("Incantation", on_first_response)
