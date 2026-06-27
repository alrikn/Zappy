/**
 * @file ui/world_status_panel.hpp
 * @brief HUD panel showing the total quantity of each resource on the map.
 * @details WorldStatusPanel is a thin logic layer over a scene
 *          (scenes/ui/world_status_panel.tscn). It keeps a running total of
 *          each of the 7 resources lying on the ground, aggregated from World's
 *          tile_updated signal, and drives a slow spin on the per-resource model
 *          previews rendered by the scene's SubViewports.
 */

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>

namespace godot {

/// Live world-resource scoreboard: per-resource ground totals with rotating
/// 3D-model icons, kept in sync with World's map_initialized/tile_updated/
/// world_reset signals.
class WorldStatusPanel : public Control {
    GDCLASS(WorldStatusPanel, Control)

private:
    static constexpr int RESOURCE_COUNT = 7;

    /// Running total of each resource currently on the ground (index 0=food ..
    /// 6=thystame). int64 so a large map cannot overflow the sum.
    std::array<int64_t, RESOURCE_COUNT> _totals{};

    /// Last-known per-tile quantities, keyed by y*width + x. Lets tile_updated
    /// (which carries absolute counts) be folded into _totals as a delta.
    std::unordered_map<int, std::array<int32_t, RESOURCE_COUNT>> _tileCache;

    int _width = 0;  ///< Map width in tiles, from map_initialized.
    int _height = 0; ///< Map height in tiles, from map_initialized.

    std::array<Label*, RESOURCE_COUNT> _countLabels{};   ///< "Count" label per row.
    std::array<MeshInstance3D*, RESOURCE_COUNT> _models{}; ///< "Model" mesh per row, spun in _process.

    /// Push _totals into the count labels.
    void refresh();

protected:
    static void _bind_methods();

public:
    /// Fetch the 7 count labels and 7 preview models, then enable processing.
    void _ready() override;

    /// Slowly spin each resource model preview about its vertical axis.
    void _process(double delta) override;

    /// Connected to World's map_initialized: store map size and reset all totals.
    void on_map_initialized(int width, int height);

    /// Connected to World's tile_updated: fold a tile's new absolute counts into the totals.
    void on_tile_updated(int x, int y, int q0, int q1, int q2, int q3, int q4, int q5, int q6);

    /// Connected to World's world_reset: clear all totals (new game / reconnect).
    void on_world_reset();
};

} // namespace godot
