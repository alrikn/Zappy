/**
 * @file entities/entity_manager.cpp
 * @brief Implementation of EntityManager.
 */

#include "entities/entity_manager.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <cstdint>

using namespace godot;

namespace {

/// Packs a tile coordinate into a single key for _incantingByTile.
int64_t tile_key(int x, int y)
{
    return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
}

} // namespace

void EntityManager::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_map_terrain_path", "path"), &EntityManager::set_map_terrain_path);
    ClassDB::bind_method(D_METHOD("get_map_terrain_path"), &EntityManager::get_map_terrain_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "map_terrain_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "MapTerrain"),
                  "set_map_terrain_path", "get_map_terrain_path");

    ClassDB::bind_method(D_METHOD("set_player_entity_scene", "scene"), &EntityManager::set_player_entity_scene);
    ClassDB::bind_method(D_METHOD("get_player_entity_scene"), &EntityManager::get_player_entity_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "player_entity_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
                  "set_player_entity_scene", "get_player_entity_scene");

    ClassDB::bind_method(D_METHOD("set_egg_entity_scene", "scene"), &EntityManager::set_egg_entity_scene);
    ClassDB::bind_method(D_METHOD("get_egg_entity_scene"), &EntityManager::get_egg_entity_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "egg_entity_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
                  "set_egg_entity_scene", "get_egg_entity_scene");

    ClassDB::bind_method(D_METHOD("on_team_registered", "name"), &EntityManager::on_team_registered);
    ClassDB::bind_method(D_METHOD("clear_all"), &EntityManager::clear_all);

    ClassDB::bind_method(D_METHOD("on_player_spawned", "id", "x", "y", "orientation", "level", "team"),
                          &EntityManager::on_player_spawned);
    ClassDB::bind_method(D_METHOD("on_player_moved", "id", "x", "y", "orientation"), &EntityManager::on_player_moved);
    ClassDB::bind_method(D_METHOD("on_player_leveled", "id", "level"), &EntityManager::on_player_leveled);
    ClassDB::bind_method(D_METHOD("on_player_died", "id"), &EntityManager::on_player_died);

    ClassDB::bind_method(D_METHOD("on_egg_laid", "egg_id", "player_id", "x", "y"), &EntityManager::on_egg_laid);
    ClassDB::bind_method(D_METHOD("on_egg_removed", "egg_id"), &EntityManager::on_egg_removed);

    ClassDB::bind_method(D_METHOD("on_incantation_started", "x", "y", "level", "ids"),
                          &EntityManager::on_incantation_started);
    ClassDB::bind_method(D_METHOD("on_incantation_ended", "x", "y", "result"), &EntityManager::on_incantation_ended);
}

/// Initializes the fixed 8-color team palette.
EntityManager::EntityManager()
{
    _palette = {
        Color(0.9f, 0.2f, 0.2f), // red
        Color(0.2f, 0.4f, 0.9f), // blue
        Color(0.2f, 0.8f, 0.3f), // green
        Color(0.9f, 0.8f, 0.1f), // yellow
        Color(0.8f, 0.2f, 0.8f), // magenta
        Color(0.1f, 0.8f, 0.8f), // cyan
        Color(0.9f, 0.5f, 0.1f), // orange
        Color(0.9f, 0.9f, 0.9f), // white
    };
}

/// Resolve map_terrain_path.
void EntityManager::_ready()
{
    _mapTerrain = get_node<MapTerrain>(_mapTerrainPath);
}

void EntityManager::set_map_terrain_path(const NodePath& path)
{
    _mapTerrainPath = path;
}

NodePath EntityManager::get_map_terrain_path() const
{
    return _mapTerrainPath;
}

void EntityManager::set_player_entity_scene(const Ref<PackedScene>& scene)
{
    _playerEntityScene = scene;
}

Ref<PackedScene> EntityManager::get_player_entity_scene() const
{
    return _playerEntityScene;
}

void EntityManager::set_egg_entity_scene(const Ref<PackedScene>& scene)
{
    _eggEntityScene = scene;
}

Ref<PackedScene> EntityManager::get_egg_entity_scene() const
{
    return _eggEntityScene;
}

/**
 * @brief Find-or-append team in the first-seen team list and return its palette color.
 * @details Teams beyond the 8-entry palette wrap around (index % 8).
 */
Color EntityManager::team_color(const String& team)
{
    for (std::size_t i = 0; i < _teamNames.size(); i++) {
        if (_teamNames[i] == team) {
            return _palette[i % _palette.size()];
        }
    }
    _teamNames.push_back(team);
    return _palette[(_teamNames.size() - 1) % _palette.size()];
}

/// World-space position where entities standing on tile (x, y) are placed.
Vector3 EntityManager::entity_position(int x, int y) const
{
    return _mapTerrain->tile_to_world(x, y) + Vector3(0.0f, 0.4f, 0.0f);
}

/// Fix a team's palette slot even before any of its players spawn.
void EntityManager::on_team_registered(const String& name)
{
    (void)team_color(name);
}

/// Free every live player and egg, then forget all team-color assignments.
void EntityManager::clear_all()
{
    for (auto& pair : _players) {
        pair.second->queue_free();
    }
    _players.clear();

    for (auto& pair : _eggs) {
        pair.second->queue_free();
    }
    _eggs.clear();

    _teamNames.clear();
}

/// Instantiate a PlayerEntity for a newly spawned player and place it on its tile.
void EntityManager::on_player_spawned(int id, int x, int y, int orientation, int level, const String& team)
{
    Node* node = _playerEntityScene->instantiate();
    PlayerEntity* entity = Object::cast_to<PlayerEntity>(node);
    add_child(entity);

    entity->set_player_id(id);
    entity->set_team_color(team_color(team));
    entity->set_orientation(orientation);
    entity->set_level(level);
    entity->update_position(entity_position(x, y), x, y);

    _players[id] = entity;
}

/// Move/turn an existing player; unknown ids are silently ignored.
void EntityManager::on_player_moved(int id, int x, int y, int orientation)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }
    it->second->set_orientation(orientation);
    it->second->update_position(entity_position(x, y), x, y);
}

/// Update an existing player's level; unknown ids are silently ignored.
void EntityManager::on_player_leveled(int id, int level)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }
    it->second->set_level(level);
}

/// Remove a player that has died; unknown ids are silently ignored.
/// The entity itself defers its actual removal until its death animation
/// finishes (or frees immediately if it has none); erasing it from _players
/// now is what makes the game logic treat it as gone right away.
void EntityManager::on_player_died(int id)
{
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }
    it->second->die();
    _players.erase(it);
}

/// Instantiate an EggEntity for a newly laid egg, colored after its laying player's team.
void EntityManager::on_egg_laid(int egg_id, int player_id, int x, int y)
{
    Node* node = _eggEntityScene->instantiate();
    EggEntity* entity = Object::cast_to<EggEntity>(node);
    add_child(entity);

    entity->set_egg_id(egg_id);

    auto it = _players.find(player_id);
    entity->set_team_color(it != _players.end() ? it->second->get_team_color() : Color(0.6f, 0.6f, 0.6f));

    entity->set_position(entity_position(x, y));

    _eggs[egg_id] = entity;
}

/// Remove an egg that hatched or died; unknown ids are silently ignored.
void EntityManager::on_egg_removed(int egg_id)
{
    auto it = _eggs.find(egg_id);
    if (it == _eggs.end()) {
        return;
    }
    it->second->queue_free();
    _eggs.erase(it);
}

/// Highlight all players participating in a new incantation, and remember
/// exactly which ids they were so on_incantation_ended can clear the
/// highlight for those same players regardless of any later position drift.
void EntityManager::on_incantation_started(int x, int y, int, const PackedInt32Array& ids)
{
    std::vector<int> incantingIds;
    for (int64_t i = 0; i < ids.size(); i++) {
        int id = static_cast<int>(ids[i]);
        incantingIds.push_back(id);
        auto it = _players.find(id);
        if (it != _players.end()) {
            it->second->set_incanting(true);
        }
    }
    _incantingByTile[tile_key(x, y)] = std::move(incantingIds);
}

/// Clear the incantation highlight for exactly the players recorded by the
/// matching on_incantation_started call for this tile; unknown tiles are
/// silently ignored.
void EntityManager::on_incantation_ended(int x, int y, bool)
{
    auto it = _incantingByTile.find(tile_key(x, y));
    if (it == _incantingByTile.end()) {
        return;
    }
    for (int id : it->second) {
        auto playerIt = _players.find(id);
        if (playerIt != _players.end()) {
            playerIt->second->set_incanting(false);
        }
    }
    _incantingByTile.erase(it);
}
