/**
 * @file ui/end_game_overlay.hpp
 * @brief HUD overlay shown once the game ends.
 * @details EndGameOverlay is a thin logic layer over a scene
 *          (scenes/ui/end_game_overlay.tscn) that provides a semi-transparent
 *          backdrop and a winner Label. Hidden by default.
 */

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

/// Full-screen overlay shown on World's game_over signal, naming the winning team.
class EndGameOverlay : public Control {
    GDCLASS(EndGameOverlay, Control)

private:
    Label* _winnerLabel = nullptr; ///< "WinnerLabel" child, fetched in _ready().

protected:
    static void _bind_methods();

public:
    /// Fetch the winner label child node.
    void _ready() override;

    /// Connected to World's game_over signal: show the overlay with the winning team.
    void on_game_over(const String& team);
};

} // namespace godot
