##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## the per player brain: a step driven state machine, one server command per tick
##
## strategy in one line: bring the whole team (all 6 players) onto a single tile
## for every incantation,  the server elevates every player of the right level that
## stands on the tile, so one ritual levels the entire  team up at once and the team
## advances in lockstep 1 -> 2 -> ... -> 8 without ever diverging
##
## the behaviour (step machine + broadcast protocol) is the reference
## strategy; the helpers are split across ai_inventory / ai_vision / ai_comms /
## ai_ritual mixins purely for readability, the logic is unchanged
##

import json

from constants import FOOD_FLOOR, TEAM_SIZE
from ai_inventory import InventoryMixin
from ai_vision import VisionMixin
from ai_comms import CommsMixin
from ai_ritual import RitualMixin


class AI(InventoryMixin, VisionMixin, CommsMixin, RitualMixin):
    """encapsulates the per player intelligence: a step driven state machine that
    matches one server command per tick, exactly as the ported ai does"""

    def __init__(self, name: str, client_num: int):
        self.inventory = {"food": 0, "linemate": 0, "deraumere": 0, "sibur": 0,
                          "mendiane": 0, "phiras": 0, "thystame": 0}
        self.look = ""
        self.broadcast = []
        self.commands_list = []
        self.step = -2
        self.data_to_write = ""
        self.level = 1
        self.run = 1
        self.useless_slot = 0
        self.shared_inventory = {}
        self.client_num = client_num
        self.team = name
        self.new_object = False
        self.to_search = ""
        self.incantation = 0
        self.master_incantation = 0
        self.nb_player_incantation = 1
        self.direction = 9
        self.ready_for_incantation = 0
        self.clear_read = 0
        self.clear_broadcast = 0
        self.fork = 1

    # ---- the main step machine ----

    def algorithm(self):
        if self.step == -2:
            self.data_to_write = "Connect_nbr\n"
            self.step += 1
        elif self.step == -1:
            if self.client_num < TEAM_SIZE and self.useless_slot == 0:
                self.data_to_write = "Fork\n"
                self.fork = 1
            else:
                self.data_to_write = "Look\n"
            self.step += 1
        elif self.step == 0:
            if self.commands_list:
                self.data_to_write = self.commands_list[0]
                self.commands_list = self.commands_list[1:]
            else:
                self.data_to_write = "Inventory\n"
                self.step += 1
        elif self.step == 1:
            if self.new_object:
                if not self.check_incantation():
                    body = ("inventory" + str(self.client_num) + ";" +
                            str(self.level) + ";" + json.dumps(self.inventory))
                    self.data_to_write = self._bcast(body)
                else:
                    body = str(self.client_num) + ";incantation;" + str(self.level)
                    self.data_to_write = self._bcast(body)
                    self.master_incantation = 1
                    self.step = 4
                    self.incantation = 1
                    self.new_object = False
                    print(f"[{self.client_num}] MASTER calling lvl{self.level} incantation")
                    return
                self.new_object = False
            else:
                self.step += 1
                self.algorithm()
                return
            self.step += 1
        elif self.step == 2:
            self.data_to_write = "Look\n"
            self.step += 1
        elif self.step == 3:
            if "food" in self.inventory and self.inventory["food"] < FOOD_FLOOR:
                self.commands_list = self.parse_look(self.look, "food")
                self.step = 0
            else:
                self.to_search = self.search_good_ressources()
                self.commands_list = self.parse_look(self.look, self.to_search)
                self.step = 0
        elif self.step == 4:
            if self.incantation == 0:
                body = str(self.client_num) + " on my way"
                self.data_to_write = self._bcast(body)
                self.incantation = 1
                return
            if self.master_incantation >= TEAM_SIZE:
                self.step += 1
                return
            if self.master_incantation >= 1:
                pass
            elif self.commands_list and not self.ready_for_incantation:
                self.data_to_write = self.commands_list[0]
                self.commands_list = self.commands_list[1:]
                self.clear_broadcast = 1
                if not self.commands_list:
                    self.clear_read = 1
            elif self.ready_for_incantation and "Broadcast" in self.data_to_write:
                self.step += 1
            else:
                self.data_to_write = ""
        elif self.step == 5:
            self.clear_broadcast = 0
            self.data_to_write = "Look\n"
            self.step += 1
        elif self.step == 6:
            self.clear_broadcast = 0
            if self.master_incantation >= TEAM_SIZE:
                self.start_incantation()
            if self.step != 7:
                self.drop_object_incantation()
                if self.commands_list:
                    self.data_to_write = self.commands_list[0]
                    self.commands_list = self.commands_list[1:]
                else:
                    self.data_to_write = "Inventory\n"
        elif self.step == 7:
            if self.master_incantation < TEAM_SIZE:
                self.data_to_write = "Connect_nbr\n"
                return
            if self.commands_list:
                self.data_to_write = self.commands_list[0]
                self.commands_list = self.commands_list[1:]
            else:
                self.commands_list = ["Look\n"]
        elif self.step == 8:
            self.data_to_write = ""  # frozen during the elevation
        elif self.step == 9:
            self.data_to_write = "Inventory\n"
            self.step += 1
        elif self.step == 10:
            body = ("inventory" + str(self.client_num) + ";" +
                    str(self.level) + ";" + json.dumps(self.inventory))
            self.data_to_write = self._bcast(body)
            self.step = 0
