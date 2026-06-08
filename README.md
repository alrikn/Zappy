# Zappy

Epitech project about making a server, a gui, and ai.

## Quick Start — Testing with Reference Server

To test the AI with the reference server, open **3 separate terminals** and run:

**Terminal 1 — Server:**

```bash
./zappy_ref-v3.0.1/linux/zappy_server -p 4242 -x 10 -y 10 -n team1 -c 6 -f 100 --auto-start on </dev/null
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

## Auto-tuning the AI (continuous learning)

The AI's behaviour is controlled by parameters in `Ai/heuristic_ai/config.py`. An
evolutionary tuner plays headless games over and over, scores them automatically,
and evolves those parameters toward higher elevation — no more reading logs by hand.

```bash
./train.sh                      # train forever on a 10x10 map, 6 AIs (Ctrl+C to stop)
./train.sh --map 20 --ais 6 --game-seconds 90
./train.sh --iterations 50      # stop after 50 candidates instead of running forever
cat best_params.json.meta       # current best score, any time
```

It writes `best_params.json` at the repo root. The AI loads it automatically at
startup (via the `ZAPPY_AI_CONFIG` env var that `train.sh` / the tuner set), so just
leaving training running makes the AI permanently better. Keep `best_params.json`
with the project — there are no model weights, no GPU, and zero cost at grading time.

`tuner_history.csv` logs fitness over time if you want to plot progress.

# AI part (jad):

fully heuristic, idk how we could make it real ai, maybe reinforcement learning algo in the wander func where, we handle connection with tcp with the server, Ai/zappy_ai is the shell script that calls main.py
