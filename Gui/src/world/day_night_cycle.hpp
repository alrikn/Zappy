/**
 * @file world/day_night_cycle.hpp
 * @brief Self-rotating directional light that drives an automatic day-night cycle.
 * @details It spins
 *          the light continuously in _process() so the sun sweeps a tilted arc
 *          across the sky. The WorldEnvironment/sky is parented to this node, so
 *          the sky shader (which reads LIGHT0_DIRECTION) follows the rotation and
 *          blends day -> sunset -> night -> sunrise on its own, with no input and
 *          no server connection required.
 */

#pragma once

#include <godot_cpp/classes/directional_light3d.hpp>

namespace godot {

/// DirectionalLight3D that rotates itself every frame to animate a day-night
/// cycle; cycle_duration_seconds and tilt_degrees are exported for easy tuning.
class DayNightCycle : public DirectionalLight3D {
    GDCLASS(DayNightCycle, DirectionalLight3D)

private:
    /// Seconds for one full 360 degree revolution; exported, guarded against <= 0.
    float _cycleDurationSeconds = 30.0f;
    /// Banks the arc so the sun is not straight overhead; exported (degrees).
    float _tiltDegrees = 20.0f;
    /// Freezes the cycle when true; exported, useful for debugging.
    bool _paused = false;

    /// Current rotation angle in radians, free-running and wrapped to [0, TAU).
    float _dayAngle = 0.0f;

    /// Rebuild the light's Transform3D from _dayAngle and _tiltDegrees and apply it.
    void apply_rotation();

protected:
    /// Bind methods and exported properties exposed to Godot.
    static void _bind_methods();

public:
    /// Pick a daytime starting angle and apply the initial rotation.
    void _ready() override;

    /// Advance _dayAngle by delta and reapply the rotation.
    void _process(double delta) override;

    /// Set the exported cycle_duration_seconds property (clamped to a small minimum).
    void set_cycle_duration_seconds(float seconds);
    /// Get the exported cycle_duration_seconds property.
    float get_cycle_duration_seconds() const;

    /// Set the exported tilt_degrees property.
    void set_tilt_degrees(float degrees);
    /// Get the exported tilt_degrees property.
    float get_tilt_degrees() const;

    /// Set the exported paused property.
    void set_paused(bool paused);
    /// Get the exported paused property.
    bool get_paused() const;
};

} // namespace godot
