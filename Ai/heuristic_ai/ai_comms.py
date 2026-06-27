##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## ai mixin: team broadcast encode/decode + homing toward the master beacon
##


class CommsMixin:
    """broadcast helpers: plaintext, prefixed with the team name so only our own
    players parse it (the server delivers every team's broadcast to everyone)"""

    def _bcast(self, body: str) -> str:
        """encode a team-prefixed message as hex and wrap it in a Broadcast command"""
        # the servers broadcast takes the text as one whitespace free token, but our
        # bodies contain spaces (json inventory, "on my way") so we hex encode the whole
        # payload (team prefix + body), this is what the ref ai did via xor hex, we keep
        # the hex (guarantees no spaces) and drop the xor (plaintext team prefix is
        # enough to isolate our teams messages and it stays debugdebuagle)
        payload = (self.team + "~" + body).encode("utf-8").hex()
        return "Broadcast " + payload + "\n"

    def parse_broadcast(self, raw: str):
        """decode an incoming server broadcast line and route it to the right handler"""
        # raw is the full server line: "message K, <hex>"
        try:
            direction = int(raw[8])
        except (ValueError, IndexError):
            return
        try:
            decoded = bytes.fromhex(raw[11:].strip()).decode("utf-8")
        except (ValueError, UnicodeDecodeError):
            return                      # not one of our hex payloads, ignore
        if not decoded.startswith(self.team + "~"):
            return                      # another team's message, ignore
        message = decoded[len(self.team) + 1:]

        if "inventory" in message:
            self.parse_shared_inventory(message[9:])
        elif "incantation" in message:
            self._handle_incantation_call(direction, message)
        elif "on my way" in message and self.master_incantation >= 1:
            self.nb_player_incantation += 1
        elif "ready" in message and self.master_incantation >= 1:
            # a follower confirmed it is standing on my tile
            self.master_incantation += 1

    def _handle_incantation_call(self, direction: int, message: str):
        """react to a master's incantation broadcast: yield if outranked, otherwise home in"""
        if self.clear_broadcast:
            self.clear_broadcast = 0
            return
        master_num = int(message.split(";")[0])
        # collision resolution: a master yields to a competing master with a
        # higher client_num, leaving exactly one deterministic master
        if self.master_incantation >= 1 and master_num > self.client_num:
            self.master_incantation = 0
            self.incantation = 0
            self.step = 0
            self.current_master = 0
            return
        # follower: ignore broadcasts from a lower priority competing master
        # so we dont take nav steps toward the wrong tile
        if self.incantation == 1 and master_num < self.current_master:
            return
        self.current_master = master_num
        # a master is calling: if i have enough food, drop what im doing and join
        if 0 <= self.step < 4 and self.inventory["food"] > 10:
            self.step = 4
            self.commands_list = []
        if self.incantation == 1:
            self.commands_list = self.go_to_broadcast(direction)

    def go_to_broadcast(self, direction: int):
        """one homing step toward the master beacon; announce 'ready' once on its tile"""
        if self.ready_for_incantation == 1 or self.commands_list:
            return
        if direction == 0:
            self.data_to_write = self._bcast("ready")
            self.direction = 0
            self.ready_for_incantation = 1
            return []
        if direction in (2, 1, 8):
            return ["Forward\n"]
        if direction in (5, 6, 7):
            return ["Right\n"]
        return ["Left\n"]
