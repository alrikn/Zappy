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


# direction to ONE homing primitive
# when we get a broadcast, k tells us which neighbour tile the sound came thru so we
# can walk toward the sender without knowing their coords
#
# the tiles are numbered, from pdf: "1 in front of the player then the tiles that trigonometrically encircle the player"
# so 1 = front then counter clockwise (ccw):
#like thiss
#   2  1  8
#   3  @  7
#   4  5  6
#
# controller: one gentle turn toward the leader then always step forward, and the
# follower re aims on the very next ping, this is the ref epitech ais follow_beacon function from thi repo
# inspired from this: https://gitlab.com/epitech-it-2027/zappy/-/blob/main/AI/zappy_ai/trentorian/ritual.py
# recipe took it as is (works against the same server family):
#
#   k=0          = arrived (same tile)
#   k=1          = straight ahead       = forward
#   k in 2,3,4   = leader on the left   = left then forward
#   k=5          = directly behind      = left left forward (about face)
#   k in 6,7,8   = leader on the right  = right then forward
#
# why "turn a bit AND always step" vs the turn only version i tried before: a turn
# only controller does pure rotations on the off axis sectors and w/ the unavoidable
# one ping feedback lag those rotations oscillate in place near the target (follower
# danced around the tile, k flicking 3<>7, never hitting 0), always stepping forward
# guarantees progress each ping while the single gentle turn re centres us, next ping
# fixes any leftover sideways error
def moves_toward(k: int) -> list[str]:
    """one homing move toward a sound coming from direction k (0 = same tile)"""
    if k == 0:
        return []
    if k == 1:
        return ["Forward"]
    if k in (2, 3, 4):
        return ["Left", "Forward"]
    if k == 5:
        return ["Left", "Left", "Forward"]
    return ["Right", "Forward"]   # k in (6, 7, 8)
