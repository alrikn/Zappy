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

**Terminal 3 — AI Players** (run as many as you want, e.g., 3):

```bash
PYTHONPATH=Ai/heuristic_ai python3 Ai/heuristic_ai/main.py -p 4242 -n team1 -h localhost &
PYTHONPATH=Ai/heuristic_ai python3 Ai/heuristic_ai/main.py -p 4242 -n team1 -h localhost &
PYTHONPATH=Ai/heuristic_ai python3 Ai/heuristic_ai/main.py -p 4242 -n team1 -h localhost &
```

The server will auto-start, the GUI will visualize the game, and the AI players will begin gathering resources and leveling up.

# AI part (jad):

fully heuristic, idk how we could make it real ai, maybe reinforcement learning algo in the wander func where, we handle connection with tcp with the server, Ai/zappy_ai is the shell script that calls main.py
