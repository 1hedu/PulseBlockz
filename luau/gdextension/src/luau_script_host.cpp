#include "luau_script_host.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "lua.h"
#include "lualib.h"

using namespace godot;
using pulseblockz::HostEvent;
using pulseblockz::ScriptWorker;

// ---- host API exposed to scripts --------------------------------------------
// These run on the WORKER thread: they validate arguments, emit a Command event and return,
// touching neither the node nor the scene tree. _drain_events() applies the command as a signal
// on the main thread.
static ScriptWorker* workerOf(lua_State* L) { return ScriptWorker::from(L); }

static void pushCommand(lua_State* L, ScriptWorker* w, HostEvent e) {
    if (!w->command(std::move(e)))
        luaL_error(L, "host command budget exceeded (%d per call)", w->maxCommandsPerCall());
}

static int api_part_create(lua_State* L) {
    double x = luaL_checknumber(L, 1), y = luaL_checknumber(L, 2), z = luaL_checknumber(L, 3);
    ScriptWorker* w = workerOf(L);
    HostEvent e; e.text = "part.create"; e.script = w->currentScript();
    e.id = w->nextObjectId(); e.x = x; e.y = y; e.z = z;
    int64_t id = e.id;
    pushCommand(L, w, std::move(e));
    lua_pushinteger(L, (int)id);
    return 1;
}

static int api_part_destroy(lua_State* L) {
    int id = luaL_checkinteger(L, 1);
    ScriptWorker* w = workerOf(L);
    HostEvent e; e.text = "part.destroy"; e.script = w->currentScript(); e.id = id;
    pushCommand(L, w, std::move(e));
    return 0;
}

static int api_part_set_position(lua_State* L) {
    int id = luaL_checkinteger(L, 1);
    double x = luaL_checknumber(L, 2), y = luaL_checknumber(L, 3), z = luaL_checknumber(L, 4);
    ScriptWorker* w = workerOf(L);
    HostEvent e; e.text = "part.move"; e.script = w->currentScript(); e.id = id; e.x = x; e.y = y; e.z = z;
    pushCommand(L, w, std::move(e));
    return 0;
}

// ---- node ---------------------------------------------------------------------
LuauScriptHost::LuauScriptHost() {
    opts_.budget.maxMillis = 4.0;
    opts_.budget.maxFrameMillis = 8.0;
    opts_.budget.maxSteps = 2'000'000;
    opts_.budget.maxMemory = 16u << 20;
    opts_.maxCommandsPerCall = 1024;
    opts_.threaded = true;
}

// Destroying the worker joins its thread, so no event can be produced past this point; a
// _drain_events still queued in the MessageQueue is dropped with the object.
LuauScriptHost::~LuauScriptHost() { worker_.reset(); }

void LuauScriptHost::ensure_worker() {
    if (worker_) return;
    worker_ = std::make_unique<ScriptWorker>(opts_, [](pulseblockz::Sandbox& sb) {
        sb.expose("Part", {{"create", &api_part_create},
                           {"destroy", &api_part_destroy},
                           {"set_position", &api_part_set_position}});
    });
    if (opts_.threaded) {
        // Called on the worker thread when the event queue goes empty -> non-empty.
        // call_deferred's MessageQueue is the documented way back to the main thread.
        worker_->setWakeHandler([this] { call_deferred(StringName("_drain_events")); });
    }
}

void LuauScriptHost::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    ensure_worker();
}

void LuauScriptHost::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) return;
    if (autoTick_ && worker_) tick_all(delta);
}

int LuauScriptHost::load_script(const String& name, const String& source) {
    ensure_worker();
    int h = worker_->load(name.utf8().get_data(), source.utf8().get_data());
    if (!opts_.threaded) _drain_events();
    return h;
}

void LuauScriptHost::unload_script(int handle) {
    if (!worker_) return;
    worker_->unload(handle);
}

void LuauScriptHost::run_main(int handle) {
    if (!worker_) return;
    worker_->runMain(handle);
    if (!opts_.threaded) _drain_events();
}

void LuauScriptHost::call_fn(int handle, const String& fn, double arg) {
    if (!worker_) return;
    worker_->call(handle, fn.utf8().get_data(), arg);
    if (!opts_.threaded) _drain_events();
}

void LuauScriptHost::tick_all(double delta) {
    if (!worker_) return;
    worker_->tick(delta);
    if (!opts_.threaded) _drain_events();
}

void LuauScriptHost::flush() {
    if (!worker_) return;
    worker_->waitIdle();
    _drain_events();
}

bool LuauScriptHost::is_idle() const { return !worker_ || worker_->idle(); }

Dictionary LuauScriptHost::to_dict(const pulseblockz::RunResult& r) {
    Dictionary d;
    d["ok"] = r.ok; d["skipped"] = r.skipped; d["error"] = String::utf8(r.error.c_str());
    d["steps"] = (int64_t)r.stepsUsed; d["millis"] = r.millisUsed;
    d["memory_peak"] = (int64_t)r.memoryPeak;
    return d;
}

void LuauScriptHost::_drain_events() {
    if (!worker_) return;
    for (const HostEvent& e : worker_->drain()) {
        switch (e.type) {
        case HostEvent::Print:
            emit_signal("script_print", String::utf8(e.name.c_str()), String::utf8(e.text.c_str()));
            break;
        case HostEvent::LoadError:
            emit_signal("script_error", String::utf8(e.name.c_str()), String::utf8(e.text.c_str()));
            break;
        case HostEvent::Killed:
            emit_signal("script_killed", String::utf8(e.name.c_str()), String::utf8(e.text.c_str()));
            break;
        case HostEvent::Result:
            emit_signal("script_result", e.script, String::utf8(e.name.c_str()), to_dict(e.result));
            break;
        case HostEvent::TickStats: {
            Dictionary d;
            d["delta"] = e.x; d["millis"] = e.result.millisUsed;
            d["ran"] = (int64_t)e.id; d["killed"] = (int64_t)e.y; d["skipped"] = (int64_t)e.z;
            d["memory"] = (int64_t)e.result.memoryNow;
            emit_signal("tick_finished", d);
            break;
        }
        case HostEvent::Command:
            if (e.text == "part.create")       emit_signal("part_create_requested", e.id, Vector3(e.x, e.y, e.z));
            else if (e.text == "part.destroy") emit_signal("part_destroy_requested", e.id);
            else if (e.text == "part.move")    emit_signal("part_move_requested", e.id, Vector3(e.x, e.y, e.z));
            break;
        }
    }
}

int64_t LuauScriptHost::get_memory_used() const { return worker_ ? (int64_t)worker_->memoryUsed() : 0; }

void LuauScriptHost::push_budget() { if (worker_) worker_->setBudget(opts_.budget, opts_.maxCommandsPerCall); }

void LuauScriptHost::set_max_millis(double v) { opts_.budget.maxMillis = v; push_budget(); }
double LuauScriptHost::get_max_millis() const { return opts_.budget.maxMillis; }
void LuauScriptHost::set_max_frame_millis(double v) { opts_.budget.maxFrameMillis = v; push_budget(); }
double LuauScriptHost::get_max_frame_millis() const { return opts_.budget.maxFrameMillis; }
void LuauScriptHost::set_max_steps(int64_t v) { opts_.budget.maxSteps = v; push_budget(); }
int64_t LuauScriptHost::get_max_steps() const { return opts_.budget.maxSteps; }
void LuauScriptHost::set_max_memory_mb(int v) { opts_.budget.maxMemory = (size_t)v << 20; push_budget(); }
int LuauScriptHost::get_max_memory_mb() const { return (int)(opts_.budget.maxMemory >> 20); }
void LuauScriptHost::set_max_commands_per_call(int v) { opts_.maxCommandsPerCall = v; push_budget(); }
int LuauScriptHost::get_max_commands_per_call() const { return opts_.maxCommandsPerCall; }
void LuauScriptHost::set_threaded(bool v) {
    if (worker_ && v != opts_.threaded)
        UtilityFunctions::push_warning("LuauScriptHost: `threaded` only takes effect before the first script loads");
    opts_.threaded = v;
}
bool LuauScriptHost::get_threaded() const { return opts_.threaded; }
void LuauScriptHost::set_auto_tick(bool v) { autoTick_ = v; }
bool LuauScriptHost::get_auto_tick() const { return autoTick_; }

void LuauScriptHost::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_script", "name", "source"), &LuauScriptHost::load_script);
    ClassDB::bind_method(D_METHOD("unload_script", "handle"), &LuauScriptHost::unload_script);
    ClassDB::bind_method(D_METHOD("run_main", "handle"), &LuauScriptHost::run_main);
    ClassDB::bind_method(D_METHOD("call_fn", "handle", "fn", "arg"), &LuauScriptHost::call_fn, DEFVAL(0.0));
    ClassDB::bind_method(D_METHOD("tick_all", "delta"), &LuauScriptHost::tick_all);
    ClassDB::bind_method(D_METHOD("flush"), &LuauScriptHost::flush);
    ClassDB::bind_method(D_METHOD("is_idle"), &LuauScriptHost::is_idle);
    ClassDB::bind_method(D_METHOD("get_memory_used"), &LuauScriptHost::get_memory_used);
    ClassDB::bind_method(D_METHOD("_drain_events"), &LuauScriptHost::_drain_events);

    ClassDB::bind_method(D_METHOD("set_max_millis", "v"), &LuauScriptHost::set_max_millis);
    ClassDB::bind_method(D_METHOD("get_max_millis"), &LuauScriptHost::get_max_millis);
    ClassDB::bind_method(D_METHOD("set_max_frame_millis", "v"), &LuauScriptHost::set_max_frame_millis);
    ClassDB::bind_method(D_METHOD("get_max_frame_millis"), &LuauScriptHost::get_max_frame_millis);
    ClassDB::bind_method(D_METHOD("set_max_steps", "v"), &LuauScriptHost::set_max_steps);
    ClassDB::bind_method(D_METHOD("get_max_steps"), &LuauScriptHost::get_max_steps);
    ClassDB::bind_method(D_METHOD("set_max_memory_mb", "v"), &LuauScriptHost::set_max_memory_mb);
    ClassDB::bind_method(D_METHOD("get_max_memory_mb"), &LuauScriptHost::get_max_memory_mb);
    ClassDB::bind_method(D_METHOD("set_max_commands_per_call", "v"), &LuauScriptHost::set_max_commands_per_call);
    ClassDB::bind_method(D_METHOD("get_max_commands_per_call"), &LuauScriptHost::get_max_commands_per_call);
    ClassDB::bind_method(D_METHOD("set_threaded", "v"), &LuauScriptHost::set_threaded);
    ClassDB::bind_method(D_METHOD("get_threaded"), &LuauScriptHost::get_threaded);
    ClassDB::bind_method(D_METHOD("set_auto_tick", "v"), &LuauScriptHost::set_auto_tick);
    ClassDB::bind_method(D_METHOD("get_auto_tick"), &LuauScriptHost::get_auto_tick);

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "threaded"), "set_threaded", "get_threaded");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_millis_per_call"), "set_max_millis", "get_max_millis");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_frame_millis"), "set_max_frame_millis", "get_max_frame_millis");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_steps_per_call"), "set_max_steps", "get_max_steps");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_memory_mb"), "set_max_memory_mb", "get_max_memory_mb");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_commands_per_call"), "set_max_commands_per_call", "get_max_commands_per_call");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_tick"), "set_auto_tick", "get_auto_tick");

    ADD_SIGNAL(MethodInfo("script_print", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "text")));
    ADD_SIGNAL(MethodInfo("script_error", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "error")));
    ADD_SIGNAL(MethodInfo("script_killed", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "reason")));
    ADD_SIGNAL(MethodInfo("script_result", PropertyInfo(Variant::INT, "handle"), PropertyInfo(Variant::STRING, "fn"), PropertyInfo(Variant::DICTIONARY, "result")));
    ADD_SIGNAL(MethodInfo("tick_finished", PropertyInfo(Variant::DICTIONARY, "stats")));
    ADD_SIGNAL(MethodInfo("part_create_requested", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::VECTOR3, "position")));
    ADD_SIGNAL(MethodInfo("part_destroy_requested", PropertyInfo(Variant::INT, "id")));
    ADD_SIGNAL(MethodInfo("part_move_requested", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::VECTOR3, "position")));
}
