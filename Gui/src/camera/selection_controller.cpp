/**
 * @file camera/selection_controller.cpp
 * @brief Implementation of SelectionController.
 */

#include "camera/selection_controller.hpp"

#include "entities/player_entity.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

namespace {
/// Physics layer (bit 2) reserved for PlayerEntity's SelectionArea, named
/// "selectable" in project.godot's [layer_names] section.
constexpr uint32_t SELECTABLE_LAYER_MASK = 1u << 1;
/// Long enough to reach any on-screen tile under an orthographic camera,
/// where every pixel's ray direction is identical.
constexpr float RAY_LENGTH = 1000.0f;
} // namespace

void SelectionController::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_camera_path", "path"), &SelectionController::set_camera_path);
    ClassDB::bind_method(D_METHOD("get_camera_path"), &SelectionController::get_camera_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "camera_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "RtsCamera"),
                 "set_camera_path", "get_camera_path");

    ADD_SIGNAL(MethodInfo("player_selected", PropertyInfo(Variant::INT, "id")));
}

void SelectionController::_ready()
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    _camera = get_node<RtsCamera>(_cameraPath);
}

/**
 * @brief Raycast on a left-click and report the hit player, if any.
 * @details Restricts the query to SELECTABLE_LAYER_MASK so only
 *          PlayerEntity::SelectionArea nodes can be hit, never MapTerrain.
 */
void SelectionController::_unhandled_input(const Ref<InputEvent>& event)
{
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    InputEventMouseButton* mb = Object::cast_to<InputEventMouseButton>(event.ptr());
    if (mb == nullptr || !mb->is_pressed() || mb->get_button_index() != MOUSE_BUTTON_LEFT)
        return;

    Vector2 mouse_pos = mb->get_position();
    Vector3 from = _camera->project_ray_origin(mouse_pos);
    Vector3 to = from + _camera->project_ray_normal(mouse_pos) * RAY_LENGTH;

    Ref<PhysicsRayQueryParameters3D> params = PhysicsRayQueryParameters3D::create(from, to, SELECTABLE_LAYER_MASK);
    params->set_collide_with_areas(true);
    params->set_collide_with_bodies(false);

    Dictionary result = get_world_3d()->get_direct_space_state()->intersect_ray(params);
    if (result.is_empty()) {
        emit_signal("player_selected", -1);
        return;
    }

    Object* collider = result["collider"];
    Node* node = Object::cast_to<Node>(collider);
    PlayerEntity* entity = (node != nullptr) ? Object::cast_to<PlayerEntity>(node->get_parent()) : nullptr;

    emit_signal("player_selected", entity != nullptr ? entity->get_player_id() : -1);
}

void SelectionController::set_camera_path(const NodePath& path)
{
    _cameraPath = path;
}

NodePath SelectionController::get_camera_path() const
{
    return _cameraPath;
}
