#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include "pulseblockz_world.h"
#include "pulseblockz_crypto.h"
#include "pulseblockz_chain.h"
#include "pulseblockz_tremolo.h"
using namespace godot;

static void init(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    ClassDB::register_class<PulseBlockzWorld>();
    ClassDB::register_class<PulseBlockzCrypto>();
    ClassDB::register_class<PulseBlockzChain>();
    // The instance class is internal: the effect makes it, a scene never holds one.
    ClassDB::register_class<PulseBlockzTremolo>();
    ClassDB::register_internal_class<PulseBlockzTremoloInstance>();
}
static void uninit(ModuleInitializationLevel) {}

extern "C" GDExtensionBool GDE_EXPORT pulseblockz_luau_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
        const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(init);
    init_obj.register_terminator(uninit);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
