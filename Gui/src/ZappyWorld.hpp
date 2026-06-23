/**
 * @file ZappyWorld.hpp
 * @brief Root Godot node for the Zappy client.
 * @details Declares ZappyWorld, the Node3D subclass that owns the server
 *          connection and world state and drives the per-frame scene update.
 */

#pragma once

#include "network/zappy_connection.hpp"
#include "world/world_state.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

/// Root node for the Zappy client: owns the server connection and world state,
/// and drives the per-frame scene update. Registered as a usable node type by
/// register_types.cpp.
class ZappyWorld : public Node3D {
    GDCLASS(ZappyWorld, Node3D)

protected:
    /// Bind methods, properties and signals exposed to Godot/GDScript.
    static void _bind_methods();

public:
    ZappyWorld();
    ~ZappyWorld() override;

    /// Connect automatically if the auto_connect property is enabled.
    void _ready() override;

    /// Poll the connection, drain incoming messages, and apply/dispatch each one.
    void _process(double delta) override;

    /// Start (or restart) the connection to a zappy_server GRAPHIC port.
    void connect_to_server(const String& host, int port);

    /// Set the exported server_host property used by connect_to_server() on _ready().
    void set_server_host(const String& host);
    /// Get the exported server_host property.
    String get_server_host() const;

    /// Set the exported server_port property used by connect_to_server() on _ready().
    void set_server_port(int port);
    /// Get the exported server_port property.
    int get_server_port() const;

    /// Set whether _ready() should call connect_to_server() automatically.
    void set_auto_connect(bool enabled);
    /// Get the exported auto_connect property.
    bool get_auto_connect() const;

private:
    /// Apply one message to _world, then emit the matching per-message signal.
    void dispatch_signals(const zappy::ServerMessage& msg);

    zappy::ZappyConnection _connection;
    zappy::WorldState       _world;

    String _serverHost{"127.0.0.1"}; ///< Exported property: host to connect to.
    int    _serverPort{4242};        ///< Exported property: GRAPHIC port.
    bool   _autoConnect{false};      ///< Exported property: connect on _ready().

    /// Set once connection_error has fired for the current connection attempt,
    /// so it is emitted only once per failure rather than every frame.
    bool _errorEmitted{false};
};

} // namespace godot
