/**
 * @file world/map_terrain.cpp
 * @brief Implementation of MapTerrain: mesh generation and noise-driven height sampling.
 */

#include "world/map_terrain.hpp"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
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

    ClassDB::bind_method(D_METHOD("set_wall_texture", "texture"), &MapTerrain::set_wall_texture);
    ClassDB::bind_method(D_METHOD("get_wall_texture"), &MapTerrain::get_wall_texture);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "wall_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"),
                  "set_wall_texture", "get_wall_texture");
}

/**
 * @brief Create the internal wall child and its dirt material, then build the mesh.
 * @details The walls live on a separate MeshInstance3D because the terrain's
 *          material_override (the elevation-gradient shader) would otherwise be
 *          forced onto them. The child is added with INTERNAL_MODE_BACK so it is
 *          never serialized into the scene. Runs in the editor too, so the block
 *          is visible in the world.tscn preview.
 */
void MapTerrain::_ready()
{
    _wallMaterial.instantiate();
    _wallMaterial->set_roughness(1.0);
    _wallMaterial->set_uv1_scale(Vector3(1.0f, 1.0f, 1.0f));
    _wallMaterial->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
    if (_wallTexture.is_valid())
        _wallMaterial->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, _wallTexture);

    _walls = memnew(MeshInstance3D);
    _walls->set_name("TerrainWalls");
    add_child(_walls, false, Node::INTERNAL_MODE_BACK);
    _walls->set_material_override(_wallMaterial);

    update_mesh();
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
 * @brief Set the dirt albedo texture used by the perimeter walls.
 * @details Stored even when the material does not exist yet, because Godot
 *          assigns exported properties before _ready() runs; _ready() then
 *          applies it. When the material already exists, the change is applied live.
 * @param p_texture Dirt texture, or a null reference for an untextured (brown) wall.
 */
void MapTerrain::set_wall_texture(const Ref<Texture2D> &p_texture)
{
    _wallTexture = p_texture;
    if (_wallMaterial.is_valid())
        _wallMaterial->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, _wallTexture);
}

/// @return The dirt albedo texture used by the perimeter walls.
Ref<Texture2D> MapTerrain::get_wall_texture() const
{
    return _wallTexture;
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

    if (_walls)
        update_walls();
}

/**
 * @brief Rebuild the 4 perimeter walls into the internal _walls child.
 * @details Each wall is a vertical strip whose top follows the terrain edge
 *          (same sample_height() calls as update_mesh(), so the seam is exact)
 *          and whose bottom drops to a single flat Y chosen so the bottom edge
 *          is never visible from the orthographic RTS camera (fixed pitch,
 *          max zoom size). Depth adds no triangles, so it is made generous.
 */
void MapTerrain::update_walls()
{
    double sizeX = _gridWidth * TILE_SIZE;
    double sizeZ = _gridHeight * TILE_SIZE;
    double halfX = sizeX * 0.5;
    double halfZ = sizeZ * 0.5;

    int colsX = std::clamp(_gridWidth * SUBDIVISIONS_PER_TILE, 1, MAX_SUBDIVISIONS) + 1;
    int colsZ = std::clamp(_gridHeight * SUBDIVISIONS_PER_TILE, 1, MAX_SUBDIVISIONS) + 1;

    // Depth that keeps the wall bottom off-screen for the orthographic camera
    // (fixed pitch 35deg, max ortho size 80): the far wall's bottom must stay
    // below the top of the screen even at full zoom-out from the opposite edge.
    const double pitch = Math::deg_to_rad(35.0);
    const double maxOrthoSize = 80.0;
    double diag = get_world_diagonal();
    double safeDepth = (maxOrthoSize * 0.5 + diag * std::sin(pitch)) / std::cos(pitch) + WALL_DEPTH_MARGIN;
    double baseY = -safeDepth;

    const double topEps = 0.05; // small upward overlap so the wall top tucks under the terrain edge

    PackedVector3Array vertices;
    PackedVector3Array normals;
    PackedVector2Array uvs;

    // Append one vertical strip along an edge. (ax, az) is the start corner;
    // (dx, dz) is the per-column step direction; n is the outward normal.
    auto add_strip = [&](double ax, double az, double dx, double dz, int cols, Vector3 n) {
        for (int i = 0; i < cols - 1; i++) {
            double x0 = ax + dx * i;
            double z0 = az + dz * i;
            double x1 = ax + dx * (i + 1);
            double z1 = az + dz * (i + 1);

            double top0 = sample_height(x0, z0) + topEps;
            double top1 = sample_height(x1, z1) + topEps;

            double dist0 = (double)i * std::sqrt(dx * dx + dz * dz);
            double dist1 = (double)(i + 1) * std::sqrt(dx * dx + dz * dz);
            double u0 = dist0 / WALL_TEX_WORLD_SIZE;
            double u1 = dist1 / WALL_TEX_WORLD_SIZE;

            Vector3 tA((float)x0, (float)top0, (float)z0);
            Vector3 tB((float)x1, (float)top1, (float)z1);
            Vector3 bA((float)x0, (float)baseY, (float)z0);
            Vector3 bB((float)x1, (float)baseY, (float)z1);

            Vector2 uvTA((float)u0, (float)(-top0 / WALL_TEX_WORLD_SIZE));
            Vector2 uvTB((float)u1, (float)(-top1 / WALL_TEX_WORLD_SIZE));
            Vector2 uvBA((float)u0, (float)(-baseY / WALL_TEX_WORLD_SIZE));
            Vector2 uvBB((float)u1, (float)(-baseY / WALL_TEX_WORLD_SIZE));

            // Two triangles, wound so the outward normal n faces the viewer.
            vertices.push_back(tA); vertices.push_back(bA); vertices.push_back(tB);
            normals.push_back(n);   normals.push_back(n);   normals.push_back(n);
            uvs.push_back(uvTA);    uvs.push_back(uvBA);     uvs.push_back(uvTB);

            vertices.push_back(tB); vertices.push_back(bA); vertices.push_back(bB);
            normals.push_back(n);   normals.push_back(n);   normals.push_back(n);
            uvs.push_back(uvTB);    uvs.push_back(uvBA);     uvs.push_back(uvBB);
        }
    };

    double stepX = sizeX / (colsX - 1);
    double stepZ = sizeZ / (colsZ - 1);

    // Left edge (x = -halfX), outward -X: walk +Z.
    add_strip(-halfX, -halfZ, 0.0, stepZ, colsZ, Vector3(-1, 0, 0));
    // Right edge (x = +halfX), outward +X: walk -Z (keeps outward winding).
    add_strip(halfX, halfZ, 0.0, -stepZ, colsZ, Vector3(1, 0, 0));
    // Front edge (z = -halfZ), outward -Z: walk -X.
    add_strip(halfX, -halfZ, -stepX, 0.0, colsX, Vector3(0, 0, -1));
    // Back edge (z = +halfZ), outward +Z: walk +X.
    add_strip(-halfX, halfZ, stepX, 0.0, colsX, Vector3(0, 0, 1));

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_NORMAL] = normals;
    arrays[Mesh::ARRAY_TEX_UV] = uvs;

    Ref<ArrayMesh> wall_mesh;
    wall_mesh.instantiate();
    wall_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    _walls->set_mesh(wall_mesh);
}
