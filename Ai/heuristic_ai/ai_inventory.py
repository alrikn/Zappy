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
        """can the team (pooled) elevate the current level once we add the object we
        just picked up to the shared total?"""
        required = LVLS[self.level].copy()
        inventory = {}
        if "total" in self.shared_inventory:
            inventory = self.shared_inventory["total"].copy()
        if self.to_search in inventory:
            inventory[self.to_search] += 1
        else:
            inventory[self.to_search] = 1
        for k in required:
            if required[k] > inventory.get(k, 0):
                return False
        return True

    def search_good_ressources(self) -> str:
        """pick a stone the TEAM still needs (so gatherers spread out), food if none"""
        required = LVLS[self.level].copy()
        liste = []
        if "total" in self.shared_inventory:
            inventory = self.shared_inventory["total"].copy()
        else:
            inventory = {r: 0 for r in RESOURCES}
            inventory["food"] = 0
        for k in required:
            if k not in inventory or required[k] > inventory[k]:
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
