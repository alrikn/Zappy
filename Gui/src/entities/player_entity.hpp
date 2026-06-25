/**
 * @file entities/player_entity.hpp
 * @brief Visual representation of one live player on the map.
 * @details PlayerEntity is a thin logic layer over a scene
 *          (scenes/entities/player_entity.tscn) that provides the body mesh,
 *          a direction indicator, and a selection area. EntityManager creates
 *          one instance per pnw and drives it from World's signals.
 */

#pragma once

#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

/// One player's avatar: a team-colored body (a bare mesh for the default
/// capsule, or an instanced animated character model), a direction indicator,
/// and a selection Area3D. Position/orientation/level/incanting state are
/// driven by EntityManager in response to World's signals.
///
/// The body is resolved generically in _ready(): every MeshInstance3D found
/// under "Body" (recursively, plus "Body" itself if it is one) gets the
/// team-color material override, and the first AnimationPlayer found under
/// "Body" (if any) is used to play idle_animation/walk_animation/etc. This
/// lets the same class work for both the plain capsule scene (no animations)
/// and an instanced animated character scene, without special-casing either.
class PlayerEntity : public Node3D {
    GDCLASS(PlayerEntity, Node3D)

private:
    Node3D* _bodyRoot = nullptr;                   ///< "Body" child, fetched in _ready().
    std::vector<MeshInstance3D*> _bodyMeshes;      ///< Every MeshInstance3D found under _bodyRoot.
    AnimationPlayer* _animationPlayer = nullptr;   ///< First AnimationPlayer found under _bodyRoot, if any.

    MeshInstance3D* _directionIndicator = nullptr; ///< "DirectionIndicator" child, fetched in _ready().
    Area3D* _selectionArea = nullptr;            ///< "SelectionArea" child, fetched in _ready().

    Ref<StandardMaterial3D> _bodyMaterial; ///< Per-instance team-color material applied to every body mesh.
    Ref<Tween> _activeTween;               ///< Tween driving the current move_to(), if any.

    int _playerId = 0; ///< Exported property: server-assigned player id.

    /// Exported properties: animation names to play on this character's AnimationPlayer.
    /// Empty by default (no-op) so the plain capsule scene, which has no AnimationPlayer,
    /// is unaffected. Each character scene fills these in with its own real clip names,
    /// since different models/asset sources rarely share identical animation names.
    String _idleAnimation;
    String _walkAnimation;
    String _incantAnimation;
    String _deathAnimation;

    /// Exported property: playback speed multiplier for walk_animation (default 1.0).
    /// Lets each character's run/walk cycle be tuned to match the fixed 0.3s
    /// tile-to-tile tween duration without recompiling.
    float _walkAnimationSpeed = 1.0f;

    /// What this entity is doing right now, for animation purposes. This is the single
    /// source of truth for which clip should be playing: every public method that can
    /// change it (update_position, move_to, set_incanting) just updates this and calls
    /// _refresh_animation() — no method decides "should I play idle/walk/incant?" on its
    /// own, so there's exactly one place a future state needs to be added to.
    enum class AnimState {
        Idle,
        Walking,
        Incanting,
    };
    AnimState _animState = AnimState::Idle;

    bool _hasPosition = false; ///< False until the first update_position() call.
    int  _gridX = 0;           ///< Tile column of the last update_position() call.
    int  _gridY = 0;           ///< Tile row of the last update_position() call.

    /// Play animation_name on _animationPlayer at the given speed if both are set
    /// and it isn't already playing. No-op otherwise.
    void _play_clip(const String& animation_name, float speed = 1.0f);
    /// Play whichever clip matches _animState. The only place that maps state to a clip.
    void _refresh_animation();
    /// Tween-finished callback: returns to the idle/incant animation after a move_to() completes.
    void _on_move_finished();
    /// AnimationPlayer-finished callback: frees this node once the death animation completes.
    void _on_death_animation_finished(const StringName& animation_name);

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

    /// Set the exported idle_animation property.
    void set_idle_animation(const String& name);
    /// Get the exported idle_animation property.
    String get_idle_animation() const;

    /// Set the exported walk_animation property.
    void set_walk_animation(const String& name);
    /// Get the exported walk_animation property.
    String get_walk_animation() const;

    /// Set the exported incant_animation property.
    void set_incant_animation(const String& name);
    /// Get the exported incant_animation property.
    String get_incant_animation() const;

    /// Set the exported death_animation property.
    void set_death_animation(const String& name);
    /// Get the exported death_animation property.
    String get_death_animation() const;

    /// Set the exported walk_animation_speed property.
    void set_walk_animation_speed(float speed);
    /// Get the exported walk_animation_speed property.
    float get_walk_animation_speed() const;

    /// Set this node's Y rotation from a protocol orientation (1=N, 2=E, 3=S, 4=W).
    void set_orientation(int orientation);

    /// Scale the whole node based on incantation level (1-8).
    void set_level(int level);

    /// Move to world_pos: instant on the first call or when (grid_x, grid_y) wraps
    /// across the map edge (|dx|>1 or |dy|>1), otherwise a smooth tween.
    void update_position(const Vector3& world_pos, int grid_x, int grid_y);

    /// Tween position to pos over duration seconds, killing any previous tween.
    void move_to(const Vector3& pos, float duration);

    /// Toggle the emissive highlight used while this player is incanting, and (if
    /// incant_animation/idle_animation are set) play the matching animation.
    void set_incanting(bool incanting);

    /// Play death_animation if set, freeing this node once it finishes; frees
    /// immediately if no death animation is configured (matches the previous
    /// instant-removal behavior for the plain capsule scene).
    void die();

    /// Get the tile column from the last update_position() call.
    int get_grid_x() const;
    /// Get the tile row from the last update_position() call.
    int get_grid_y() const;
};

} // namespace godot
