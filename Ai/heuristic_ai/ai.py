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
        self.current_master = 0
        self._steps = {
            -2: self._step_connect,
            -1: self._step_fork_or_look,
             0: self._step_drain_commands,
             1: self._step_broadcast_or_escalate,
             2: self._step_look,
             3: self._step_decide_target,
             4: self._step_homing,
             5: self._step_pre_ritual_look,
             6: self._step_ritual_drop,
             7: self._step_ritual_wait,
             8: self._step_frozen,
             9: self._step_post_incantation_inventory,
            10: self._step_broadcast_inventory,
        }

    # ---- the main step machine ----

    def algorithm(self):
        handler = self._steps.get(self.step)
        if handler:
            handler()

    # ---- step handlers ----

    def _step_connect(self):
        self.data_to_write = "Connect_nbr\n"
        self.step += 1

    def _step_fork_or_look(self):
        if self.client_num < TEAM_SIZE and self.useless_slot == 0:
            self.data_to_write = "Fork\n"
            self.fork = 1
        else:
            self.data_to_write = "Look\n"
        self.step += 1

    def _step_drain_commands(self):
        if self.commands_list:
            self.data_to_write = self.commands_list.pop(0)
        else:
            self.data_to_write = "Inventory\n"
            self.step += 1

    def _step_broadcast_or_escalate(self):
        if not self.new_object:
            self.step += 1
            self.algorithm()
            return
        self.new_object = False
        if self.check_incantation():
            body = str(self.client_num) + ";incantation;" + str(self.level)
            self.data_to_write = self._bcast(body)
            self.master_incantation = 1
            self.step = 4
            self.incantation = 1
            print(f"[{self.client_num}] MASTER calling lvl{self.level} incantation")
            return
        body = ("inventory" + str(self.client_num) + ";" +
                str(self.level) + ";" + json.dumps(self.inventory))
        self.data_to_write = self._bcast(body)
        self.step += 1

    def _step_look(self):
        self.data_to_write = "Look\n"
        self.step += 1

    def _step_decide_target(self):
        if self.inventory.get("food", 0) < FOOD_FLOOR:
            self.commands_list = self.parse_look(self.look, "food")
        else:
            self.to_search = self.search_good_ressources()
            self.commands_list = self.parse_look(self.look, self.to_search)
        self.step = 0

    def _step_homing(self):
        if self.incantation == 0:
            self.data_to_write = self._bcast(str(self.client_num) + " on my way")
            self.incantation = 1
            return
        if self.master_incantation >= TEAM_SIZE:
            self.step += 1
            return
        if self.master_incantation >= 1:
            return  # master: wait for all followers to arrive
        if self.commands_list and not self.ready_for_incantation:
            self.data_to_write = self.commands_list.pop(0)
            self.clear_broadcast = 1
            if not self.commands_list:
                self.clear_read = 1
        elif self.ready_for_incantation and "Broadcast" in self.data_to_write:
            self.step += 1
        else:
            self.data_to_write = ""

    def _step_pre_ritual_look(self):
        self.clear_broadcast = 0
        self.data_to_write = "Look\n"
        self.step += 1

    def _step_ritual_drop(self):
        self.clear_broadcast = 0
        if self.master_incantation >= TEAM_SIZE:
            self.start_incantation()
        if self.step != 7:
            self.drop_object_incantation()
            self.data_to_write = self.commands_list.pop(0) if self.commands_list else "Inventory\n"

    def _step_ritual_wait(self):
        if self.master_incantation < TEAM_SIZE:
            self.data_to_write = "Connect_nbr\n"
            return
        if self.commands_list:
            self.data_to_write = self.commands_list.pop(0)
        else:
            self.commands_list = ["Look\n"]
            self.step = 6  # retry start_incantation after fresh look

    def _step_frozen(self):
        self.data_to_write = ""  # frozen during the elevation

    def _step_post_incantation_inventory(self):
        self.data_to_write = "Inventory\n"
        self.step += 1

    def _step_broadcast_inventory(self):
        body = ("inventory" + str(self.client_num) + ";" +
                str(self.level) + ";" + json.dumps(self.inventory))
        self.data_to_write = self._bcast(body)
        self.step = 0
