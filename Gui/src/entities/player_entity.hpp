/**
 * @file entities/player_entity.hpp
 * @brief Visual representation of one live player on the map.
 * @details PlayerEntity is a thin logic layer over a scene
 *          (scenes/entities/player_entity.tscn) that provides the body mesh,
 *          a direction indicator, and a selection area. EntityManager creates
 *          one instance per pnw and drives it from World's signals.
 */

#pragma once

#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/// One player's avatar: capsule body (team-colored), direction indicator,
/// and a selection Area3D. Position/orientation/level/incanting state are
/// driven by EntityManager in response to World's signals.
class PlayerEntity : public Node3D {
    GDCLASS(PlayerEntity, Node3D)

private:
    MeshInstance3D* _body = nullptr;             ///< "Body" child, fetched in _ready().
    MeshInstance3D* _directionIndicator = nullptr; ///< "DirectionIndicator" child, fetched in _ready().
    Area3D* _selectionArea = nullptr;            ///< "SelectionArea" child, fetched in _ready().

    Ref<StandardMaterial3D> _bodyMaterial; ///< Per-instance team-color material applied to _body.
    Ref<Tween> _activeTween;               ///< Tween driving the current move_to(), if any.

    int _playerId = 0; ///< Exported property: server-assigned player id.

    bool _hasPosition = false; ///< False until the first update_position() call.
    int  _gridX = 0;           ///< Tile column of the last update_position() call.
    int  _gridY = 0;           ///< Tile row of the last update_position() call.

protected:
    /// Bind methods and properties exposed to Godot/GDScript.
    static void _bind_methods();

public:
    /// Fetch child nodes by name and install the per-instance body material.
    void _ready() override;

    /// Set the exported player_id property.
    void set_player_id(int id);
    /// Get the exported player_id property.
    int get_player_id() const;

    /// Set the body material's albedo to the given team color.
    void set_team_color(const Color& color);
    /// Get the body material's current albedo.
    Color get_team_color() const;

    /// Set this node's Y rotation from a protocol orientation (1=N, 2=E, 3=S, 4=W).
    void set_orientation(int orientation);

    /// Scale the whole node based on incantation level (1-8).
    void set_level(int level);

    /// Move to world_pos: instant on the first call or when (grid_x, grid_y) wraps
    /// across the map edge (|dx|>1 or |dy|>1), otherwise a smooth tween.
    void update_position(const Vector3& world_pos, int grid_x, int grid_y);

    /// Tween position to pos over duration seconds, killing any previous tween.
    void move_to(const Vector3& pos, float duration);

    /// Toggle the emissive highlight used while this player is incanting.
    void set_incanting(bool incanting);

    /// Get the tile column from the last update_position() call.
    int get_grid_x() const;
    /// Get the tile row from the last update_position() call.
    int get_grid_y() const;
};

} // namespace godot
