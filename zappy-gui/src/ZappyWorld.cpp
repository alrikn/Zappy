/**
 * @file ZappyWorld.cpp
 * @brief Implementation of ZappyWorld: Godot bindings, signal definitions, and the
 *        per-message dispatch that turns a parsed ServerMessage into a Godot signal.
 */

#include "ZappyWorld.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

using namespace godot;

namespace {

/// Merges multiple callable types into one — used with std::visit so the compiler
/// picks the correct overload based on the active ServerMessage alternative.
template<typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

} // namespace

void ZappyWorld::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port"), &ZappyWorld::connect_to_server);

    ClassDB::bind_method(D_METHOD("set_server_host", "host"), &ZappyWorld::set_server_host);
    ClassDB::bind_method(D_METHOD("get_server_host"), &ZappyWorld::get_server_host);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "server_host"), "set_server_host", "get_server_host");

    ClassDB::bind_method(D_METHOD("set_server_port", "port"), &ZappyWorld::set_server_port);
    ClassDB::bind_method(D_METHOD("get_server_port"), &ZappyWorld::get_server_port);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "server_port"), "set_server_port", "get_server_port");

    ClassDB::bind_method(D_METHOD("set_auto_connect", "enabled"), &ZappyWorld::set_auto_connect);
    ClassDB::bind_method(D_METHOD("get_auto_connect"), &ZappyWorld::get_auto_connect);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_connect"), "set_auto_connect", "get_auto_connect");

    ADD_SIGNAL(MethodInfo("map_initialized",
        PropertyInfo(Variant::INT, "width"), PropertyInfo(Variant::INT, "height")));

    ADD_SIGNAL(MethodInfo("tile_updated",
        PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y"),
        PropertyInfo(Variant::INT, "q0"), PropertyInfo(Variant::INT, "q1"), PropertyInfo(Variant::INT, "q2"),
        PropertyInfo(Variant::INT, "q3"), PropertyInfo(Variant::INT, "q4"), PropertyInfo(Variant::INT, "q5"),
        PropertyInfo(Variant::INT, "q6")));

    ADD_SIGNAL(MethodInfo("player_spawned",
        PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y"),
        PropertyInfo(Variant::INT, "orientation"), PropertyInfo(Variant::INT, "level"),
        PropertyInfo(Variant::STRING, "team")));

    ADD_SIGNAL(MethodInfo("player_moved",
        PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y"),
        PropertyInfo(Variant::INT, "orientation")));

    ADD_SIGNAL(MethodInfo("player_leveled",
        PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "level")));

    ADD_SIGNAL(MethodInfo("player_inventory_updated",
        PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y"),
        PropertyInfo(Variant::INT, "q0"), PropertyInfo(Variant::INT, "q1"), PropertyInfo(Variant::INT, "q2"),
        PropertyInfo(Variant::INT, "q3"), PropertyInfo(Variant::INT, "q4"), PropertyInfo(Variant::INT, "q5"),
        PropertyInfo(Variant::INT, "q6")));

    ADD_SIGNAL(MethodInfo("player_died", PropertyInfo(Variant::INT, "id")));

    ADD_SIGNAL(MethodInfo("egg_laid",
        PropertyInfo(Variant::INT, "egg_id"), PropertyInfo(Variant::INT, "player_id"),
        PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y")));

    ADD_SIGNAL(MethodInfo("egg_removed", PropertyInfo(Variant::INT, "egg_id")));

    ADD_SIGNAL(MethodInfo("team_registered", PropertyInfo(Variant::STRING, "name")));

    ADD_SIGNAL(MethodInfo("incantation_started",
        PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y"), PropertyInfo(Variant::INT, "level"),
        PropertyInfo(Variant::PACKED_INT32_ARRAY, "ids")));

    ADD_SIGNAL(MethodInfo("incantation_ended",
        PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y"), PropertyInfo(Variant::BOOL, "result")));

    ADD_SIGNAL(MethodInfo("broadcast",
        PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "message")));

    ADD_SIGNAL(MethodInfo("time_updated", PropertyInfo(Variant::INT, "t")));

    ADD_SIGNAL(MethodInfo("server_message", PropertyInfo(Variant::STRING, "message")));

    ADD_SIGNAL(MethodInfo("game_over", PropertyInfo(Variant::STRING, "team")));

    ADD_SIGNAL(MethodInfo("connection_error", PropertyInfo(Variant::STRING, "message")));
}

/// Default-constructs the connection and world state; no connection is opened yet.
ZappyWorld::ZappyWorld()
{
}

/// Nothing to release explicitly; _connection and _world clean up themselves.
ZappyWorld::~ZappyWorld()
{
}

void ZappyWorld::_ready()
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    if (_autoConnect) {
        connect_to_server(_serverHost, _serverPort);
    }
}

void ZappyWorld::_process(double)
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    _connection.poll();

    if (_connection.has_error()) {
        if (!_errorEmitted) {
            emit_signal("connection_error", String(_connection.error_message().c_str()));
            _errorEmitted = true;
        }
        return;
    }

    for (const zappy::ServerMessage& msg : _connection.drain()) {
        _world.apply(msg);
        dispatch_signals(msg);
    }
}

void ZappyWorld::connect_to_server(const String& host, int port)
{
    _errorEmitted = false;
    _connection.connect_to_host(host, port);
}

void ZappyWorld::set_server_host(const String& host)
{
    _serverHost = host;
}

String ZappyWorld::get_server_host() const
{
    return _serverHost;
}

void ZappyWorld::set_server_port(int port)
{
    _serverPort = port;
}

int ZappyWorld::get_server_port() const
{
    return _serverPort;
}

void ZappyWorld::set_auto_connect(bool enabled)
{
    _autoConnect = enabled;
}

bool ZappyWorld::get_auto_connect() const
{
    return _autoConnect;
}

void ZappyWorld::dispatch_signals(const zappy::ServerMessage& msg)
{
    using namespace zappy;

    std::visit(overloaded{
        [this](const MsgMapSize& m) {
            emit_signal("map_initialized", static_cast<int>(m.x), static_cast<int>(m.y));
        },
        [this](const MsgTileContent& m) {
            emit_signal("tile_updated",
                static_cast<int>(m.x), static_cast<int>(m.y),
                static_cast<int>(m.resources[0]), static_cast<int>(m.resources[1]),
                static_cast<int>(m.resources[2]), static_cast<int>(m.resources[3]),
                static_cast<int>(m.resources[4]), static_cast<int>(m.resources[5]),
                static_cast<int>(m.resources[6]));
        },
        [this](const MsgTeamName& m) {
            emit_signal("team_registered", String(m.name.c_str()));
        },
        [this](const MsgPlayerNew& m) {
            emit_signal("player_spawned",
                static_cast<int>(m.id), static_cast<int>(m.x), static_cast<int>(m.y),
                static_cast<int>(m.orientation), static_cast<int>(m.level), String(m.team.c_str()));
        },
        [this](const MsgPlayerPosition& m) {
            emit_signal("player_moved",
                static_cast<int>(m.id), static_cast<int>(m.x), static_cast<int>(m.y),
                static_cast<int>(m.orientation));
        },
        [this](const MsgPlayerLevel& m) {
            emit_signal("player_leveled", static_cast<int>(m.id), static_cast<int>(m.level));
        },
        [this](const MsgPlayerInventory& m) {
            emit_signal("player_inventory_updated",
                static_cast<int>(m.id), static_cast<int>(m.x), static_cast<int>(m.y),
                static_cast<int>(m.resources[0]), static_cast<int>(m.resources[1]),
                static_cast<int>(m.resources[2]), static_cast<int>(m.resources[3]),
                static_cast<int>(m.resources[4]), static_cast<int>(m.resources[5]),
                static_cast<int>(m.resources[6]));
        },
        [](const MsgPlayerExpulsion&) {
            // No signal — purely visual.
        },
        [this](const MsgPlayerBroadcast& m) {
            emit_signal("broadcast", static_cast<int>(m.id), String(m.message.c_str()));
        },
        [this](const MsgPlayerDeath& m) {
            emit_signal("player_died", static_cast<int>(m.id));
        },
        [this](const MsgIncantationStart& m) {
            PackedInt32Array ids;
            for (const uint32_t id : m.players) {
                ids.push_back(static_cast<int32_t>(id));
            }
            emit_signal("incantation_started",
                static_cast<int>(m.x), static_cast<int>(m.y), static_cast<int>(m.level), ids);
        },
        [this](const MsgIncantationEnd& m) {
            emit_signal("incantation_ended", static_cast<int>(m.x), static_cast<int>(m.y), m.result);
        },
        [](const MsgEggLaying&) {
            // No signal — purely visual.
        },
        [](const MsgResourceDrop&) {
            // No signal — purely visual.
        },
        [](const MsgResourceCollect&) {
            // No signal — purely visual.
        },
        [this](const MsgEggLaid& m) {
            emit_signal("egg_laid",
                static_cast<int>(m.eggId), static_cast<int>(m.playerId),
                static_cast<int>(m.x), static_cast<int>(m.y));
        },
        [this](const MsgEggConnection& m) {
            emit_signal("egg_removed", static_cast<int>(m.eggId));
        },
        [this](const MsgEggDeath& m) {
            emit_signal("egg_removed", static_cast<int>(m.eggId));
        },
        [this](const MsgTimeUnit& m) {
            emit_signal("time_updated", static_cast<int>(m.t));
        },
        [this](const MsgTimeUnitSet& m) {
            emit_signal("time_updated", static_cast<int>(m.t));
        },
        [this](const MsgEndGame& m) {
            emit_signal("game_over", String(m.team.c_str()));
        },
        [this](const MsgServerMessage& m) {
            emit_signal("server_message", String(m.message.c_str()));
        },
        [](const MsgUnknownCommand&) {
            // No signal.
        },
        [](const MsgBadParameter&) {
            // No signal.
        },
    }, msg);
}
