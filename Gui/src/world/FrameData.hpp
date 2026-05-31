/**
 * @file world/FrameData.hpp
 * @brief Pure-data types shared by WorldState (internal state) and the Renderer (read-only snapshot).
 * @details Responsibility: define every value type that crosses the boundary between
 *          the game logic layer and the rendering layer — Orientation, Tile, Player,
 *          Egg, and the full-world FrameData snapshot.
 *
 *          Architecture: WorldState holds live copies of these types in its private
 *          members and populates a FrameData value inside snapshot(). The Renderer
 *          receives that FrameData by value and owns it for exactly one frame, with
 *          no locks held. This file has zero Vulkan includes: world data is rendering-agnostic.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Compass direction a player is facing.
 * @details Values match the Zappy GUI protocol exactly: 1=North, 2=East, 3=South, 4=West.
 *          Stored as uint8_t to minimise the size of Player structs copied into FrameData.
 *
 *          Lifetime: a plain enum value — no heap allocation, no ownership.
 */
enum class Orientation : uint8_t {
    North = 1, ///< Facing north.
    East  = 2, ///< Facing east.
    South = 3, ///< Facing south.
    West  = 4, ///< Facing west.
};

/**
 * @brief Contents of one map tile: the quantity of each of the 7 resource types.
 * @details The index order matches the protocol's bct/mct fields:
 *          resources[0]=food, [1]=linemate, [2]=deraumere, [3]=sibur,
 *          [4]=mendiane, [5]=phiras, [6]=thystame.
 *
 *          Tiles are stored in a flat std::vector<Tile> indexed by y * mapWidth + x.
 *          This layout is cache-friendly when the renderer iterates all tiles row by row.
 *
 *          Lifetime: value type — created on the stack or inside a vector. No heap
 *          allocation of its own; the enclosing vector manages the memory.
 */
struct Tile {
    /// Quantity of each resource present on this tile.
    /// Index 0=food, 1=linemate, 2=deraumere, 3=sibur, 4=mendiane, 5=phiras, 6=thystame.
    std::array<uint32_t, 7> resources{};
};

/**
 * @brief One live player currently on the Trantor map.
 * @details Updated by pnw (new player), ppo (move/turn), plv (level up),
 *          pin (inventory), pic (incantation start), pie (incantation end),
 *          and removed by pdi (death).
 *
 *          Lifetime: value stored in WorldState::_players keyed by player ID.
 *          Copied into FrameData::players by WorldState::snapshot().
 */
struct Player {
    uint32_t    id;                      ///< Unique player ID assigned by the server.
    uint32_t    x;                       ///< Current tile column (0-based).
    uint32_t    y;                       ///< Current tile row (0-based).
    Orientation orientation;             ///< Direction the player is facing.
    uint8_t     level;                   ///< Current incantation level (1–8).
    std::string team;                    ///< Name of the team this player belongs to.

    /// Inventory: same 7-slot layout as Tile::resources.
    std::array<uint32_t, 7> inventory{};

    bool incanting{false}; ///< True while this player is inside a pic/pie ritual.
};

/**
 * @brief One egg currently on the map, waiting to hatch.
 * @details Eggs are created by enw (egg laid) and removed by ebo (hatched) or edi (died).
 *
 *          Lifetime: value stored in WorldState::_eggs keyed by egg ID.
 *          Copied into FrameData::eggs by WorldState::snapshot().
 */
struct Egg {
    uint32_t eggId;    ///< Unique egg identifier assigned by the server.
    uint32_t playerId; ///< ID of the player that laid this egg.
    uint32_t x;        ///< Tile column where the egg sits.
    uint32_t y;        ///< Tile row where the egg sits.
};

/**
 * @brief Complete, immutable snapshot of the world state for one rendered frame.
 * @details Produced by WorldState::snapshot(), which holds the internal mutex only
 *          during the copy. The Renderer receives this by value and owns it entirely —
 *          no shared ownership, no reference counting, no lock held during the draw loop.
 *
 *          The tiles vector uses flat 2D indexing: tiles[y * mapWidth + x].
 *
 *          Lifetime: created in WorldState::snapshot() (stack + heap via vector/map
 *          constructors), moved/copied to the caller, then destroyed at the end of the
 *          frame loop iteration. Ownership always belongs to one thread at a time.
 */
struct FrameData {
    uint32_t mapWidth{0};  ///< Number of columns in the tile grid.
    uint32_t mapHeight{0}; ///< Number of rows in the tile grid.

    /// Flat tile grid. tiles[y * mapWidth + x] gives the tile at (x, y).
    std::vector<Tile> tiles;

    /// All live players, keyed by their numeric ID for O(1) lookup.
    std::unordered_map<uint32_t, Player> players;

    /// All eggs currently on the map, keyed by egg ID.
    std::unordered_map<uint32_t, Egg> eggs;

    /// Ordered list of team names received from the server via tna messages.
    std::vector<std::string> teams;

    uint32_t    timeUnit{0};    ///< Current server time unit (ticks per second).
    bool        gameOver{false}; ///< True after seg is received.
    std::string winningTeam;    ///< Name of the winning team, set when gameOver is true.
};
