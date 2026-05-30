/**
 * @file WorldState.hpp
 * @brief Authoritative game state as seen by the GUI.
 * @details Responsibility: store a consistent snapshot of the Trantor world map,
 *          all player positions and inventories, all eggs, and the current time unit.
 *          This data is updated by the network thread and read by the renderer thread.
 *
 *          HARD RULE: this file and every file under world/ must have ZERO Vulkan
 *          includes. World state is pure game data — it knows nothing about rendering.
 *
 *          Placeholder until the world feature is implemented. The class exists so
 *          main.cpp compiles; tiles, players, eggs and the mutex are added in the world feature.
 *
 *          Architecture position:
 *          NetworkClient parses protocol messages and calls WorldState::apply(message).
 *          WorldState::snapshot() returns an immutable FrameData copy that the
 *          Renderer uses for an entire frame without holding any lock.
 */

#pragma once

/**
 * @brief Mutable game state updated by the network thread.
 * @details Lifetime: created once in main(), passed by reference to both the network
 *          thread and the renderer. Protected by a mutex once the world feature is built.
 *
 *          Non-copyable: there is exactly one authoritative world state per process.
 */
class WorldState {
public:
    /**
     * @brief Default constructor — world dimensions unknown until the server sends the msz message.
     * @details All fields start empty.
     */
    WorldState() = default;

    ~WorldState() = default;

    WorldState(const WorldState&) = delete;             ///< Non-copyable.
    WorldState& operator=(const WorldState&) = delete;  ///< Non-copyable.

    /**
     * @brief Movable for potential future thread-wrapper use.
     * @details Protected by a mutex once the world feature is implemented.
     */
    WorldState(WorldState&&) = default;
    WorldState& operator=(WorldState&&) = default;
};
