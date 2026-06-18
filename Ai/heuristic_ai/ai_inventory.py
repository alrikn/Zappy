##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## AI mixin: inventory parsing + POOLED team inventory + incantation readiness
##

import json
import random
from collections import Counter

from constants import LVLS, RESOURCES


class InventoryMixin:
    """inventory / incantation reasoning (uses the POOLED team inventory)"""

    def check_incantation(self) -> bool:
        """can this bot solo elevate? fires only when own inventory has all needed stones
        parse_inventory already ran at step 0 so self.inventory is current, no +1 needed"""
        required = LVLS[self.level].copy()
        for k in required:
            if required[k] > self.inventory.get(k, 0):
                return False
        return True

    def search_good_ressources(self) -> str:
        """pick a stone tihs bot still needs to solo declare; food when the bot has all"""
        required = LVLS[self.level].copy()
        liste = []
        for k in required:
            if required[k] > self.inventory.get(k, 0):
                liste.append(k)
        if not liste:
            return "food"
        return random.choice(liste)

    def parse_inventory(self, data: str):
        for char in "[]":
            data = data.replace(char, "")
        data = data.split(",")
        for i in range(len(data)):
            data[i] = data[i][1:]
        data[len(data) - 1] = data[len(data) - 1][:-1]
        for elem in data:
            if elem:
                self.inventory[elem.split()[0]] = int(elem.split()[1])

    def parse_shared_inventory(self, data: str):
        """a teammate shared its inventory: merge it and recompute the pooled total"""
        client, _, inventory = data.split(";")
        self.shared_inventory[self.client_num] = self.inventory
        self.shared_inventory[client] = json.loads(inventory)
        c = Counter()
        for d in self.shared_inventory:
            if d == "total":
                continue
            c.update(self.shared_inventory[d])
        self.shared_inventory["total"] = dict(c)

    def update_shared_inventory(self):
        self.shared_inventory[self.client_num] = self.inventory
        c = Counter()
        for d in self.shared_inventory:
            if d == "total":
                continue
            c.update(self.shared_inventory[d])
        self.shared_inventory["total"] = dict(c)
