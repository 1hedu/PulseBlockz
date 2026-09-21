#include "luau_sandbox.h"

#include "lua.h"
#include "lualib.h"
#include "luacode.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace pulseblockz {

static double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

// registry table of handle -> thread: keeps script threads off the collector and findable
static const char* kThreads = "pulseblockz.threads";

Sandbox::Sandbox(Budget budget) : budget_(budget) {
    print_ = [](const std::string& s) { std::fputs(s.c_str(), stdout); std::fputc('\n', stdout); };

    L_ = lua_newstate(&Sandbox::alloc, this);
    lua_setthreaddata(L_, this);

    luaL_openlibs(L_);

    // Luau ships no io/package/loadstring, but os and debug are there: creator code must
    // not time or fingerprint the host, nor read stack frames.
    lua_pushnil(L_); lua_setglobal(L_, "os");
    lua_pushnil(L_); lua_setglobal(L_, "debug");
    lua_pushnil(L_); lua_setglobal(L_, "collectgarbage");
    lua_pushnil(L_); lua_setglobal(L_, "gcinfo");
    lua_pushnil(L_); lua_setglobal(L_, "getfenv");
    lua_pushnil(L_); lua_setglobal(L_, "setfenv");

    lua_pushcfunction(L_, [](lua_State* L) -> int {
        auto* self = static_cast<Sandbox*>(lua_getthreaddata(lua_mainthread(L)));
        std::string out;
        int n = lua_gettop(L);
        for (int i = 1; i <= n; i++) {
            size_t len; const char* s = luaL_tolstring(L, i, &len);
            if (i > 1) out += '\t';
            out.append(s, len);
            lua_pop(L, 1);
        }
        self->print_(out);
        return 0;
    }, "print");
    lua_setglobal(L_, "print");

    lua_newtable(L_);
    lua_setfield(L_, LUA_REGISTRYINDEX, kThreads);

    lua_Callbacks* cb = lua_callbacks(L_);
    cb->interrupt = &Sandbox::interrupt;
    cb->debugbreak = [](lua_State* L, lua_Debug* ar) {
        auto* self = static_cast<Sandbox*>(lua_getthreaddata(lua_mainthread(L)));
        if (self && self->onBreak) self->onBreak(L, ar);
    };
    cb->debugstep = [](lua_State* L, lua_Debug* ar) {
        auto* self = static_cast<Sandbox*>(lua_getthreaddata(lua_mainthread(L)));
        if (self && self->onStep) self->onStep(L, ar);
    };
}

Sandbox::~Sandbox() {
    if (L_) lua_close(L_);
}

void Sandbox::seal() {
    if (sealed_) return;
    // Freezes the stdlib tables and _G; a script thread gets a writable env over them.
    luaL_sandbox(L_);
    sealed_ = true;
}

void Sandbox::expose(const char* tableName, std::initializer_list<std::pair<const char*, HostFn>> fns) {
    if (sealed_) { std::fprintf(stderr, "Sandbox::expose(%s) after seal — ignored\n", tableName); return; }
    lua_newtable(L_);
    for (auto& [name, fn] : fns) {
        lua_pushcfunction(L_, fn, name);
        lua_setfield(L_, -2, name);
    }
    lua_setreadonly(L_, -1, true);
    lua_setglobal(L_, tableName);
}

void Sandbox::setPrintHandler(std::function<void(const std::string&)> fn) { print_ = std::move(fn); }

// ---- memory budget ----------------------------------------------------------
void* Sandbox::alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* self = static_cast<Sandbox*>(ud);
    if (nsize == 0) {
        self->memUsed_ -= osize;
        std::free(ptr);
        return nullptr;
    }
    size_t next = self->memUsed_ - osize + nsize;
    if (self->budget_.maxMemory && next > self->budget_.maxMemory) return nullptr; // Luau raises "not enough memory"
    void* p = std::realloc(ptr, nsize);
    if (!p) return nullptr;
    self->memUsed_ = next;
    if (next > self->memPeak_) self->memPeak_ = next;
    return p;
}

// ---- step / time budget ------------------------------------------------------
void Sandbox::interrupt(lua_State* L, int gc) {
    if (gc >= 0) return; // GC-phase callback; not a script step
    auto* self = static_cast<Sandbox*>(lua_getthreaddata(lua_mainthread(L)));
    self->steps_++;
    if (self->budget_.maxSteps && self->steps_ > self->budget_.maxSteps) {
        self->aborted_ = true;
        self->abortReason_ = "step budget exceeded";
        luaL_error(L, "script killed: step budget exceeded (%llu)", (unsigned long long)self->budget_.maxSteps);
    }
    // Every 1024 steps: a steady_clock read per interrupt would dominate a tight loop.
    if ((self->steps_ & 1023) == 0) {
        if (nowMs() > self->deadlineMs_) {
            self->aborted_ = true;
            self->abortReason_ = "time budget exceeded";
            luaL_error(L, "script killed: time budget exceeded (%.2f ms)", self->callMs_);
        }
    }
}

Sandbox* Sandbox::from(lua_State* L) {
    return static_cast<Sandbox*>(lua_getthreaddata(lua_mainthread(L)));
}

void Sandbox::beginFrame() { frameMs_ = 0; }

// ---- loading ----------------------------------------------------------------
int Sandbox::loadScript(const std::string& chunkname, const std::string& source, std::string* errorOut) {
    seal();
    lua_CompileOptions opts = {};
    opts.optimizationLevel = 1;
    opts.debugLevel = 1;
    size_t bcLen = 0;
    char* bc = luau_compile(source.c_str(), source.size(), &opts, &bcLen);

    lua_State* T = lua_newthread(L_);
    luaL_sandboxthread(T);

    int status = luau_load(T, chunkname.c_str(), bc, bcLen, 0);
    std::free(bc);
    if (status != 0) {
        if (errorOut) errorOut->assign(lua_tostring(T, -1));
        lua_pop(L_, 1); // drop thread
        return 0;
    }
    // The main chunk stays at T's top; the registry entry keeps the thread alive.
    int handle = nextHandle_++;
    lua_getfield(L_, LUA_REGISTRYINDEX, kThreads);
    lua_pushvalue(L_, -2);          // thread
    lua_rawseti(L_, -2, handle);
    lua_pop(L_, 2);                 // registry table + thread
    return handle;
}

static lua_State* threadFor(lua_State* L, int handle) {
    lua_getfield(L, LUA_REGISTRYINDEX, kThreads);
    lua_rawgeti(L, -1, handle);
    lua_State* T = lua_tothread(L, -1);
    lua_pop(L, 2);
    return T;
}

void Sandbox::unloadScript(int handle) {
    lua_getfield(L_, LUA_REGISTRYINDEX, kThreads);
    lua_pushnil(L_);
    lua_rawseti(L_, -2, handle);
    lua_pop(L_, 1);
    lua_gc(L_, LUA_GCCOLLECT, 0);
}

// ---- execution ----------------------------------------------------------------
// False when the frame budget is already spent: r comes back skipped and `popOnSkip`
// values are dropped from T so the caller's stack stays balanced.
bool Sandbox::armBudget(RunResult& r, lua_State* T, int popOnSkip) {
    callMs_ = budget_.maxMillis;
    if (budget_.maxFrameMillis > 0) {
        double remaining = budget_.maxFrameMillis - frameMs_;
        if (remaining <= 0) {
            lua_pop(T, popOnSkip);
            r.skipped = true;
            r.error = "skipped: frame budget exhausted";
            r.memoryNow = memUsed_; r.memoryPeak = memPeak_;
            return false;
        }
        if (remaining < callMs_) callMs_ = remaining;
    }
    steps_ = 0; aborted_ = false; abortReason_.clear();
    startMs_ = nowMs();
    deadlineMs_ = startMs_ + callMs_;
    return true;
}

void Sandbox::finishBudget(RunResult& r) {
    r.millisUsed = nowMs() - startMs_;
    frameMs_ += r.millisUsed;
    r.stepsUsed = steps_;
    r.memoryNow = memUsed_;
    r.memoryPeak = memPeak_;
    r.killed = aborted_;
}

RunResult Sandbox::pcallUnderBudget(lua_State* T, int nargs) {
    RunResult r;
    // nargs + 1: the function and its args, to drop if the call never starts
    if (!armBudget(r, T, nargs + 1)) return r;
    int status = lua_pcall(T, nargs, 0, 0);
    finishBudget(r);
    r.ok = (status == LUA_OK);
    if (!r.ok) {
        const char* msg = lua_tostring(T, -1);
        r.error = msg ? msg : (status == LUA_ERRMEM ? "not enough memory" : "unknown error");
        lua_pop(T, 1);
        // Revives a thread the error may have killed, keeping its global table, so the
        // script's own functions survive. Never luaL_sandboxthread here: that layers
        // another globals table whose __index points at the old one, and 100 such links
        // hit Luau's MAXTAGLOOP -- "'__index' chain too long" outside any pcall.
        lua_resetthread(T);
    }
    return r;
}

RunResult Sandbox::resumeUnderBudget(lua_State* co, int nargs, bool asError, bool keepOnSkip) {
    RunResult r;
    if (!armBudget(r, co, keepOnSkip ? 0 : nargs)) return r;
    int status = asError ? lua_resumeerror(co, nullptr) : lua_resume(co, nullptr, nargs);
    finishBudget(r);
    if (status == LUA_OK || status == LUA_YIELD || status == LUA_BREAK) {
        r.ok = true;
        r.yielded = (status != LUA_OK);
        r.broke = (status == LUA_BREAK);
        // Yielded or returned values are left on co for the caller.
        return r;
    }
    const char* msg = lua_tostring(co, -1);
    r.error = msg ? msg : (status == LUA_ERRMEM ? "not enough memory" : "unknown error");
    lua_pop(co, 1);
    return r;
}

RunResult Sandbox::runMain(int handle) {
    lua_State* T = threadFor(L_, handle);
    if (!T) { RunResult r; r.error = "bad handle"; return r; }
    // pcall consumes the main chunk, so call a copy and leave the original at T's top.
    lua_pushvalue(T, -1);
    return pcallUnderBudget(T, 0);
}

RunResult Sandbox::call(int handle, const char* fnName, double numberArg) {
    lua_State* T = threadFor(L_, handle);
    if (!T) { RunResult r; r.error = "bad handle"; return r; }
    lua_getglobal(T, fnName);
    if (!lua_isfunction(T, -1)) { lua_pop(T, 1); RunResult r; r.ok = true; return r; }
    lua_pushnumber(T, numberArg);
    return pcallUnderBudget(T, 1);
}

RunResult Sandbox::call(int handle, const char* fnName) {
    lua_State* T = threadFor(L_, handle);
    if (!T) { RunResult r; r.error = "bad handle"; return r; }
    lua_getglobal(T, fnName);
    if (!lua_isfunction(T, -1)) { lua_pop(T, 1); RunResult r; r.ok = true; return r; }
    return pcallUnderBudget(T, 0);
}

} // namespace pulseblockz
