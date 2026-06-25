/**
 * @file ui/connect_dialog.hpp
 * @brief HUD panel for entering a host/port and starting the server connection.
 * @details ConnectDialog is a thin logic layer over a scene
 *          (scenes/ui/connect_dialog.tscn) that provides the host/port
 *          LineEdits, the connect Button and a status Label. It resolves the
 *          ZappyWorld node via an exported NodePath (world_path), overridden
 *          in world.tscn at the instanced ConnectDialog node — the same
 *          pattern EntityManager/TileMarkers use for map_terrain_path. A
 *          scene-unique-name ("%World") lookup does not work here: World is
 *          the scene's root node, and unique-name resolution is keyed off a
 *          node's owner, which the root itself never has.
 */

#pragma once

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class ZappyWorld;

/// Host/port entry dialog: starts the connection via ZappyWorld::connect_to_server,
/// hides once the connection is confirmed live (map_initialized), and reappears
/// with an error message if the connection fails (connection_error).
class ServerConnectDialog : public Control {
    GDCLASS(ServerConnectDialog, Control)

private:
    LineEdit* _hostEdit = nullptr;      ///< "Panel/VBox/HostEdit" child, fetched in _ready().
    LineEdit* _portEdit = nullptr;      ///< "Panel/VBox/PortEdit" child, fetched in _ready().
    Button*   _connectButton = nullptr; ///< "Panel/VBox/ConnectButton" child, fetched in _ready().
    Label*    _statusLabel = nullptr;   ///< "Panel/VBox/StatusLabel" child, fetched in _ready().

    ZappyWorld* _world = nullptr; ///< Resolved from world_path in _ready().
    NodePath _worldPath;          ///< Exported property: path to the ZappyWorld node.

protected:
    /// Bind methods, properties exposed to Godot/GDScript.
    static void _bind_methods();

public:
    /// Fetch child nodes, resolve the World node, and wire the connect button.
    void _ready() override;

    /// Set the exported world_path property used by _ready() to resolve ZappyWorld.
    void set_world_path(const NodePath& path);
    /// Get the exported world_path property.
    NodePath get_world_path() const;

    /// Connect button "pressed" handler: reads host/port, calls
    /// ZappyWorld::connect_to_server, clears the status label.
    void on_connect_pressed();

    /// Connected to World's map_initialized signal: hide this dialog. Takes the
    /// signal's (width, height) arguments, unused, since Godot requires exact
    /// arity between a signal and the method it's connected to.
    void on_connected(int width, int height);

    /// Connected to World's connection_error signal: show this dialog again
    /// and display the error message.
    void on_connection_error(const String& message);
};

} // namespace godot
