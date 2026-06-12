##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## shared constants for the ai: stone recipes, team size, food reserve
##

RESOURCES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

# stone requirements to elevate from each level (player count requirement is handled
# implicitly: we always assemble all 6, which covers every levels min)
LVLS = {
    1: {"linemate": 1},
    2: {"linemate": 1, "deraumere": 1, "sibur": 1},
    3: {"linemate": 2, "sibur": 1, "phiras": 2},
    4: {"linemate": 1, "deraumere": 1, "sibur": 2, "phiras": 1},
    5: {"linemate": 1, "deraumere": 2, "sibur": 1, "mendiane": 3},
    6: {"linemate": 1, "deraumere": 2, "sibur": 3, "phiras": 1},
    7: {"linemate": 2, "deraumere": 2, "sibur": 2, "mendiane": 2, "phiras": 2, "thystame": 1},
}

# how big the team should grow (matches the zappy goal of 6 players elevating together)
TEAM_SIZE = 6
# food reserve to keep before going back to gathering stones, the high floor is what
# stops players starving mid ritual (the ritual freezes the player for 300/f), a deep
# buffer is what carries the team through the long high level gather cycles up to lvl 6
FOOD_FLOOR = 45
