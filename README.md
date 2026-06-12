# Zappy

Epitech project about making a server, a gui, and ai.

## Quick Start — Testing with Reference Server

To test the AI with the reference server, open **3 separate terminals** and run:

**Terminal 1 — Server:**

```bash
./zappy_ref-v3.0.1/linux/zappy_server -p 4242 -x 20 -y 20 -n team1 -c 6 -f 100 --auto-start on </dev/null
```

**Terminal 2 — GUI:**

```bash
./zappy_ref-v3.0.1/linux/zappy_gui.AppImage -p 4242 -h localhost
```

**Terminal 3 — AI** (launch ONE, it self-replicates to a full team of 6 via forking):

```bash
./Ai/zappy_ai -p 4242 -n team1 -h localhost
```

to kill the process

```bash
pkill -f "heuristic_ai/main.py"; pkill -f "zappy_ref-v3.0.1/linux/zappy_server"
```

the server auto starts, the GUI visualizes the game, and the single AI forks itself
into 6 players that gather resources and elevate together

# AI part (jad):

fully heuristic, idk how we could make it real ai, maybe reinforcement learning algo in the wander func where, we handle connection with tcp with the server, Ai/zappy_ai is the shell script that calls main.py
strategy: the whole 6 player team converges on a single tile for
every incantation, so one ritual elevates the entire team at once and they advance
in lockstep, reaches levle 6 sometimes like not eprfect not psosible but reaches 2 to 6
