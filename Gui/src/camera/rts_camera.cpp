/**
 * @file camera/rts_camera.cpp
 * @brief Implementation of RtsCamera.
 */

#include "camera/rts_camera.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void RtsCamera::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_map_initialized", "width", "height"), &RtsCamera::on_map_initialized);

    ClassDB::bind_method(D_METHOD("set_pitch_degrees", "degrees"), &RtsCamera::set_pitch_degrees);
    ClassDB::bind_method(D_METHOD("get_pitch_degrees"), &RtsCamera::get_pitch_degrees);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_degrees"), "set_pitch_degrees", "get_pitch_degrees");

    ClassDB::bind_method(D_METHOD("set_allow_yaw_rotation", "enabled"), &RtsCamera::set_allow_yaw_rotation);
    ClassDB::bind_method(D_METHOD("get_allow_yaw_rotation"), &RtsCamera::get_allow_yaw_rotation);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_yaw_rotation"), "set_allow_yaw_rotation", "get_allow_yaw_rotation");

    ClassDB::bind_method(D_METHOD("set_yaw_speed", "degrees_per_second"), &RtsCamera::set_yaw_speed);
    ClassDB::bind_method(D_METHOD("get_yaw_speed"), &RtsCamera::get_yaw_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "yaw_speed"), "set_yaw_speed", "get_yaw_speed");

    ClassDB::bind_method(D_METHOD("set_zoom_size", "size"), &RtsCamera::set_zoom_size);
    ClassDB::bind_method(D_METHOD("get_zoom_size"), &RtsCamera::get_zoom_size);

    ClassDB::bind_method(D_METHOD("set_pan_speed", "speed"), &RtsCamera::set_pan_speed);
    ClassDB::bind_method(D_METHOD("get_pan_speed"), &RtsCamera::get_pan_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pan_speed"), "set_pan_speed", "get_pan_speed");

    ClassDB::bind_method(D_METHOD("set_min_size", "size"), &RtsCamera::set_min_size);
    ClassDB::bind_method(D_METHOD("get_min_size"), &RtsCamera::get_min_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_size"), "set_min_size", "get_min_size");

    ClassDB::bind_method(D_METHOD("set_max_size", "size"), &RtsCamera::set_max_size);
    ClassDB::bind_method(D_METHOD("get_max_size"), &RtsCamera::get_max_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_size"), "set_max_size", "get_max_size");

    ClassDB::bind_method(D_METHOD("set_margin_factor", "factor"), &RtsCamera::set_margin_factor);
    ClassDB::bind_method(D_METHOD("get_margin_factor"), &RtsCamera::get_margin_factor);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin_factor"), "set_margin_factor", "get_margin_factor");

    ClassDB::bind_method(D_METHOD("set_map_terrain_path", "path"), &RtsCamera::set_map_terrain_path);
    ClassDB::bind_method(D_METHOD("get_map_terrain_path"), &RtsCamera::get_map_terrain_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "map_terrain_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "MapTerrain"),
                 "set_map_terrain_path", "get_map_terrain_path");
}

/**
 * @brief Switch to orthogonal projection and resolve the MapTerrain reference.
 * @details Applies the default pitch/yaw/size immediately, so the view is
 *          already correctly framed even before on_map_initialized() runs.
 */
void RtsCamera::_ready()
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    set_projection(Camera3D::PROJECTION_ORTHOGONAL);
    _mapTerrain = get_node<MapTerrain>(_mapTerrainPath);
    apply_view();
}

/**
 * @brief Poll WASD/arrow keys to pan, and Q/E to rotate, continuously while held.
 * @details Pan is scaled by _size so screen-space pan speed stays constant
 *          regardless of zoom level. Yaw has no snapping or easing: it tracks
 *          the held key directly at yaw_speed degrees/sec.
 */
void RtsCamera::_process(double delta)
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    Input* input = Input::get_singleton();
    bool changed = false;

    if (_allowYawRotation) {
        if (input->is_physical_key_pressed(KEY_Q)) {
            _yawDegrees -= _yawSpeed * (float)delta;
            changed = true;
        }
        if (input->is_physical_key_pressed(KEY_E)) {
            _yawDegrees += _yawSpeed * (float)delta;
            changed = true;
        }
    }

    double yaw_rad = Math::deg_to_rad((double)_yawDegrees);
    Vector3 forward_ground((float)std::sin(yaw_rad), 0.0f, (float)-std::cos(yaw_rad));
    Vector3 right_ground((float)std::cos(yaw_rad), 0.0f, (float)std::sin(yaw_rad));

    Vector3 move;
    if (input->is_physical_key_pressed(KEY_W) || input->is_physical_key_pressed(KEY_UP))
        move += forward_ground;
    if (input->is_physical_key_pressed(KEY_S) || input->is_physical_key_pressed(KEY_DOWN))
        move -= forward_ground;
    if (input->is_physical_key_pressed(KEY_D) || input->is_physical_key_pressed(KEY_RIGHT))
        move += right_ground;
    if (input->is_physical_key_pressed(KEY_A) || input->is_physical_key_pressed(KEY_LEFT))
        move -= right_ground;

    if (move.length_squared() > 0.0f) {
        float speed = _panSpeed * (float)delta * (_size / REFERENCE_SIZE);
        _panTarget += move.normalized() * speed;
        clamp_pan_target();
        changed = true;
    }

    if (changed) {
        apply_view();
    }
}

/**
 * @brief Handle middle-mouse-drag panning and scroll-wheel zoom.
 */
void RtsCamera::_unhandled_input(const Ref<InputEvent>& event)
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    if (InputEventMouseButton* mb = Object::cast_to<InputEventMouseButton>(event.ptr())) {
        if (mb->get_button_index() == MOUSE_BUTTON_MIDDLE) {
            _dragging = mb->is_pressed();
            _lastMousePosition = mb->get_position();
        } else if (mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP) {
            set_zoom_size(_size * 0.9f);
        } else if (mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) {
            set_zoom_size(_size * 1.1f);
        }
        return;
    }

    if (InputEventMouseMotion* mm = Object::cast_to<InputEventMouseMotion>(event.ptr())) {
        if (!_dragging)
            return;

        Vector2 viewport_size = get_viewport()->get_visible_rect().size;
        if (viewport_size.y <= 0.0f)
            return;

        float units_per_pixel = _size / viewport_size.y;
        Vector2 delta = mm->get_relative();

        double yaw_rad = Math::deg_to_rad((double)_yawDegrees);
        Vector3 forward_ground((float)std::sin(yaw_rad), 0.0f, (float)-std::cos(yaw_rad));
        Vector3 right_ground((float)std::cos(yaw_rad), 0.0f, (float)std::sin(yaw_rad));

        _panTarget -= right_ground * (delta.x * units_per_pixel);
        _panTarget += forward_ground * (delta.y * units_per_pixel);
        clamp_pan_target();
        apply_view();
    }
}

/**
 * @brief Frame the whole map and reset the pan/zoom state.
 * @details The map is centered on the world origin by MapTerrain::tile_to_world(),
 *          so the initial pan target is the origin, not a map corner.
 */
void RtsCamera::on_map_initialized(int width, int height)
{
    double tile_size = MapTerrain::get_tile_size();
    _gridHalfWidth = (float)(width * tile_size / 2.0);
    _gridHalfHeight = (float)(height * tile_size / 2.0);

    _panTarget = Vector3(0.0f, 0.0f, 0.0f);
    _size = std::clamp((float)(std::max(width, height) * tile_size * _marginFactor), _minSize, _maxSize);
    apply_view();
}

/**
 * @brief Recompute the camera's world transform from _panTarget/_yawDegrees/_pitchDegrees/_size.
 */
void RtsCamera::apply_view()
{
    double yaw_rad = Math::deg_to_rad((double)_yawDegrees);
    double pitch_rad = Math::deg_to_rad((double)_pitchDegrees);

    Vector3 forward(
        (float)(std::sin(yaw_rad) * std::cos(pitch_rad)),
        (float)std::sin(pitch_rad),
        (float)(-std::cos(yaw_rad) * std::cos(pitch_rad)));

    float distance = _size * DISTANCE_FACTOR;
    set_position(_panTarget - forward * distance);
    look_at(_panTarget, Vector3(0.0f, 1.0f, 0.0f));
    set_size(_size);
}

/// Clamp _panTarget.x/.z so the camera can't pan past the map's edges.
void RtsCamera::clamp_pan_target()
{
    _panTarget.x = std::clamp(_panTarget.x, -_gridHalfWidth, _gridHalfWidth);
    _panTarget.z = std::clamp(_panTarget.z, -_gridHalfHeight, _gridHalfHeight);
}

void RtsCamera::set_pitch_degrees(float degrees)
{
    _pitchDegrees = degrees;
    apply_view();
}

float RtsCamera::get_pitch_degrees() const
{
    return _pitchDegrees;
}

void RtsCamera::set_allow_yaw_rotation(bool enabled)
{
    _allowYawRotation = enabled;
}

bool RtsCamera::get_allow_yaw_rotation() const
{
    return _allowYawRotation;
}

void RtsCamera::set_yaw_speed(float degrees_per_second)
{
    _yawSpeed = degrees_per_second;
}

float RtsCamera::get_yaw_speed() const
{
    return _yawSpeed;
}

void RtsCamera::set_zoom_size(float size)
{
    _size = std::clamp(size, _minSize, _maxSize);
    apply_view();
}

float RtsCamera::get_zoom_size() const
{
    return _size;
}

void RtsCamera::set_pan_speed(float speed)
{
    _panSpeed = speed;
}

float RtsCamera::get_pan_speed() const
{
    return _panSpeed;
}

void RtsCamera::set_min_size(float size)
{
    _minSize = size;
}

float RtsCamera::get_min_size() const
{
    return _minSize;
}

void RtsCamera::set_max_size(float size)
{
    _maxSize = size;
}

float RtsCamera::get_max_size() const
{
    return _maxSize;
}

void RtsCamera::set_margin_factor(float factor)
{
    _marginFactor = factor;
}

float RtsCamera::get_margin_factor() const
{
    return _marginFactor;
}

void RtsCamera::set_map_terrain_path(const NodePath& path)
{
    _mapTerrainPath = path;
}

NodePath RtsCamera::get_map_terrain_path() const
{
    return _mapTerrainPath;
}
