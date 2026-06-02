##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## player state enum for the state machine
##

# state machine pattern, basically each player is alwyas in one of these states
# and transitions between them based on whats happening
from enum import Enum, auto


class State(Enum):
    SURVIVE       = auto()  # food < 5, drop eveyting and find food now
    GATHER_FOOD   = auto()  # food < 20, focus on food before doing anything else
    GATHER_STONES = auto()  # food is fine, go collect the stones we need for next level
    SEEK_TEAM     = auto()  # have all stones, now navigate toward the incantation leader
    WAIT_TEAM     = auto()  # on the leaders tile, waitng for enough players to show up
    INCANTATING   = auto()  # froze during the ritual, cant do anythg
    FORKING       = auto()  # currently doing a fork to spawn a new player

    def __str__(self):
        return self.name
