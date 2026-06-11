##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## tunable parameters for the heuristic ai, loadable from a json file
##

import json
import os

# evry magic number that affects behaviour lives here so the auto tuner
# (tools/tuner.py) can search over them without touching the code
#
# the ai loads overrides from the json file pointed to by the ZAPPY_AI_CONFIG
# env var at startup, anything not in that file falls back to these defaults
# values are deliberately flat (no nesting) fait expres   so mutation/serialization stays straightforward
# DYNAMIC STUFF HANDLING DEPENIN ON THE MAP SIZE and nmb o players

DEFAULTS: dict[str, float] = {
    # food economy
    # food_safe / food_low are derived per map: food per player * mult, clamped
    # food per player = (0.5 * width * height) / max(6, slots+1)
    "food_critical":   5,      # emergency floor, fixed whatever the map
    "food_safe_mult":  1.2,    # stop hoarding food at food_pp * this
    "food_safe_min":   15,
    "food_safe_max":   50,     # bumped from 40, the top student ais hoard like 45 to 52 on
                               # roomy maps, the reserve covers the meet + 300/f ritual freeze
    # bigger maps burn more food just travelling between sparse resources so the safe
    # reserve grows w/ map span (w+h), its the ref ais idea (their mini_food has a
    # +(w+h)*k term), barely matters on small maps, big deal on large ones
    "food_travel_factor": 0.3,
    "food_low_mult":   0.9,    # min food to proactively go (re)gather food at food_pp * this
    "food_low_min":    12,
    "food_low_max":    25,
    # joining a leader thats ALREADY broadcasting is cheap (like 1 to 4 food measured,
    # the partner is a known nearby target, no long search), gating it behind the
    # conservative food_low made evry potential partner reject the call on food scarce
    # maps so rituals never fired, this is a survival based floor instead:
    # food_join = food_critical + join_food_buffer, way below food_low
    "join_food_buffer": 3,

    # coordination timing (in main loop ticks, ~0.01s each)
    "seek_timeout":         500,   # give up navigating to a leader after this
    "wait_timeout_mult":    2.0,   # leader waits seek_timeout * this for followers
    "listen_before_lead":   30,    # listen for existing need_inc before self leading
    "extra_player_wait":    300,   # after min players arrive, wait this for more (unused now)
    "extra_wait_min_level": 4,     # only bother waiting for extras at level >= this (unused now)
    "bcast_interval":       2,     # leader re broadcasts evry this many ticks, followers take
                                   # ONE homing step per ping so a low interval keeps them
                                   # re aiming and converging fast on big maps

    # forking
    "fork_min_food":  30,    # need at least this much food before forking
    "fork_interval":  150,   # ticks between fork attempts

    # behaviour switches (0/1)
    "leader_eat_in_wait": 1,   # leader eats food landing on its tile while waiting
}


class Config:
    """holds the resolved parameter set for one AI instance"""

    def __init__(self, values: dict[str, float]):
        self._v = values

    def get(self, key: str) -> float:
        return self._v.get(key, DEFAULTS[key])

    def i(self, key: str) -> int:
        return int(self.get(key))

    def f(self, key: str) -> float:
        return float(self.get(key))

    def b(self, key: str) -> bool:
        return bool(int(self.get(key)))

    def as_dict(self) -> dict[str, float]:
        return dict(self._v)


def load() -> Config:
    """
    builds a config from defaults merged with the json file at zappy_ai_config
    (if set and readable), never raises, a missing or bad file just means defaults
    """
    values = dict(DEFAULTS)
    path = os.environ.get("ZAPPY_AI_CONFIG")
    if path and os.path.isfile(path):
        try:
            with open(path) as fh:
                override = json.load(fh)
            if isinstance(override, dict):
                for k, v in override.items():
                    if k in DEFAULTS and isinstance(v, (int, float)):
                        values[k] = v
        except (OSError, ValueError):
            pass  # keep defaults on any read/parse error
    return Config(values)
