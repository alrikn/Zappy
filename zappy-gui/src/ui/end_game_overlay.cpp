/**
 * @file ui/end_game_overlay.cpp
 * @brief Implementation of EndGameOverlay.
 */

#include "ui/end_game_overlay.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>

using namespace godot;

void EndGameOverlay::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_game_over", "team"), &EndGameOverlay::on_game_over);
}

void EndGameOverlay::_ready()
{
    _winnerLabel = get_node<Label>(NodePath("WinnerLabel"));
}

void EndGameOverlay::on_game_over(const String& team)
{
    _winnerLabel->set_text("Game Over — Team " + team + " wins!");
    show();
}
