##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## player ai entry class, holds the state machine and main loop
##

import os
import random
import time
from connection import Connection
from state_machine import State
from resources import has_all_stones, next_stone_to_collect, players_needed, ELEVATION, STONES
import commands as cmd
import broadcast as bcast
import config as cfg
from vision import count_players_on_tile
from .actions import PlayerActionsMixin


def _uid() -> str:
    # short random hex id to identify this player instance in broadcasts
    return f"{random.randint(0, 0xFFFF):04x}"


class PlayerAI(PlayerActionsMixin):
    def __init__(self, conn: Connection, team_name: str, world_x: int, world_y: int, client_num: int = 5):
        self.conn      = conn
        self.team_name = team_name
        self.world_x   = world_x
        self.world_y   = world_y

        self.level     = 1
        self.inventory: dict[str, int] = {"food": 10}
        self.state     = State.GATHER_FOOD
        self.uid       = _uid()

        # all tunable behaviour comes from config (json overridable for the tuner)
        self.cfg = cfg.load()

        # dynamic food thresholds, scale to map size and estimated player count so the
        # ai doesnt hoard more food than the map can sustain
        total_food  = max(1, int(0.5 * world_x * world_y))
        # assume at least 6 players (zappy goal), slots+1 gives a real upper bound when
        # we re the first to connect (slots=c-1), max(6,...) keeps it sane even if other
        # players joined before us and slots is already smaller
        est_players = max(6, client_num + 1)
        food_pp     = total_food / est_players  # food available per player on map

        self.FOOD_CRITICAL = self.cfg.i("food_critical")
        # safe reserve = density term (food per player) + travel term (map span), the
        # travel term gives big sparse maps a fatter buffer to survive the long walks
        # between resources and the ritual freeze, like how the strongest ref ais hoard
        # ~45 to 52 on roomy maps while staying lean on small ones
        food_target = food_pp * self.cfg.f("food_safe_mult") \
                    + (world_x + world_y) * self.cfg.f("food_travel_factor")
        # survivability floor: one coordination cycle (walk to the leader + the 300/f
        # ritual freeze) costs food roughly proportional to map span, force a safe
        # reserve big enough to survive it even if the tuner (tuned on a small map) set
        # food_safe_max too low, else players gather a tiny buffer and starve mid
        # rendezvous on a big map, ~0.8*(w+h) on top of the critical floor
        survive_floor = self.FOOD_CRITICAL + int((world_x + world_y) * 0.8)
        self.FOOD_SAFE = max(self.cfg.i("food_safe_min"), survive_floor,
                             min(self.cfg.i("food_safe_max"), int(food_target)))
        self.FOOD_LOW  = max(self.cfg.i("food_low_min"),
                             min(self.cfg.i("food_low_max"),
                                 int(food_pp * self.cfg.f("food_low_mult"))))
        # coherence guard: food_low must sit below food_safe, else a player hits "safe",
        # starts gathering stones and is instantly back under "low" so it can never build
        # a buffer, the tuner search has no such constraint so it can make inverted
        # configs, clamp here so behaviour stays sane whatever
        if self.FOOD_LOW >= self.FOOD_SAFE:
            self.FOOD_LOW = max(self.FOOD_CRITICAL + 1, self.FOOD_SAFE - 3)
        # gate for joining a leader: must be high enough to survive the round trip +
        # ritual, on a big map the walk to the leader is long so this scales w/ map span,
        # committing at food=6 (what a small map tuned config gives) is a sure death on
        # 20x20, floor at critical + ~0.5*(w+h), the tuner can only raise it
        self.FOOD_JOIN = max(self.FOOD_CRITICAL + self.cfg.i("join_food_buffer"),
                             self.FOOD_CRITICAL + int((world_x + world_y) * 0.5))
        # the seek/wait budgets must scale w/ map size: homing to the leader on a 20x20
        # takes roughly twice the steps of a 10x10 so a fixed tick budget that works on a
        # small map times out before followers arrive on a big one
        # span = 1.0 on 10x10, 2.0 on 20x20, etc
        span = max(1.0, (world_x + world_y) / 20.0)
        self.SEEK_TIMEOUT       = int(self.cfg.i("seek_timeout") * span)
        self.WAIT_TIMEOUT       = int(self.SEEK_TIMEOUT * self.cfg.f("wait_timeout_mult"))
        self.LISTEN_BEFORE_LEAD = self.cfg.i("listen_before_lead")
        self.EXTRA_PLAYER_WAIT  = self.cfg.i("extra_player_wait")
        self.EXTRA_WAIT_MIN_LVL = self.cfg.i("extra_wait_min_level")
        self.BCAST_INTERVAL     = max(1, self.cfg.i("bcast_interval"))

        print(f"[{self.uid}] map={world_x}x{world_y} total_food={total_food} "
              f"est_players={est_players} FOOD_SAFE={self.FOOD_SAFE} "
              f"FOOD_LOW={self.FOOD_LOW} FOOD_JOIN={self.FOOD_JOIN}")

        # latest look result, updated evry loop
        self._tiles: list[list[str]] = []

        # dead reckoning position tracking, in a private frame relative to spawn, the
        # protocol never tells us our absolute coords or orientation so this cant be
        # shared w/ teammates for rendezvous, its only so we can explore efficiently
        # (head for tiles we havnt visited lately instead of random walking), facing
        # 0..3 are the four cardinals in our own frame, pos wraps on the torus, eject
        # can desync this which only hurts exploration quality never correctness
        self._facing = 0
        self._pos = [0, 0]
        self._visited: dict[tuple[int, int], int] = {}
        self._move_tick = 0

        # these flags track wether we have pending async calls in flight
        self._inventory_pending = False
        self._look_pending      = False
        self._action_pending    = False  # waiting for a move/turn/take response

        # coordination state
        self._leader_uid: str | None = None  # uid of whoever called NEED_INC
        self._leader_k:   int | None = None  # most recent direction to leader (sound k)
        self._k_fresh:    bool = False       # true for one tick after a new bearing
                                             # arrives so we only act on fresh data
        self._home_ticks: int = 0            # throttle counter for homing logs
        self._home_steps: int = 0            # homing moves taken this seek attempt
        self._ready_count: int = 0           # followers that said IM_READY to us
        self._seek_ticks:  int = 0           # anti stall counter for seek+wait

        # ritual state
        self._stones_dropped  = False  # did we already drop our stones for this ritual?
        self._bcast_ticks     = 0      # leader re broadcast interval counter
        self._full_wait_ticks = 0      # ticks waited after min players (unused now)
        self._present_ticks   = 0      # ticks the tile held >= needed bodies (im_ready fallback)
        self._incant_t0       = 0.0    # wall clock when we fired incant (to tag ko phase)

        # forking
        self._fork_ticks = 0          # how long since last fork attempt

        # cooldown after failing coordination, prevents immediately becoming a
        # new leader when we already have all stones but just timed out as follower
        self._coord_cooldown = 0

        # log throttle: suppress repeated "rejected NEED_INC" lines
        self._reject_count = 0

        # hook up the unsolicited event handlers
        self.conn.on_broadcast(self._on_broadcast)
        self.conn.on_eject(self._on_eject)
        # make starvation visible instead of a silent process exit (the connection
        # calls this then exits on the server "dead" message)
        self.conn.on_dead(lambda: print(f"[{self.uid}] DIED (starved) at level {self.level}"))
        # FRAGILE STUFF HERE TODO : the connection only has one on_level_up slot, we register
        # self._on_level_up here but cmd.incantation() overwrites it with its own
        # handler each time we incantate then sets it back to None when done, so this
        # init handler is basicaly dead after the first incantation, its not a crash i tested it
        # bc the incantation flow tracks our level itself via _on_incantation_result,
        # but if we ever want to react to external level up events this needs a rework
        # (ideally a list of handlers instead of a single slot) but idkkk
        self.conn.on_level_up(self._on_level_up)

        # loop pacing: sleep between iterations so we dont busy spin the cpu, at ~0.01s
        # the ai does ~100 actions/sec which becomes the bottleneck once the server runs
        # fast (high f), lower it (e.g ZAPPY_AI_TICK=0.001) to let the ai keep up w/ a
        # sped up server so games progress faster in wall clock
        try:
            self._loop_sleep = max(0.0, float(os.environ.get("ZAPPY_AI_TICK", "0.01")))
        except ValueError:
            self._loop_sleep = 0.01

        # get initial state before the loop starts
        self._request_inventory()
        self._request_look()

    def run(self):
        """main loop, runs forever until the player dies or the game ends"""
        while True:
            self.conn.pump()       # read socket, dispatch callbacks
            self._update_state()   # check if we need to transition
            self._act()            # do one action based on current state
            if self._loop_sleep:
                time.sleep(self._loop_sleep)

    # state transition logic

    def _update_state(self):
        food = self.inventory.get("food", 0)

        # food check always wins whatever state we are in
        # (except during incantation where we re frozen and cant do anything anyway)
        if food < self.FOOD_CRITICAL and self.state not in (State.SURVIVE, State.INCANTATING):
            self._transition(State.SURVIVE)
            return

        if self.state == State.SURVIVE and food >= self.FOOD_CRITICAL + 2:
            self._transition(State.GATHER_FOOD)

        elif self.state == State.GATHER_FOOD and food >= self.FOOD_SAFE:
            self._transition(State.GATHER_STONES)

        elif self.state == State.GATHER_STONES:
            if self._coord_cooldown > 0:
                self._coord_cooldown -= 1
            elif food < self.FOOD_LOW:
                # food check first: coordination burns a bunch of food, gotta enter w/ enough
                self._transition(State.GATHER_FOOD)
            # lvl 8 is the max, after that just eat food and survive
            elif self.level < 8 and has_all_stones(self.inventory, self.level):
                if players_needed(self.level) <= 1:
                    # solo ritual (level 1 needs only 1 player): skip the whole team
                    # dance, broadcasting need_inc here lured other level 1 players into
                    # homing toward us then we fired instantly and vanished, stranding
                    # them till they timed out, that poisoned evry rendezvous on big maps,
                    # claim self leadership and go straight to drop+fire without recruiting
                    self._leader_uid  = self.uid
                    self._ready_count = 0
                    self._transition(State.WAIT_TEAM)
                else:
                    self._transition(State.SEEK_TEAM)

        elif self.state == State.SEEK_TEAM:
            self._seek_ticks += 1
            if self._seek_ticks > self.SEEK_TIMEOUT:
                # diagnostic: did homing stall? show the last bearing + steps taken
                if self._leader_uid is not None and self._leader_uid != self.uid:
                    print(f"[{self.uid}] SEEK TIMEOUT homing to {self._leader_uid}: "
                          f"{self._home_steps} steps, last k={self._leader_k} (never reached 0)")
                # followers get short cooldown, leaders get longer one, without a cooldown
                # the leader immediately re enters seek_team since it still has all its
                # stones, makes a tight loop
                self._coord_cooldown = 50 if self._leader_uid != self.uid else 150
                self._transition(State.GATHER_STONES)

        elif self.state == State.WAIT_TEAM:
            self._seek_ticks += 1
            if self._seek_ticks > self.WAIT_TIMEOUT:
                # same logic, leaders that waited w/ nobody showing up back off longer
                # before trying again
                self._coord_cooldown = 50 if self._leader_uid != self.uid else 200
                # waited too long, nobody came
                self._transition(State.GATHER_STONES)

    def _transition(self, new_state: State):
        food = self.inventory.get("food", 0)
        print(f"[{self.uid}] {self.state} -> {new_state}  food={food}")
        if new_state == State.SEEK_TEAM:
            # reset seek counter on every (re)entry including leader-yield transitions
            self._seek_ticks = 0
            self._home_steps = 0
        if new_state == State.WAIT_TEAM:
            self._stones_dropped  = False
            self._bcast_ticks     = 0
            self._full_wait_ticks = 0
            self._present_ticks   = 0
        if new_state in (State.GATHER_FOOD, State.GATHER_STONES, State.SURVIVE):
            self._leader_uid     = None
            self._leader_k       = None
            self._seek_ticks     = 0
            self._stones_dropped = False
        self.state           = new_state
        self._action_pending = False

    # per state action handlers

    def _act(self):
        if self._action_pending:
            return
        # always keep inventory and vision fresh
        if not self._inventory_pending:
            self._request_inventory()
        if not self._look_pending:
            self._request_look()

        # try to fork periodically when not in a critical state
        if self.state in (State.GATHER_FOOD, State.GATHER_STONES):
            self._fork_ticks += 1
            if self._fork_ticks >= self.cfg.i("fork_interval"):
                self._fork_ticks = 0
                self._try_fork()

        if self.state == State.SURVIVE:
            self._act_survive()
        elif self.state == State.GATHER_FOOD:
            self._act_gather_food()
        elif self.state == State.GATHER_STONES:
            self._act_gather_stones()
        elif self.state == State.SEEK_TEAM:
            self._act_seek_team()
        elif self.state == State.WAIT_TEAM:
            self._act_wait_team()
        elif self.state == State.FORKING:
            pass  # handled by the fork callback
        elif self.state == State.INCANTATING:
            pass  # frozen, nothing we can do

    def _act_survive(self):
        self._navigate_to_resource("food")

    def _act_gather_food(self):
        self._navigate_to_resource("food")

    def _act_gather_stones(self):
        stone = next_stone_to_collect(self.inventory, self.level)
        if stone is None:
            return
        self._navigate_to_resource(stone)

    def _act_seek_team(self):
        if self._leader_uid is None:
            # brief listen window: give nearby need_inc broadcasts time to arrive before
            # we declare leadership, cuts down on competing simultaneous leaders
            if self._seek_ticks < self.LISTEN_BEFORE_LEAD:
                return
            self._become_leader()
            return

        # sound homing toward the leader beacon: aim then advance, one homing primitive
        # per fresh ping (moves_toward rotates to face the leader or steps forward once
        # the leader is in our forward arc), we take a single primitive per ping and
        # never walk blind between pings, both blind forward runs and turn+step in one
        # move make the follower orbit the beacon at a constant bearing instead of
        # landing on its tile (see broadcast.py)
        if self._leader_k == 0:
            # standing on the leader tile, announce arrival and wait for start
            print(f"[{self.uid}] ARRIVED at {self._leader_uid} after {self._home_steps} "
                  f"homing steps (seek_ticks={self._seek_ticks})")
            self._leader_k = None
            self._k_fresh  = False
            cmd.broadcast(self.conn, bcast.encode(bcast.IM_READY, self._leader_uid),
                           self._noop)
            self._transition(State.WAIT_TEAM)
            return

        if self._k_fresh and self._leader_k is not None:
            # one step per fresh bearing then wait for the next ping to re aim,
            # moves_toward returns at most a turn + a single forward so each ping
            # advances us <=1 tile straight at the leader, we do NOT keep walking
            # forward between pings, a blind straight run on a stale bearing overshoots
            # and makes the follower orbit the beacon forever (k oscillating 6,3,4 and
            # never reaching 0 like we saw on 20x20), the leader re broadcasts evry
            # bcast_interval ticks so pings are frequent enough that one step per ping
            # still converges quick
            self._k_fresh = False
            self._home_steps += 1
            self._home_log()
            self._execute_moves(bcast.moves_toward(self._leader_k))

    def _act_wait_team(self):
        # drop our required stones onto the tile once
        if not self._stones_dropped:
            self._drop_for_ritual()
            self._stones_dropped = True
            return  # wait for a fresh look reflecting the dropped stones

        if self._leader_uid == self.uid:
            needed   = players_needed(self.level)
            # re broadcast so followers can keep recalibrating their direction to us but
            # ONLY for real team rituals, a solo (needed==1) leader broadcasting would
            # lure other same level players into chasing it for nothing
            if needed > 1:
                self._bcast_ticks += 1
                if self._bcast_ticks >= self.BCAST_INTERVAL:
                    self._bcast_ticks = 0
                    text = bcast.encode(bcast.NEED_INC, f"{self.level}:{self.uid}")
                    cmd.broadcast(self.conn, text, self._noop)

            # check if enough players are on our tile AND stones are present
            on_tile  = count_players_on_tile(self._tiles, 0)
            tile     = self._tiles[0] if self._tiles else []
            req      = ELEVATION.get(self.level, {})
            stones_ok = all(tile.count(s) >= req.get(s, 0) for s in STONES if req.get(s, 0) > 0)

            # _ready_count is followers that acked us, +1 for ourselves, fallback: if the
            # tile visibly held enough bodies for a while but im_ready got lost, fire
            # anyway so we dont deadlock
            confirmed = 1 + self._ready_count
            self._present_ticks = self._present_ticks + 1 if on_tile >= needed else 0
            ready = confirmed >= needed or self._present_ticks >= 30

            # diagnostic: show exactly which condition gates firing while we wait
            self._wait_dbg = getattr(self, "_wait_dbg", 0) + 1
            if self._wait_dbg % 25 == 1:
                print(f"[{self.uid}] WAIT-LEADER lvl{self.level} on_tile={on_tile} "
                      f"need={needed} stones_ok={stones_ok} ready={ready} "
                      f"rcnt={self._ready_count} present={self._present_ticks} "
                      f"food={self.inventory.get('food', 0)} tile={tile}")

            # fire the instant we have what we need, we only ever need exactly "needed"
            # players so theres nothing to gain by waiting for more, and waiting was
            # actively harmful: the diagnostic showed leaders sting  on a fully satisfied
            # tile (on_tile==need, stones_ok, ready) for dozens of ticks under the old
            # "wait for more players to  pile on" branch till the partner gave up and
            # walked off so the ritual never fired, also takes propty over eating
            if on_tile >= needed and stones_ok and ready:
                self._fire_incantation()
                return

        elif self._leader_uid is not None:
            # follower: keep re announcing readiness, a single im_ready sent on arrival is
            # easily lost in a broadcast collision (we saw leaders fire w/ rcnt=0 ie never
            # counting the follower standing right on the tile), by re broadcasting evry
            # so often the leader reliably counts us and fires on a confirmed committed
            # same level partner instead of a random passer by
            self._bcast_ticks += 1
            if self._bcast_ticks >= self.BCAST_INTERVAL:
                self._bcast_ticks = 0
                cmd.broadcast(self.conn, bcast.encode(bcast.IM_READY, self._leader_uid),
                               self._noop)

        # leader (not ready to fire) AND followers: the player is anchored and cant
        # forage so it starves while waiting, eat any food on the tile to stay alive long
        # enough for the ritual to assemble, this was a big cause of abandoned rituals but
        # it runs AFTER the leader fire check so it never delays firing
        if self.cfg.b("leader_eat_in_wait") and self._tiles and "food" in self._tiles[0]:
            self._action_pending = True
            def on_eat(ok: bool):
                if ok:
                    try:
                        self._tiles[0].remove("food")
                    except (ValueError, IndexError):
                        pass
                self._action_pending = False
            cmd.take(self.conn, "food", on_eat)
