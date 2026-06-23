/**
 * @file world/world_types.hpp
 * @brief Pure-data types describing the live world: Orientation, Tile, Player, Egg.
 * @details No Godot/rendering dependencies. Owned by WorldState and queried directly
 *          by ZappyWorld and later renderer/HUD nodes.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace zappy::world_types {

/**
 * @brief Compass direction a player is facing.
 * @details Values match the Zappy GUI protocol exactly: 1=North, 2=East, 3=South, 4=West.
 */
enum class Orientation : uint8_t {
    North = 1, ///< Facing north.
    East  = 2, ///< Facing east.
    South = 3, ///< Facing south.
    West  = 4, ///< Facing west.
};

/**
 * @brief Contents of one map tile: the quantity of each of the 7 resource types.
 * @details Index order matches the protocol's bct/mct fields:
 *          resources[0]=food, [1]=linemate, [2]=deraumere, [3]=sibur,
 *          [4]=mendiane, [5]=phiras, [6]=thystame.
 *
 *          Tiles are stored in a flat vector indexed by y * mapWidth + x.
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
 */
struct Player {
    uint32_t    id;          ///< Unique player ID assigned by the server.
    uint32_t    x;           ///< Current tile column (0-based).
    uint32_t    y;           ///< Current tile row (0-based).
    Orientation orientation; ///< Direction the player is facing.
    uint8_t     level;       ///< Current incantation level (1-8).
    std::string team;        ///< Name of the team this player belongs to.

    /// Inventory: same 7-slot layout as Tile::resources.
    std::array<uint32_t, 7> inventory{};

    bool incanting{false}; ///< True while this player is inside a pic/pie ritual.
};

/**
 * @brief One egg currently on the map, waiting to hatch.
 * @details Eggs are created by enw (egg laid) and removed by ebo (hatched) or edi (died).
 */
struct Egg {
    uint32_t eggId;    ///< Unique egg identifier assigned by the server.
    uint32_t playerId; ///< ID of the player that laid this egg.
    uint32_t x;        ///< Tile column where the egg sits.
    uint32_t y;        ///< Tile row where the egg sits.
};

} // namespace zappy::world_types
