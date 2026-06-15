/**
 * @file network/server_message.hpp
 * @brief Typed structs for every server-to-GUI protocol message, plus the ServerMessage variant.
 * @details One plain-data struct per message type described in the zappy GUI protocol
 *          specification. No parsing logic lives here — pure data definitions.
 *
 *          protocol_parser::parse_line() constructs one of these structs and wraps it in
 *          a ServerMessage variant value. ZappyWorld::_process() drains these from
 *          ZappyConnection and dispatches each to WorldState::apply() via std::visit.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace zappy {

/**
 * @brief Response to "msz" — total map dimensions in tiles.
 * @details Protocol line: "msz X Y"
 */
struct MsgMapSize {
    uint32_t x; ///< Map width in tiles.
    uint32_t y; ///< Map height in tiles.
};

/**
 * @brief Response to "bct" or "mct" — resource contents of one tile.
 * @details Protocol line: "bct X Y q0 q1 q2 q3 q4 q5 q6"
 *          resources[0] = food, [1]=linemate, [2]=deraumere, [3]=sibur,
 *          [4]=mendiane, [5]=phiras, [6]=thystame.
 */
struct MsgTileContent {
    uint32_t                x;         ///< Tile column.
    uint32_t                y;         ///< Tile row.
    std::array<uint32_t, 7> resources; ///< Quantity of each resource type.
};

/**
 * @brief Response to "tna" — one team name.
 * @details Protocol line: "tna TeamName". The server sends one line per team.
 */
struct MsgTeamName {
    std::string name; ///< Team name string.
};

/**
 * @brief New player connected or appeared — "pnw".
 * @details Protocol line: "pnw #id X Y orientation level TeamName"
 *          orientation: 1=North 2=East 3=South 4=West.
 */
struct MsgPlayerNew {
    uint32_t    id;          ///< Unique player number (stripped of '#').
    uint32_t    x;           ///< Tile column.
    uint32_t    y;           ///< Tile row.
    uint8_t     orientation; ///< Direction the player faces: 1=N, 2=E, 3=S, 4=W.
    uint8_t     level;       ///< Incantation level (1-8).
    std::string team;        ///< Team name this player belongs to.
};

/**
 * @brief Player moved or turned — "ppo".
 * @details Protocol line: "ppo #id X Y orientation"
 */
struct MsgPlayerPosition {
    uint32_t id;          ///< Player number.
    uint32_t x;           ///< New tile column.
    uint32_t y;           ///< New tile row.
    uint8_t  orientation; ///< New facing direction: 1=N, 2=E, 3=S, 4=W.
};

/**
 * @brief Player's incantation level changed — "plv".
 * @details Protocol line: "plv #id level"
 */
struct MsgPlayerLevel {
    uint32_t id;    ///< Player number.
    uint8_t  level; ///< New level (1-8).
};

/**
 * @brief Player inventory snapshot — "pin".
 * @details Protocol line: "pin #id X Y q0 q1 q2 q3 q4 q5 q6"
 *          resources[0]=food ... resources[6]=thystame (same order as MsgTileContent).
 */
struct MsgPlayerInventory {
    uint32_t                id;        ///< Player number.
    uint32_t                x;         ///< Current tile column.
    uint32_t                y;         ///< Current tile row.
    std::array<uint32_t, 7> resources; ///< Quantity of each resource carried.
};

/**
 * @brief Player expelled another player — "pex".
 * @details Protocol line: "pex #id"
 */
struct MsgPlayerExpulsion {
    uint32_t id; ///< ID of the player that performed the expulsion.
};

/**
 * @brief Player broadcast message — "pbc".
 * @details Protocol line: "pbc #id message text"
 *          The message may contain spaces; everything after the id token is the text.
 */
struct MsgPlayerBroadcast {
    uint32_t    id;      ///< Sending player's number.
    std::string message; ///< Broadcast text (may contain spaces).
};

/**
 * @brief Player died — "pdi".
 * @details Protocol line: "pdi #id"
 */
struct MsgPlayerDeath {
    uint32_t id; ///< ID of the deceased player.
};

/**
 * @brief Incantation ritual started — "pic".
 * @details Protocol line: "pic X Y level #id1 #id2 ..."
 *          players contains all participant IDs (including the initiating player).
 */
struct MsgIncantationStart {
    uint32_t              x;       ///< Tile column of the ritual.
    uint32_t              y;       ///< Tile row of the ritual.
    uint8_t               level;   ///< Incantation level being attempted.
    std::vector<uint32_t> players; ///< IDs of all participating players.
};

/**
 * @brief Incantation ritual ended — "pie".
 * @details Protocol line: "pie X Y result"
 *          result is "ok" (true) or "ko" (false).
 */
struct MsgIncantationEnd {
    uint32_t x;      ///< Tile column.
    uint32_t y;      ///< Tile row.
    bool     result; ///< true = incantation succeeded.
};

/**
 * @brief Player started laying an egg — "pfk".
 * @details Protocol line: "pfk #id"
 */
struct MsgEggLaying {
    uint32_t playerId; ///< ID of the laying player.
};

/**
 * @brief Player dropped a resource — "pdr".
 * @details Protocol line: "pdr #id resource"
 *          resource index: 0=food ... 6=thystame.
 */
struct MsgResourceDrop {
    uint32_t playerId; ///< ID of the dropping player.
    uint8_t  resource; ///< Resource index (0-6).
};

/**
 * @brief Player picked up a resource — "pgt".
 * @details Protocol line: "pgt #id resource"
 */
struct MsgResourceCollect {
    uint32_t playerId; ///< ID of the collecting player.
    uint8_t  resource; ///< Resource index (0-6).
};

/**
 * @brief An egg was laid at a position — "enw".
 * @details Protocol line: "enw #eggId #playerId X Y"
 */
struct MsgEggLaid {
    uint32_t eggId;    ///< Unique egg identifier.
    uint32_t playerId; ///< Player that laid this egg.
    uint32_t x;        ///< Tile column where the egg was laid.
    uint32_t y;        ///< Tile row where the egg was laid.
};

/**
 * @brief A player connected by hatching from an egg — "ebo".
 * @details Protocol line: "ebo #eggId"
 */
struct MsgEggConnection {
    uint32_t eggId; ///< ID of the hatched egg.
};

/**
 * @brief An egg died (timed out) — "edi".
 * @details Protocol line: "edi #eggId"
 */
struct MsgEggDeath {
    uint32_t eggId; ///< ID of the dead egg.
};

/**
 * @brief Current time unit query response — "sgt".
 * @details Protocol line: "sgt t"
 */
struct MsgTimeUnit {
    uint32_t t; ///< Time unit frequency (server ticks per second).
};

/**
 * @brief Time unit was changed — "sst".
 * @details Protocol line: "sst t". Sent by the server after the GUI calls "sst <t>".
 */
struct MsgTimeUnitSet {
    uint32_t t; ///< New time unit value.
};

/**
 * @brief End of game — a team has won — "seg".
 * @details Protocol line: "seg TeamName"
 */
struct MsgEndGame {
    std::string team; ///< Name of the winning team.
};

/**
 * @brief Generic server message — "smg".
 * @details Protocol line: "smg message text". Free-form informational string from the server.
 */
struct MsgServerMessage {
    std::string message; ///< Message content (may contain spaces).
};

/**
 * @brief Server did not recognise the command — "suc".
 * @details Protocol line: "suc"
 */
struct MsgUnknownCommand {};

/**
 * @brief Server reports a bad parameter — "sbp".
 * @details Protocol line: "sbp"
 */
struct MsgBadParameter {};

/**
 * @brief Type-safe union of all possible server-to-GUI messages.
 * @details Use std::visit to dispatch on the active type.
 */
using ServerMessage = std::variant<
    MsgMapSize,
    MsgTileContent,
    MsgTeamName,
    MsgPlayerNew,
    MsgPlayerPosition,
    MsgPlayerLevel,
    MsgPlayerInventory,
    MsgPlayerExpulsion,
    MsgPlayerBroadcast,
    MsgPlayerDeath,
    MsgIncantationStart,
    MsgIncantationEnd,
    MsgEggLaying,
    MsgResourceDrop,
    MsgResourceCollect,
    MsgEggLaid,
    MsgEggConnection,
    MsgEggDeath,
    MsgTimeUnit,
    MsgTimeUnitSet,
    MsgEndGame,
    MsgServerMessage,
    MsgUnknownCommand,
    MsgBadParameter
>;

} // namespace zappy
