/**
 * @file entities/entity_manager.cpp
 * @brief Implementation of EntityManager.
 */

#include "entities/entity_manager.hpp"

#include "vfx/broadcast_ripple_vfx.hpp"

#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <cstdint>

using namespace godot;

namespace {

/// Packs a tile coordinate into a single key for _incantingByTile.
int64_t tile_key(int x, int y)
{
    return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
}

/// Minimum gap between broadcast ripples spawned for the same player, in
/// milliseconds. AI clients pbc-broadcast far faster than this, so without the
/// throttle the map floods with overlapping ripples.
constexpr uint64_t BROADCAST_COOLDOWN_MS = 2000;

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

    ClassDB::bind_method(D_METHOD("set_incantation_vfx_scene", "scene"), &EntityManager::set_incantation_vfx_scene);
    ClassDB::bind_method(D_METHOD("get_incantation_vfx_scene"), &EntityManager::get_incantation_vfx_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "incantation_vfx_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
                  "set_incantation_vfx_scene", "get_incantation_vfx_scene");

    ClassDB::bind_method(D_METHOD("set_broadcast_vfx_scene", "scene"), &EntityManager::set_broadcast_vfx_scene);
    ClassDB::bind_method(D_METHOD("get_broadcast_vfx_scene"), &EntityManager::get_broadcast_vfx_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "broadcast_vfx_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
                  "set_broadcast_vfx_scene", "get_broadcast_vfx_scene");

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

    ClassDB::bind_method(D_METHOD("on_broadcast", "id", "message"), &EntityManager::on_broadcast);
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

void EntityManager::set_incantation_vfx_scene(const Ref<PackedScene>& scene)
{
    _incantationVfxScene = scene;
}

Ref<PackedScene> EntityManager::get_incantation_vfx_scene() const
{
    return _incantationVfxScene;
}

void EntityManager::set_broadcast_vfx_scene(const Ref<PackedScene>& scene)
{
    _broadcastVfxScene = scene;
}

Ref<PackedScene> EntityManager::get_broadcast_vfx_scene() const
{
    return _broadcastVfxScene;
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

    for (auto& pair : _incantationVfxByTile) {
        pair.second->queue_free();
    }
    _incantationVfxByTile.clear();

    _incantingByTile.clear();
    _lastBroadcastMs.clear();

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

    if (_incantationVfxScene.is_valid()) {
        const int64_t key = tile_key(x, y);
        // Defensive: if a VFX somehow already exists for this tile, free it first to avoid a leak.
        auto existing = _incantationVfxByTile.find(key);
        if (existing != _incantationVfxByTile.end()) {
            existing->second->queue_free();
        }
        Node3D* vfx = Object::cast_to<Node3D>(_incantationVfxScene->instantiate());
        add_child(vfx);
        // Lift above the terrain surface — the VFX is additive (writes no depth), so at
        // ground level its bright core is occluded by the opaque terrain. Scale it up and
        // boost emission/light so it reads clearly from the zoomed-out top-down camera
        // against the bright daytime scene (gl_compatibility has no glow bloom to help).
        vfx->set_position(_mapTerrain->tile_to_world(x, y) + Vector3(0.0f, 0.5f, 0.0f));
        vfx->set_scale(Vector3(3.0f, 3.0f, 3.0f));
        vfx->set("emission", 8.0);
        vfx->set("light_energy", 8.0);
        _incantationVfxByTile[key] = vfx;
    }
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

    auto vfxIt = _incantationVfxByTile.find(tile_key(x, y));
    if (vfxIt != _incantationVfxByTile.end()) {
        vfxIt->second->queue_free();
        _incantationVfxByTile.erase(vfxIt);
    }
}

/// Spawn a one-shot, ground-conforming ripple on the broadcasting player's tile,
/// tinted with their team color. The ripple frees itself after ~1 second, so no
/// per-tile bookkeeping is needed here. Unknown players are silently ignored.
void EntityManager::on_broadcast(int id, const String&)
{
    if (_broadcastVfxScene.is_null()) {
        return;
    }
    auto it = _players.find(id);
    if (it == _players.end()) {
        return;
    }
    // Throttle the frequent pbc broadcasts to at most one ripple per player per
    // cooldown, so the map doesn't flood with overlapping waves.
    uint64_t now = Time::get_singleton()->get_ticks_msec();
    auto lastIt = _lastBroadcastMs.find(id);
    if (lastIt != _lastBroadcastMs.end() && now - lastIt->second < BROADCAST_COOLDOWN_MS) {
        return;
    }
    _lastBroadcastMs[id] = now;

    PlayerEntity* player = it->second;
    BroadcastRippleVfx* vfx = Object::cast_to<BroadcastRippleVfx>(_broadcastVfxScene->instantiate());
    if (vfx == nullptr) {
        return;
    }
    vfx->set_position(_mapTerrain->tile_to_world(player->get_grid_x(), player->get_grid_y()));
    add_child(vfx);
    vfx->configure(_mapTerrain, player->get_team_color());
}
