##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## buffered tcp connection with a command pipeline
##

import socket
import sys
import select
from collections import deque
from typing import Callable

# the server lets us send up to 10 cmds without waiting for responses
# pipeline budget, we are using  adequeue to pass it to the server
# https://www.geeksforgeeks.org/deque-in-python/


class Connection:
    MAX_PIPELINE = 10

    def __init__(self, host: str, port: int):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect((host, port))
        self._sock.setblocking(False)

        self._recv_buf: str = ""
        # deque is perfect here bc we need fifo order and fast popleft
        self._pending: deque[tuple[str, Callable]] = deque()
        self._in_flight: int = 0

        # callbacks for messages the server sends without us asking
        self._on_dead: Callable | None = None
        self._on_broadcast: Callable | None = None  # called with (k: int, text: str)
        self._on_eject: Callable | None = None       # called with (k: int)
        self._on_level_up: Callable | None = None    # called with (new_level: int)

    # handshake is blocking, we call it once before the main loop starts

    def handshake(self, team_name: str) -> tuple[int, int, int]:
        """
        does the initial handshake with the server
        the protocol is:
          server -> WELCOME
          client -> team name
          server -> number of slots left
          server -> world width and height
        returns (world_x, world_y, slots)
        """
        welcome = self._recv_line_blocking()
        if welcome != "WELCOME":
            print(f"expected WELCOME, got: {welcome!r}", file=sys.stderr)
            sys.exit(1)

        self._send_raw(team_name + "\n")

        slots_line = self._recv_line_blocking()
        dims_line  = self._recv_line_blocking()

        try:
            slots = int(slots_line.strip())
            x, y = map(int, dims_line.strip().split())
        except ValueError:
            print(f"bad handshake response: {slots_line!r} / {dims_line!r}", file=sys.stderr)
            sys.exit(1)

        return x, y, slots

    def push(self, command: str, callback: Callable[[str], None]) -> bool:
        """
        queues a command to be sent to the server,
        returns false if the pipeline is full, caller should retry later,
        commands get repsonse in the same order they were sent (fifo)
        so we just match responses to callbacks in order
        """
        if self._in_flight >= self.MAX_PIPELINE:
            return False
        self._send_raw(command + "\n")
        self._pending.append((command, callback))
        self._in_flight += 1
        return True

