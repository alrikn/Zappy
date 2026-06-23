/**
 * @file camera/rts_camera.hpp
 * @brief Orthographic, fixed-pitch RTS-style camera for the world view.
 * @details RtsCamera never changes its elevation angle: only its ground-plane pan target,
 *          its orthographic size (zoom), and its yaw (continuously rotated
 *          by holding Q/E) respond to input, keeping the map's silhouette
 *          consistently readable from any zoom level or facing.
 */

#pragma once

#include "world/map_terrain.hpp"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/// Orthographic camera panned by keyboard/mouse-drag, zoomed by the scroll
/// wheel, and rotated continuously while Q/E is held; pitch is fixed.
class RtsCamera : public Camera3D {
    GDCLASS(RtsCamera, Camera3D)

private:
    /// Baseline orthographic size used to scale pan_speed so screen-space
    /// pan speed stays constant across zoom levels.
    static constexpr float REFERENCE_SIZE = 20.0f;
    /// Distance the camera trails behind its pan target along its view ray,
    /// expressed as a multiple of size; has no effect on the orthographic
    /// image, it only needs to keep the camera outside the terrain mesh.
    static constexpr float DISTANCE_FACTOR = 3.0f;

    float _pitchDegrees = -35.0f; ///< Exported property: fixed elevation angle, set once.
    bool _allowYawRotation = true; ///< Exported property: enables the Q/E yaw rotation.
    float _yawSpeed = 90.0f;     ///< Exported property: yaw rotation speed in degrees/sec while Q/E is held.
    float _yawDegrees = 0.0f;    ///< Current yaw, free-running (not snapped to any increment).

    Vector3 _panTarget;  ///< Ground point the camera looks at.
    float _size = REFERENCE_SIZE; ///< Mirrors Camera3D.size; exported as the zoom parameter.
    float _panSpeed = 10.0f;  ///< Exported property: pan speed in world units/sec at REFERENCE_SIZE.
    float _minSize = 3.0f;    ///< Exported property: closest zoom-in.
    float _maxSize = 80.0f;   ///< Exported property: farthest zoom-out.
    float _marginFactor = 1.1f; ///< Exported property: extra margin applied by on_map_initialized().

    NodePath _mapTerrainPath;       ///< Exported property: path to the MapTerrain node.
    MapTerrain* _mapTerrain = nullptr; ///< Resolved from _mapTerrainPath in _ready().

    float _gridHalfWidth = 0.0f;  ///< Half the map's world-space width, for pan clamping.
    float _gridHalfHeight = 0.0f; ///< Half the map's world-space height, for pan clamping.

    bool _dragging = false;       ///< True while the middle mouse button is held.
    Vector2 _lastMousePosition;   ///< Last mouse position seen during a drag.

    /// Move the camera to _panTarget, looking along the current yaw/pitch,
    /// and push _size into Camera3D's own size property.
    void apply_view();

    /// Clamp _panTarget.x/.z to the map's world-space bounds (a no-op before
    /// on_map_initialized() has run, since the bounds default to zero).
    void clamp_pan_target();

protected:
    /// Bind methods, properties and signals exposed to Godot/GDScript.
    static void _bind_methods();

public:
    /// Switch to orthogonal projection and resolve the MapTerrain reference.
    void _ready() override;

    /// Poll WASD/arrow keys to pan the camera.
    void _process(double delta) override;

    /// Handle middle-mouse-drag panning, scroll-wheel zoom, and Q/E yaw snap.
    void _unhandled_input(const Ref<InputEvent>& event) override;

    /// Frame the whole map: center the pan target and pick a size that fits
    /// width x height tiles plus margin_factor. Connected to World's
    /// map_initialized signal.
    void on_map_initialized(int width, int height);

    /// Set the exported pitch_degrees property.
    void set_pitch_degrees(float degrees);
    /// Get the exported pitch_degrees property.
    float get_pitch_degrees() const;

    /// Set the exported allow_yaw_rotation property.
    void set_allow_yaw_rotation(bool enabled);
    /// Get the exported allow_yaw_rotation property.
    bool get_allow_yaw_rotation() const;

    /// Set the exported yaw_speed property.
    void set_yaw_speed(float degrees_per_second);
    /// Get the exported yaw_speed property.
    float get_yaw_speed() const;

    /// Set the zoom (Camera3D.size) and refresh the view.
    void set_zoom_size(float size);
    /// Get the current zoom (Camera3D.size).
    float get_zoom_size() const;

    /// Set the exported pan_speed property.
    void set_pan_speed(float speed);
    /// Get the exported pan_speed property.
    float get_pan_speed() const;

    /// Set the exported min_size property.
    void set_min_size(float size);
    /// Get the exported min_size property.
    float get_min_size() const;

    /// Set the exported max_size property.
    void set_max_size(float size);
    /// Get the exported max_size property.
    float get_max_size() const;

    /// Set the exported margin_factor property.
    void set_margin_factor(float factor);
    /// Get the exported margin_factor property.
    float get_margin_factor() const;

    /// Set the exported map_terrain_path property.
    void set_map_terrain_path(const NodePath& path);
    /// Get the exported map_terrain_path property.
    NodePath get_map_terrain_path() const;
};

} // namespace godot
