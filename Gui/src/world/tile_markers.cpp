/**
 * @file world/tile_markers.cpp
 * @brief Implementation of TileMarkers.
 */

#include "world/tile_markers.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <cmath>
#include <cstdint>

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
 * @brief Fixed display scale for resource `index`.
 * @details Each model is multiplied so its largest dimension lands near ~0.5
 *          world units (a quarter of a TILE_SIZE tile). The food model is ~5x
 *          larger than the mineral crystals natively, hence the separate factor.
 *          Markers are shown or hidden by quantity, not resized by it.
 */
float TileMarkers::resource_scale(int index)
{
    // Index 0 = food (native max-dim ~0.131), indices 1-6 = minerals (~0.024).
    return index == 0 ? 4.0f : 21.0f;
}

/**
 * @brief Deterministic horizontal facing angle (radians) for food on tile (x, y).
 * @details Hashes the tile coordinates so each tile's food faces a stable
 *          pseudo-random direction (no flicker across tile updates). Only the
 *          Y-axis (yaw) is varied; the model's baked tilt is left intact.
 */
float TileMarkers::food_yaw(int x, int y)
{
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (float)(h & 0xffffffu) / (float)0x1000000u * (2.0f * (float)Math_PI);
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
        float scale = quantities[i] > 0 ? resource_scale(i) : 0.0f;
        Vector3 origin = base + resource_offset(i) + Vector3(0.0f, 0.1f, 0.0f);

        Basis basis = Basis::from_scale(Vector3(scale, scale, scale));
        if (i == 0) {
            // Food faces a stable pseudo-random horizontal direction per tile.
            basis = Basis(Vector3(0.0f, 1.0f, 0.0f), food_yaw(x, y)) * basis;
        }

        Transform3D transform(basis, origin);
        _multimeshes[i]->set_instance_transform(idx, transform);
    }
}
