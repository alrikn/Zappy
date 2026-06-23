/**
 * @file world/world_state.cpp
 * @brief Implementation of WorldState: apply() dispatcher and all on*() message handlers.
 * @details Ported from the Vulkan client's WorldState, minus the mutex/snapshot (this
 *          version is single-threaded, owned directly by ZappyWorld).
 *
 *          Error policy in apply(): silently ignore any invalid message (unknown IDs,
 *          out-of-bounds coordinates). The server is external; messages may arrive out
 *          of order during bootstrap. We never abort from the message path.
 */

#include "world/world_state.hpp"

#include <utility>

namespace zappy {

namespace {

/// Merges multiple callable types into one — used with std::visit so the compiler
/// picks the correct overload based on the active ServerMessage alternative.
template<typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

} // namespace

void WorldState::apply(const ServerMessage& msg)
{
    std::visit(overloaded{
        [this](const MsgMapSize& m)         { onMapSize(m); },
        [this](const MsgTileContent& m)     { onTileContent(m); },
        [this](const MsgTeamName& m)        { onTeamName(m); },
        [this](const MsgPlayerNew& m)       { onPlayerNew(m); },
        [this](const MsgPlayerPosition& m)  { onPlayerPosition(m); },
        [this](const MsgPlayerLevel& m)     { onPlayerLevel(m); },
        [this](const MsgPlayerInventory& m) { onPlayerInventory(m); },
        [](const MsgPlayerExpulsion&) {
            // Expulsion has no persistent world-state effect — only a visual effect.
            // The expelled player's position will be updated by the following ppo message.
        },
        [](const MsgPlayerBroadcast&) {
            // Broadcasts are ephemeral events shown in the UI overlay, not stored state.
        },
        [this](const MsgPlayerDeath& m)      { onPlayerDeath(m); },
        [this](const MsgIncantationStart& m) { onIncantationStart(m); },
        [this](const MsgIncantationEnd& m)   { onIncantationEnd(m); },
        [](const MsgEggLaying&) {
            // pfk only signals that the laying action has begun; enw carries the
            // actual egg data. Nothing to store here.
        },
        [](const MsgResourceDrop&) {
            // Resource drops update the tile via bct, which arrives separately.
        },
        [](const MsgResourceCollect&) {
            // Resource collections update the tile via bct, which arrives separately.
        },
        [this](const MsgEggLaid& m)       { onEggLaid(m); },
        [this](const MsgEggConnection& m) { onEggConnection(m); },
        [this](const MsgEggDeath& m)      { onEggDeath(m); },
        [this](const MsgTimeUnit& m)      { onTimeUnit(m); },
        [this](const MsgTimeUnitSet& m)   { onTimeUnitSet(m); },
        [this](const MsgEndGame& m)       { onEndGame(m); },
        [](const MsgServerMessage&) {
            // Informational text — surfaced as a signal by ZappyWorld, no stored state.
        },
        [](const MsgUnknownCommand&) {
            // Server reported an unrecognised command — no stored state.
        },
        [](const MsgBadParameter&) {
            // Server reported a bad parameter — no stored state.
        },
    }, msg);
}

void WorldState::reset() noexcept
{
    _width  = 0;
    _height = 0;
    _tiles.clear();
    _players.clear();
    _eggs.clear();
    _teams.clear();
    _timeUnit    = 0;
    _gameOver    = false;
    _winningTeam.clear();
}

std::optional<std::reference_wrapper<const world_types::Player>> WorldState::find_player(uint32_t id) const noexcept
{
    const auto it = _players.find(id);
    if (it == _players.end()) {
        return std::nullopt;
    }
    return std::cref(it->second);
}

/**
 * @brief Resize the tile grid to the dimensions specified by the server.
 * @details The server sends "msz" once at bootstrap. If we receive it again (unlikely
 *          but possible on reconnect) we resize and zero-fill the entire grid. Any tile
 *          content received before msz will be ignored by onTileContent().
 */
void WorldState::onMapSize(const MsgMapSize& m)
{
    _width  = m.x;
    _height = m.y;
    _tiles.assign(static_cast<std::size_t>(_width) * _height, world_types::Tile{});
}

/**
 * @brief Update the resource counts on one tile.
 * @details If the map has not been sized yet, or the coordinates are out of bounds,
 *          the message is silently ignored. The server may send bct before msz during
 *          initial bootstrap; we accept that gracefully.
 */
void WorldState::onTileContent(const MsgTileContent& m)
{
    if (_width == 0 || _height == 0) {
        return;
    }
    if (m.x >= _width || m.y >= _height) {
        return;
    }
    const std::size_t idx = static_cast<std::size_t>(m.y) * _width + m.x;
    _tiles[idx].resources = m.resources;
}

/**
 * @brief Append a team name to the known-teams list.
 * @details Duplicate names are silently ignored.
 */
void WorldState::onTeamName(const MsgTeamName& m)
{
    for (const auto& existing : _teams) {
        if (existing == m.name) {
            return;
        }
    }
    _teams.push_back(m.name);
}

/**
 * @brief Insert a newly connected player into the players map.
 * @details If a player with the same ID already exists, the old entry is overwritten.
 */
void WorldState::onPlayerNew(const MsgPlayerNew& m)
{
    world_types::Player p;
    p.id          = m.id;
    p.x           = m.x;
    p.y           = m.y;
    p.orientation = static_cast<world_types::Orientation>(m.orientation);
    p.level       = m.level;
    p.team        = m.team;
    _players[m.id] = std::move(p);
}

/**
 * @brief Update an existing player's position and orientation.
 * @details If the player ID is unknown (e.g. ppo arrived before pnw), the message is
 *          silently ignored.
 */
void WorldState::onPlayerPosition(const MsgPlayerPosition& m)
{
    const auto it = _players.find(m.id);
    if (it == _players.end()) {
        return;
    }
    it->second.x           = m.x;
    it->second.y           = m.y;
    it->second.orientation = static_cast<world_types::Orientation>(m.orientation);
}

/**
 * @brief Update an existing player's incantation level.
 * @details If the player ID is unknown, the message is silently ignored.
 */
void WorldState::onPlayerLevel(const MsgPlayerLevel& m)
{
    const auto it = _players.find(m.id);
    if (it == _players.end()) {
        return;
    }
    it->second.level = m.level;
}

/**
 * @brief Overwrite a player's entire inventory with the server snapshot.
 * @details The server sends "pin" after every resource pick-up or drop. We replace
 *          the entire inventory array. pin also carries position; update it as a
 *          convenient side-effect.
 */
void WorldState::onPlayerInventory(const MsgPlayerInventory& m)
{
    const auto it = _players.find(m.id);
    if (it == _players.end()) {
        return;
    }
    it->second.x         = m.x;
    it->second.y         = m.y;
    it->second.inventory = m.resources;
}

/**
 * @brief Remove a dead player from the players map.
 * @details Erasing from an unordered_map with an unknown key is a no-op.
 */
void WorldState::onPlayerDeath(const MsgPlayerDeath& m)
{
    _players.erase(m.id);
}

/**
 * @brief Set the incanting flag on all players participating in a ritual.
 * @details Players whose IDs are not in _players (arrived before pnw) are silently
 *          skipped — the flag defaults to false anyway.
 */
void WorldState::onIncantationStart(const MsgIncantationStart& m)
{
    for (const uint32_t pid : m.players) {
        const auto it = _players.find(pid);
        if (it != _players.end()) {
            it->second.incanting = true;
        }
    }
}

/**
 * @brief Clear the incanting flag on every player standing on the ritual tile.
 * @details We clear by position rather than by ID list (the pie message does not
 *          include a participant list). Any player whose (x, y) matches the tile
 *          has their flag cleared regardless of whether they were in the pic list.
 */
void WorldState::onIncantationEnd(const MsgIncantationEnd& m)
{
    for (auto& [id, player] : _players) {
        if (player.x == m.x && player.y == m.y) {
            player.incanting = false;
        }
    }
}

/**
 * @brief Insert a newly laid egg into the eggs map.
 * @details Duplicate egg IDs are overwritten.
 */
void WorldState::onEggLaid(const MsgEggLaid& m)
{
    world_types::Egg egg;
    egg.eggId    = m.eggId;
    egg.playerId = m.playerId;
    egg.x        = m.x;
    egg.y        = m.y;
    _eggs[m.eggId] = egg;
}

/**
 * @brief Remove an egg that was consumed by a new player connection (ebo).
 * @details If the egg ID is unknown, this is a no-op.
 */
void WorldState::onEggConnection(const MsgEggConnection& m)
{
    _eggs.erase(m.eggId);
}

/**
 * @brief Remove an egg that died (timed out) before hatching.
 * @details If the egg ID is unknown, this is a no-op.
 */
void WorldState::onEggDeath(const MsgEggDeath& m)
{
    _eggs.erase(m.eggId);
}

/**
 * @brief Store the server's current time unit (response to sgt query).
 * @details Also updated by sst (time unit change). Both messages carry a single
 *          uint32_t and both mean "the current time unit is now t".
 */
void WorldState::onTimeUnit(const MsgTimeUnit& m)
{
    _timeUnit = m.t;
}

/**
 * @brief Store an updated time unit after a set request was acknowledged.
 */
void WorldState::onTimeUnitSet(const MsgTimeUnitSet& m)
{
    _timeUnit = m.t;
}

/**
 * @brief Record end-of-game state and the name of the winning team.
 */
void WorldState::onEndGame(const MsgEndGame& m)
{
    _gameOver    = true;
    _winningTeam = m.team;
}

} // namespace zappy
