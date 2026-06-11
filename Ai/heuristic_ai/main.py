##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## entry point, parse args and launch the ai
##

import argparse
import sys
from connection import Connection
from player.state import PlayerAI


def parse_args() -> tuple[int, str, str]:
    # using argparse here, way cleaner than doing it manualy
    # https://docs.python.org/3/library/argparse.html
    parser = argparse.ArgumentParser(
        prog="zappy_ai",
        usage="./zappy_ai -p port -n name -h machine",
        add_help=False,
    )
    parser.add_argument("-p", type=int, dest="port", required=True)
    parser.add_argument("-n", type=str, dest="name", required=True)
    parser.add_argument("-h", type=str, dest="machine", default="localhost")
    parser.add_argument("--help", action="help")

    try:
        args = parser.parse_args()
    except SystemExit:
        sys.exit(84)

    # basic sanity chek on the port
    if args.port <= 0 or args.port > 65535:
        print("USAGE: ./zappy_ai -p port -n name -h machine", file=sys.stderr)
        sys.exit(84)

    return args.port, args.name, args.machine


def main():
    port, name, machine = parse_args()

    # open connection and do the handshake first, then launch the ai loop
    conn = Connection(machine, port)
    world_x, world_y, slots = conn.handshake(name)

    ai = PlayerAI(conn, name, world_x, world_y, slots)
    ai.run()


if __name__ == "__main__":
    main()
