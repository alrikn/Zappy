/**
 * @file register_types.hpp
 * @brief GDExtension module init/terminate entry points.
 * @details Implemented in register_types.cpp and registered with godot-cpp via
 *          the zappy_gui_library_init() entry point.
 */

#pragma once

#include <godot_cpp/core/class_db.hpp>

/**
 * @brief Register all GDExtension classes provided by this module.
 * @details Called by Godot for every initialization level; classes are only
 *          registered once p_level reaches MODULE_INITIALIZATION_LEVEL_SCENE.
 * @param p_level Initialization level currently being entered.
 */
void initialize_zappy_gui_module(godot::ModuleInitializationLevel p_level);

/**
 * @brief Tear down anything set up by initialize_zappy_gui_module().
 * @param p_level Initialization level currently being exited.
 */
void uninitialize_zappy_gui_module(godot::ModuleInitializationLevel p_level);
