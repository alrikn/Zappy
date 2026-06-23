/**
 * @file ui/hud_root.hpp
 * @brief Pure layout container for the HUD overlay.
 * @details HudRoot has no signal-handling logic of its own — each panel
 *          instanced under it (scenes/ui/hud_root.tscn) is wired directly to
 *          ZappyWorld's signals in world.tscn's [connection] blocks, the same
 *          pattern EntityManager/TileMarkers/MapTerrain already use.
 */

#pragma once

#include <godot_cpp/classes/control.hpp>

namespace godot {

/// Full-rect layout container instancing the HUD panels; carries no logic.
class HudRoot : public Control {
    GDCLASS(HudRoot, Control)

protected:
    static void _bind_methods();

public:
    void _ready() override;
};

} // namespace godot
