#!/usr/bin/env python3
"""
Minimal mock zappy_server for testing NetworkClient.

Protocol sequence:
  server -> "WELCOME\n"
  client -> "GRAPHIC\n"
  client -> "msz\n"  "mct\n"  "tna\n"  "sgt\n"   (bootstrap)
  server -> representative lines for all major message types

Usage:
  python3 tests/mock_server.py [port]   (default port: 14242)

Exit codes:
  0 — all expected bootstrap commands were received
  1 — expected command not received / timeout / connection error
"""

import socket
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 14242
TIMEOUT = 10  # seconds to wait for client messages

received_cmds = []
errors = []


def handle_client(conn: socket.socket) -> None:
    conn.settimeout(TIMEOUT)

    # ── Handshake ────────────────────────────────────────────────────────────
    conn.sendall(b"WELCOME\n")
    print("[mock] sent: WELCOME")

    # Read until we get "GRAPHIC\n"
    buf = b""
    try:
        while b"\n" not in buf:
            chunk = conn.recv(256)
            if not chunk:
                errors.append("Connection closed before GRAPHIC received")
                return
            buf += chunk
    except socket.timeout:
        errors.append("Timeout waiting for GRAPHIC")
        return

    line, buf = buf.split(b"\n", 1)
    cmd = line.decode().strip()
    received_cmds.append(cmd)
    if cmd != "GRAPHIC":
        errors.append(f"Expected GRAPHIC, got: {cmd!r}")
        return
    print(f"[mock] received: {cmd!r}")

    # ── Bootstrap commands: msz, mct, tna, sgt ───────────────────────────────
    # Read up to 4 bootstrap lines (may arrive concatenated in one recv)
    bootstrap_expected = {"msz", "mct", "tna", "sgt"}
    bootstrap_received = set()

    try:
        while bootstrap_expected - bootstrap_received:
            while b"\n" not in buf:
                chunk = conn.recv(1024)
                if not chunk:
                    break
                buf += chunk
            if b"\n" not in buf:
                break
            line, buf = buf.split(b"\n", 1)
            cmd = line.decode().strip()
            received_cmds.append(cmd)
            print(f"[mock] received: {cmd!r}")
            bootstrap_received.add(cmd)
    except socket.timeout:
        errors.append(f"Timeout waiting for bootstrap commands; got so far: {bootstrap_received}")
        return

    missing = bootstrap_expected - bootstrap_received
    if missing:
        errors.append(f"Bootstrap commands not received: {missing}")
        return

    # ── Send representative protocol lines ───────────────────────────────────
    # These lines exercise every parser branch that the C++ test checks.
    messages = [
        # Map size
        b"msz 10 8\n",
        # Tile content (bct)
        b"bct 0 0 1 0 0 0 0 0 0\n",
        b"bct 3 4 0 2 1 0 0 0 0\n",
        # Team name (one per line)
        b"tna TeamA\n",
        b"tna TeamB\n",
        # Time unit
        b"sgt 100\n",
        # New player
        b"pnw #1 2 3 1 1 TeamA\n",
        # Player position
        b"ppo #1 5 6 2\n",
        # Player level
        b"plv #1 3\n",
        # Player inventory
        b"pin #1 5 6 10 0 1 0 0 0 0\n",
        # Player expulsion
        b"pex #1\n",
        # Player broadcast
        b"pbc #1 hello world\n",
        # Player death
        b"pdi #1\n",
        # Incantation start
        b"pic 2 3 2 #1 #2\n",
        # Incantation end
        b"pie 2 3 ok\n",
        # Egg laying
        b"pfk #1\n",
        # Resource drop
        b"pdr #1 2\n",
        # Resource collect
        b"pgt #1 3\n",
        # Egg laid
        b"enw #10 #1 2 3\n",
        # Egg connection
        b"ebo #10\n",
        # Egg death
        b"edi #10\n",
        # Time unit set
        b"sst 50\n",
        # End game
        b"seg TeamA\n",
        # Server message
        b"smg hello from server\n",
        # Unknown command
        b"suc\n",
        # Bad parameter
        b"sbp\n",
        # Unknown prefix (should be silently skipped)
        b"xyz unknown garbage\n",
    ]

    for msg in messages:
        conn.sendall(msg)
        print(f"[mock] sent: {msg.decode().rstrip()!r}")
        time.sleep(0.01)  # small pacing to avoid coalescing all lines

    # Give the client time to parse everything before we close
    time.sleep(0.5)
    conn.close()
    print("[mock] connection closed")


def main() -> None:
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", PORT))
    srv.listen(1)
    print(f"[mock] listening on 127.0.0.1:{PORT}")

    srv.settimeout(15)
    try:
        conn, addr = srv.accept()
        print(f"[mock] accepted connection from {addr}")
    except socket.timeout:
        print("[mock] ERROR: no client connected within 15 seconds")
        sys.exit(1)
    finally:
        srv.close()

    handle_client(conn)

    if errors:
        for e in errors:
            print(f"[mock] ERROR: {e}")
        sys.exit(1)

    missing = {"msz", "mct", "tna", "sgt"} - set(received_cmds)
    if missing:
        print(f"[mock] ERROR: bootstrap commands missing: {missing}")
        sys.exit(1)

    print("[mock] PASS — all bootstrap commands received and all messages sent")
    sys.exit(0)


if __name__ == "__main__":
    main()
