/**
 * @file ui/team_panel.hpp
 * @brief HUD scoreboard: live player count and max level per team.
 * @details TeamPanel is a thin logic layer over a scene
 *          (scenes/ui/team_panel.tscn) that provides a single ItemList.
 *          Controls have no access to WorldState, so TeamPanel keeps its own
 *          minimal id->(team, level) view rebuilt from World's signals.
 */

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/variant/string.hpp>

#include <map>
#include <unordered_map>
#include <utility>

namespace godot {

/// Live per-team player count and max level, kept in sync with World's
/// team_registered/player_spawned/player_leveled/player_died signals.
class TeamPanel : public Control {
    GDCLASS(TeamPanel, Control)

private:
    /// Per-team aggregate shown in the list; keyed by team name.
    struct TeamStats {
        int playerCount = 0;
        int maxLevel = 0;
    };

    /// Seeded by on_team_registered; std::map (not unordered_map) keeps teams
    /// in sorted-by-name order for free, so refresh_display() needs no sort step.
    std::map<String, TeamStats> _teamStats;

    /// Minimal per-player record needed to update _teamStats on death: a
    /// player_died signal only carries an id, so the team and level it last
    /// held must be looked up here rather than from the signal itself.
    std::unordered_map<int, std::pair<String, int>> _players;

    ItemList* _teamList = nullptr; ///< "Panel/TeamList" child, fetched in _ready().

    /// Rebuild every row of _teamList from _teamStats, sorted by team name.
    void refresh_display();

    /// Recompute _teamStats[team].maxLevel by scanning _players for that team.
    void recompute_max_level(const String& team);

protected:
    static void _bind_methods();

public:
    void _ready() override;

    /// Connected to World's team_registered signal: reserve a stats entry.
    void on_team_registered(const String& name);
    /// Connected to World's player_spawned signal: count the new player.
    void on_player_spawned(int id, int x, int y, int orientation, int level, const String& team);
    /// Connected to World's player_leveled signal: update max level.
    void on_player_leveled(int id, int level);
    /// Connected to World's player_died signal: decrement count, rescan max level.
    void on_player_died(int id);
};

} // namespace godot
