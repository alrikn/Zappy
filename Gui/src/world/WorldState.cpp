/**
 * @file world/WorldState.cpp
 * @brief Implementation of WorldState: apply() dispatcher and all on*() message handlers.
 * @details Responsibility: interpret every incoming ServerMessage and mutate the internal
 *          game-state fields accordingly. Also implements snapshot() which atomically
 *          copies all fields into a FrameData value for the renderer.
 *
 *          Architecture:
 *          apply() is the single public entry point. It acquires _mutex, then calls
 *          std::visit with an overload set (a lambda that calls the right on*() method
 *          based on the variant's active type). Each on*() method runs while the lock
 *          is already held — they must never try to re-acquire _mutex.
 *
 *          snapshot() acquires _mutex, copies all fields, and returns by value. The
 *          caller receives total ownership with no lock held afterward.
 *
 *          Error policy in apply(): log-and-ignore for any invalid message (unknown IDs,
 *          out-of-bounds coordinates). The server is external; messages may arrive out
 *          of order during bootstrap. We never throw from the message path.
 *
 *          Known limitation: messages for unknown IDs (e.g. ppo for a player whose pnw
 *          has not arrived yet) are permanently discarded, which can leave the world state
 *          silently wrong when messages arrive out of order. A proper fix would be a
 *          deferred retry queue: buffer unresolvable messages for a few frames and
 *          re-apply them once the referenced entity exists. (TODO: create issue)
 */

#include "world/WorldState.hpp"

#include <mutex>

#include <spdlog/spdlog.h>

/**
 * @brief Variadic template helper that merges multiple callable types into one.
 * @details The `overloaded` struct publicly inherits from every type in the
 *          parameter pack `Ts...` and brings all of their `operator()` overloads
 *          into scope with a single `using` declaration. When used with
 *          `std::visit`, the compiler selects the correct overload based on the
 *          active alternative in the variant — at compile time, with no runtime
 *          branching overhead and no `if constexpr` chains.
 *
 *          This is a C++17 pattern. The deduction guide below lets you write
 *          `overloaded{ lambda1, lambda2, ... }` without naming the template
 *          arguments explicitly — the compiler deduces `Ts...` from the
 *          arguments to the brace-initialiser.
 *
 *          Lifetime: instantiated inline inside apply() as a temporary; the
 *          compiler normally elides the copy entirely.
 *
 * @tparam Ts  Each type must be a callable (typically a lambda or function object).
 */
template<typename... Ts>
struct overloaded : Ts... {
    /// Bring every operator() from every base class into the same overload set.
    /// Without this, a call would be ambiguous when two bases both define
    /// operator() — using Ts::operator()... resolves that by unambiguously
    /// exposing all of them.
    using Ts::operator()...;
};

/**
 * @brief C++17 class-template-argument-deduction (CTAD) guide for overloaded.
 * @details Tells the compiler: when you see overloaded{a, b, c}, deduce
 *          Ts = {A, B, C} from the argument types automatically.
 *          Without this guide you would have to write overloaded<A,B,C>{a,b,c}
 *          by hand, naming every lambda type explicitly.
 */
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/**
 * @brief Dispatch one server message to the correct on*() handler.
 * @details Acquires _mutex via std::lock_guard (RAII), then calls std::visit with
 *          an overloaded visitor. std::visit examines which concrete alternative is
 *          stored in the ServerMessage variant and invokes the matching lambda from
 *          the overloaded set — the dispatch is resolved entirely at compile time
 *          via C++ overload resolution, not a runtime if/else chain.
 *
 *          Each lambda in the overloaded set either calls a dedicated on*() handler
 *          or performs a log-and-ignore for message types that carry no persistent
 *          world-state (expulsions, broadcasts, resource pick-up/drop signals, and
 *          protocol-error markers). Every one of the 24 alternatives in ServerMessage
 *          is handled by a named lambda — there is no catch-all; the compiler enforces
 *          exhaustiveness and would reject a build if any alternative were missing.
 *
 *          std::lock_guard is RAII: the mutex is acquired on construction and
 *          released automatically when the guard goes out of scope — even if an
 *          exception propagates out of a handler.
 */
void WorldState::apply(const ServerMessage& msg)
{
    // std::lock_guard<std::mutex>: acquires _mutex on construction and releases it
    // on destruction (when this function returns or unwinds). RAII ensures the mutex
    // is never left locked even if a handler throws.
    const std::lock_guard<std::mutex> lock(_mutex);

    // std::visit: given a std::variant, calls the provided callable with whichever
    // concrete alternative is currently active in the variant.
    // overloaded{...}: merges all the lambdas below into one callable with one
    // operator() per message type. The compiler picks the correct overload based
    // on the active variant alternative — zero runtime branches.
    std::visit(overloaded{
        [this](const MsgMapSize& m)           { onMapSize(m); },
        [this](const MsgTileContent& m)        { onTileContent(m); },
        [this](const MsgTeamName& m)           { onTeamName(m); },
        [this](const MsgPlayerNew& m)          { onPlayerNew(m); },
        [this](const MsgPlayerPosition& m)     { onPlayerPosition(m); },
        [this](const MsgPlayerLevel& m)        { onPlayerLevel(m); },
        [this](const MsgPlayerInventory& m)    { onPlayerInventory(m); },
        [](const MsgPlayerExpulsion& m) {
            // Expulsion has no persistent world-state effect — only a visual effect.
            // The expelled player's position will be updated by the following ppo message.
            spdlog::debug("WorldState: player {} expelled", m.id);
        },
        [](const MsgPlayerBroadcast& m) {
            // Broadcasts are ephemeral events shown in the UI overlay, not stored state.
            spdlog::debug("WorldState: player {} broadcast: {}", m.id, m.message);
        },
        [this](const MsgPlayerDeath& m)        { onPlayerDeath(m); },
        [this](const MsgIncantationStart& m)   { onIncantationStart(m); },
        [this](const MsgIncantationEnd& m)     { onIncantationEnd(m); },
        [](const MsgEggLaying& m) {
            // pfk only signals that the laying action has begun; enw carries the
            // actual egg data. Nothing to store here.
            spdlog::debug("WorldState: player {} started laying", m.playerId);
        },
        [](const MsgResourceDrop& m) {
            // Resource drops update the tile via bct which arrives separately.
            spdlog::debug("WorldState: player {} dropped resource {}", m.playerId, m.resource);
        },
        [](const MsgResourceCollect& m) {
            // Resource collections update the tile via bct which arrives separately.
            spdlog::debug("WorldState: player {} collected resource {}", m.playerId, m.resource);
        },
        [this](const MsgEggLaid& m)            { onEggLaid(m); },
        [this](const MsgEggConnection& m)      { onEggConnection(m); },
        [this](const MsgEggDeath& m)           { onEggDeath(m); },
        [this](const MsgTimeUnit& m)           { onTimeUnit(m); },
        [this](const MsgTimeUnitSet& m)        { onTimeUnitSet(m); },
        [this](const MsgEndGame& m)            { onEndGame(m); },
        [](const MsgServerMessage& m) {
            spdlog::info("WorldState: server message: {}", m.message);
        },
        [](const MsgUnknownCommand&) {
            spdlog::warn("WorldState: server reported unknown command");
        },
        [](const MsgBadParameter&) {
            spdlog::warn("WorldState: server reported bad parameter");
        },
    }, msg);
}

/**
 * @brief Copy all current state into a FrameData value and return it.
 * @details Acquires _mutex for the duration of the copy. The caller receives total
 *          ownership of the returned struct.
 *
 * @return A FrameData snapshot of the entire world at this instant.
 */
FrameData WorldState::snapshot() const
{
    // Acquire the mutex while copying. mutable _mutex allows this in a const method.
    const std::lock_guard<std::mutex> lock(_mutex);

    FrameData frame;
    frame.mapWidth    = _width;
    frame.mapHeight   = _height;
    frame.tiles       = _tiles;       // std::vector copy — O(n) where n = width * height
    frame.players     = _players;     // std::unordered_map copy — O(number of players)
    frame.eggs        = _eggs;        // std::unordered_map copy — O(number of eggs)
    frame.teams       = _teams;       // std::vector<string> copy
    frame.timeUnit    = _timeUnit;
    frame.gameOver    = _gameOver;
    frame.winningTeam = _winningTeam;
    return frame;
}

/**
 * @brief Resize the tile grid to the dimensions specified by the server.
 * @details The server sends "msz" once at bootstrap. If we receive it again (unlikely
 *          but possible on reconnect) we resize and zero-fill the entire grid.
 *          Any tile content received before msz will be ignored by onTileContent().
 */
void WorldState::onMapSize(const MsgMapSize& m)
{
    _width  = m.x;
    _height = m.y;
    // Resize the flat vector to width * height elements. Value-initialisation ({})
    // zero-fills all resource counts in every Tile.
    _tiles.assign(static_cast<std::size_t>(_width) * _height, Tile{});
    spdlog::info("WorldState: map size set to {}x{}", _width, _height);
}

/**
 * @brief Update the resource counts on one tile.
 * @details If the map has not been sized yet (_width == 0 or _height == 0), or if the
 *          coordinates are out of bounds, the message is logged and ignored. The server
 *          may send bct before msz during initial bootstrap; we accept that gracefully.
 */
void WorldState::onTileContent(const MsgTileContent& m)
{
    if (_width == 0 || _height == 0) {
        // Map not yet sized — cannot index tiles safely. Skip silently.
        spdlog::debug("WorldState: bct received before msz, discarding tile ({}, {})", m.x, m.y);
        return;
    }
    if (m.x >= _width || m.y >= _height) {
        spdlog::warn("WorldState: bct out of bounds ({},{}) for map {}x{}", m.x, m.y, _width, _height);
        return;
    }
    // Flat 2D indexing: row y starts at offset y * _width; column x is the offset within that row.
    const std::size_t idx = static_cast<std::size_t>(m.y) * _width + m.x;
    _tiles[idx].resources = m.resources;
}

/**
 * @brief Append a team name to the known-teams list.
 * @details Duplicate names are silently ignored — the server should not send the same
 *          team twice, but guard against it anyway.
 */
void WorldState::onTeamName(const MsgTeamName& m)
{
    for (const auto& existing : _teams) {
        if (existing == m.name) {
            spdlog::debug("WorldState: duplicate team name '{}' ignored", m.name);
            return;
        }
    }
    _teams.push_back(m.name);
    spdlog::debug("WorldState: team '{}' added", m.name);
}

/**
 * @brief Insert a newly connected player into the players map.
 * @details If a player with the same ID already exists (should not happen under
 *          normal server behaviour, but may happen on reconnect), the old entry is
 *          overwritten with a warning.
 */
void WorldState::onPlayerNew(const MsgPlayerNew& m)
{
    if (_players.count(m.id)) {
        spdlog::warn("WorldState: pnw for existing player id {}, overwriting", m.id);
    }
    Player p;
    p.id          = m.id;
    p.x           = m.x;
    p.y           = m.y;
    // Cast the raw uint8_t orientation value from the network message to the typed enum.
    p.orientation = static_cast<Orientation>(m.orientation);
    p.level       = m.level;
    p.team        = m.team;
    _players[m.id] = std::move(p);
    spdlog::debug("WorldState: player {} added at ({},{}) team '{}'", m.id, m.x, m.y, m.team);
}

/**
 * @brief Update an existing player's position and orientation.
 * @details If the player ID is unknown (e.g. ppo arrived before pnw), the message is
 *          logged as a warning and ignored.
 */
void WorldState::onPlayerPosition(const MsgPlayerPosition& m)
{
    const auto it = _players.find(m.id);
    if (it == _players.end()) {
        spdlog::warn("WorldState: ppo for unknown player id {}", m.id);
        return;
    }
    it->second.x           = m.x;
    it->second.y           = m.y;
    it->second.orientation = static_cast<Orientation>(m.orientation);
}

/**
 * @brief Update an existing player's incantation level.
 * @details If the player ID is unknown, the message is logged and ignored.
 */
void WorldState::onPlayerLevel(const MsgPlayerLevel& m)
{
    const auto it = _players.find(m.id);
    if (it == _players.end()) {
        spdlog::warn("WorldState: plv for unknown player id {}", m.id);
        return;
    }
    it->second.level = m.level;
    spdlog::debug("WorldState: player {} levelled to {}", m.id, m.level);
}

/**
 * @brief Overwrite a player's entire inventory with the server snapshot.
 * @details The server sends "pin" after every resource pick-up or drop. We replace
 *          the entire inventory array.
 */
void WorldState::onPlayerInventory(const MsgPlayerInventory& m)
{
    const auto it = _players.find(m.id);
    if (it == _players.end()) {
        spdlog::warn("WorldState: pin for unknown player id {}", m.id);
        return;
    }
    // The pin message also carries position; update it as a convenient side-effect.
    it->second.x         = m.x;
    it->second.y         = m.y;
    it->second.inventory = m.resources;
}

/**
 * @brief Remove a dead player from the players map.
 * @details Erasing from an unordered_map with an unknown key is a no-op, but we warn
 *          anyway for diagnostics.
 */
void WorldState::onPlayerDeath(const MsgPlayerDeath& m)
{
    const std::size_t removed = _players.erase(m.id);
    if (removed == 0) {
        spdlog::warn("WorldState: pdi for unknown player id {}", m.id);
    } else {
        spdlog::debug("WorldState: player {} removed (died)", m.id);
    }
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
    spdlog::debug("WorldState: incantation started at ({},{}) level {}, {} players",
        m.x, m.y, m.level, m.players.size());
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
    spdlog::debug("WorldState: incantation ended at ({},{}) result={}", m.x, m.y, m.result);
}

/**
 * @brief Insert a newly laid egg into the eggs map.
 * @details Duplicate egg IDs are overwritten with a warning.
 */
void WorldState::onEggLaid(const MsgEggLaid& m)
{
    if (_eggs.count(m.eggId)) {
        spdlog::warn("WorldState: enw for existing egg id {}, overwriting", m.eggId);
    }
    Egg egg;
    egg.eggId    = m.eggId;
    egg.playerId = m.playerId;
    egg.x        = m.x;
    egg.y        = m.y;
    _eggs[m.eggId] = egg;
    spdlog::debug("WorldState: egg {} laid by player {} at ({},{})", m.eggId, m.playerId, m.x, m.y);
}

/**
 * @brief Remove an egg that was consumed by a new player connection (ebo).
 * @details If the egg ID is unknown, we warn and do nothing.
 */
void WorldState::onEggConnection(const MsgEggConnection& m)
{
    const std::size_t removed = _eggs.erase(m.eggId);
    if (removed == 0) {
        spdlog::warn("WorldState: ebo for unknown egg id {}", m.eggId);
    } else {
        spdlog::debug("WorldState: egg {} hatched (player connected)", m.eggId);
    }
}

/**
 * @brief Remove an egg that died (timed out) before hatching.
 * @details If the egg ID is unknown, we warn and do nothing.
 */
void WorldState::onEggDeath(const MsgEggDeath& m)
{
    const std::size_t removed = _eggs.erase(m.eggId);
    if (removed == 0) {
        spdlog::warn("WorldState: edi for unknown egg id {}", m.eggId);
    } else {
        spdlog::debug("WorldState: egg {} died", m.eggId);
    }
}

/**
 * @brief Store the server's current time unit (response to sgt query).
 * @details Also updates on sst (time unit change). Both messages carry a single
 *          uint32_t and both mean "the current time unit is now t".
 */
void WorldState::onTimeUnit(const MsgTimeUnit& m)
{
    _timeUnit = m.t;
    spdlog::debug("WorldState: time unit set to {}", _timeUnit);
}

/**
 * @brief Store an updated time unit after a set request was acknowledged.
 * @details The server sends "sst t" after it has applied a time-unit change
 *          requested by the GUI. We update _timeUnit to match.
 */
void WorldState::onTimeUnitSet(const MsgTimeUnitSet& m)
{
    _timeUnit = m.t;
    spdlog::debug("WorldState: time unit updated to {}", _timeUnit);
}

/**
 * @brief Record end-of-game state and the name of the winning team.
 * @details After this, _gameOver is true and FrameData::gameOver will be true in
 *          every subsequent snapshot(). The renderer uses this flag to display a
 *          game-over screen.
 */
void WorldState::onEndGame(const MsgEndGame& m)
{
    _gameOver    = true;
    _winningTeam = m.team;
    spdlog::info("WorldState: game over — winning team '{}'", _winningTeam);
}
