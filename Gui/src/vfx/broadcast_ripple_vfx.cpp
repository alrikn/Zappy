/**
 * @file vfx/broadcast_ripple_vfx.cpp
 * @brief Implementation of BroadcastRippleVfx.
 */

#include "vfx/broadcast_ripple_vfx.hpp"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <cmath>

using namespace godot;

void BroadcastRippleVfx::_bind_methods()
{
    // configure() is called directly from EntityManager (C++), and the rings are
    // built at runtime, so nothing needs to be exposed to Godot/GDScript here.
}

/// Build one unshaded, alpha-blended, double-sided mesh per ring and attach it.
void BroadcastRippleVfx::_ready()
{
    for (int i = 0; i < RING_COUNT; i++) {
        Ring& r = _rings[i];

        r.material.instantiate();
        r.material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
        r.material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        r.material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
        r.material->set_albedo(Color(_teamColor.r, _teamColor.g, _teamColor.b, 0.0f));

        r.mesh.instantiate();

        r.instance = memnew(MeshInstance3D);
        r.instance->set_mesh(r.mesh);
        r.instance->set_material_override(r.material);
        r.instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        add_child(r.instance);
    }
}

void BroadcastRippleVfx::_process(double delta)
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    _elapsed += delta;

    Vector3 center = get_global_position();
    for (int i = 0; i < RING_COUNT; i++) {
        double t = (_elapsed - i * RING_STAGGER) / RING_LIFE;
        rebuild_ring(i, t, center);
    }

    if (_elapsed >= TOTAL) {
        queue_free();
    }
}

void BroadcastRippleVfx::configure(MapTerrain* terrain, const Color& color)
{
    _terrain = terrain;
    _teamColor = color;
    if (_terrain != nullptr) {
        // Reach every corner from any tile, so the ripple crosses the whole map.
        _maxRadius = _terrain->get_world_diagonal();
    }
}

/// Sample the terrain at world (center + offset) and return the point in this
/// node's local space, lifted by LIFT. Falls back to a flat LIFT when no terrain.
Vector3 BroadcastRippleVfx::ground_point(double radius, double angle, const Vector3& center) const
{
    double lx = radius * std::cos(angle);
    double lz = radius * std::sin(angle);
    double ly = LIFT;
    if (_terrain != nullptr) {
        ly = _terrain->get_height_at(center.x + lx, center.z + lz) - center.y + LIFT;
    }
    return Vector3((float)lx, (float)ly, (float)lz);
}

/// Regenerate ring i as a thin ground-hugging band; t<0 (not yet born) or t>1
/// (already faded) leaves the mesh empty so the ring is invisible.
void BroadcastRippleVfx::rebuild_ring(int i, double t, const Vector3& center)
{
    Ring& r = _rings[i];
    r.mesh->clear_surfaces();
    if (t < 0.0 || t > 1.0) {
        return;
    }

    double meanRadius = START_RADIUS + (_maxRadius - START_RADIUS) * t;
    double inner = meanRadius - RING_WIDTH * 0.5;
    double outer = meanRadius + RING_WIDTH * 0.5;
    if (inner < 0.0) {
        inner = 0.0;
    }
    double alpha = BASE_ALPHA * (1.0 - t);
    r.material->set_albedo(Color(_teamColor.r, _teamColor.g, _teamColor.b, (float)alpha));

    // Scale the segment count with the ring's circumference so it stays smooth
    // and hugs the terrain accurately whether it's tiny or spanning the map.
    int segments = (int)std::lround(TAU * meanRadius / SEGMENT_ARC);
    if (segments < MIN_SEGMENTS) {
        segments = MIN_SEGMENTS;
    }
    if (segments > MAX_SEGMENTS) {
        segments = MAX_SEGMENTS;
    }

    // Triangle strip alternating inner/outer vertices around the full circle.
    r.mesh->surface_begin(Mesh::PRIMITIVE_TRIANGLE_STRIP);
    for (int s = 0; s <= segments; s++) {
        double angle = (TAU * s) / segments;
        r.mesh->surface_add_vertex(ground_point(inner, angle, center));
        r.mesh->surface_add_vertex(ground_point(outer, angle, center));
    }
    r.mesh->surface_end();
}
