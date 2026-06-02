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
