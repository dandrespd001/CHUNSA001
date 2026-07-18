#pragma once

// chunsa_sim — init GDExtension (Sprint 0.2). Nivel SCENE; entry symbol
// `chunsa_gdext_init` (ver demo/chunsa_sim.gdextension).

#include <godot_cpp/godot.hpp>

void chunsa_gdext_initialize(godot::ModuleInitializationLevel p_level);
void chunsa_gdext_uninitialize(godot::ModuleInitializationLevel p_level);
