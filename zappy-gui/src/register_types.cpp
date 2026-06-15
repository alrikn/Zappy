/**
 * @file register_types.cpp
 * @brief Implementation of the GDExtension module init/terminate hooks and the
 *        zappy_gui_library_init() entry point loaded by Godot.
 */

#include "register_types.hpp"
#include "ZappyWorld.hpp"
#include "entities/egg_entity.hpp"
#include "entities/entity_manager.hpp"
#include "entities/player_entity.hpp"
#include "world/map_terrain.hpp"
#include "world/tile_markers.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_zappy_gui_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<ZappyWorld>();
    ClassDB::register_class<MapTerrain>();
    ClassDB::register_class<TileMarkers>();
    ClassDB::register_class<PlayerEntity>();
    ClassDB::register_class<EggEntity>();
    ClassDB::register_class<EntityManager>();
}

void uninitialize_zappy_gui_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {

/**
 * @brief GDExtension library entry point referenced by entry_symbol in
 *        bin/zappy_gui.gdextension.
 * @details Builds the godot-cpp InitObject, registers the initializer/terminator
 *          hooks above, and sets the minimum initialization level to SCENE.
 * @param p_get_proc_address Function pointer table provided by Godot.
 * @param p_library Opaque handle to this GDExtension library.
 * @param r_initialization Output initialization descriptor filled by init_obj.init().
 * @return true on successful initialization.
 */
GDExtensionBool GDE_EXPORT zappy_gui_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr         p_library,
    GDExtensionInitialization*         r_initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_zappy_gui_module);
    init_obj.register_terminator(uninitialize_zappy_gui_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}

} // extern "C"
