/**
 * @file camera/selection_controller.hpp
 * @brief Click-to-select: raycasts against player avatars and reports the result.
 * @details SelectionController is a sibling of RtsCamera, not a part of it, so
 *          view control and selection stay independently testable/swappable.
 *          It reuses the SelectionArea Area3D each PlayerEntity already has.
 */

#pragma once

#include "camera/rts_camera.hpp"

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace godot {

/// Resolves left clicks to a player id by raycasting against the
/// "selectable" physics layer and walking from the hit Area3D to its owning
/// PlayerEntity. Emits player_selected(-1) on a miss (clicking empty ground).
class SelectionController : public Node3D {
    GDCLASS(SelectionController, Node3D)

private:
    NodePath _cameraPath;       ///< Exported property: path to the RtsCamera node.
    RtsCamera* _camera = nullptr; ///< Resolved from _cameraPath in _ready().

protected:
    /// Bind methods, properties and the player_selected signal.
    static void _bind_methods();

public:
    /// Resolve camera_path.
    void _ready() override;

    /// Raycast on left-click and emit player_selected(id) or player_selected(-1).
    void _unhandled_input(const Ref<InputEvent>& event) override;

    /// Set the exported camera_path property.
    void set_camera_path(const NodePath& path);
    /// Get the exported camera_path property.
    NodePath get_camera_path() const;
};

} // namespace godot
