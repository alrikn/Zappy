/**
 * @file ui/broadcast_log.cpp
 * @brief Implementation of BroadcastLog.
 */

#include "ui/broadcast_log.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>

using namespace godot;

void BroadcastLog::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_broadcast", "id", "message"), &BroadcastLog::on_broadcast);
    ClassDB::bind_method(D_METHOD("on_server_message", "message"), &BroadcastLog::on_server_message);
}

void BroadcastLog::_ready()
{
    _log = get_node<RichTextLabel>(NodePath("Panel/LogText"));
    _log->set_scroll_follow(true);
}

void BroadcastLog::on_broadcast(int id, const String& message)
{
    // add_text(), not append_text(): the message is untrusted player input
    // and must not be interpreted as BBCode.
    _log->add_text("[Player " + String::num_int64(id) + "] " + message + "\n");
}

void BroadcastLog::on_server_message(const String& message)
{
    _log->add_text("[Server] " + message + "\n");
}
