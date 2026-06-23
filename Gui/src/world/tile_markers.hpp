/**
 * @file world/tile_markers.hpp
 * @brief Per-tile resource markers driven by tile_updated.
 * @details TileMarkers is a thin logic layer over a scene
 *          (scenes/world/tile_markers.tscn) that holds one MultiMeshInstance3D
 *          per resource type (food, linemate, deraumere, sibur, mendiane,
 *          phiras, thystame). Each tile's resource quantities are reflected as
 *          per-instance transforms (position + scale) in the matching multimesh.
 */

#pragma once

#include "world/map_terrain.hpp"

#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <array>

namespace godot {

/// Holds 7 pre-built MultiMeshInstance3D children (one per resource type) and
/// updates their per-tile instance transforms from World's map_initialized and
/// tile_updated signals.
class TileMarkers : public Node3D {
    GDCLASS(TileMarkers, Node3D)

private:
    static constexpr int RESOURCE_COUNT = 7; ///< food, linemate, deraumere, sibur, mendiane, phiras, thystame.

    /// MultiMeshInstance3D children, fetched by name in _ready(), in resource-index order.
    std::array<MultiMeshInstance3D*, RESOURCE_COUNT> _meshInstances{};
    /// MultiMesh resources fetched from _meshInstances in _ready().
    std::array<Ref<MultiMesh>, RESOURCE_COUNT> _multimeshes{};

    MapTerrain* _mapTerrain = nullptr; ///< Resolved from map_terrain_path in _ready().
    NodePath _mapTerrainPath;          ///< Exported property: path to the MapTerrain node.

    int _gridWidth = 0;  ///< Map width in tiles, set by initialize().
    int _gridHeight = 0; ///< Map height in tiles, set by initialize().

    /// World-space offset of resource marker `index` within a tile: index 0
    /// (food) at the tile center, indices 1-6 arranged in a hexagon at radius ~0.6.
    static Vector3 resource_offset(int index);

    /// Map a resource quantity to a marker scale: 0 (hidden) for quantity <= 0,
    /// otherwise 0.15 + 0.05*quantity, clamped to 0.65.
    static float quantity_to_scale(int quantity);

protected:
    /// Bind methods and properties exposed to Godot/GDScript.
    static void _bind_methods();

public:
    /// Resolve map_terrain_path and fetch the 7 MultiMeshInstance3D children.
    void _ready() override;

    /// Set the exported map_terrain_path property.
    void set_map_terrain_path(const NodePath& path);
    /// Get the exported map_terrain_path property.
    NodePath get_map_terrain_path() const;

    /// Size all 7 multimeshes to width*height instances, all hidden (scale 0).
    /// Connected from World's map_initialized signal.
    void initialize(int width, int height);

    /// Update the marker transforms for tile (x, y) from its resource quantities
    /// q0 (food) through q6 (thystame). Connected from World's tile_updated signal.
    void update_tile(int x, int y, int q0, int q1, int q2, int q3, int q4, int q5, int q6);
};

} // namespace godot
