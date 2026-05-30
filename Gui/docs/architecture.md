# Zappy GUI — Architecture

This is a living document. Update it when the architecture changes.

---

## Three-Component Architecture

The application is built from three top-level subsystems, each with a distinct
responsibility and a strict boundary:

```
┌──────────────────────────────────────────────────────┐
│                     main.cpp                         │
│  Creates all three subsystems and runs the event loop │
└──────────┬──────────────────────┬────────────────────┘
           │                      │
           ▼                      ▼
┌─────────────────┐    ┌──────────────────────────────┐
│  NetworkClient  │    │          Renderer             │
│                 │───▶│                               │
│  TCP socket     │    │  Vulkan instance, device,     │
│  Protocol parser│    │  swapchain, pipelines         │
│  Message queue  │    │  Reads FrameData each frame   │
└────────┬────────┘    └──────────────────────────────┘
         │                         ▲
         │ apply(msg)              │ snapshot()
         ▼                         │
┌─────────────────────────────────┘
│          WorldState              │
│                                  │
│  Tiles, players, eggs, time      │
│  Mutex-protected                 │
│  Zero Vulkan includes            │
└──────────────────────────────────┘
```

### NetworkClient (`src/network/`)
- Owns the TCP socket to `zappy_server`
- Runs on a dedicated thread (added in the network feature)
- Parses incoming bytes into protocol messages
- Calls `WorldState::apply(message)` for each parsed message

### WorldState (`src/world/`)
- The authoritative game state: tiles, players, eggs, time unit, team names
- Protected by a `std::mutex` — the network thread writes, the render thread reads
- **Zero Vulkan includes** — this is a hard rule. World state knows nothing about rendering.
- Provides `WorldState::snapshot()` which returns an immutable `FrameData` copy

### Renderer (`src/renderer/`)
- Owns all Vulkan objects: instance, device, swapchain, command buffers, pipelines
- Called from the main thread once per frame via `drawFrame()`
- Does NOT lock WorldState during rendering — it works from a FrameData snapshot

---

## The FrameData Snapshot Pattern

### The problem
The render loop runs at ~60 fps. The network loop receives updates sporadically (when
the server sends messages). If the renderer locked WorldState for the entire frame, the
network thread would be blocked. If the network thread held the lock and sent many
messages, the renderer would stutter.

### The solution
```
WorldState (mutex-protected)
    │
    │  snapshot()  — acquires mutex, copies entire state, releases mutex
    ▼
FrameData  (immutable, no mutex needed)
    │
    │  passed to Renderer::drawFrame()
    ▼
GPU commands submitted — renderer never touches WorldState again this frame
```

`FrameData` is a plain `struct` containing copies (not references) of everything the
renderer needs for one frame. The mutex is held only during the copy — typically
microseconds, not the full frame time.

### Why not a read-write lock?
A `std::shared_mutex` (reader-writer lock) would allow multiple readers. But because
the renderer always copies the entire state anyway (you need a consistent snapshot, not
live references), a brief exclusive lock for the copy is simpler and sufficient.

---

## Toroidal World Wrapping

The Trantor world is a torus: walking off the right edge wraps to the left, and
walking off the bottom edge wraps to the top.

In the protocol, tile coordinates are always in `[0, width)` × `[0, height)`.
There is no "out of bounds" — all coordinates are modulo the map size.

### Implications for rendering

When rendering an object at position `(x, y)`:
- If `x` is near the right edge (e.g. `x = width - 1`) and we draw a tile of visual
  width larger than one grid cell, the visual representation may cross the seam.
- To handle this correctly, the renderer must draw "ghost copies" of objects near
  the seam — the same object drawn again at `(x - width, y)` and `(x, y - height)`.

This is the same technique used by toroidal game worlds in classic games (Asteroids,
Snake). It is not implemented yet — it will be addressed in the tile renderer feature.

---

## Protocol Message Routing

Each server→GUI message is handled by a specific class:

| Message | Format | Handler |
|---------|--------|---------|
| `msz X Y` | Map size | `WorldState::setMapSize(x, y)` |
| `bct X Y q0..q6` | Tile content | `WorldState::setTile(x, y, resources)` |
| `tna N` | Team name | `WorldState::addTeam(name)` |
| `pnw #n X Y O L N` | Player connected | `WorldState::addPlayer(...)` |
| `ppo #n X Y O` | Player position | `WorldState::updatePlayerPos(...)` |
| `plv #n L` | Player level | `WorldState::updatePlayerLevel(...)` |
| `pin #n X Y q0..q6` | Player inventory | `WorldState::updatePlayerInventory(...)` |
| `pex #n` | Player expelled | `WorldState::expelPlayer(n)` |
| `pbc #n M` | Broadcast | `WorldState::logBroadcast(n, msg)` |
| `pic X Y L #n...` | Incantation start | `WorldState::startIncantation(...)` |
| `pie X Y R` | Incantation end | `WorldState::endIncantation(x, y, success)` |
| `pfk #n` | Egg laid | `WorldState::addEgg(n, ...)` |
| `pdr #n i` | Resource dropped | `WorldState::playerDropResource(n, i)` |
| `pgt #n i` | Resource collected | `WorldState::playerCollectResource(n, i)` |
| `pdi #n` | Player death | `WorldState::removePlayer(n)` |
| `enw #e #n X Y` | Egg spawned | `WorldState::updateEggPos(e, n, x, y)` |
| `ebo #e` | Egg hatched | `WorldState::hatchEgg(e)` |
| `edi #e` | Egg died | `WorldState::removeEgg(e)` |
| `sgt T` | Time unit | `WorldState::setTimeUnit(t)` |
| `seg N` | Game over | `WorldState::setGameOver(team)` |
| `smg M` | Server message | logged to spdlog |
| `suc` | Unknown command | logged to spdlog |
| `sbp` | Bad parameter | logged to spdlog |

The methods listed in the Handler column are not yet implemented — this table is the
design contract for the WorldState feature.

---

## Shader Compilation Pipeline

Vulkan does not accept GLSL source code at runtime. It requires SPIR-V binaries.

```
src/shaders/world.vert   ──┐
src/shaders/world.frag   ──┤  CMake custom_command: glslc → .spv
src/shaders/ui.vert      ──┤  Output: build/debug/shaders/*.spv
src/shaders/ui.frag      ──┘         (build artifact, never committed)
                                          │
                                          │  loaded at runtime via
                                          ▼  SHADERS_DIR compile definition
                             Renderer::loadShader("world.vert.spv")
```

The `shaders` CMake custom target is a build-order dependency of `zappy_gui`.
This guarantees the `.spv` files exist before the executable runs — the build will
fail rather than produce a binary that crashes on missing shaders.

The `SHADERS_DIR` compile definition expands to the absolute path of the shader output
directory at build time. This means the binary always finds its shaders regardless of
where it is launched from.

---

## Dependency Download Map

All dependencies are downloaded by CMake at configure time via `FetchContent`.
After the first configure, they are cached in `build/debug/_deps/` and not re-downloaded.
`fclean` (or `rm -rf build/`) deletes the cache — the next configure will re-download.

| Library | Cached at | Import target |
|---------|-----------|---------------|
| vk-bootstrap | `build/debug/_deps/vk-bootstrap-src/` | `vk-bootstrap::vk-bootstrap` |
| VulkanMemoryAllocator | `build/debug/_deps/vulkanmemoryallocator-src/` | `GPUOpen::VulkanMemoryAllocator` |
| glm | `build/debug/_deps/glm-src/` | `glm::glm` |
| imgui | `build/debug/_deps/imgui-src/` | `imgui` (compiled in CMakeLists) |
| tinyobjloader | `build/debug/_deps/tinyobjloader-src/` | `tinyobjloader` |
| stb | `build/debug/_deps/stb-src/` | `stb_headers` (INTERFACE) |
| spdlog | `build/debug/_deps/spdlog-src/` | `spdlog::spdlog` |
