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

    def pump(self):
        """
        reads all avialble data from the socket and dispatches responses,
        uses select with timeout=0 so it doesnt block
        call this every itaration of the main ai loop
        """
        # select with 0 timeout = just check if data is available, dont block
        readable, _, _ = select.select([self._sock], [], [], 0)
        if not readable:
            return

        try:
            chunk = self._sock.recv(4096).decode()
        except BlockingIOError:
            return

        if not chunk:
            # server closed the connection
            sys.exit(0)

        self._recv_buf += chunk
        self._dispatch_lines()

    def _dispatch_lines(self):
        # split buffer by newlines and ahdnle each complete line
        while "\n" in self._recv_buf:
            line, self._recv_buf = self._recv_buf.split("\n", 1)
            line = line.strip()
            if not line:
                continue
            self._handle_line(line)

    def _handle_line(self, line: str):
        # unsolicited msgs have to be caught before we try to match a pending cmd
        # otherwise wed feed "dead" to whatever callback is waiting, which is wrong
        if line == "dead":
            if self._on_dead:
                self._on_dead()
            sys.exit(0)

        if line.startswith("message "):
            self._handle_broadcast(line)
            return

        if line.startswith("eject: "):
            try:
                k = int(line.split(": ", 1)[1])
            except ValueError:
                return
            if self._on_eject:
                self._on_eject(k)
            return

        # "Current level: k" is the 2nd part of a succesful incantation response
        # it arrives after the freeze period ends, so we treat it as unsolictied
        if line.startswith("Current level:"):
            try:
                level = int(line.split(":")[-1].strip())
            except ValueError:
                level = 0
            if self._on_level_up:
                self._on_level_up(level)
            return

        # if we get here its a normal response to the oldest pending command
        if self._pending:
            _, callback = self._pending.popleft()
            self._in_flight -= 1
            callback(line)

