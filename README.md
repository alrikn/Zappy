# Zappy

Zappy is a network-based survival game where teams of AI-controlled players compete on a shared map, gathering resources and coordinating rituals to level up. It's an Epitech school project made up of three parts: a game server, a graphical client, and an AI client.

- **Server** — hosts the game world, manages players, resources, and rules.
- **GUI** — visualizes the game in real time.
- **AI** — connects to the server and plays the game autonomously, forking itself into a full team.

## Quick Start

Open **3 separate terminals** and run:

**Terminal 1 — Server:**

```bash
./Server/zappy_server -p 4242 -x 20 -y 20 -n team1 -c 6 -f 100
```

**Terminal 2 — GUI:**

```bash
./zappy_gui
```

**Terminal 3 — AI** (launch ONE, it self-replicates to a full team of 6 via forking):

```bash
./Ai/zappy_ai -p 4242 -n team1 -h localhost 2>&1
```

The server starts the game, the GUI visualizes it, and the single AI process forks itself into 6 players that gather resources and elevate together.
