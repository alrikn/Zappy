##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## ai mixin: drop the required stones on the tile, then fire the incantation
##

from constants import LVLS


class RitualMixin:
    """dropping stones for the ritual + firing it once everything is present"""

    def _tile_deficit(self) -> dict:
        """missing stone counts for the current tile vs the level requirement"""
        tile = self.look.split(",")[0]
        while tile and not tile[0].isalpha():
            tile = tile[1:]
        items = tile.split()
        deficit = LVLS[self.level].copy()
        for stone in deficit:
            deficit[stone] -= items.count(stone)
        return deficit

    def drop_object_incantation(self):
        """drop my contribution of the required stones onto the current tile"""
        if self.commands_list:
            return
        for stone, needed in self._tile_deficit().items():
            if needed < 1:
                continue
            if self.inventory.get(stone, 0) > 0:
                self.commands_list = ["Set " + stone + "\n", "Look\n"]
                self.inventory[stone] -= 1
                return
        self.step = 7
        self.data_to_write = ""

    def start_incantation(self):
        """all stones present on the tile -> fire the ritual"""
        if any(v > 0 for v in self._tile_deficit().values()):
            return
        self.data_to_write = "Incantation\n"
        self.step += 1
