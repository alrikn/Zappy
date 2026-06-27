/**
 * @file ui/world_status_panel.cpp
 * @brief Implementation of WorldStatusPanel.
 */

#include "ui/world_status_panel.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

namespace {

/// Row node names, in resource-index order (0=food .. 6=thystame). Must match the
/// "Row<Name>" nodes in scenes/ui/world_status_panel.tscn.
constexpr const char* kResourceNodeNames[7] = {
    "Food", "Linemate", "Deraumere", "Sibur", "Mendiane", "Phiras", "Thystame",
};

/// Spin speed of the model previews, in radians per second.
constexpr double kSpinSpeed = 0.6;

} // namespace

void WorldStatusPanel::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_map_initialized", "width", "height"),
                          &WorldStatusPanel::on_map_initialized);
    ClassDB::bind_method(D_METHOD("on_tile_updated", "x", "y", "q0", "q1", "q2", "q3", "q4", "q5", "q6"),
                          &WorldStatusPanel::on_tile_updated);
    ClassDB::bind_method(D_METHOD("on_world_reset"), &WorldStatusPanel::on_world_reset);
}

void WorldStatusPanel::_ready()
{
    for (int i = 0; i < RESOURCE_COUNT; i++) {
        String row = String("Panel/VBox/Row") + kResourceNodeNames[i];
        _countLabels[i] = get_node<Label>(NodePath(row + "/Count"));
        _models[i] = get_node<MeshInstance3D>(NodePath(row + "/Icon/SubViewport/Model"));
    }

    set_process(true);
    refresh();
}

void WorldStatusPanel::_process(double delta)
{
    for (int i = 0; i < RESOURCE_COUNT; i++) {
        if (_models[i] == nullptr) {
            continue;
        }
        _models[i]->rotate_y((float)(kSpinSpeed * delta));
    }
}

void WorldStatusPanel::on_map_initialized(int width, int height)
{
    _width = width;
    _height = height;
    _tileCache.clear();
    _totals.fill(0);
    refresh();
}

void WorldStatusPanel::on_tile_updated(int x, int y, int q0, int q1, int q2, int q3, int q4, int q5, int q6)
{
    if (_width == 0 || _height == 0) {
        return;
    }
    if (x < 0 || y < 0 || x >= _width || y >= _height) {
        return;
    }

    int idx = y * _width + x;
    std::array<int32_t, RESOURCE_COUNT> next = {q0, q1, q2, q3, q4, q5, q6};

    std::array<int32_t, RESOURCE_COUNT>& cached = _tileCache[idx];
    for (int i = 0; i < RESOURCE_COUNT; i++) {
        _totals[i] += next[i] - cached[i];
    }
    cached = next;

    refresh();
}

void WorldStatusPanel::on_world_reset()
{
    _width = 0;
    _height = 0;
    _tileCache.clear();
    _totals.fill(0);
    refresh();
}

void WorldStatusPanel::refresh()
{
    for (int i = 0; i < RESOURCE_COUNT; i++) {
        if (_countLabels[i] == nullptr) {
            continue;
        }
        _countLabels[i]->set_text(String::num_int64(_totals[i]));
    }
}
