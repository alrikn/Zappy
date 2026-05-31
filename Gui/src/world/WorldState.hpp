/**
 * @file world/WorldState.hpp
 * @brief Authoritative mutable game state for the Trantor world.
 * @details Responsibility: hold the live, up-to-date representation of everything the
 *          server has told us — map dimensions, per-tile resource counts, live players,
 *          eggs, known team names, the current time unit, and the game-over flag.
 *
 *          Architecture:
 *          - The main thread calls apply(msg) once per ServerMessage drained from the
 *            NetworkClient queue. apply() acquires _mutex, dispatches via std::visit to
 *            the matching on*() private handler, and releases the lock.
 *          - The renderer (currently main thread; later a dedicated thread) calls
 *            snapshot() which acquires _mutex, copies all state into a FrameData value,
 *            and returns it. The Renderer owns that copy with no lock held.
 *
 *          This file and every file under world/ must have zero Vulkan includes.
 *          World state is pure game data — it knows nothing about rendering.
 */

#pragma once

#include "world/FrameData.hpp"
#include "network/ServerMessage.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Mutable game state updated from the server message stream.
 * @details Lifetime: created once in main() via std::make_unique<WorldState>().
 *          Owned exclusively by main(). Not copyable — there is exactly one authoritative
 *          world state per process. Not movable — std::mutex cannot be moved after
 *          construction, and moving would violate the ownership guarantee.
 *
 *          Thread-safety: apply() and snapshot() are both protected by _mutex.
 *          All other members are private and never accessed without the lock.
 */
class WorldState {
public:
    /**
     * @brief Construct an empty world — dimensions and content unknown until msz/bct arrive.
     * @details All containers are empty; _width and _height are zero until onMapSize() runs.
     */
    WorldState() = default;

    /** @brief Default destructor. All members clean up themselves via RAII. */
    ~WorldState() = default;

    /// Non-copyable: std::mutex cannot be copied, and there must be exactly one WorldState.
    WorldState(const WorldState&)            = delete; ///< Non-copyable.
    WorldState& operator=(const WorldState&) = delete; ///< Non-copyable.

    /// Non-movable: std::mutex cannot be moved after first use; ownership must not transfer.
    WorldState(WorldState&&)            = delete; ///< Non-movable.
    WorldState& operator=(WorldState&&) = delete; ///< Non-movable.

    /**
     * @brief Apply one server message to the world state.
     * @details Acquires _mutex, then uses std::visit to call the matching on*() private
     *          handler based on which type is stored in the ServerMessage variant.
     *          Invalid messages (unknown player IDs, out-of-bounds coordinates) are
     *          logged and silently ignored — they never throw.
     *
     * @param msg A fully-parsed ServerMessage value from NetworkClient::poll().
     */
    void apply(const ServerMessage& msg);

    /**
     * @brief Return a complete, independent copy of the current world state.
     * @details Acquires _mutex for the duration of the copy, then releases it.
     *          The caller receives total ownership of the returned FrameData with no
     *          locks held. Safe to call from any thread.
     *
     * @return A FrameData value containing all tiles, players, eggs, teams, and metadata.
     */
    [[nodiscard]] FrameData snapshot() const;

private:
    /// Mutex protecting all private members below.
    /// Declared mutable so snapshot() const can still acquire it.
    mutable std::mutex _mutex;

    uint32_t          _width{0};  ///< Map width in tiles; set by onMapSize().
    uint32_t          _height{0}; ///< Map height in tiles; set by onMapSize().
    std::vector<Tile> _tiles;     ///< Flat tile grid; size = _width * _height after msz.

    /// Live players keyed by their numeric ID.
    std::unordered_map<uint32_t, Player> _players;

    /// Eggs currently on the map, keyed by egg ID.
    std::unordered_map<uint32_t, Egg> _eggs;

    /// Team names in the order received from tna messages.
    std::vector<std::string> _teams;

    uint32_t    _timeUnit{0};    ///< Current server time unit.
    bool        _gameOver{false}; ///< True after seg is received.
    std::string _winningTeam;    ///< Winning team name; non-empty when _gameOver is true.

    /// Private message handlers — each called by apply() while _mutex is held.
    /// They must never acquire _mutex themselves (that would deadlock).

    /**
     * @brief Handle "msz" — resize the tile grid to the given dimensions.
     * @param m The parsed MsgMapSize containing new width and height.
     */
    void onMapSize(const MsgMapSize& m);

    /**
     * @brief Handle "bct"/"mct" — update resource counts on one tile.
     * @param m The parsed MsgTileContent.
     */
    void onTileContent(const MsgTileContent& m);

    /**
     * @brief Handle "tna" — record one team name.
     * @param m The parsed MsgTeamName.
     */
    void onTeamName(const MsgTeamName& m);

    /**
     * @brief Handle "pnw" — insert a newly connected player.
     * @param m The parsed MsgPlayerNew.
     */
    void onPlayerNew(const MsgPlayerNew& m);

    /**
     * @brief Handle "ppo" — update a player's position and orientation.
     * @param m The parsed MsgPlayerPosition.
     */
    void onPlayerPosition(const MsgPlayerPosition& m);

    /**
     * @brief Handle "plv" — update a player's incantation level.
     * @param m The parsed MsgPlayerLevel.
     */
    void onPlayerLevel(const MsgPlayerLevel& m);

    /**
     * @brief Handle "pin" — overwrite a player's inventory snapshot.
     * @param m The parsed MsgPlayerInventory.
     */
    void onPlayerInventory(const MsgPlayerInventory& m);

    /**
     * @brief Handle "pdi" — remove a player who has died.
     * @param m The parsed MsgPlayerDeath.
     */
    void onPlayerDeath(const MsgPlayerDeath& m);

    /**
     * @brief Handle "pic" — mark all participating players as currently incanting.
     * @param m The parsed MsgIncantationStart containing the participant list.
     */
    void onIncantationStart(const MsgIncantationStart& m);

    /**
     * @brief Handle "pie" — clear the incanting flag on all players at the given tile.
     * @param m The parsed MsgIncantationEnd.
     */
    void onIncantationEnd(const MsgIncantationEnd& m);

    /**
     * @brief Handle "enw" — record a newly laid egg.
     * @param m The parsed MsgEggLaid.
     */
    void onEggLaid(const MsgEggLaid& m);

    /**
     * @brief Handle "ebo" — remove an egg that hatched into a new player connection.
     * @param m The parsed MsgEggConnection.
     */
    void onEggConnection(const MsgEggConnection& m);

    /**
     * @brief Handle "edi" — remove an egg that died (timed out).
     * @param m The parsed MsgEggDeath.
     */
    void onEggDeath(const MsgEggDeath& m);

    /**
     * @brief Handle "sgt" — store the server's current time unit.
     * @param m The parsed MsgTimeUnit.
     */
    void onTimeUnit(const MsgTimeUnit& m);

    /**
     * @brief Handle "sst" — update the time unit after a set request.
     * @param m The parsed MsgTimeUnitSet.
     */
    void onTimeUnitSet(const MsgTimeUnitSet& m);

    /**
     * @brief Handle "seg" — record end-of-game and the winning team name.
     * @param m The parsed MsgEndGame.
     */
    void onEndGame(const MsgEndGame& m);
};
