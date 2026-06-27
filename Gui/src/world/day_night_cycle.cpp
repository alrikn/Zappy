/**
 * @file world/day_night_cycle.cpp
 * @brief Implementation of DayNightCycle.
 */

#include "world/day_night_cycle.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

void DayNightCycle::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_cycle_duration_seconds", "seconds"), &DayNightCycle::set_cycle_duration_seconds);
    ClassDB::bind_method(D_METHOD("get_cycle_duration_seconds"), &DayNightCycle::get_cycle_duration_seconds);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cycle_duration_seconds"),
                 "set_cycle_duration_seconds", "get_cycle_duration_seconds");

    ClassDB::bind_method(D_METHOD("set_tilt_degrees", "degrees"), &DayNightCycle::set_tilt_degrees);
    ClassDB::bind_method(D_METHOD("get_tilt_degrees"), &DayNightCycle::get_tilt_degrees);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tilt_degrees"), "set_tilt_degrees", "get_tilt_degrees");

    ClassDB::bind_method(D_METHOD("set_paused", "paused"), &DayNightCycle::set_paused);
    ClassDB::bind_method(D_METHOD("get_paused"), &DayNightCycle::get_paused);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "get_paused");
}

/**
 * @brief Pick a daytime starting angle and apply the initial rotation.
 * @details Starting a quarter-turn in puts the sun high in the sky at launch,
 *          so the scene opens in daylight rather than at night.
 */
void DayNightCycle::_ready()
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    _dayAngle = (float)(Math_PI * 0.5);
    apply_rotation();
}

/**
 * @brief Advance _dayAngle by delta and reapply the rotation.
 * @details Angular speed is TAU / cycle_duration_seconds so one revolution takes
 *          exactly cycle_duration_seconds. The angle is wrapped to [0, TAU) to
 *          avoid unbounded growth over long sessions.
 */
void DayNightCycle::_process(double delta)
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    if (_paused) {
        return;
    }

    double speed = Math_TAU / (double)_cycleDurationSeconds;
    _dayAngle = (float)Math::fposmod(_dayAngle + speed * delta, Math_TAU);
    apply_rotation();
}

/**
 * @brief Rebuild the light's Transform3D from _dayAngle and _tiltDegrees.
 * @details The inner rotation about X sweeps the light direction's elevation
 *          (LIGHT0_DIRECTION.y) up and down, which the sky shader uses to blend
 *          day/night. The outer tilt about Z banks the whole arc so the sun is
 *          not straight overhead. The original (0, 10, 10) position is preserved.
 */
void DayNightCycle::apply_rotation()
{
    double tilt_rad = Math::deg_to_rad((double)_tiltDegrees);
    Basis basis = Basis(Vector3(0.0f, 0.0f, 1.0f), tilt_rad)
                * Basis(Vector3(1.0f, 0.0f, 0.0f), (double)_dayAngle);

    set_transform(Transform3D(basis, Vector3(0.0f, 10.0f, 10.0f)));
}

void DayNightCycle::set_cycle_duration_seconds(float seconds)
{
    _cycleDurationSeconds = Math::max(seconds, 0.1f);
}

float DayNightCycle::get_cycle_duration_seconds() const
{
    return _cycleDurationSeconds;
}

void DayNightCycle::set_tilt_degrees(float degrees)
{
    _tiltDegrees = degrees;
}

float DayNightCycle::get_tilt_degrees() const
{
    return _tiltDegrees;
}

void DayNightCycle::set_paused(bool paused)
{
    _paused = paused;
}

bool DayNightCycle::get_paused() const
{
    return _paused;
}
