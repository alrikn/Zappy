/**
 * @file world/tile_markers.cpp
 * @brief Implementation of TileMarkers.
 */

#include "world/tile_markers.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace {

/// Names of the 7 MultiMeshInstance3D children, in resource-index order
/// (0=food, 1=linemate, 2=deraumere, 3=sibur, 4=mendiane, 5=phiras, 6=thystame).
constexpr const char* kResourceNodeNames[7] = {
    "Food", "Linemate", "Deraumere", "Sibur", "Mendiane", "Phiras", "Thystame",
};

} // namespace

void TileMarkers::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_map_terrain_path", "path"), &TileMarkers::set_map_terrain_path);
    ClassDB::bind_method(D_METHOD("get_map_terrain_path"), &TileMarkers::get_map_terrain_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "map_terrain_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "MapTerrain"),
                  "set_map_terrain_path", "get_map_terrain_path");

    ClassDB::bind_method(D_METHOD("initialize", "width", "height"), &TileMarkers::initialize);
    ClassDB::bind_method(D_METHOD("update_tile", "x", "y", "q0", "q1", "q2", "q3", "q4", "q5", "q6"),
                          &TileMarkers::update_tile);
}

/// Resolve map_terrain_path and fetch the 7 MultiMeshInstance3D children and their multimeshes.
void TileMarkers::_ready()
{
    _mapTerrain = get_node<MapTerrain>(_mapTerrainPath);

    for (int i = 0; i < RESOURCE_COUNT; i++) {
        _meshInstances[i] = get_node<MultiMeshInstance3D>(NodePath(kResourceNodeNames[i]));
        _multimeshes[i] = _meshInstances[i]->get_multimesh();
    }
}

void TileMarkers::set_map_terrain_path(const NodePath& path)
{
    _mapTerrainPath = path;
}

NodePath TileMarkers::get_map_terrain_path() const
{
    return _mapTerrainPath;
}

/**
 * @brief World-space offset of resource marker `index` within a tile.
 * @details Index 0 (food) sits at the tile center. Indices 1-6 are arranged in
 *          a hexagon at radius ~0.6 around the center.
 */
Vector3 TileMarkers::resource_offset(int index)
{
    if (index == 0) {
        return Vector3(0.0f, 0.0f, 0.0f);
    }

    double angle = index * (2.0 * Math_PI / 6.0);
    return Vector3((float)(std::cos(angle) * 0.6), 0.0f, (float)(std::sin(angle) * 0.6));
}

/**
 * @brief Map a resource quantity to a marker scale.
 * @return 0 (hidden) for quantity <= 0, otherwise 0.15 + 0.05*quantity clamped to 0.65.
 */
float TileMarkers::quantity_to_scale(int quantity)
{
    if (quantity <= 0) {
        return 0.0f;
    }
    return std::min(0.15f + 0.05f * (float)quantity, 0.65f);
}

/**
 * @brief Size all 7 multimeshes to width*height instances, all hidden.
 * @details Every instance starts with a zero-scale transform so no markers are
 *          visible until the first tile_updated for each tile arrives.
 */
void TileMarkers::initialize(int width, int height)
{
    _gridWidth = width;
    _gridHeight = height;

    int32_t count = width * height;
    Transform3D hidden(Basis::from_scale(Vector3(0.0f, 0.0f, 0.0f)), Vector3());

    for (int i = 0; i < RESOURCE_COUNT; i++) {
        _multimeshes[i]->set_instance_count(count);
        for (int32_t idx = 0; idx < count; idx++) {
            _multimeshes[i]->set_instance_transform(idx, hidden);
        }
    }
}

/**
 * @brief Update the marker transforms for tile (x, y).
 * @details Out-of-range coordinates are silently ignored, matching
 *          WorldState::onTileContent()'s tolerance policy.
 */
void TileMarkers::update_tile(int x, int y, int q0, int q1, int q2, int q3, int q4, int q5, int q6)
{
    if (_gridWidth == 0 || _gridHeight == 0) {
        return;
    }
    if (x < 0 || y < 0 || x >= _gridWidth || y >= _gridHeight) {
        return;
    }

    int32_t idx = y * _gridWidth + x;
    int quantities[RESOURCE_COUNT] = {q0, q1, q2, q3, q4, q5, q6};
    Vector3 base = _mapTerrain->tile_to_world(x, y);

    for (int i = 0; i < RESOURCE_COUNT; i++) {
        float scale = quantity_to_scale(quantities[i]);
        Vector3 origin = base + resource_offset(i) + Vector3(0.0f, 0.1f, 0.0f);
        Transform3D transform(Basis::from_scale(Vector3(scale, scale, scale)), origin);
        _multimeshes[i]->set_instance_transform(idx, transform);
    }
}
