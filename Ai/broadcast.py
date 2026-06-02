##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## broadcast message encode/decode + direction navigation
##

PREFIX = "ZAPPY"

# msg types
NEED_INC  = "NEED_INC"   # leader calling teammates: payload = "level:uid"
IM_COMING = "IM_COMING"  # follower acknowledging:   payload = "uid"
IM_READY  = "IM_READY"   # follower on leader tile:  payload = "uid"
START     = "START"      # leader firing incantation: payload = "uid"
LVL_UP    = "LVL"        # announcing level change:  payload = "level"
FORKING   = "FORK"       # announcing fork:          payload = "team"


def encode(msg_type: str, payload: str) -> str:
    """builds a bcast msg string that only our team will understand"""
    return f"{PREFIX}:{msg_type}:{payload}"


def decode(text: str) -> tuple[str, str] | None:
    """
    parses a received broadcast txt
    returns (type, payload) if it looks like one of our messages
    or None if it came from anther team or is malformed
    """
    if not text.startswith(PREFIX + ":"):
        return None
    parts = text.split(":", 2)
    if len(parts) != 3:
        return None
    return parts[1], parts[2]


# direction -> movement commands
# when we receive a broadcast, K tells us which direction the sound came from
# we use this to navigate toward the sender without knowing their coordinates
# https://www.geeksforgeeks.org/what-is-a-state-machine/
#
# the numbering around the player (@ = us):
#
#   8  1  2
#   7  @  3
#   6  5  4
#
# K=0 -> same tile as us
# K=1 -> directly ahead
# K=2 -> ahead and to the right
# K=3 -> to the right
# K=4 -> behind and to the right
# K=5 -> directly behind
# K=6 -> behind and to the left
# K=7 -> to the left
# K=8 -> ahead and to the left

# NOT SURE ABT THIS CHECK DONT PUSH // TO REVIEW
DIRECTION_MOVES: dict[int, list[str]] = {
    0: [],
    1: ["Forward"],
    2: ["Forward"],               # ahead-right, close the ahead gap first
    3: ["Right", "Forward"],
    4: ["Right", "Right", "Forward"],
    5: ["Right", "Right", "Forward"],
    6: ["Left", "Left", "Forward"],
    7: ["Left", "Forward"],
    8: ["Forward"],               # ahead-left, close the ahead gap first
}


def moves_toward(k: int) -> list[str]:
    """
    returns one steps worth of move cmds to get closer to whoever broadcast from dir k,
    we call this every time we get a new bcast from the leader to recalibrate
    """
    return DIRECTION_MOVES.get(k, ["Forward"])
