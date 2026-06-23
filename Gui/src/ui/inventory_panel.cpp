/**
 * @file ui/inventory_panel.cpp
 * @brief Implementation of InventoryPanel.
 */

#include "ui/inventory_panel.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/node_path.hpp>

using namespace godot;

namespace {

/// Resource names in protocol order, matching server_message.hpp's q0..q6.
constexpr const char* kResourceNames[7] = {
    "Food", "Linemate", "Deraumere", "Sibur", "Mendiane", "Phiras", "Thystame"
};

} // namespace

void InventoryPanel::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("select_player", "id"), &InventoryPanel::select_player);

    ClassDB::bind_method(D_METHOD("on_player_spawned", "id", "x", "y", "orientation", "level", "team"),
        &InventoryPanel::on_player_spawned);
    ClassDB::bind_method(D_METHOD("on_player_moved", "id", "x", "y", "orientation"),
        &InventoryPanel::on_player_moved);
    ClassDB::bind_method(D_METHOD("on_player_leveled", "id", "level"), &InventoryPanel::on_player_leveled);
    ClassDB::bind_method(
        D_METHOD("on_player_inventory_updated", "id", "x", "y", "q0", "q1", "q2", "q3", "q4", "q5", "q6"),
        &InventoryPanel::on_player_inventory_updated);
    ClassDB::bind_method(D_METHOD("on_player_died", "id"), &InventoryPanel::on_player_died);

    ClassDB::bind_method(D_METHOD("on_player_list_item_selected", "index"),
        &InventoryPanel::on_player_list_item_selected);
}

void InventoryPanel::_ready()
{
    _playerList = get_node<ItemList>(NodePath("Panel/VBox/PlayerList"));
    _infoLabel = get_node<Label>(NodePath("Panel/VBox/InfoLabel"));
    _inventoryLabel = get_node<Label>(NodePath("Panel/VBox/InventoryLabel"));

    _playerList->connect("item_selected", callable_mp(this, &InventoryPanel::on_player_list_item_selected));
}

void InventoryPanel::select_player(int id)
{
    if (_players.find(id) == _players.end()) {
        _selectedId = -1;
    } else {
        _selectedId = id;
    }

    // The panel (and its player list) stays visible regardless of selection —
    // it's the only way to pick a player, so hiding it on no-selection would
    // make it impossible to ever select one.
    refresh_selected_display();
}

void InventoryPanel::on_player_spawned(int id, int x, int y, int orientation, int level, const String& team)
{
    PlayerSummary summary;
    summary.x = x;
    summary.y = y;
    summary.orientation = orientation;
    summary.level = level;
    summary.team = team;
    _players[id] = summary;

    refresh_player_list();
}

void InventoryPanel::on_player_moved(int id, int x, int y, int orientation)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }

    it->second.x = x;
    it->second.y = y;
    it->second.orientation = orientation;

    if (id == _selectedId) {
        refresh_selected_display();
    }
}

void InventoryPanel::on_player_leveled(int id, int level)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }

    it->second.level = level;
    refresh_player_list();

    if (id == _selectedId) {
        refresh_selected_display();
    }
}

void InventoryPanel::on_player_inventory_updated(
    int id, int x, int y, int q0, int q1, int q2, int q3, int q4, int q5, int q6)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }

    it->second.x = x;
    it->second.y = y;
    it->second.inventory = {q0, q1, q2, q3, q4, q5, q6};

    if (id == _selectedId) {
        refresh_selected_display();
    }
}

void InventoryPanel::on_player_died(int id)
{
    _players.erase(id);

    if (id == _selectedId) {
        select_player(-1);
    }

    refresh_player_list();
}

void InventoryPanel::on_player_list_item_selected(int index)
{
    int id = _playerList->get_item_metadata(index);
    select_player(id);
}

void InventoryPanel::refresh_player_list()
{
    _playerList->clear();

    for (const auto& entry : _players) {
        int id = entry.first;
        const PlayerSummary& summary = entry.second;
        String row = "#" + String::num_int64(id) + " (" + summary.team + ") lvl " + String::num_int64(summary.level);
        int index = _playerList->add_item(row);
        _playerList->set_item_metadata(index, id);
    }
}

void InventoryPanel::refresh_selected_display()
{
    auto it = _players.find(_selectedId);
    if (it == _players.end()) {
        _infoLabel->set_text("");
        _inventoryLabel->set_text("");
        return;
    }

    const PlayerSummary& summary = it->second;
    String info = "Team: " + summary.team + "\nLevel: " + String::num_int64(summary.level)
        + "\nPosition: (" + String::num_int64(summary.x) + ", " + String::num_int64(summary.y) + ")"
        + "\nOrientation: " + String::num_int64(summary.orientation);
    _infoLabel->set_text(info);

    String inventory;
    for (int i = 0; i < 7; i++) {
        inventory += String(kResourceNames[i]) + ": " + String::num_int64(summary.inventory[i]) + "\n";
    }
    _inventoryLabel->set_text(inventory);
}
