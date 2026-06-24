/**
 * @file entities/entity_manager.hpp
 * @brief Spawns and updates PlayerEntity/EggEntity instances from World's signals.
 * @details EntityManager is a thin logic layer over a scene
 *          (scenes/world/entity_manager.tscn) with no children of its own: it
 *          instantiates player_entity_scene/egg_entity_scene on demand, adds
 *          them as children, and forwards World's per-entity signals to them.
 */

#pragma once

#include "entities/egg_entity.hpp"
#include "entities/player_entity.hpp"
#include "world/map_terrain.hpp"

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <array>
#include <unordered_map>
#include <vector>

namespace godot {

/// Owns the live PlayerEntity/EggEntity instances, keyed by server-assigned id,
/// and reacts to World's team_registered/player_*/egg_* signals to keep them
/// in sync. Assigns each team a stable color from an 8-entry palette in the
/// order teams are first seen.
class EntityManager : public Node3D {
    GDCLASS(EntityManager, Node3D)

private:
    std::unordered_map<int, PlayerEntity*> _players; ///< Live players, keyed by id.
    std::unordered_map<int, EggEntity*> _eggs;        ///< Live eggs, keyed by id.
    /// Player ids currently incanting, keyed by tile (packed as (x << 32) | (uint32)y).
    /// Recorded from on_incantation_started's id list so on_incantation_ended can clear
    /// set_incanting(false) for exactly those players, instead of re-deriving membership
    /// from each player's current tracked grid position (which can drift out of sync).
    std::unordered_map<int64_t, std::vector<int>> _incantingByTile;
    std::vector<String> _teamNames;                   ///< Teams in first-seen order, indexes into _palette.
    std::array<Color, 8> _palette;                    ///< Fixed team-color palette.

    MapTerrain* _mapTerrain = nullptr; ///< Resolved from map_terrain_path in _ready().
    NodePath _mapTerrainPath;          ///< Exported property: path to the MapTerrain node.

    Ref<PackedScene> _playerEntityScene; ///< Exported property: scene instantiated per player.
    Ref<PackedScene> _eggEntityScene;    ///< Exported property: scene instantiated per egg.

protected:
    /// Bind methods, properties and the fixed team-color palette.
    static void _bind_methods();

public:
    EntityManager();

    /// Resolve map_terrain_path.
    void _ready() override;

    /// Set the exported map_terrain_path property.
    void set_map_terrain_path(const NodePath& path);
    /// Get the exported map_terrain_path property.
    NodePath get_map_terrain_path() const;

    /// Set the exported player_entity_scene property.
    void set_player_entity_scene(const Ref<PackedScene>& scene);
    /// Get the exported player_entity_scene property.
    Ref<PackedScene> get_player_entity_scene() const;

    /// Set the exported egg_entity_scene property.
    void set_egg_entity_scene(const Ref<PackedScene>& scene);
    /// Get the exported egg_entity_scene property.
    Ref<PackedScene> get_egg_entity_scene() const;

    /// Find-or-append team in _teamNames and return its palette color.
    Color team_color(const String& team);

    /// World-space position where entities standing on tile (x, y) are placed:
    /// tile_to_world(x, y) raised slightly above the terrain surface.
    Vector3 entity_position(int x, int y) const;

    /// Fix a team's palette slot even before any of its players spawn.
    void on_team_registered(const String& name);

    /// Free every live player and egg and forget all team-color assignments.
    /// Called when a new connection starts, so a previous session's entities
    /// don't linger once their server-assigned ids are reused by a new game.
    void clear_all();

    /// Instantiate a PlayerEntity for a newly spawned player.
    void on_player_spawned(int id, int x, int y, int orientation, int level, const String& team);
    /// Move/turn an existing player.
    void on_player_moved(int id, int x, int y, int orientation);
    /// Update an existing player's level (and visual scale).
    void on_player_leveled(int id, int level);
    /// Remove a player that has died.
    void on_player_died(int id);

    /// Instantiate an EggEntity for a newly laid egg.
    void on_egg_laid(int egg_id, int player_id, int x, int y);
    /// Remove an egg that hatched or died.
    void on_egg_removed(int egg_id);

    /// Highlight all players participating in a new incantation.
    void on_incantation_started(int x, int y, int level, const PackedInt32Array& ids);
    /// Clear the incantation highlight for all players on tile (x, y).
    void on_incantation_ended(int x, int y, bool result);
};

} // namespace godot
