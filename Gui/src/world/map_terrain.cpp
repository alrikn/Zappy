/**
 * @file world/map_terrain.cpp
 * @brief Implementation of MapTerrain: mesh generation and noise-driven height sampling.
 */

#include "world/map_terrain.hpp"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void MapTerrain::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_grid_size", "width", "height"), &MapTerrain::set_grid_size);
    ClassDB::bind_method(D_METHOD("tile_to_world", "x", "y"), &MapTerrain::tile_to_world);

    ClassDB::bind_method(D_METHOD("set_noise", "noise"), &MapTerrain::set_noise);
    ClassDB::bind_method(D_METHOD("get_noise"), &MapTerrain::get_noise);

    ClassDB::bind_method(D_METHOD("randomize_seed"), &MapTerrain::randomize_seed);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"),
                  "set_noise", "get_noise");

    ClassDB::bind_method(D_METHOD("set_height", "height"), &MapTerrain::set_height);
    ClassDB::bind_method(D_METHOD("get_height"), &MapTerrain::get_height);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "4,128,4"),
                  "set_height", "get_height");
}

/**
 * @brief Resize the terrain grid and rebuild the mesh to match.
 * @details Both dimensions are clamped to at least 1 tile, so a degenerate
 *          msz response cannot produce an empty mesh.
 * @param width New map width in tiles.
 * @param height New map height in tiles.
 */
void MapTerrain::set_grid_size(int width, int height)
{
    _gridWidth = std::max(width, 1);
    _gridHeight = std::max(height, 1);
    update_mesh();
}

/**
 * @brief Replace the noise resource used for terrain displacement and rebuild the mesh.
 * @details If the new noise resource is valid, connects update_mesh() to its
 *          "changed" signal (if not already connected) so editing noise
 *          parameters live in the editor refreshes the terrain automatically.
 * @param p_noise New noise resource, or a null reference for flat terrain.
 */
void MapTerrain::set_noise(const Ref<FastNoiseLite> &p_noise)
{
    noise = p_noise;
    update_mesh();

    if (noise.is_valid()) {
        Callable callable = callable_mp(this, &MapTerrain::update_mesh);
        if (!noise->is_connected("changed", callable))
            noise->connect("changed", callable);
    }
}

/// @return The noise resource currently used to displace the terrain.
Ref<FastNoiseLite> MapTerrain::get_noise() const
{
    return noise;
}

/**
 * @brief Assign a random seed to the noise resource so the terrain looks different each time.
 * @details Setting the seed fires the noise resource's "changed" signal, which
 *          set_noise() already connected to update_mesh(), so the mesh rebuilds on its own.
 */
void MapTerrain::randomize_seed()
{
    if (noise.is_valid())
        noise->set_seed(UtilityFunctions::randi());
}

/**
 * @brief Set the vertical scale applied to noise samples and rebuild the mesh.
 * @details If a ShaderMaterial override is set, also pushes height * 2.0 into
 *          its "height" shader parameter so any shader-side effects (e.g. a
 *          matching water/fog plane) stay in sync with the mesh.
 * @param p_height New vertical scale.
 */
void MapTerrain::set_height(double p_height)
{
    height = p_height;

    Ref<ShaderMaterial> shader_material = get_material_override();
    if (shader_material.is_valid())
        shader_material->set_shader_parameter("height", height * 2.0);

    update_mesh();
}

/// @return The vertical scale currently applied to noise samples.
double MapTerrain::get_height() const
{
    return height;
}

/**
 * @brief Sample the noise field at world-space (x, z), scaled by height.
 * @param x World-space X coordinate.
 * @param z World-space Z coordinate.
 * @return noise->get_noise_2d(x, z) * height, or 0.0 if no noise resource is set.
 */
double MapTerrain::sample_height(double x, double z) const
{
    if (!noise.is_valid())
        return 0.0;
    return noise->get_noise_2d((float)x, (float)z) * height;
}

/**
 * @brief Estimate the surface normal at (x, z) via central differences of sample_height().
 * @param x World-space X coordinate.
 * @param z World-space Z coordinate.
 * @param epsilon_x Sample offset along X used for the finite-difference slope.
 * @param epsilon_z Sample offset along Z used for the finite-difference slope.
 * @return Normalized surface normal at (x, z).
 */
Vector3 MapTerrain::sample_normal(double x, double z, double epsilon_x, double epsilon_z) const
{
    Vector3 normal(
        (sample_height(x + epsilon_x, z) - sample_height(x - epsilon_x, z)) / (2.0 * epsilon_x),
        1.0,
        (sample_height(x, z + epsilon_z) - sample_height(x, z - epsilon_z)) / (2.0 * epsilon_z)
    );
    return normal.normalized();
}

/**
 * @brief Compute the world-space position of the center of tile (x, y) on the terrain surface.
 * @param x Tile column.
 * @param y Tile row.
 * @return World-space position with Y set to the terrain height at that point.
 */
Vector3 MapTerrain::tile_to_world(int x, int y) const
{
    double worldX = (x + 0.5 - _gridWidth / 2.0) * TILE_SIZE;
    double worldZ = (y + 0.5 - _gridHeight / 2.0) * TILE_SIZE;
    return Vector3((float)worldX, (float)sample_height(worldX, worldZ), (float)worldZ);
}

/**
 * @brief World-space terrain surface height (Y) at world coordinates (x, z).
 * @param world_x World-space X coordinate.
 * @param world_z World-space Z coordinate.
 * @return Terrain height at (x, z), or 0 when no noise resource is set.
 */
double MapTerrain::get_height_at(double world_x, double world_z) const
{
    return sample_height(world_x, world_z);
}

/**
 * @brief World-space length of the map's diagonal.
 * @return sqrt((gridWidth*TILE)^2 + (gridHeight*TILE)^2) in world units.
 */
double MapTerrain::get_world_diagonal() const
{
    double w = _gridWidth * TILE_SIZE;
    double h = _gridHeight * TILE_SIZE;
    return std::sqrt(w * w + h * h);
}

/**
 * @brief Rebuild the plane mesh from the current grid size, noise and height.
 * @details Generates a PlaneMesh sized to the grid (in TILE_SIZE units) with
 *          subdivisions proportional to the grid size (clamped to
 *          MAX_SUBDIVISIONS), then displaces each vertex and recomputes its
 *          normal/tangent from the noise field before assigning the result
 *          as this node's mesh. If no noise resource is set, the mesh is left
 *          flat with the PlaneMesh's default normals/tangents.
 */
void MapTerrain::update_mesh()
{
    double sizeX = _gridWidth * TILE_SIZE;
    double sizeZ = _gridHeight * TILE_SIZE;

    int subdivW = std::clamp(_gridWidth * SUBDIVISIONS_PER_TILE, 1, MAX_SUBDIVISIONS);
    int subdivD = std::clamp(_gridHeight * SUBDIVISIONS_PER_TILE, 1, MAX_SUBDIVISIONS);

    double epsilonX = sizeX / subdivW;
    double epsilonZ = sizeZ / subdivD;

    Ref<PlaneMesh> plane;
    plane.instantiate();
    plane->set_subdivide_width(subdivW);
    plane->set_subdivide_depth(subdivD);
    plane->set_size(Vector2(sizeX, sizeZ));

    Array plane_arrays = plane->get_mesh_arrays();

    PackedVector3Array vertex_array = plane_arrays[Mesh::ARRAY_VERTEX];
    PackedVector3Array normal_array = plane_arrays[Mesh::ARRAY_NORMAL];
    PackedFloat32Array tangent_array = plane_arrays[Mesh::ARRAY_TANGENT];

    for (int64_t i = 0; i < vertex_array.size(); i++) {
        Vector3 vertex = vertex_array[i];
        Vector3 normal(0.0, 1.0, 0.0);
        Vector3 tangent(1.0, 0.0, 0.0);

        if (noise.is_valid()) {
            vertex.y = sample_height(vertex.x, vertex.z);
            normal = sample_normal(vertex.x, vertex.z, epsilonX, epsilonZ);
            tangent = normal.cross(Vector3(0.0, 1.0, 0.0));
        }

        vertex_array[i] = vertex;
        normal_array[i] = normal;
        tangent_array[4 * i] = tangent.x;
        tangent_array[4 * i + 1] = tangent.y;
        tangent_array[4 * i + 2] = tangent.z;
    }

    plane_arrays[Mesh::ARRAY_VERTEX] = vertex_array;
    plane_arrays[Mesh::ARRAY_NORMAL] = normal_array;
    plane_arrays[Mesh::ARRAY_TANGENT] = tangent_array;

    Ref<ArrayMesh> array_mesh;
    array_mesh.instantiate();
    array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, plane_arrays);

    set_mesh(array_mesh);
}
