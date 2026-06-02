##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## elevation table and resource deficit helpers
##

# all 6 stone types in the game (not counting food)
STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

# elevation requirements taken straight from the subject pdf
# key is current level, value is what you need to go to next level
ELEVATION: dict[int, dict] = {
    1: {"players": 1, "linemate": 1, "deraumere": 0, "sibur": 0, "mendiane": 0, "phiras": 0, "thystame": 0},
    2: {"players": 2, "linemate": 1, "deraumere": 1, "sibur": 1, "mendiane": 0, "phiras": 0, "thystame": 0},
    3: {"players": 2, "linemate": 2, "deraumere": 0, "sibur": 1, "mendiane": 0, "phiras": 2, "thystame": 0},
    4: {"players": 4, "linemate": 1, "deraumere": 1, "sibur": 2, "mendiane": 0, "phiras": 1, "thystame": 0},
    5: {"players": 4, "linemate": 1, "deraumere": 2, "sibur": 1, "mendiane": 3, "phiras": 0, "thystame": 0},
    6: {"players": 6, "linemate": 1, "deraumere": 2, "sibur": 3, "mendiane": 0, "phiras": 1, "thystame": 0},
    7: {"players": 6, "linemate": 2, "deraumere": 2, "sibur": 2, "mendiane": 2, "phiras": 2, "thystame": 1},
}

# rarest first, so we collect the hard to find ones with priority
# thystame only appears at level 7 and has the lowest density on the map
RARITY_ORDER = ["thystame", "phiras", "mendiane", "sibur", "deraumere", "linemate"]


def get_deficit(inventory: dict[str, int], level: int) -> dict[str, int]:
    """
    compares current inventory against what we need for the next incantation,
    returns a dict of stone -> how many more we still need,
    only includes stones where we are actually short, not the ones we have enuf of
    """
    if level not in ELEVATION:
        return {}
    needed = ELEVATION[level]
    deficit = {}
    for stone in STONES:
        have = inventory.get(stone, 0)
        need = needed.get(stone, 0)
        if have < need:
            deficit[stone] = need - have
    return deficit


