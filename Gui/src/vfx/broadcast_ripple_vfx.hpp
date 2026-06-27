/**
 * @file vfx/broadcast_ripple_vfx.hpp
 * @brief One-shot, ground-conforming ripple played when a player broadcasts.
 * @details BroadcastRippleVfx draws RING_COUNT concentric rings that expand
 *          outward and fade over ~1 second, then frees itself. Each ring is a
 *          thin annulus rebuilt every frame from RING_SEGMENTS segments whose Y
 *          is sampled from the terrain, so the rings hug the hills instead of
 *          clipping into them. The material is unshaded + alpha-blended (the
 *          gl_compatibility renderer has no glow bloom), tinted with the
 *          broadcasting player's team color.
 */

#pragma once

#include "world/map_terrain.hpp"

#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <array>

namespace godot {

/// A self-freeing, team-colored ground ripple spawned on a broadcaster's tile.
class BroadcastRippleVfx : public Node3D {
    GDCLASS(BroadcastRippleVfx, Node3D)

private:
    static constexpr int RING_COUNT = 1;          ///< Number of concentric rings.
    static constexpr double SEGMENT_ARC = 1.5;    ///< Target world length per segment; segments scale with radius.
    static constexpr int MIN_SEGMENTS = 24;       ///< Lower bound on a ring's segment count (small rings).
    static constexpr int MAX_SEGMENTS = 360;      ///< Upper bound on a ring's segment count (map-wide rings).
    static constexpr double TAU = 6.283185307179586; ///< 2*pi, for the angular sweep.
    static constexpr double RING_STAGGER = 0.4;   ///< Seconds between successive rings starting (their spacing).
    static constexpr double RING_LIFE = 2.6;      ///< Seconds for one ring to cross the map and fade out.
    static constexpr double START_RADIUS = 0.3;   ///< Ring radius at birth, world units.
    static constexpr double FALLBACK_MAX_RADIUS = 30.0; ///< Max radius when no terrain is known (flat fallback).
    static constexpr double RING_WIDTH = 0.25;    ///< Constant band thickness, world units (thin vs. ring spacing).
    static constexpr double LIFT = 0.08;          ///< Height above terrain, avoids z-fighting.
    static constexpr double BASE_ALPHA = 0.85;    ///< Ring opacity at birth.
    /// Total lifetime: last ring starts at RING_STAGGER*(RING_COUNT-1) and lives RING_LIFE.
    static constexpr double TOTAL = RING_STAGGER * (RING_COUNT - 1) + RING_LIFE;

    /// One ring's render resources; its ImmediateMesh is regenerated every frame.
    struct Ring {
        MeshInstance3D* instance = nullptr;
        Ref<ImmediateMesh> mesh;
        Ref<StandardMaterial3D> material;
    };

    std::array<Ring, RING_COUNT> _rings;
    Color _teamColor = Color(1, 1, 1); ///< Tint, set by configure().
    MapTerrain* _terrain = nullptr;    ///< Height source for ground conforming; may be null (flat).
    double _maxRadius = FALLBACK_MAX_RADIUS; ///< Fully-expanded radius; map diagonal when terrain is known.
    double _elapsed = 0.0;             ///< Seconds since spawn.

    /// Local-space point at polar (radius, angle) around this node's center,
    /// with Y lifted to the terrain surface so the ring hugs the hills.
    Vector3 ground_point(double radius, double angle, const Vector3& center) const;

    /// Rebuild ring i's geometry/alpha for normalized life t; t outside [0,1] clears it.
    void rebuild_ring(int i, double t, const Vector3& center);

protected:
    static void _bind_methods();

public:
    /// Build the ring meshes/materials and attach them as children.
    void _ready() override;

    /// Advance the ripple, regenerate ring geometry, and free self when done.
    void _process(double delta) override;

    /// Provide the terrain (for ground-conforming height) and the tint color.
    /// Called by EntityManager right after add_child().
    void configure(MapTerrain* terrain, const Color& color);
};

} // namespace godot
