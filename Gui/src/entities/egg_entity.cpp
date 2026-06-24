/**
 * @file entities/egg_entity.cpp
 * @brief Implementation of EggEntity.
 */

#include "entities/egg_entity.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>

using namespace godot;

void EggEntity::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_egg_id", "id"), &EggEntity::set_egg_id);
    ClassDB::bind_method(D_METHOD("get_egg_id"), &EggEntity::get_egg_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "egg_id"), "set_egg_id", "get_egg_id");

    ClassDB::bind_method(D_METHOD("set_team_color", "color"), &EggEntity::set_team_color);
    ClassDB::bind_method(D_METHOD("get_team_color"), &EggEntity::get_team_color);
}

/// Fetch the "Mesh" child and install the per-instance team-color material.
void EggEntity::_ready()
{
    _mesh = get_node<MeshInstance3D>(NodePath("Mesh"));

    _material.instantiate();
    _mesh->set_surface_override_material(0, _material);
}

void EggEntity::set_egg_id(int id)
{
    _eggId = id;
}

int EggEntity::get_egg_id() const
{
    return _eggId;
}

void EggEntity::set_team_color(const Color& color)
{
    _material->set_albedo(color);
}

Color EggEntity::get_team_color() const
{
    return _material->get_albedo();
}
