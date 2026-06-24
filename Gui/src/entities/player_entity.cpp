/**
 * @file entities/player_entity.cpp
 * @brief Implementation of PlayerEntity.
 */

#include "entities/player_entity.hpp"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void PlayerEntity::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_player_id", "id"), &PlayerEntity::set_player_id);
    ClassDB::bind_method(D_METHOD("get_player_id"), &PlayerEntity::get_player_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "player_id"), "set_player_id", "get_player_id");

    ClassDB::bind_method(D_METHOD("set_team_color", "color"), &PlayerEntity::set_team_color);
    ClassDB::bind_method(D_METHOD("get_team_color"), &PlayerEntity::get_team_color);

    ClassDB::bind_method(D_METHOD("set_idle_animation", "name"), &PlayerEntity::set_idle_animation);
    ClassDB::bind_method(D_METHOD("get_idle_animation"), &PlayerEntity::get_idle_animation);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "idle_animation"), "set_idle_animation", "get_idle_animation");

    ClassDB::bind_method(D_METHOD("set_walk_animation", "name"), &PlayerEntity::set_walk_animation);
    ClassDB::bind_method(D_METHOD("get_walk_animation"), &PlayerEntity::get_walk_animation);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "walk_animation"), "set_walk_animation", "get_walk_animation");

    ClassDB::bind_method(D_METHOD("set_incant_animation", "name"), &PlayerEntity::set_incant_animation);
    ClassDB::bind_method(D_METHOD("get_incant_animation"), &PlayerEntity::get_incant_animation);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "incant_animation"), "set_incant_animation", "get_incant_animation");

    ClassDB::bind_method(D_METHOD("set_death_animation", "name"), &PlayerEntity::set_death_animation);
    ClassDB::bind_method(D_METHOD("get_death_animation"), &PlayerEntity::get_death_animation);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "death_animation"), "set_death_animation", "get_death_animation");

    ClassDB::bind_method(D_METHOD("set_walk_animation_speed", "speed"), &PlayerEntity::set_walk_animation_speed);
    ClassDB::bind_method(D_METHOD("get_walk_animation_speed"), &PlayerEntity::get_walk_animation_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "walk_animation_speed"), "set_walk_animation_speed", "get_walk_animation_speed");

    ClassDB::bind_method(D_METHOD("set_orientation", "orientation"), &PlayerEntity::set_orientation);
    ClassDB::bind_method(D_METHOD("set_level", "level"), &PlayerEntity::set_level);

    ClassDB::bind_method(D_METHOD("update_position", "world_pos", "grid_x", "grid_y"), &PlayerEntity::update_position);
    ClassDB::bind_method(D_METHOD("move_to", "pos", "duration"), &PlayerEntity::move_to);

    ClassDB::bind_method(D_METHOD("set_incanting", "incanting"), &PlayerEntity::set_incanting);
    ClassDB::bind_method(D_METHOD("die"), &PlayerEntity::die);

    ClassDB::bind_method(D_METHOD("get_grid_x"), &PlayerEntity::get_grid_x);
    ClassDB::bind_method(D_METHOD("get_grid_y"), &PlayerEntity::get_grid_y);
}

/**
 * @brief Fetch the pre-built child nodes and install the per-instance body material.
 * @details "Body" may be a bare MeshInstance3D (the default capsule) or the root of an
 *          instanced character scene (mesh + skeleton + AnimationPlayer nested inside).
 *          Every MeshInstance3D found under it gets the team-color material override,
 *          and the first AnimationPlayer found (if any) drives play_animation calls.
 *          The body material is created in code (not baked into the scene) because
 *          its albedo (team color) varies per instance.
 */
void PlayerEntity::_ready()
{
    _bodyRoot = get_node<Node3D>(NodePath("Body"));
    _directionIndicator = get_node<MeshInstance3D>(NodePath("DirectionIndicator"));
    _selectionArea = get_node<Area3D>(NodePath("SelectionArea"));

    if (_bodyRoot == nullptr) {
        return;
    }

    _bodyMeshes.clear();
    if (MeshInstance3D* direct = Object::cast_to<MeshInstance3D>(_bodyRoot)) {
        _bodyMeshes.push_back(direct);
    }
    TypedArray<Node> meshNodes = _bodyRoot->find_children("*", "MeshInstance3D", true, false);
    for (int64_t i = 0; i < meshNodes.size(); i++) {
        if (MeshInstance3D* mesh = Object::cast_to<MeshInstance3D>(meshNodes[i])) {
            _bodyMeshes.push_back(mesh);
        }
    }

    TypedArray<Node> animPlayers = _bodyRoot->find_children("*", "AnimationPlayer", true, false);
    _animationPlayer = animPlayers.is_empty() ? nullptr : Object::cast_to<AnimationPlayer>(animPlayers[0]);

    _bodyMaterial.instantiate();
    for (MeshInstance3D* mesh : _bodyMeshes) {
        mesh->set_surface_override_material(0, _bodyMaterial);
    }
}

void PlayerEntity::set_player_id(int id)
{
    _playerId = id;
}

int PlayerEntity::get_player_id() const
{
    return _playerId;
}

void PlayerEntity::set_team_color(const Color& color)
{
    _bodyMaterial->set_albedo(color);
}

Color PlayerEntity::get_team_color() const
{
    return _bodyMaterial->get_albedo();
}

void PlayerEntity::set_idle_animation(const String& name)
{
    _idleAnimation = name;
}

String PlayerEntity::get_idle_animation() const
{
    return _idleAnimation;
}

void PlayerEntity::set_walk_animation(const String& name)
{
    _walkAnimation = name;
}

String PlayerEntity::get_walk_animation() const
{
    return _walkAnimation;
}

void PlayerEntity::set_incant_animation(const String& name)
{
    _incantAnimation = name;
}

String PlayerEntity::get_incant_animation() const
{
    return _incantAnimation;
}

void PlayerEntity::set_death_animation(const String& name)
{
    _deathAnimation = name;
}

String PlayerEntity::get_death_animation() const
{
    return _deathAnimation;
}

void PlayerEntity::set_walk_animation_speed(float speed)
{
    _walkAnimationSpeed = speed;
}

float PlayerEntity::get_walk_animation_speed() const
{
    return _walkAnimationSpeed;
}

/**
 * @brief Play animation_name on _animationPlayer at the given speed, unless
 *        it's already the currently-playing animation, or either is unset.
 */
void PlayerEntity::_play_clip(const String& animation_name, float speed)
{
    if (_animationPlayer == nullptr || animation_name.is_empty()) {
        return;
    }
    if (_animationPlayer->is_playing() && _animationPlayer->get_current_animation() == animation_name) {
        return;
    }
    _animationPlayer->play(animation_name, -1.0, speed);
}

/**
 * @brief Set this node's Y rotation from a protocol orientation.
 * @details North=0, East=-pi/2, South=-pi, West=-3pi/2, matching tile_to_world's
 *          X=East/Z=South axes and Godot's default -Z forward.
 * @param orientation Protocol orientation: 1=North, 2=East, 3=South, 4=West.
 */
void PlayerEntity::set_orientation(int orientation)
{
    Vector3 rot = get_rotation();
    rot.y = -(orientation - 1) * (Math_PI / 2.0);
    set_rotation(rot);
}

/**
 * @brief Scale the whole node based on incantation level.
 * @param level Incantation level (1-8); scale grows linearly, clamped to [1, 2].
 */
void PlayerEntity::set_level(int level)
{
    double s = std::clamp(1.0 + 0.12 * (level - 1), 1.0, 2.0);
    set_scale(Vector3((float)s, (float)s, (float)s));
}

/**
 * @brief Update this entity's position from a new tile coordinate.
 * @details The first call, a toroidal wrap-around (tile moved by more than one
 *          step on either axis), or a same-tile update (a pure turn — the
 *          server reports turning in place the same way as moving, just with
 *          an unchanged x/y) places the entity instantly and plays idle_animation
 *          rather than walk_animation, since nothing actually moved. Otherwise
 *          the move is tweened.
 */
void PlayerEntity::update_position(const Vector3& world_pos, int grid_x, int grid_y)
{
    bool wrapped = _hasPosition &&
        (std::abs(grid_x - _gridX) > 1 || std::abs(grid_y - _gridY) > 1);
    bool sameTile = _hasPosition && grid_x == _gridX && grid_y == _gridY;

    if (!_hasPosition || wrapped || sameTile) {
        set_position(world_pos);
        if (!_isIncanting) {
            _play_clip(_idleAnimation);
        }
    } else {
        move_to(world_pos, 0.3f);
    }

    _hasPosition = true;
    _gridX = grid_x;
    _gridY = grid_y;
}

/**
 * @brief Tween this node's position to pos over duration seconds.
 * @details Kills any previously active tween started by this method first.
 *          Plays walk_animation for the duration of the tween, then
 *          idle_animation once it finishes.
 */
void PlayerEntity::move_to(const Vector3& pos, float duration)
{
    if (_activeTween.is_valid()) {
        _activeTween->kill();
    }

    if (!_isIncanting) {
        _play_clip(_walkAnimation, _walkAnimationSpeed);
    }

    _activeTween = create_tween();
    _activeTween->set_trans(Tween::TRANS_SINE);
    _activeTween->set_ease(Tween::EASE_IN_OUT);
    _activeTween->tween_property(this, NodePath("position"), pos, duration);
    _activeTween->connect("finished", callable_mp(this, &PlayerEntity::_on_move_finished));
}

void PlayerEntity::_on_move_finished()
{
    if (!_isIncanting) {
        _play_clip(_idleAnimation);
    }
}

/**
 * @brief Toggle the emissive highlight shown while this player is incanting,
 *        and switch between incant_animation/idle_animation if set. Also
 *        records incanting state so movement-driven animation calls
 *        (update_position/move_to) don't clobber the incant animation if a
 *        position/orientation refresh for this player arrives while incanting.
 */
void PlayerEntity::set_incanting(bool incanting)
{
    _isIncanting = incanting;
    _play_clip(incanting ? _incantAnimation : _idleAnimation);

    _bodyMaterial->set_feature(BaseMaterial3D::FEATURE_EMISSION, incanting);
    if (incanting) {
        _bodyMaterial->set_emission(Color(1.0, 0.9, 0.2));
        _bodyMaterial->set_emission_energy_multiplier(1.5);
    }
}

/**
 * @brief Play death_animation and free this node once it finishes; frees
 *        immediately if no death animation is configured.
 */
void PlayerEntity::die()
{
    if (_animationPlayer != nullptr && !_deathAnimation.is_empty()) {
        _animationPlayer->play(_deathAnimation);
        _animationPlayer->connect("animation_finished",
            callable_mp(this, &PlayerEntity::_on_death_animation_finished), Object::CONNECT_ONE_SHOT);
    } else {
        queue_free();
    }
}

void PlayerEntity::_on_death_animation_finished(const StringName&)
{
    queue_free();
}

int PlayerEntity::get_grid_x() const
{
    return _gridX;
}

int PlayerEntity::get_grid_y() const
{
    return _gridY;
}
