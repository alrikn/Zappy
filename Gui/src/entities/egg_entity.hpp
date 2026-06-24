/**
 * @file entities/egg_entity.hpp
 * @brief Visual representation of one egg waiting to hatch.
 * @details EggEntity is a thin logic layer over a scene
 *          (scenes/entities/egg_entity.tscn) that provides the egg mesh.
 *          EntityManager creates one instance per enw and drives it from
 *          World's signals.
 */

#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {

/// One egg's visual: a small sphere colored with its laying player's team color.
class EggEntity : public Node3D {
    GDCLASS(EggEntity, Node3D)

private:
    MeshInstance3D* _mesh = nullptr; ///< "Mesh" child, fetched in _ready().

    Ref<StandardMaterial3D> _material; ///< Per-instance team-color material applied to _mesh.

    int _eggId = 0; ///< Exported property: server-assigned egg id.

protected:
    /// Bind methods and properties exposed to Godot/GDScript.
    static void _bind_methods();

public:
    /// Fetch the mesh child and install the per-instance material.
    void _ready() override;

    /// Set the exported egg_id property.
    void set_egg_id(int id);
    /// Get the exported egg_id property.
    int get_egg_id() const;

    /// Set the mesh material's albedo to the given team color.
    void set_team_color(const Color& color);
    /// Get the mesh material's current albedo.
    Color get_team_color() const;
};

} // namespace godot
