##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## look response parser and tile navigation helpers
##


def parse_look(raw: str) -> list[list[str]]:
    """
    parses the raw look rsps from the server into a list of tiles,
    each tile is a list of the objects on it (strings),
    tile 0 is always the plyers current position

    example server response: [player, food linemate, , thystame, ]
    -> [[player], [food, linemate], [], [thystame], []]
    """
    raw = raw.strip()
    if raw.startswith("["):
        raw = raw[1:]
    if raw.endswith("]"):
        raw = raw[:-1]
    tiles = raw.split(",")
    return [t.split() for t in tiles]


def find_resource(tiles: list[list[str]], resource: str) -> int | None:
    """
    searches thru the look result for a given resource,
    returns the tile index of the closest one, or None if not visible,
    tile 0 is included here bc we mitgh want to take from current tile,
    tiles are checed nearest first so we natuarl get the closest one
    """
    for i, tile in enumerate(tiles):
        if resource in tile:
            return i
    return None


def count_players_on_tile(tiles: list[list[str]], tile_index: int = 0) -> int:
    """counts how many players are on a given tile in the look result"""
    if tile_index >= len(tiles):
        return 0
    return tiles[tile_index].count("player")


def tile_to_moves(tile_index: int, level: int) -> list[str]:
    """
    given a tile index from look and the cueent level, returns the moves
    needed to get one step closer to that tile (Forward / Right / Left)

    the look grid layout: row r starts at index r^2, has 2r+1 tiles,
    center of row r is at column index r (0 indexed from left)

      row 0 (current): index 0
      row 1:           indices 1..3   (3 tiles, center at col 1)
      row 2:           indices 4..8   (5 tiles, center at col 2)
      row r:           indices r^2 .. r^2+2r

    we only return one step at a time, caller calls this again next tick
    """
    if tile_index == 0:
        return []

    # row r starts at r^2, so row = isqrt(tile_index)
    row = int(tile_index ** 0.5)
    col_in_row = tile_index - row * row  # 0 indexed from left
    center = row                          # center column index

    offset = col_in_row - center  # negative = left, positive = right

    # if the tile is mostly ahead (small lateral offset) close the distance first
    if row > 1 and abs(offset) < row:
        return ["Forward"] * min(row, 3)

    # otherwise fix the lateral offset first, then step forward
    commands = []
    if offset < 0:
        commands.append("Left")
    elif offset > 0:
        commands.append("Right")
    commands.append("Forward")
    return commands
