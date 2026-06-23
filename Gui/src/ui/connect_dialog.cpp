/**
 * @file ui/connect_dialog.cpp
 * @brief Implementation of ConnectDialog.
 */

#include "ui/connect_dialog.hpp"

#include "ZappyWorld.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/node_path.hpp>

using namespace godot;

void ConnectDialog::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_connect_pressed"), &ConnectDialog::on_connect_pressed);
    ClassDB::bind_method(D_METHOD("on_connected", "width", "height"), &ConnectDialog::on_connected);
    ClassDB::bind_method(D_METHOD("on_connection_error", "message"), &ConnectDialog::on_connection_error);

    ClassDB::bind_method(D_METHOD("set_world_path", "path"), &ConnectDialog::set_world_path);
    ClassDB::bind_method(D_METHOD("get_world_path"), &ConnectDialog::get_world_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "world_path"), "set_world_path", "get_world_path");
}

/**
 * @brief Fetch the pre-built child nodes, resolve the World node, and wire the connect button.
 * @details The button's "pressed" signal is connected here in code (not in the
 *          .tscn) because this is a self-contained node wiring its own child's
 *          signal to its own handler, the same pattern MapTerrain uses for its
 *          noise resource's "changed" signal.
 */
void ConnectDialog::_ready()
{
    _hostEdit = get_node<LineEdit>(NodePath("Panel/VBox/HostEdit"));
    _portEdit = get_node<LineEdit>(NodePath("Panel/VBox/PortEdit"));
    _connectButton = get_node<Button>(NodePath("Panel/VBox/ConnectButton"));
    _statusLabel = get_node<Label>(NodePath("Panel/VBox/StatusLabel"));

    _world = get_node<ZappyWorld>(_worldPath);

    _connectButton->connect("pressed", callable_mp(this, &ConnectDialog::on_connect_pressed));
}

void ConnectDialog::set_world_path(const NodePath& path)
{
    _worldPath = path;
}

NodePath ConnectDialog::get_world_path() const
{
    return _worldPath;
}

/**
 * @brief Read the host/port fields and start the connection.
 * @details An empty or non-numeric port falls back to 4242 rather than
 *          calling connect_to_server with port 0.
 */
void ConnectDialog::on_connect_pressed()
{
    String host = _hostEdit->get_text();
    if (host.is_empty()) {
        host = "127.0.0.1";
    }

    int port = _portEdit->get_text().to_int();
    if (port == 0) {
        port = 4242;
    }

    _statusLabel->set_text("");
    _world->connect_to_server(host, port);
}

void ConnectDialog::on_connected(int, int)
{
    hide();
}

void ConnectDialog::on_connection_error(const String& message)
{
    show();
    _statusLabel->set_text(message);
}
