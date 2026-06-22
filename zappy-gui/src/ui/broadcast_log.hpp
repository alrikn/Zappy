/**
 * @file ui/broadcast_log.hpp
 * @brief HUD panel: scrolling log of player broadcasts and server messages.
 * @details BroadcastLog is a thin logic layer over a scene
 *          (scenes/ui/broadcast_log.tscn) that provides a single
 *          RichTextLabel. Append-only, auto-scrolling.
 */

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

/// Appends World's broadcast/server_message signals as lines of text.
class BroadcastLog : public Control {
    GDCLASS(BroadcastLog, Control)

private:
    RichTextLabel* _log = nullptr; ///< "Panel/LogText" child, fetched in _ready().

protected:
    static void _bind_methods();

public:
    /// Fetch the log child node and enable auto-scroll-to-bottom.
    void _ready() override;

    /// Connected to World's broadcast signal: appends "[Player <id>] <message>".
    void on_broadcast(int id, const String& message);
    /// Connected to World's server_message signal: appends "[Server] <message>".
    void on_server_message(const String& message);
};

} // namespace godot
