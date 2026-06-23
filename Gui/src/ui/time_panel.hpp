/**
 * @file ui/time_panel.hpp
 * @brief HUD panel showing the current server time unit.
 * @details TimePanel is a thin logic layer over a scene
 *          (scenes/ui/time_panel.tscn) that provides a single Label. Read-only:
 *          it never sends commands back to the server.
 */

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>

namespace godot {

/// Displays the current time unit, updated from World's time_updated signal.
class TimePanel : public Control {
    GDCLASS(TimePanel, Control)

private:
    Label* _timeLabel = nullptr; ///< "TimeLabel" child, fetched in _ready().

protected:
    static void _bind_methods();

public:
    /// Fetch the time label child node.
    void _ready() override;

    /// Connected to World's time_updated signal: set the label to "Time: <t>".
    void on_time_updated(int t);
};

} // namespace godot
