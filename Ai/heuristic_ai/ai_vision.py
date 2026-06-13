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
        nb = 0
        for elem in array:
            if elem == []:
                return nb
            nb += 1
        return nb

    def find_object(self, mp: list, obj: str):
        v = 8
        h = 0
        while h < self.size_look(mp[v]):
            if mp[v][h] != [] and obj in mp[v][h][0]:
                return [v, h]
            else:
                h += 1
            tv = v
            while tv >= v - h:
                if mp[v][h] != [] and obj in mp[tv][h][0]:
                    return [tv, h]
                tv -= 1
            tv = v
            while tv <= v + h:
                if mp[v][h] != [] and obj in mp[tv][h][0]:
                    return [tv, h]
                tv += 1
            h += 1
        return None

    def get_nb_of_lines(self, data: list) -> int:
        return int(math.sqrt(len(data)))

    def fill_map(self, mp: list, data: list) -> list:
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
        return [[[] for _ in range(9)] for _ in range(17)]

    def parse_look(self, data: str, obj: str) -> list:
        import re
        data1 = data.split(",")
        liste = []
        for i in range(len(data1)):
            liste.append(' '.join(re.split(r'\W+', data1[i])[1:]))
        data = liste
        mp = self.generate_empty_map()
        mp = self.fill_map(mp, data)
        coord = self.find_object(mp, obj)
        res = []
        if coord is None:
            res.append(random.choice(["Forward\n", "Right\n", "Left\n"]))
            res.append(random.choice(["Forward\n", "Right\n", "Left\n"]))
            res.append(random.choice(["Forward\n", "Right\n", "Left\n"]))
            return res
        elif coord[0] == 8 and coord[1] == 0:
            return ["Take " + obj + "\n"]
        else:
            for _ in range(int(coord[0]) - 8):
                res.append("Forward\n")
            if coord[0] == 8:
                for _ in range(int(coord[1])):
                    res.append("Forward\n")
                res.append("Take " + obj + "\n")
                res.append("Inventory\n")
            if coord[1] == 0:
                res.append("Take " + obj + "\n")
            if int(coord[0]) < 8:
                res.append("Left\n")
                for _ in range(8 - int(coord[0])):
                    res.append("Forward\n")
                res.append("Take " + obj + "\n")
                res.append("Inventory\n")
            if int(coord[0]) > 8:
                res.append("Right\n")
                for _ in range(int(coord[0]) - 8):
                    res.append("Forward\n")
                res.append("Take " + obj + "\n")
                res.append("Inventory\n")
        return res
