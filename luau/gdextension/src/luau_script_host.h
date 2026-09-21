// Godot 4 node owning one script worker (one Sandbox) per experience.
// Build: ../SConstruct, with godot-cpp (4.3+) checked out at ../godot-cpp.
//
// Every method posts a job to the pulseblockz::ScriptWorker thread and returns. What the scripts
// do comes back as HostEvents; the worker wakes this node with call_deferred("_drain_events"),
// which turns them into signals on the main thread, the only place scene-tree work is allowed.
// `threaded = false`, set before the first script loads, runs the same jobs inline.
#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>
#include "script_worker.h"

namespace godot {

class LuauScriptHost : public Node {
    GDCLASS(LuauScriptHost, Node)

public:
    LuauScriptHost();
    ~LuauScriptHost() override;

    // Budgets, bound as properties so an experience can tune them.
    void set_max_millis(double v);        double get_max_millis() const;
    void set_max_frame_millis(double v);  double get_max_frame_millis() const;
    void set_max_steps(int64_t v);        int64_t get_max_steps() const;
    void set_max_memory_mb(int v);        int get_max_memory_mb() const;
    void set_max_commands_per_call(int v); int get_max_commands_per_call() const;
    void set_threaded(bool v);            bool get_threaded() const;
    void set_auto_tick(bool v);           bool get_auto_tick() const;

    /// Compiles on the worker. The handle comes back at once; a compile error arrives later as
    /// `script_error(name, error)` and leaves the handle a no-op.
    int load_script(const String& name, const String& source);
    void unload_script(int handle);
    /// Run the top-level chunk. Completion: `script_result(handle, "main", stats)`.
    void run_main(int handle);
    /// Calls a global (e.g. "OnPlayerJoin"). Completion: `script_result(handle, fn, stats)`.
    void call_fn(int handle, const String& fn, double arg = 0.0);
    /// Tick(delta) on every loaded script under the frame budget. Ticks queued faster than the
    /// worker runs them coalesce, deltas summed. Completion: `tick_finished(stats)`; a script
    /// over budget: `script_killed(name, reason)`.
    void tick_all(double delta);
    /// Blocks until every queued job has run and delivers the events now rather than at end of
    /// frame: lockstep servers and tests.
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
};

} // namespace godot
