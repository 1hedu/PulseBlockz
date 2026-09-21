// Runs a Sandbox off the host's main thread: jobs in, events out, through queues. Engine-agnostic;
// the Godot node in gdextension/ is a thin adapter (call_deferred as the wake).
//
// The host thread -- whoever constructs the worker -- calls load/unload/runMain/call/tick/
// setBudget/drain. Threaded, those queue and return; the worker thread owns the Sandbox, so host
// functions registered in `setup` must not touch engine state: they emit() an event the host
// applies after drain(). threaded=false runs every job inline on the host thread, same API.
#pragma once
#include "luau_sandbox.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace pulseblockz {

struct HostEvent {
    enum Type {
        Print,      // script printed `text`
        LoadError,  // load() failed: `text` = compile error; `script` is now dead
        Killed,     // a call blew a budget: `text` = reason, `result` = stats
        Result,     // runMain/call finished: `name` = fn ("main" for runMain)
        TickStats,  // per batch: millisUsed = wall time, x = delta, id = ran, y = killed, z = skipped
        Command,    // host API call: `text` = op ("part.create"...), id, x/y/z
    };
    Type        type;
    int         script = 0;     // host handle (0 = not script-specific)
    std::string name;           // script name (or fn name for Result)
    std::string text;
    RunResult   result;
    int64_t     id = 0;
    double      x = 0, y = 0, z = 0;
};

class ScriptWorker {
public:
    struct Options {
        Budget budget;
        bool   threaded = true;
        /// Cap on Command events one invocation may emit (0 = unlimited). The budget bounds CPU on the worker; this bounds what a script hands the host thread.
        int    maxCommandsPerCall = 1024;
    };
    /// Runs on the worker thread before any script loads: expose() host tables here.
    using SetupFn = std::function<void(Sandbox&)>;

    ScriptWorker(Options opts, SetupFn setup);
    ~ScriptWorker();  // drains the job queue, stops the thread, destroys the sandbox
    ScriptWorker(const ScriptWorker&) = delete;
    ScriptWorker& operator=(const ScriptWorker&) = delete;

    // ---- host-thread API ---------------------------------------------------------
    /// Returns a host handle at once; compiling happens on the worker, and failure arrives as a LoadError with the handle left a no-op.
    int  load(const std::string& name, const std::string& source);
    void unload(int handle);
    void runMain(int handle);                                       // -> Result("main")
    void call(int handle, const std::string& fn, double arg = 0);   // -> Result(fn)
    /// Tick(delta) on every script under Budget::maxFrameMillis, which bounds a batch that per-call
    /// budgets alone would let reach N scripts x maxMillis. A queued tick absorbs the next delta:
    /// a slow batch means a bigger dt, not a backlog.
    void tick(double delta);                                         // -> TickStats
    void setBudget(const Budget& b, int maxCommandsPerCall = -1);   // -1 = keep current

    /// Take every pending event, in the order the worker produced them.
    std::vector<HostEvent> drain();
    /// Called on the WORKER thread when the event queue goes empty -> non-empty; schedule a drain from it (Godot: call_deferred), never block.
    void setWakeHandler(std::function<void()> fn);

    /// Block until every queued job has run (threaded mode). For tests and lockstep.
    void waitIdle();
    bool idle() const;
    size_t memoryUsed() const { return memUsed_.load(std::memory_order_relaxed); }
    bool threaded() const { return threaded_; }

    // ---- worker-thread API (for host functions) ------------------------------------
    static ScriptWorker* from(lua_State* L) { return static_cast<ScriptWorker*>(Sandbox::from(L)->userData()); }
    void emit(HostEvent ev);        // any thread
    /// emit() with the cap applied: returns false and drops the event once the invocation is over it. Raise a Luau error on false, to kill the script.
    bool command(HostEvent ev);
    int  commandsThisCall() const { return commandsThisCall_; }
    int  maxCommandsPerCall() const { return opts_.maxCommandsPerCall; }
    int  currentScript() const { return current_; }
    const std::string& currentScriptName() const;
    Sandbox& sandbox() { return *sb_; }
    /// Monotonic id source for host objects scripts create (parts etc).
    int64_t nextObjectId() { return nextObjectId_++; }

private:
    struct Job {
        enum Kind { Load, Unload, RunMain, Call, Tick, SetBudget, Stop } kind = Stop;
        int handle = 0;
        std::string a, b;
        double num = 0;
        Budget budget;
        int maxCommands = -1;
    };
    void submit(Job j);
    void execute(Job& j);
    void loop();
    void ensureSandbox();
    void doTick(double delta);

    Options opts_;
    SetupFn setup_;
    bool threaded_;

    // worker-owned
    std::unique_ptr<Sandbox> sb_;
    std::unordered_map<int, int> sbHandles_;       // host handle -> sandbox handle
    std::unordered_map<int, std::string> names_;
    std::vector<int> order_;                       // load order, for round-robin ticks
    size_t rr_ = 0;
    int current_ = 0;
    int commandsThisCall_ = 0;
    int64_t nextObjectId_ = 1;

    // shared
    std::atomic<int> nextHandle_{1};
    std::atomic<size_t> memUsed_{0};
    mutable std::mutex jobMu_;
    std::condition_variable jobCv_, idleCv_;
    std::deque<Job> jobs_;
    bool running_ = false;                          // a job is executing
    std::mutex evMu_;
    std::vector<HostEvent> events_;
    std::function<void()> wake_;
    std::thread thread_;
};

} // namespace pulseblockz
