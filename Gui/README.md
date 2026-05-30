# zappy_gui

3D Vulkan graphical client for the Epitech Zappy project. Connects to a `zappy_server`
via TCP, receives real-time game state updates, and renders the toroidal Trantor world
using Vulkan. Written in C++20.

## Build

```bash
# Install system dependencies (Ubuntu/Debian)
sudo apt-get install -y libvulkan-dev glslc libglfw3-dev libfreetype-dev

# Configure and build (debug + ASan)
cmake --preset debug
cmake --build build/debug --target zappy_gui

# Or via Makefile:
make
```

## Run

```bash
./zappy_gui -p <port> -h <server-hostname>
```

## Development


See `docs/architecture.md` for the three-component architecture and design rationale.
