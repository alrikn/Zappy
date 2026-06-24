##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## lockstep socket client: drives one AI, one server command per response
##
## child processes are spawned by relaunching our main.py with -i <next id>, the
## client_id (our ordinal in the fork chain) comes from -i or the ZAPPY_AI_IDX env var
##

import os
import sys
import socket
import select
import subprocess

from constants import TEAM_SIZE
from ai import AI


class Client:
    """lockstep socket client: send exactly one cld, wait for its response,
    route it by the cmd we last sent, then compute the next cmd"""

    def __init__(self, hostname: str, port: str, team_name: str, client_id: str,
                 machine_arg: str):
        self.team = team_name
        self.hostname = hostname
        self.port = port
        self.machine = machine_arg
        self.client_num = int(client_id)
        self.data_width = 0
        self.data_height = 0
        self.socket = None
        self.just_log = 0
        self.logged = 0
        self.ai = AI(self.team, self.client_num)

    def connect_to_server(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.hostname, int(self.port)))
        self.socket.setblocking(False)

    def save_info(self, message: str, nb: int):
        if nb == 1:
            # server replies with the remaining slot count, or "ko" if the team is
            # already full (no slot for us) -> nothing to do, exit cleanly
            try:
                self.ai.useless_slot = int(message)
            except ValueError:
                print(f"[{self.client_num}] no slot available (server said "
                      f"{message!r}), team full", file=sys.stderr)
                sys.exit(0)
            self.ai.data_to_write = ""
            self.just_log = 2
        else:
            parts = message.split()
            self.data_width = int(parts[0])
            self.data_height = int(parts[1])
            self.ai.data_to_write = ""
            self.just_log = 3
            self.logged = 1
            self.ai.run = 1

    def _spawn_child(self):
        """relaunch our own main.py to fill the next slot in the fork chain"""
        main_py = os.path.join(os.path.dirname(os.path.abspath(__file__)), "main.py")
        env = dict(os.environ)
        env["PYTHONUNBUFFERED"] = "1"
        try:
            subprocess.Popen(
                [sys.executable, main_py, "-p", str(self.port), "-n", self.team,
                 "-h", self.machine, "-i", str(self.client_num + 1)],
                env=env, stdin=subprocess.DEVNULL)
            print(f"[{self.client_num}] forked + spawned child #{self.client_num + 1}")
        except OSError as e:
            print(f"[{self.client_num}] spawn failed: {e}")

    def launch_client(self):
        message = ""
        while True:
            readable, writable, _ = select.select([self.socket], [self.socket], [], 0.05)

            if readable:
                data = self.socket.recv(4096).decode("utf-8")
                if not data:
                    self.socket.close()
                    return
                message += data
                tmp = message.split("\n")
                for elem in tmp[:-1]:
                    if "dead" in elem:
                        print(f"[{self.client_num}] DIED (starved) at level {self.ai.level}")
                        sys.exit(0)
                    if "WELCOME" in elem and self.logged == 0:
                        self.ai.data_to_write = self.team + "\n"
                    elif self.just_log < 3:
                        if "message" in elem:
                            continue
                        self.save_info(elem, self.just_log)
                    elif "Elevation underway" in elem:
                        self.ai.step = 8
                    elif "Current level:" in elem:
                        self.ai.level = int(''.join(filter(str.isdigit, elem)))
                        print(f"[{self.client_num}] {elem.strip()}")
                        if self.ai.level == 8:
                            sys.exit(0)
                        self.ai.step = 9
                        self.ai.new_object = False
                        self.ai.to_search = ""
                        self.ai.incantation = 0
                        self.ai.master_incantation = 0
                        self.ai.nb_player_incantation = 1
                        self.ai.direction = 9
                        self.ai.ready_for_incantation = 0
                        self.ai.clear_read = 0
                        self.ai.clear_broadcast = 0
                        self.ai.current_master = 0
                        self.ai.data_to_write = ""
                        self.ai.commands_list = []
                    elif self.just_log >= 3 and "message" in elem:
                        if self.ai.clear_read == 1:
                            self.ai.clear_read = 0
                            message = message.split("\n")[-1]
                            continue
                        self.ai.broadcast = elem
                        self.ai.parse_broadcast(elem)
                        message = message.split("\n")[-1]
                        continue
                    elif ("Take" in self.ai.data_to_write
                          and "food" not in self.ai.data_to_write and elem == "ok"):
                        self.ai.update_shared_inventory()
                        self.ai.new_object = True
                    elif self.ai.data_to_write == "Inventory\n":
                        try:
                            self.ai.parse_inventory(elem)
                        except ValueError:
                            pass
                    elif self.ai.data_to_write == "Look\n":
                        self.ai.look = elem
                    elif self.ai.data_to_write == "Connect_nbr\n":
                        try:
                            self.ai.useless_slot = int(elem)
                        except ValueError:
                            self.ai.useless_slot = 0
                        if (self.ai.useless_slot != 0 and self.client_num < TEAM_SIZE
                                and self.ai.fork == 1):
                            self._spawn_child()
                            self.ai.fork = 0
                    message = message.split("\n")[-1]
                    self.ai.run = 1

            if writable:
                if self.logged and self.ai.run == 1:
                    self.ai.algorithm()
                if self.ai.data_to_write and self.ai.run != 0:
                    if self.ai.data_to_write == (self.team + "\n") and self.logged == 0:
                        self.just_log = 1
                    self.socket.send(self.ai.data_to_write.encode())
                    self.ai.run = 0


def run(hostname: str, port: str, name: str, client_id: str):
    client = Client(hostname, port, name, client_id, hostname)
    client.connect_to_server()
    client.launch_client()
