#!/usr/bin/env python3
"""Minimal GUI mock server — does the WELCOME/GRAPHIC handshake then stays silent."""

import socket, sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 4242
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", port))
srv.listen(1)
print(f"Listening on :{port} — Ctrl-C to stop")

conn, addr = srv.accept()
print(f"Connected: {addr}")

# The GUI blocks in connect() until it receives WELCOME\n.
conn.sendall(b"WELCOME\n")
print("Sent: WELCOME")

# Drain incoming data (GRAPHIC\n + msz\n mct\n tna\n sgt\n) without blocking forever.
conn.settimeout(2.0)
buf = b""
try:
    while True:
        chunk = conn.recv(4096)
        if not chunk:
            break
        buf += chunk
except socket.timeout:
    pass  # all bootstrap commands received, GUI is now in its event loop

print(f"Received: {buf.decode(errors='replace').strip()!r}")
print("GUI is running — Ctrl-C to disconnect")

conn.settimeout(None)
try:
    while True:
        if not conn.recv(4096):
            break
except KeyboardInterrupt:
    pass
finally:
    conn.close()
    srv.close()

