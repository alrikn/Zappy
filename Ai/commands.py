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

