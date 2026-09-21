// Godot 4 node that owns one script worker (one Sandbox) per experience.
// Build: see ../SConstruct. Requires godot-cpp (4.3+) checked out at ../godot-cpp.
//
// Threading model (Phase 1 decision):
//   Creator scripts run on a worker thread owned by pulseblockz::ScriptWorker. Every
//   method here is non-blocking: it posts a job and returns. Whatever the scripts do
//   (print, Part.create, blow a budget) comes back as HostEvents; the worker wakes
//   this node with call_deferred("_drain_events"), which turns them into signals on
//   the main thread, so scene-tree work always happens where Godot expects it.
//   Set `threaded = false` (before the first script loads) for the old inline
//   behaviour: same API, same signals, but everything happens inside the call.
#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "script_worker.h"

namespace godot {

class LuauScriptHost : public Node {
    GDCLASS(LuauScriptHost, Node)

public:
    LuauScriptHost();
    ~LuauScriptHost() override;

    // Budgets (exported so they're tunable in the inspector / per-experience)
    void set_max_millis(double v);        double get_max_millis() const;
    void set_max_frame_millis(double v);  double get_max_frame_millis() const;
    void set_max_steps(int64_t v);        int64_t get_max_steps() const;
    void set_max_memory_mb(int v);        int get_max_memory_mb() const;
    void set_max_commands_per_call(int v); int get_max_commands_per_call() const;
    void set_threaded(bool v);            bool get_threaded() const;
    void set_auto_tick(bool v);           bool get_auto_tick() const;

    /// Compile + load on the worker. Returns a handle immediately; a compile error
    /// arrives as `script_error(name, error)` and the handle becomes a no-op.
    int load_script(const String& name, const String& source);
    /// Also emits `part_destroy_requested` for every part the script created, so
    /// a script can be replaced (hot reload) without leaving its objects behind.
    void unload_script(int handle);
    /// Run the top-level chunk. Completion: `script_result(handle, "main", stats)`.
    void run_main(int handle);
    /// Call a global (e.g. "OnPlayerJoin") with an optional number. Completion:
    /// `script_result(handle, fn, stats)`.
    void call_fn(int handle, const String& fn, double arg = 0.0);
    /// Tick(delta) on every loaded script under the frame budget. Ticks queued
    /// faster than the worker runs them are coalesced (deltas summed). Completion:
    /// `tick_finished(stats)`; budget kills: `script_killed(name, reason)`.
    void tick_all(double delta);
    /// Block until the worker has run every queued job, then deliver its events
    /// now instead of at end of frame. For lockstep servers and tests.
    void flush();
    bool is_idle() const;

    int64_t get_memory_used() const;

    void _ready() override;
    void _process(double delta) override;
    /// Deferred-call target: turns queued HostEvents into signals. Main thread only.
    void _drain_events();

protected:
    static void _bind_methods();

private:
    void ensure_worker();
    void push_budget();
    static Dictionary to_dict(const pulseblockz::RunResult& r);

    pulseblockz::ScriptWorker::Options opts_;
    std::unique_ptr<pulseblockz::ScriptWorker> worker_;
    bool autoTick_ = true;
    std::unordered_set<int> live_;                                // handles not yet unloaded
    std::unordered_map<int, std::vector<int64_t>> owned_;         // handle -> parts it created
};

} // namespace godot
