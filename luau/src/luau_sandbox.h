// PulseBlockz Luau sandbox — engine-agnostic. Godot binding lives in gdextension/.
// Host-enforced, not trusted to the script: steps and time in Luau's interrupt, memory in the
// allocator (over budget Luau raises a catchable "not enough memory"), os and debug removed and
// the globals frozen. io, loadstring and require do not exist in Luau at all.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

struct lua_State;

struct lua_Debug;   // the VM's frame description, for the debugger callbacks
namespace pulseblockz {

struct Budget {
    uint64_t maxSteps   = 1'000'000;  // interrupt ticks (≈ loop iterations + calls); 0 = unlimited
    double   maxMillis  = 4.0;        // wall clock per call
    double   maxFrameMillis = 0.0;    // wall clock for every call since beginFrame(); 0 = unlimited
    size_t   maxMemory  = 8u << 20;   // bytes, per sandbox (all scripts share); 0 = unlimited
};

struct RunResult {
    bool        ok = false;
    bool        skipped = false; // frame budget already spent: the call never started
    bool        killed = false;  // !ok because a step/time budget was exceeded
    bool        yielded = false; // resumeUnderBudget: the coroutine yielded (ok=true)
    bool        broke = false;   // yielded at a breakpoint or a debugger step; resume with 0 args to go on
    std::string error;       // populated when !ok
    uint64_t    stepsUsed = 0;
    double      millisUsed = 0;
    size_t      memoryNow = 0;
    size_t      memoryPeak = 0;
};

// The lua_State* is the calling script's own thread, not the main state; the return is the
// number of results left on that stack.
using HostFn = int (*)(lua_State*);

class Sandbox {
public:
    explicit Sandbox(Budget budget);
    ~Sandbox();
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    /// Must run before the first loadScript or seal(): either one freezes the globals for good.
    void expose(const char* tableName, std::initializer_list<std::pair<const char*, HostFn>> fns);

    /// Where print() goes; stdout when unset.
    void setPrintHandler(std::function<void(const std::string&)> fn);

    /// Fired by the VM at a BREAK instruction (lua_breakpoint) and, in single-step mode, at
    /// every instruction. A handler that calls lua_break(L) parks the coroutine with LUA_BREAK.
    std::function<void(lua_State*, lua_Debug*)> onBreak, onStep;

    /// Compiles into a thread with its own global environment (luaL_sandboxthread); `chunkname`
    /// names it in errors. Returns a handle > 0, or 0. Top-level code runs at runMain(), not here.
    int loadScript(const std::string& chunkname, const std::string& source, std::string* errorOut = nullptr);
    void unloadScript(int handle);

    /// Execute the script's top-level chunk under budget.
    RunResult runMain(int handle);

    /// Calls a global the script defined, under budget. No such global is ok=true and does
    /// nothing: scripts opt in to "Tick" and the rest.
    RunResult call(int handle, const char* fnName, double numberArg);
    RunResult call(int handle, const char* fnName);

    /// Opens a frame's batch: each call in it runs under min(maxMillis, maxFrameMillis -
    /// frameMillisUsed()) and returns skipped=true once that is gone, so N scripts cannot cost
    /// N x maxMillis. Only beginFrame() clears frameMs_, so a standalone call needs one too.
    void beginFrame();
    double frameMillisUsed() const { return frameMs_; }

    size_t memoryNow() const { return memUsed_; }
    size_t memoryPeak() const { return memPeak_; }
    void setBudget(const Budget& b) { budget_ = b; }
    const Budget& budget() const { return budget_; }
    lua_State* rawState() { return L_; }

    /// Opaque slot for the owner; a host function reaches it from any script thread via from(L).
    void setUserData(void* p) { userData_ = p; }
    void* userData() const { return userData_; }
    static Sandbox* from(lua_State* L);

    /// luaL_sandbox: stdlib tables and _G become read-only. loadScript does it implicitly; an
    /// embedder driving its own globals and threads (rbx_runtime) calls it when setup is done.
    void seal();
    /// Resumes `co` with nargs values already on its stack, under the per-call and frame budgets.
    /// ok && yielded: waiting; ok && !yielded: returned; !ok: raised and dead, killed telling a
    /// budget apart from a script error. asError raises the top of `co` inside it rather than
    /// passing it (a RemoteFunction whose other side errored). keepOnSkip leaves the function and
    /// its args on `co` for a retry next frame.
    RunResult resumeUnderBudget(lua_State* co, int nargs, bool asError = false, bool keepOnSkip = false);

    /// No thread affinity, no internal locking: use from one thread at a time (script_worker.h).

private:
    static void* alloc(void* ud, void* ptr, size_t osize, size_t nsize);
    static void interrupt(lua_State* L, int gc);
    RunResult pcallUnderBudget(lua_State* T, int nargs);

    Budget budget_;
    lua_State* L_ = nullptr;
    size_t memUsed_ = 0, memPeak_ = 0;
    void* userData_ = nullptr;
    // per-invocation
    uint64_t steps_ = 0;
    double startMs_ = 0;
    double deadlineMs_ = 0;   // startMs_ + effective budget for this call
    double callMs_ = 0;       // effective budget (for the error message)
    // per-frame
    double frameMs_ = 0;
    bool aborted_ = false;
    std::string abortReason_;
    std::function<void(const std::string&)> print_;
    int nextHandle_ = 1;
    bool sealed_ = false;
    bool armBudget(RunResult& r, lua_State* T, int popOnSkip);
    void finishBudget(RunResult& r);
};

} // namespace pulseblockz
