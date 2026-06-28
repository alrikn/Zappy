##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## ai mixin: look parsing + navigation toward a visible object
##

import math
import random


class VisionMixin:
    """look parsing / navigation toward a visible object"""

    def size_look(self, array: list) -> int:
        """return the number of non-empty depth slots in a single column of the map"""
        nb = 0
        for elem in array:
            if elem == []:
                return nb
            nb += 1
        return nb

    def find_object(self, mp: list, obj: str):
        """scan the map outward from the player's position and return [row, depth] of obj.
        depth >= 1 tiles are checked before depth 0 so the player moves toward food on
        other tiles rather than staying put on a crowded post-incantation tile."""
        v = 8
        max_depth = self.size_look(mp[v])
        for h in range(1, max_depth):
            for tv in range(v - h, v + h + 1):
                if 0 <= tv < len(mp) and mp[tv][h] != [] and obj in mp[tv][h][0]:
                    return [tv, h]
        if mp[v][0] != [] and obj in mp[v][0][0]:
            return [v, 0]
        return None

    def get_nb_of_lines(self, data: list) -> int:
        """derive the vision radius from the total number of tiles in a look response"""
        return int(math.sqrt(len(data)))

    def fill_map(self, mp: list, data: list) -> list:
        """place look response tiles into the 17x9 grid in diamond order"""
        nb = 1
        v = 8
        h = 0
        i = 0
        line = self.get_nb_of_lines(data)
        for _ in range(line):
            tv = v - h
            for _ in range(nb):
                mp[tv][h].append(data[i])
                tv += 1
                i += 1
            nb = nb + 2
            h += 1
        return mp

    def generate_empty_map(self) -> list:
        """allocate a blank 17-wide x 9-deep grid to hold the look response"""
        return [[[] for _ in range(9)] for _ in range(17)]

    def parse_look(self, data: str, obj: str) -> list:
        """parse a look response and return a command list that navigates to obj"""
        import re
        if not data or data.startswith("[ food"):
            return []
        tiles = [' '.join(re.split(r'\W+', cell)[1:]) for cell in data.split(",")]
        mp = self.generate_empty_map()
        mp = self.fill_map(mp, tiles)
        coord = self.find_object(mp, obj)
        if coord is None:
            return [random.choice(["Forward\n", "Right\n", "Left\n"]) for _ in range(3)]
        row, depth = coord
        if row == 8 and depth == 0:
            return ["Take " + obj + "\n"]
        cmds = []
        if row < 8:
            cmds.extend(["Forward\n"] * depth)
            cmds.append("Left\n")
            cmds.extend(["Forward\n"] * (8 - row))
        elif row > 8:
            cmds.extend(["Forward\n"] * depth)
            cmds.append("Right\n")
            cmds.extend(["Forward\n"] * (row - 8))
        else:  # row == 8, depth > 0: object is straight ahead
            cmds.extend(["Forward\n"] * depth)
        cmds.append("Take " + obj + "\n")
        cmds.append("Inventory\n")
        return cmds
