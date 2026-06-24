/**
 * @file entities/player_entity.cpp
 * @brief Implementation of PlayerEntity.
 */

#include "entities/player_entity.hpp"

#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/node_path.hpp>

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

    ClassDB::bind_method(D_METHOD("set_orientation", "orientation"), &PlayerEntity::set_orientation);
    ClassDB::bind_method(D_METHOD("set_level", "level"), &PlayerEntity::set_level);

    ClassDB::bind_method(D_METHOD("update_position", "world_pos", "grid_x", "grid_y"), &PlayerEntity::update_position);
    ClassDB::bind_method(D_METHOD("move_to", "pos", "duration"), &PlayerEntity::move_to);

    ClassDB::bind_method(D_METHOD("set_incanting", "incanting"), &PlayerEntity::set_incanting);

    ClassDB::bind_method(D_METHOD("get_grid_x"), &PlayerEntity::get_grid_x);
    ClassDB::bind_method(D_METHOD("get_grid_y"), &PlayerEntity::get_grid_y);
}

/**
 * @brief Fetch the pre-built child nodes and install the per-instance body material.
 * @details The body material is created in code (not baked into the scene) because
 *          its albedo (team color) varies per instance.
 */
void PlayerEntity::_ready()
{
    _body = get_node<MeshInstance3D>(NodePath("Body"));
    _directionIndicator = get_node<MeshInstance3D>(NodePath("DirectionIndicator"));
    _selectionArea = get_node<Area3D>(NodePath("SelectionArea"));

    _bodyMaterial.instantiate();
    _body->set_surface_override_material(0, _bodyMaterial);
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
 * @details The first call, or any call where the tile moved by more than one
 *          step on either axis (a toroidal wrap-around), places the entity
 *          instantly. Otherwise the move is tweened.
 */
void PlayerEntity::update_position(const Vector3& world_pos, int grid_x, int grid_y)
{
    bool wrapped = _hasPosition &&
        (std::abs(grid_x - _gridX) > 1 || std::abs(grid_y - _gridY) > 1);

    if (!_hasPosition || wrapped) {
        set_position(world_pos);
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
 */
void PlayerEntity::move_to(const Vector3& pos, float duration)
{
    if (_activeTween.is_valid()) {
        _activeTween->kill();
    }

    _activeTween = create_tween();
    _activeTween->set_trans(Tween::TRANS_SINE);
    _activeTween->set_ease(Tween::EASE_IN_OUT);
    _activeTween->tween_property(this, NodePath("position"), pos, duration);
}

/**
 * @brief Toggle the emissive highlight shown while this player is incanting.
 */
void PlayerEntity::set_incanting(bool incanting)
{
    _bodyMaterial->set_feature(BaseMaterial3D::FEATURE_EMISSION, incanting);
    if (incanting) {
        _bodyMaterial->set_emission(Color(1.0, 0.9, 0.2));
        _bodyMaterial->set_emission_energy_multiplier(1.5);
    }
}

int PlayerEntity::get_grid_x() const
{
    return _gridX;
}

int PlayerEntity::get_grid_y() const
{
    return _gridY;
}
