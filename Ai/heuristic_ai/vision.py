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
    given a tile index from look and the current level, returns the moves
    needed to get one  step closer to that tile (Forward / Right / Left)

    the look grid layout at level L:
      row 0 (current tile): index 0
      row 1 (1 step ahead): indices 1 .. 2L+1      (width = 2L+1 tiles)
      row 2 (2 steps ahead): indices 2L+2 .. 4L+2
      row r: indices (r-1)*(2L+1)+1 .. r*(2L+1)

    within each row tiles go left to right, center column is index L,
    we only return one step at a time, caller calls this again next tick
    """
    if tile_index == 0:
        return []

    width = 2 * level + 1

    # figure out wich row this tile is in and where in that row it is
    # tiles_before_row(r) = 1 + (r-1)*width  for r >= 1
    row = (tile_index - 1) // width + 1
    col_in_row = (tile_index - 1) % width  # 0 = leftmost col
    center = level                          # center column index

    commands = []

    # turn toward the right column first?????????????????????
    offset = col_in_row - center  # negative = left of center, positive = right
    if offset < 0:
        commands.append("Left")
    elif offset > 0:
        commands.append("Right")

    # then always step forward
    # we do one step at a time and re calc   after each move
    commands.append("Forward")

    return commands
