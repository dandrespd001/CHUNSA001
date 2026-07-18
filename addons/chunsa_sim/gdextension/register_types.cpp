#include "register_types.h"

// chunsa_sim — init GDExtension (Sprint 0.2): registra ChunsaSimNode.

#include <godot_cpp/core/class_db.hpp>

#include "chunsa_sim_node.h"

void chunsa_gdext_initialize(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    godot::ClassDB::register_class<ChunsaSimNode>();
}

void chunsa_gdext_uninitialize(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {

GDExtensionBool GDE_EXPORT chunsa_gdext_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization* r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(chunsa_gdext_initialize);
    init_obj.register_terminator(chunsa_gdext_uninitialize);
    init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}

}
