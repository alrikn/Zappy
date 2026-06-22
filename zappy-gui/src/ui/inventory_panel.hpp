/**
 * @file ui/inventory_panel.hpp
 * @brief HUD panel showing one player's live position/level/inventory.
 * @details InventoryPanel is a thin logic layer over a scene
 *          (scenes/ui/inventory_panel.tscn) that provides a player-picker
 *          ItemList plus info/inventory Labels. There is no 3D click-selection
 *          yet, so the picker list is the stand-in selection
 *          mechanism; select_player() is the public entry point a
 *          future SelectionController will call directly once real
 *          click-picking exists. Controls have no access to WorldState, so
 *          this panel keeps its own minimal per-player view rebuilt from
 *          World's signals.
 */

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/variant/string.hpp>

#include <array>
#include <unordered_map>

namespace godot {

/// Player picker + live inventory/position/level display for one selected
/// live player, kept in sync with World's player_* signals.
class InventoryPanel : public Control {
    GDCLASS(InventoryPanel, Control)

private:
    /// Independent live view of one player, since this Control cannot reach WorldState.
    struct PlayerSummary {
        int x = 0, y = 0, orientation = 0, level = 1;
        String team;
        std::array<int, 7> inventory{};
    };

    std::unordered_map<int, PlayerSummary> _players; ///< Live players, keyed by id.
    int _selectedId = -1;                            ///< -1 = no selection; detail labels stay empty.

    ItemList* _playerList = nullptr;     ///< "Panel/VBox/PlayerList" child, fetched in _ready().
    Label* _infoLabel = nullptr;         ///< "Panel/VBox/InfoLabel" child, fetched in _ready().
    Label* _inventoryLabel = nullptr;    ///< "Panel/VBox/InventoryLabel" child, fetched in _ready().

    /// Rebuild _playerList rows from _players, sorted by id; each row's
    /// metadata holds the player id for on_player_list_item_selected().
    void refresh_player_list();

    /// Rebuild _infoLabel/_inventoryLabel from _players[_selectedId].
    void refresh_selected_display();

protected:
    static void _bind_methods();

public:
    void _ready() override;

    /// Select a player to display by id. No-op (clears the display) if id
    /// is not a currently-live player. Public entry point for a
    /// future click-based selection to call directly.
    void select_player(int id);

    /// Connected to World's player_spawned signal.
    void on_player_spawned(int id, int x, int y, int orientation, int level, const String& team);
    /// Connected to World's player_moved signal.
    void on_player_moved(int id, int x, int y, int orientation);
    /// Connected to World's player_leveled signal.
    void on_player_leveled(int id, int level);
    /// Connected to World's player_inventory_updated signal.
    void on_player_inventory_updated(int id, int x, int y, int q0, int q1, int q2, int q3, int q4, int q5, int q6);
    /// Connected to World's player_died signal.
    void on_player_died(int id);

    /// Connected in _ready() to the player list's own "item_selected" signal.
    void on_player_list_item_selected(int index);
};

} // namespace godot
