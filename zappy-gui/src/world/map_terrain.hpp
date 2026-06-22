/**
 * @file world/map_terrain.hpp
 * @brief Procedural terrain mesh sized to the server's map dimensions.
 * @details MapTerrain generates a noise-displaced plane mesh covering the
 *          server's map (msz X Y), keeping the same noise-based "smooth hills"
 *          look as the old ProceduralPlane prototype. tile_to_world() is used
 *          to place entities/resources on the terrain surface.
 */

#pragma once

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/// Terrain mesh sized to the server's map (msz X Y), keeping the same
/// noise-based "smooth hills" look as the old ProceduralPlane prototype.
/// tile_to_world() is used to place entities/resources on the terrain.
class MapTerrain : public MeshInstance3D {
    GDCLASS(MapTerrain, MeshInstance3D)

private:
    static constexpr double TILE_SIZE = 2.0;       ///< World units per map tile.
    static constexpr int SUBDIVISIONS_PER_TILE = 2; ///< Mesh subdivisions per tile.
    static constexpr int MAX_SUBDIVISIONS = 200;    ///< Clamp for very large maps.

    int _gridWidth = 50;  ///< Map width in tiles, set by set_grid_size().
    int _gridHeight = 50; ///< Map height in tiles, set by set_grid_size().

    Ref<FastNoiseLite> noise; ///< Noise source for terrain displacement; may be null (flat terrain).
    double height = 64.0;     ///< Vertical scale applied to the noise sample.

    /// Sample the noise field at world-space (x, z), scaled by height. Returns 0 if noise is unset.
    double sample_height(double x, double z) const;

    /// Estimate the surface normal at (x, z) via central differences of sample_height().
    Vector3 sample_normal(double x, double z, double epsilon_x, double epsilon_z) const;

    /// Rebuild the plane mesh from the current grid size, noise and height.
    void update_mesh();

protected:
    static void _bind_methods();

public:
    /// Resize the terrain to width x height tiles (called on map_initialized).
    void set_grid_size(int width, int height);

    /// World-space position of the center of tile (x, y), sitting on the hills.
    Vector3 tile_to_world(int x, int y) const;

    /// Set the noise resource used to displace the terrain and rebuild the mesh.
    void set_noise(const Ref<FastNoiseLite> &p_noise);
    /// Get the noise resource used to displace the terrain.
    Ref<FastNoiseLite> get_noise() const;

    /// Set the vertical scale applied to the noise sample and rebuild the mesh.
    void set_height(double p_height);
    /// Get the vertical scale applied to the noise sample.
    double get_height() const;
};

}
