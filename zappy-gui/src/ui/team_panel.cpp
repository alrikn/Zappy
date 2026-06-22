/**
 * @file ui/team_panel.cpp
 * @brief Implementation of TeamPanel.
 */

#include "ui/team_panel.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>

using namespace godot;

void TeamPanel::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_team_registered", "name"), &TeamPanel::on_team_registered);
    ClassDB::bind_method(D_METHOD("on_player_spawned", "id", "x", "y", "orientation", "level", "team"),
        &TeamPanel::on_player_spawned);
    ClassDB::bind_method(D_METHOD("on_player_leveled", "id", "level"), &TeamPanel::on_player_leveled);
    ClassDB::bind_method(D_METHOD("on_player_died", "id"), &TeamPanel::on_player_died);
}

void TeamPanel::_ready()
{
    _teamList = get_node<ItemList>(NodePath("Panel/TeamList"));
}

void TeamPanel::on_team_registered(const String& name)
{
    _teamStats[name]; // default-constructs a zeroed TeamStats if not already present.
    refresh_display();
}

void TeamPanel::on_player_spawned(int id, int, int, int, int level, const String& team)
{
    _players[id] = {team, level};

    TeamStats& stats = _teamStats[team];
    stats.playerCount++;
    if (level > stats.maxLevel) {
        stats.maxLevel = level;
    }

    refresh_display();
}

void TeamPanel::on_player_leveled(int id, int level)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }

    it->second.second = level;

    TeamStats& stats = _teamStats[it->second.first];
    if (level > stats.maxLevel) {
        stats.maxLevel = level;
    }

    refresh_display();
}

void TeamPanel::on_player_died(int id)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }

    String team = it->second.first;
    _players.erase(it);

    auto statsIt = _teamStats.find(team);
    if (statsIt != _teamStats.end()) {
        statsIt->second.playerCount--;
    }

    // maxLevel isn't incrementally invertible: if the dying player held the
    // team's current max, the new max can only be found by rescanning the
    // team's remaining players.
    recompute_max_level(team);

    refresh_display();
}

void TeamPanel::recompute_max_level(const String& team)
{
    auto statsIt = _teamStats.find(team);
    if (statsIt == _teamStats.end()) {
        return;
    }

    int maxLevel = 0;
    for (const auto& entry : _players) {
        if (entry.second.first == team && entry.second.second > maxLevel) {
            maxLevel = entry.second.second;
        }
    }
    statsIt->second.maxLevel = maxLevel;
}

void TeamPanel::refresh_display()
{
    _teamList->clear();
    for (const auto& entry : _teamStats) {
        // A copy, not a const reference: concatenating a literal directly onto a
        // const String is ambiguous (equally-valid implicit conversion to either
        // String or StringName); a non-const String resolves unambiguously.
        String team = entry.first;
        const TeamStats& stats = entry.second;
        String row = team + ": " + String::num_int64(stats.playerCount) + " players, max level "
            + String::num_int64(stats.maxLevel);
        _teamList->add_item(row);
    }
}
