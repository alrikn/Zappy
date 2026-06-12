##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## entry point, parse args and launch the ai
##

import argparse
import os
import sys
import client


def parse_args() -> tuple[int, str, str, str]:
    # using argparse here, way cleaner than doing it manualy
    # https://docs.python.org/3/library/argparse.html
    parser = argparse.ArgumentParser(
        prog="zappy_ai",
        usage="./zappy_ai -p port -n name -h machine [-i id]",
        add_help=False,
    )
    parser.add_argument("-p", type=int, dest="port", required=True)
    parser.add_argument("-n", type=str, dest="name", required=True)
    parser.add_argument("-h", type=str, dest="machine", default="localhost")
    # client id = our ordinal in the self replicating fork chain (1 = the one you
    # launched). a child gets id parent+1, falls back to the ZAPPY_AI_IDX env var
    parser.add_argument("-i", type=str, dest="client_id",
                        default=os.environ.get("ZAPPY_AI_IDX", "1"))
    parser.add_argument("--help", action="help")

    try:
        args = parser.parse_args()
    except SystemExit:
        sys.exit(84)

    # basic sanity chek on the port
    if args.port <= 0 or args.port > 65535:
        print("USAGE: ./zappy_ai -p port -n name -h machine", file=sys.stderr)
        sys.exit(84)

    return args.port, args.name, args.machine, args.client_id


def main():
    port, name, machine, client_id = parse_args()
    client.run(machine, str(port), name, client_id)


if __name__ == "__main__":
    main()
