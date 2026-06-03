# Zappy

Epitech project about making a server, a gui, and ai.

## How to execute the program

start with running the server:

```bash
./zappy_server -p 4242 -x 10 -y 10 -n steve -c 7 -f 80
```

Then, in another terminal, run the gui:

```bash
./zappy_gui -p 4242 -h localhost
```

Then run the bots.

# AI part (jad):

fully heuristic, idk how we could make it real ai, maybe reinforcement learning algo in the wander func where, we handle connection with tcp with the server, Ai/zappy_ai is the shell script that calls main.py

to launch it: ./zappy_ai -p port -n teamname -h machine

