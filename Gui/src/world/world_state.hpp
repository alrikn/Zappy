/**
 * @file world/world_state.hpp
 * @brief Authoritative mutable game state for the Trantor world.
 * @details Holds the live representation of everything the server has told us — map
 *          dimensions, per-tile resource counts, live players, eggs, known team names,
 *          the current time unit, and the game-over flag.
 *
 *          Single-threaded: ZappyWorld::_process() drains messages from ZappyConnection
 *          and calls apply(msg) once per message, on the main thread. No locking, no
 *          snapshot — owned directly by ZappyWorld and queried via the accessors below.
 *
 *          This file and world_types.hpp have zero Godot includes — world state is
 *          rendering-agnostic.
 */

#pragma once

#include "network/server_message.hpp"
#include "world/world_types.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace zappy {

/**
 * @brief Mutable game state updated from the server message stream.
 * @details Lifetime: owned as a plain member of ZappyWorld. Not copyable/movable —
 *          there is exactly one world state per connection.
 */
class WorldState {
public:
    WorldState() = default;
    ~WorldState() = default;

    WorldState(const WorldState&)            = delete;
    WorldState& operator=(const WorldState&) = delete;
    WorldState(WorldState&&)                 = delete;
    WorldState& operator=(WorldState&&)      = delete;

    /**
     * @brief Apply one server message to the world state.
     * @details Dispatches via std::visit to the matching on*() private handler.
     *          Invalid messages (unknown player IDs, out-of-bounds coordinates) are
     *          silently ignored — they never abort or assert. The server is external;
     *          messages may arrive out of order during bootstrap.
     * @param msg A fully-parsed ServerMessage value drained from ZappyConnection.
     */
    void apply(const ServerMessage& msg);

    /**
     * @brief Discard all accumulated state, returning to the post-construction state.
     * @details Call before starting a new connection so a previous session's players,
     *          eggs, tiles, and team names don't linger once the new server's bootstrap
     *          messages start arriving with their own (likely overlapping) IDs.
     */
    void reset() noexcept;

    // ── Accessors ────────────────────────────────────────────────────────────
    [[nodiscard]] uint32_t width() const noexcept { return _width; }
    [[nodiscard]] uint32_t height() const noexcept { return _height; }
    [[nodiscard]] const std::vector<world_types::Tile>& tiles() const noexcept { return _tiles; }
    [[nodiscard]] const std::unordered_map<uint32_t, world_types::Player>& players() const noexcept { return _players; }
    [[nodiscard]] const std::unordered_map<uint32_t, world_types::Egg>& eggs() const noexcept { return _eggs; }
    [[nodiscard]] const std::vector<std::string>& teams() const noexcept { return _teams; }
    [[nodiscard]] uint32_t time_unit() const noexcept { return _timeUnit; }
    [[nodiscard]] bool is_game_over() const noexcept { return _gameOver; }
    [[nodiscard]] const std::string& winning_team() const noexcept { return _winningTeam; }

    /// Look up a single player by id. Returns nullopt if not found.
    [[nodiscard]] std::optional<std::reference_wrapper<const world_types::Player>> find_player(uint32_t id) const noexcept;

private:
    uint32_t _width{0};  ///< Map width in tiles; set by onMapSize().
    uint32_t _height{0}; ///< Map height in tiles; set by onMapSize().
    std::vector<world_types::Tile> _tiles; ///< Flat tile grid; size = _width * _height after msz.

    /// Live players keyed by their numeric ID.
    std::unordered_map<uint32_t, world_types::Player> _players;

    /// Eggs currently on the map, keyed by egg ID.
    std::unordered_map<uint32_t, world_types::Egg> _eggs;

    /// Team names in the order received from tna messages.
    std::vector<std::string> _teams;

    uint32_t    _timeUnit{0};     ///< Current server time unit.
    bool        _gameOver{false}; ///< True after seg is received.
    std::string _winningTeam;     ///< Winning team name; non-empty when _gameOver is true.

    /// Private message handlers — each called by apply() for the matching variant
    /// alternative. log-and-ignore policy from the original implementation becomes
    /// silently-ignore here (no logging facility at this layer).

    void onMapSize(const MsgMapSize& m);
    void onTileContent(const MsgTileContent& m);
    void onTeamName(const MsgTeamName& m);
    void onPlayerNew(const MsgPlayerNew& m);
    void onPlayerPosition(const MsgPlayerPosition& m);
    void onPlayerLevel(const MsgPlayerLevel& m);
    void onPlayerInventory(const MsgPlayerInventory& m);
    void onPlayerDeath(const MsgPlayerDeath& m);
    void onIncantationStart(const MsgIncantationStart& m);
    void onIncantationEnd(const MsgIncantationEnd& m);
    void onEggLaid(const MsgEggLaid& m);
    void onEggConnection(const MsgEggConnection& m);
    void onEggDeath(const MsgEggDeath& m);
    void onTimeUnit(const MsgTimeUnit& m);
    void onTimeUnitSet(const MsgTimeUnitSet& m);
    void onEndGame(const MsgEndGame& m);
};

} // namespace zappy
