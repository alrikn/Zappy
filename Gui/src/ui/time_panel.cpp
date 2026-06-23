/**
 * @file ui/time_panel.cpp
 * @brief Implementation of TimePanel.
 */

#include "ui/time_panel.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>

using namespace godot;

void TimePanel::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_time_updated", "t"), &TimePanel::on_time_updated);
}

void TimePanel::_ready()
{
    _timeLabel = get_node<Label>(NodePath("Panel/TimeLabel"));
}

void TimePanel::on_time_updated(int t)
{
    _timeLabel->set_text(String("Time: ") + String::num_int64(t));
}
