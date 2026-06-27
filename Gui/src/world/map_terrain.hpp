/**
 * @file world/map_terrain.hpp
 * @brief Procedural terrain mesh sized to the server's map dimensions.
 * @details MapTerrain generates a noise-displaced plane mesh covering the
 *          server's map (msz X Y) with a noise-based "smooth hills" look.
 *          tile_to_world() is used to place entities/resources on the terrain surface.
 */

#pragma once

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/// Terrain mesh sized to the server's map (msz X Y) with a noise-based
/// "smooth hills" look. tile_to_world() is used to place entities/resources on the terrain.
class MapTerrain : public MeshInstance3D {
    GDCLASS(MapTerrain, MeshInstance3D)

private:
    static constexpr double TILE_SIZE = 2.0;       ///< World units per map tile.
    static constexpr int SUBDIVISIONS_PER_TILE = 2; ///< Mesh subdivisions per tile.
    static constexpr int MAX_SUBDIVISIONS = 200;    ///< Clamp for very large maps.

    static constexpr double WALL_TEX_WORLD_SIZE = 4.0; ///< World units per dirt-texture tile (wall UV scale).
    static constexpr double WALL_DEPTH_MARGIN = 16.0;  ///< Extra slack below the computed safe wall depth.

    int _gridWidth = 50;  ///< Map width in tiles, set by set_grid_size().
    int _gridHeight = 50; ///< Map height in tiles, set by set_grid_size().

    Ref<FastNoiseLite> noise; ///< Noise source for terrain displacement; may be null (flat terrain).
    double height = 64.0;     ///< Vertical scale applied to the noise sample.

    MeshInstance3D *_walls = nullptr;          ///< Internal child holding the perimeter "block" walls.
    Ref<StandardMaterial3D> _wallMaterial;     ///< Dirt material applied to the walls.
    Ref<Texture2D> _wallTexture;               ///< Exported: dirt albedo texture for the walls.

    /// Sample the noise field at world-space (x, z), scaled by height. Returns 0 if noise is unset.
    double sample_height(double x, double z) const;

    /// Estimate the surface normal at (x, z) via central differences of sample_height().
    Vector3 sample_normal(double x, double z, double epsilon_x, double epsilon_z) const;

    /// Rebuild the plane mesh from the current grid size, noise and height.
    void update_mesh();

    /// Rebuild the 4 perimeter walls so their tops hug the terrain edges and
    /// their bottoms extend below the camera's reach.
    void update_walls();

protected:
    static void _bind_methods();

public:
    /// Create the wall child + dirt material, then build the initial mesh.
    void _ready() override;

    /// World units per map tile, shared with anything that needs to convert
    /// between tile coordinates and world space (e.g. RtsCamera's pan clamp).
    static double get_tile_size() { return TILE_SIZE; }

    /// Resize the terrain to width x height tiles (called on map_initialized).
    void set_grid_size(int width, int height);

    /// World-space position of the center of tile (x, y), sitting on the hills.
    Vector3 tile_to_world(int x, int y) const;

    /// World-space terrain surface height (Y) at world coordinates (x, z);
    /// returns 0 when the terrain is flat (no noise). Lets ground-hugging
    /// effects (e.g. the broadcast ripple) conform to the hills.
    double get_height_at(double world_x, double world_z) const;

    /// World-space length of the map's diagonal (sqrt(w^2 + h^2) in world units).
    /// A ripple using this as its max radius reaches every corner from any tile.
    double get_world_diagonal() const;

    /// Set the noise resource used to displace the terrain and rebuild the mesh.
    void set_noise(const Ref<FastNoiseLite> &p_noise);
    /// Get the noise resource used to displace the terrain.
    Ref<FastNoiseLite> get_noise() const;

    /// Assign a random seed to the noise resource, giving the terrain a new look (purely visual).
    void randomize_seed();

    /// Set the vertical scale applied to the noise sample and rebuild the mesh.
    void set_height(double p_height);
    /// Get the vertical scale applied to the noise sample.
    double get_height() const;

    /// Set the dirt albedo texture used by the perimeter walls.
    void set_wall_texture(const Ref<Texture2D> &p_texture);
    /// Get the dirt albedo texture used by the perimeter walls.
    Ref<Texture2D> get_wall_texture() const;
};

}
