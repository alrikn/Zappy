##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## ai mixin: drop the required stones on the tile, then fire the incantation
##

from constants import LVLS


class RitualMixin:
    """dropping stones for the ritual + firing it once everything is present"""

    def drop_object_incantation(self):
        """drop my contribution of the required stones onto the current tile"""
        if self.commands_list:
            return
        data = self.look.split(",")[0]
        while True:
            if len(data) == 0 or data[0].isalpha():
                break
            data = data[1:]
        data = data.split(" ")
        required = LVLS[self.level].copy()
        for k in required:
            for j in data:
                if j == k:
                    required[k] -= 1
        for k in required:
            if required[k] < 1:
                continue
            if k in self.inventory and self.inventory[k] != 0:
                self.commands_list = ["Set " + k + "\n"]
                self.commands_list.append("Look\n")
                self.inventory[k] -= 1
                return
        self.step = 7
        self.data_to_write = ""
        return

    def start_incantation(self):
        """all stones present on the tile -> fire the ritual"""
        data = self.look.split(",")[0]
        while True:
            if len(data) == 0 or data[0].isalpha():
                break
            data = data[1:]
        data = data.split(" ")
        required = LVLS[self.level].copy()
        for k in required:
            for j in data:
                if j == k:
                    required[k] -= 1
        for k in required:
            if required[k] > 0:
                return
        self.data_to_write = "Incantation\n"
        self.commands_list = ["Incantation\n"]
        self.step += 1
