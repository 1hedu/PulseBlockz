// Runtime core: scheduler (task.*, wait, signals firing), script lifecycle,
// players, tweens, step(). The Instance API surface lives in rbx_api.cpp.
#include "rbx_internal.h"
#include "rbx_host.h"
#include "rbx_audio.h"
#include "luacode.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace pulseblockz::rbx {

thread_local const char* g_cframeValueLastHalf = nullptr;

static const char* kTaskCtx = "pulseblockz.ctx";   // nothing keys off it; a Task holds its own ScriptCtx

// os.date: strftime or the *t table, local or UTC (leading "!").
static int os_date(lua_State* L) {
    const char* fmt = luaL_optstring(L, 1, "%c");
    std::time_t t = lua_isnoneornil(L, 2) ? std::time(nullptr) : (std::time_t)luaL_checknumber(L, 2);
    bool utc = fmt[0] == '!'; if (utc) fmt++;
    std::tm tmv{};
#ifdef _WIN32
    if (utc) gmtime_s(&tmv, &t); else localtime_s(&tmv, &t);
#else
    if (utc) gmtime_r(&t, &tmv); else localtime_r(&t, &tmv);
#endif
    if (!std::strcmp(fmt, "*t")) {
        lua_newtable(L);
        lua_pushnumber(L, tmv.tm_year + 1900); lua_setfield(L, -2, "year");
        lua_pushnumber(L, tmv.tm_mon + 1); lua_setfield(L, -2, "month");
        lua_pushnumber(L, tmv.tm_mday); lua_setfield(L, -2, "day");
        lua_pushnumber(L, tmv.tm_hour); lua_setfield(L, -2, "hour");
        lua_pushnumber(L, tmv.tm_min); lua_setfield(L, -2, "min");
        lua_pushnumber(L, tmv.tm_sec); lua_setfield(L, -2, "sec");
        lua_pushnumber(L, tmv.tm_wday + 1); lua_setfield(L, -2, "wday");
        lua_pushnumber(L, tmv.tm_yday + 1); lua_setfield(L, -2, "yday");
        return 1;
    }
    char buf[256];
    size_t n = std::strftime(buf, sizeof buf, fmt, &tmv);
    lua_pushlstring(L, buf, n); return 1;
}

// ---- construction --------------------------------------------------------------------
Runtime::Impl::Impl(Runtime* s, Options o, Callbacks c)
    : self(s), opts(std::move(o)), cb(std::move(c)), dm(opts.isServer), sb(opts.budget), replicator(dm) {
    L = sb.rawState();
    sb.setUserData(self);
    lua_callbacks(L)->userthread = [](lua_State* parent, lua_State* T) {
        Runtime* rt = Runtime::from(T);
        if (!rt) return;
        Impl& r = rt->impl();
        if (r.tearingDown) return;
        if (!parent) { r.threadCtx.erase(T); return; }   // collected
        if (auto ctx = r.ctxOf(parent)) r.threadCtx[T] = std::move(ctx);
    };
    sb.onBreak = [this](lua_State* co, lua_Debug* ar) { onDebugBreak(co, ar); };
    sb.onStep = [this](lua_State* co, lua_Debug* ar) { onDebugStep(co, ar); };
    sb.setPrintHandler([this](const std::string& text) {
        // Host output only: the task API's print (below) covers script threads.
        if (cb.print) cb.print("", text);
    });

    dm.onReleased = [this](int64_t id) {
        editableImages.erase(id);
        editableMeshes.erase(id);
        if (Instance* i = dm.find(id)) if (i->className() == "DataModelContent") {
            size_t n = i->get("Data").s.size();
            dataModelContentBytes = n > dataModelContentBytes ? 0 : dataModelContentBytes - n;
        }
    };
    dm.onPropertyChanged = [this](Instance& i, const std::string& prop) {
        // A Content naming an object this side does not have gets a placeholder.
        if (!prop.empty() && prop[0] != '#' && prop[0] != '@')
            if (const PropDef* d = i.cls().findProp(prop); d && d->type == Value::Content) {
                Value v = i.get(prop);
                if (v.n == Value::ContentObject && v.ref && !dm.find(v.ref))
                    placeholderFor(v.ref, !v.s.empty() ? v.s : d->contentObjects ? d->contentObjects : "EditableImage");
            }
        if (prop[0] == '#') {
            Instance* cs = dm.getService("CollectionService");
            std::string tag = prop.substr(1);
            bool added = i.hasTag(tag);
            if (Signal* s = findSignal(*cs, (added ? "Tag+:" : "Tag-:") + tag)) if (s->hasConns()) fire(*s, [&](lua_State* co) { pushInstance(co, &i); return 1; });
            // TagAdded when the tag reaches its first instance in the tree, TagRemoved when it leaves its last.
            if (Signal* s = findSignal(*cs, added ? "TagAdded" : "TagRemoved")) if (s->hasConns()) {
                int others = 0;
                for (Instance* d : dm.root()->getDescendants()) if (d != &i && d->hasTag(tag)) { others++; break; }
                if (others == 0) fire(*s, [&](lua_State* co) { lua_pushstring(co, tag.c_str()); return 1; });
            }
            return;
        }
        if ((prop == "Enabled" || prop == "Disabled") && i.isA("BaseScript") && !i.binding) {
            if (shouldRun(i)) queueStart(i); else stopScript(i);
        }
        if (i.className() == "Lighting" && (prop == "ClockTime" || prop == "TimeOfDay")) {
            // One value in two forms, as on Roblox. Writing the twin re-enters here; the echo
            // dies only on setImpl's unchanged-value early return, so a value that does not
            // round-trip exactly -- or a normalising step added below -- loops forever.
            if (prop == "ClockTime") {
                double h = std::fmod(i.get("ClockTime").n, 24.0); if (h < 0) h += 24;
                int s = (int)std::lround(h * 3600) % 86400;
                char buf[16]; std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
                i.setInternal("TimeOfDay", Value::string(buf));
            } else {
                int hh = 0, mm = 0, ss = 0;
                std::sscanf(i.get("TimeOfDay").s.c_str(), "%d:%d:%d", &hh, &mm, &ss);
                i.setInternal("ClockTime", Value::number(std::fmod(hh + mm / 60.0 + ss / 3600.0, 24.0)));
            }
        }
        // LightingChanged(skyChanged): a property of Lighting itself, or, with true, of a Sky under it.
        // Once per value: a Content's string half speaks for the pair, and an alias not at all.
        if (prop[0] != '@' && prop != "Parent") {   // a Sky's arrival is onParentChanged's
            Instance* lighting = i.className() == "Lighting" ? &i : i.isA("Sky") && i.parent() && i.parent()->className() == "Lighting" ? i.parent() : nullptr;
            const PropDef* pd = lighting ? i.cls().findProp(prop) : nullptr;
            const bool echo = pd && (!pd->aliasOf.empty() || (pd->type == Value::Content && !pd->twin.empty()));
            if (lighting && lighting->binding && !echo) if (Signal* s = findSignal(*lighting, "LightingChanged")) if (s->hasConns()) fireValues(*lighting, "LightingChanged", {Value::boolean(lighting != &i)});
        }
        if (i.isA("BodyColors") && prop.size() > 6 && prop.compare(prop.size() - 6, 6, "Color3") == 0) bodyColorsChanged(i);
        if (i.isA("Humanoid") && prop == "StateName") {
            // FreeFalling(active) on the way into and out of Freefall; Swimming(speed) on the
            // way into and out of Swimming, the speed the root's on entry and 0 on leaving.
            const std::string was = humanoidState[i.id()], now = i.get("StateName").s;
            if (was != now) {
                humanoidState[i.id()] = now;   // before the handlers run: one may destroy the Humanoid
                if (i.binding && (was == "Freefall" || now == "Freefall")) if (Signal* s = findSignal(i, "FreeFalling")) if (s->hasConns()) fireValues(i, "FreeFalling", {Value::boolean(now == "Freefall")});
                if (i.binding && (was == "Swimming" || now == "Swimming")) if (Signal* s = findSignal(i, "Swimming")) if (s->hasConns()) {
                    double speed = 0;
                    if (now == "Swimming") if (Instance* root = i.parent() ? i.parent()->findFirstChild("HumanoidRootPart") : nullptr) {
                        Vec3 v = root->get("AssemblyLinearVelocity").v;
                        speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
                    }
                    fireValues(i, "Swimming", {Value::number(speed)});
                }
            }
        }
        if (i.isA("Sound") && (prop == "Playing" || prop == "IsLoaded")) {
            bool playing = i.get("Playing").b, loaded = i.get("IsLoaded").b;
            i.setInternal("IsPlaying", Value::boolean(playing && loaded));
            i.setInternal("IsPaused", Value::boolean(!(playing && loaded)));
            Value id = i.get("SoundId");
            if (prop == "IsLoaded") { if (loaded) fireValues(i, "Loaded", {id}); }
            else if (playing) fireValues(i, soundVerb == SoundResuming ? "Resumed" : "Played", {id});
            else {
                // The server's clock ran out just before this copy's: an end here, not a Stop.
                bool ended = soundVerb == SoundEnding;
                if (!ended && dm.applyingRemote()) {
                    double len = i.get("TimeLength").n;
                    if (len > 0 && i.get("TimePosition").n >= len - 0.25) { ended = true; i.setSilent("TimePosition", Value::number(0)); }
                }
                fireValues(i, soundVerb == SoundPausing ? "Paused" : ended ? "Ended" : "Stopped", {id});
            }
        }
        if ((prop == "Font" || prop == "FontFace") && i.cls().findProp("FontFace") && !dm.applyingRemote()) {
            // One choice in two properties, as on Roblox; a face no Enum.Font names reads back Unknown.
            if (prop == "Font") {
                Value face;
                if (fontOfEnum(i.get("Font").s, face) && i.get("FontFace") != face) i.set("FontFace", face);
            } else {
                std::string name = fontEnumOf(i.get("FontFace"));
                if (i.get("Font").s != name) i.set("Font", Value::enumItem(name, findEnum("Font")->find(name)->value));
            }
        }
        if (i.isA("Player") && (prop == "Team" || prop == "TeamColor")) {
            // One choice in two properties, as on Roblox; the server sets both halves.
            if (prop == "Team") {
                int64_t ref = i.get("Team").ref;
                Instance* team = ref ? dm.find(ref) : nullptr;
                if (team && !team->isA("Team")) team = nullptr;
                if (dm.isServer()) {
                    if (team) { i.set("TeamColor", team->get("TeamColor")); i.set("Neutral", Value::boolean(false)); }
                    else i.set("Neutral", Value::boolean(true));
                }
                int64_t was = playerTeam[i.id()], now = team ? team->id() : 0;
                if (was != now) {
                    playerTeam[i.id()] = now;
                    if (Instance* old = dm.find(was)) fireValues(*old, "PlayerRemoved", {Value::instance(i.id())});
                    if (team) fireValues(*team, "PlayerAdded", {Value::instance(i.id())});
                }
            } else if (dm.isServer()) {
                Instance* team = teamByColor(i.get("TeamColor"));
                if ((team ? team->id() : 0) != i.get("Team").ref) i.set("Team", Value::instance(team ? team->id() : 0));
            }
        }
        if (!i.binding) return;
        if (prop[0] == '@') {
            if (Signal* s = findSignal(i, "AttributeChanged")) if (s->hasConns()) fireValues(i, "AttributeChanged", {Value::string(prop.substr(1))});
            if (Signal* s = findSignal(i, "Attr:" + prop.substr(1))) if (s->hasConns()) fire(*s, [](lua_State*) { return 0; });
            return;
        }
        if (Signal* s = findSignal(i, "Changed")) if (s->hasConns()) {
            // A ValueBase's Changed carries the new value, not the property name, as on Roblox: a
            // BrickColorValue's as a BrickColor. A CFrameValue's Value is two halves, a RayValue's too, so
            // each fires once, on the last half, with the whole CFrame or Ray.
            const std::string& cn = i.className();
            if (prop == "Value" && cn.size() > 5 && cn.compare(cn.size() - 5, 5, "Value") == 0) {
                if (i.isA("BrickColorValue")) { int n = brickNearest(i.get("Value").c); fire(*s, [n](lua_State* co) { pushBrickColor(co, n); return 1; }); }
                else fireValues(i, "Changed", {i.get("Value")});
            } else if ((prop == "ValuePosition" || prop == "ValueOrientation") && i.isA("CFrameValue")) {
                if (!g_cframeValueLastHalf || prop == g_cframeValueLastHalf) {
                    CFrameV cf = cframeFromPosOrient(i.get("ValuePosition").v, i.get("ValueOrientation").v);
                    fire(*s, [cf](lua_State* co) { pushCFrame(co, cf); return 1; });
                }
            } else if ((prop == "ValueOrigin" || prop == "ValueDirection") && i.isA("RayValue")) {
                if (!g_cframeValueLastHalf || prop == g_cframeValueLastHalf) {
                    RayV ray{i.get("ValueOrigin").v, i.get("ValueDirection").v};
                    fire(*s, [ray](lua_State* co) { pushRay(co, ray); return 1; });
                }
            } else {
                fireValues(i, "Changed", {Value::string(prop)});
            }
        }
        if (Signal* s = findSignal(i, "Prop:" + prop)) if (s->hasConns()) fire(*s, [](lua_State*) { return 0; });
        // A RayValue's Value signal hears the pair once, on the last half to change.
        if ((prop == "ValueOrigin" || prop == "ValueDirection") && i.isA("RayValue") && (!g_cframeValueLastHalf || prop == g_cframeValueLastHalf))
            if (Signal* s = findSignal(i, "Prop:Value")) if (s->hasConns()) fire(*s, [](lua_State*) { return 0; });
        // A CFrame is its Position + Orientation pair (C0 / C1 on a joint): its signal hears either half.
        {
            auto ends = [&](const char* suf) { size_t n = std::strlen(suf); return prop.size() >= n && prop.compare(prop.size() - n, n, suf) == 0; };
            std::string cf;
            if (ends("Position")) cf = prop.substr(0, prop.size() - 8);
            else if (ends("Orientation")) cf = prop.substr(0, prop.size() - 11);
            else cf = "-";
            bool isCf = cf.empty() ? (i.isA("BasePart") || i.isA("Camera") || i.isA("Attachment")) : ((cf == "C0" || cf == "C1") && i.isA("JointInstance"));
            if (isCf) if (Signal* s = findSignal(i, cf.empty() ? "Prop:CFrame" : "Prop:" + cf)) if (s->hasConns()) fire(*s, [](lua_State*) { return 0; });
        }
        if ((prop == "Enabled" || prop == "Disabled") && i.isA("BaseScript")) {
            if (shouldRun(i)) queueStart(i); else stopScript(i);
        }
        if (i.isA("Humanoid") && i.get("SeatPart").ref) {
            if ((prop == "Jump" && i.get("Jump").b) || (prop == "Sit" && !i.get("Sit").b) || (prop == "Health" && i.get("Health").n <= 0)) unseat(i);
            else if (prop == "MoveDirection") steerVehicle(i);
        }
        if (prop == "Health" && i.isA("Humanoid")) {
            Value h = i.get("Health");
            if (Signal* s = findSignal(i, "HealthChanged")) if (s->hasConns()) fireValues(i, "HealthChanged", {h});
            if (h.n <= 0) {
                if (Signal* s = findSignal(i, "Died")) if (s->hasConns()) fireValues(i, "Died", {});
                if (dm.isServer() && i.parent()) {
                    Instance* players = dm.getService("Players");
                    for (auto& p : players->children())
                        if (p->isA("Player") && p->get("Character").ref == i.parent()->id() && players->get("CharacterAutoLoads").b) {
                            int64_t pid = p->id();
                            bool queued = false;
                            for (auto& [t, id] : respawns) if (id == pid) queued = true;
                            if (!queued) respawns.emplace_back(now + players->get("RespawnTime").n, pid);
                        }
                }
            }
        }
    };
    dm.onParentChanged = [this](Instance& i, Instance* oldP, Instance* newP) {
        if (oldP && oldP->binding) if (Signal* s = findSignal(*oldP, "ChildRemoved")) if (s->hasConns()) fireValues(*oldP, "ChildRemoved", {Value::instance(i.id())});
        if (newP && newP->binding) if (Signal* s = findSignal(*newP, "ChildAdded")) if (s->hasConns()) fireValues(*newP, "ChildAdded", {Value::instance(i.id())});
        for (Instance* a = oldP; a; a = a->parent())
            if (a->binding) if (Signal* s = findSignal(*a, "DescendantRemoving")) if (s->hasConns()) fireValues(*a, "DescendantRemoving", {Value::instance(i.id())});
        for (Instance* a = newP; a; a = a->parent())
            if (a->binding) if (Signal* s = findSignal(*a, "DescendantAdded")) if (s->hasConns()) fireValues(*a, "DescendantAdded", {Value::instance(i.id())});
        if (i.binding) if (Signal* s = findSignal(i, "AncestryChanged")) if (s->hasConns()) fireValues(i, "AncestryChanged", {Value::instance(i.id()), Value::instance(newP ? newP->id() : 0)});
        if (i.cls().service) {   // ServiceProvider's: a service entering or leaving the DataModel
            Instance* root = dm.root();
            if (root && root->binding && oldP == root) if (Signal* s = findSignal(*root, "ServiceRemoving")) if (s->hasConns()) fireValues(*root, "ServiceRemoving", {Value::instance(i.id())});
            if (root && root->binding && newP == root) if (Signal* s = findSignal(*root, "ServiceAdded")) if (s->hasConns()) fireValues(*root, "ServiceAdded", {Value::instance(i.id())});
        }
        if (!childWaiters.empty() && newP && !applyingBatch) checkChildWaiters();   // a replicated batch lands whole first
        if (newP && i.isA("BaseScript")) queueStart(i);
        for (Instance* d : i.getDescendants()) if (newP && d->isA("BaseScript")) queueStart(*d);
        if (i.isA("Tool")) toolMoved(i, oldP, newP);
        if (i.isA("BodyColors") && newP) bodyColorsChanged(i);
        if (i.isA("ProximityPrompt")) { if (newP) prompts.insert(i.id()); else promptGone(i.id()); }
        if (i.isA("Sound")) { if (newP) sounds.insert(i.id()); else { sounds.erase(i.id()); soundLoops.erase(i.id()); } }
        if (i.isA("AudioPlayer")) {
            if (newP) {
                audioPlayers.insert(i.id());
                // AutoPlay: first entry into the DataModel only, and never for a replicated player.
                if (!oldP && i.get("AutoPlay").b && !dm.applyingRemote()) audioPlay(i, true);
            } else audioPlayers.erase(i.id());
        }
        if (i.isA("Wire")) { if (newP) wires.insert(i.id()); else wires.erase(i.id()); }
        if (i.isA("InputBinding")) { if (newP) inputBindings.insert(i.id()); else { inputBindings.erase(i.id()); bindingKeysDown.erase(i.id()); } }
        if (i.isA("Motor6D") || i.isA("Motor")) { if (newP) motors.insert(i.id()); else motors.erase(i.id()); }
        if (i.isA("Explosion") && newP && !applyingBatch) if (Instance* ws = dm.getService("Workspace")) if (i.isDescendantOf(ws)) detonate(i);
        // A Sky coming to or going from Lighting is a sky change.
        if (i.isA("Sky")) for (Instance* p : {oldP, newP})
            if (p && p->binding && p->className() == "Lighting") if (Signal* s = findSignal(*p, "LightingChanged")) if (s->hasConns()) fireValues(*p, "LightingChanged", {Value::boolean(true)});
    };
    dm.onDestroying = [this](Instance& i) {
        if ((i.isA("Seat") || i.isA("VehicleSeat")) && i.get("Occupant").ref) if (Instance* h = dm.findRef(i.get("Occupant").ref)) unseat(*h);
        if (i.isA("Weld") && i.name() == "SeatWeld")   // Roblox stands the character up when its SeatWeld goes
            if (Instance* p1 = dm.findRef(i.get("Part1").ref)) if (Instance* m = p1->parent()) if (Instance* h = m->findFirstChildOfClass("Humanoid"))
                if (h->get("SeatPart").ref == i.get("Part0").ref) unseat(*h);
        if (i.isA("Humanoid") && i.get("SeatPart").ref) unseat(i);
        if (i.isA("Humanoid")) { appliedDescriptions.erase(i.id()); humanoidState.erase(i.id()); }
        if (i.isA("AnimationTrack")) trackParams.erase(i.id());
        if (i.isA("Tool")) { if (heldTools.count(i.id())) toolMoved(i, i.parent(), nullptr); else if (!dm.isServer() && i.get("HotbarSlot").n > 0) refreshHotbar(); }
        if (i.isA("ProximityPrompt")) promptGone(i.id());
        if (i.isA("Sound")) { sounds.erase(i.id()); soundLoops.erase(i.id()); }
        if (i.isA("AudioPlayer")) audioPlayers.erase(i.id());
        if (i.isA("Wire")) wires.erase(i.id());
        if (i.isA("InputBinding")) { inputBindings.erase(i.id()); bindingKeysDown.erase(i.id()); }
        if (i.isA("Motor6D") || i.isA("Motor")) motors.erase(i.id());
        if (i.isA("BaseScript")) stopScript(i);
        for (Instance* d : i.getDescendants()) if (d->isA("BaseScript")) stopScript(*d);
        if (i.binding) {
            if (Signal* s = findSignal(i, "Destroying")) if (s->hasConns()) fireValues(i, "Destroying", {});
            disconnectAll(i);
        }
    };

    // Roblox's os: time, clock and date only.
    lua_newtable(L);
    lua_pushcfunction(L, [](lua_State* L) -> int {
        using namespace std::chrono;
        lua_pushnumber(L, (double)duration_cast<seconds>(system_clock::now().time_since_epoch()).count()); return 1;
    }, "time"); lua_setfield(L, -2, "time");
    lua_pushcfunction(L, [](lua_State* L) -> int { lua_pushnumber(L, lua_clock()); return 1; }, "clock"); lua_setfield(L, -2, "clock");
    lua_pushcfunction(L, os_date, "date"); lua_setfield(L, -2, "date");
    lua_setglobal(L, "os");
    // As on Roblox, collectgarbage takes only "count" (kilobytes in use); gcinfo is that number.
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* opt = luaL_optstring(L, 1, "collect");
        if (std::strcmp(opt, "count")) luaL_error(L, "collectgarbage must be called with 'count'; use gcinfo() instead");
        lua_pushnumber(L, lua_gc(L, LUA_GCCOUNT, 0) + lua_gc(L, LUA_GCCOUNTB, 0) / 1024.0);
        return 1;
    }, "collectgarbage");
    lua_setglobal(L, "collectgarbage");
    lua_pushcfunction(L, [](lua_State* L) -> int { lua_pushinteger(L, lua_gc(L, LUA_GCCOUNT, 0)); return 1; }, "gcinfo");
    lua_setglobal(L, "gcinfo");

    openTypes(L);
    openTaskApi(*this);
    openInstanceApi(*this);

    lua_newtable(L); lua_setglobal(L, "shared");
    lua_newtable(L); lua_setglobal(L, "_G");
    {
        // A RemoteFunction callback's result or error reaches `reply` even if it yields.
        const char* src = "return function(reply, fn, ...) reply(pcall(fn, ...)) end";
        lua_CompileOptions copts = {};
        size_t bcLen = 0;
        char* bc = luau_compile(src, std::strlen(src), &copts, &bcLen);
        if (luau_load(L, "=invoke", bc, bcLen, 0) == 0 && lua_pcall(L, 0, 1, 0) == 0) invokeWrapperRef = lua_ref(L, -1);
        lua_pop(L, 1);
        std::free(bc);
        // ContextActionService handlers: their result reaches `sink` (Pass or not).
        const char* asrc = "return function(sink, fn, ...) sink(fn(...)) end";
        bc = luau_compile(asrc, std::strlen(asrc), &copts, &bcLen);
        if (luau_load(L, "=action", bc, bcLen, 0) == 0 && lua_pcall(L, 0, 1, 0) == 0) actionWrapperRef = lua_ref(L, -1);
        lua_pop(L, 1);
        std::free(bc);
    }
    sb.seal();
    // seal() froze every table the globals hold; these two are the scripts' shared scratch space.
    for (const char* shared : {"_G", "shared"}) {
        lua_getglobal(L, shared);
        lua_setreadonly(L, -1, false);
        lua_pop(L, 1);
    }
    // The DataModel's services exist from the start so `game.Workspace` etc. resolve.
    for (const char* svc : {"Workspace", "Players", "Lighting", "ReplicatedFirst", "ReplicatedStorage", "ServerScriptService",
                            "ServerStorage", "StarterGui", "StarterPack", "StarterPlayer", "Teams", "SoundService", "Chat", "RunService", "TextChatService"})
        dm.getService(svc);
    if (opts.isServer) makeDefaultChannels();
    // Fixed ids: a client's copy is the one the server's replication refers to.
    Instance* sp = dm.getService("StarterPlayer");
    int64_t skel = DataModel::kFirstServerId - 2;
    for (const char* c : {"StarterPlayerScripts", "StarterCharacterScripts"}) {
        if (!sp->findFirstChildOfClass(c)) { Instance::Ptr k = dm.createWithId(skel, c); k->setName(c); k->setParent(sp); }
        skel++;
    }
    // One Terrain under the Workspace, at a fixed id; no heights until something sculpts it.
    if (Instance* ws = dm.getService("Workspace"))
        if (!ws->findFirstChildOfClass("Terrain")) {
            Instance::Ptr t = dm.createWithId(DataModel::kFirstServerId - 3, "Terrain");
            t->setName("Terrain");
            t->setInternal("Anchored", Value::boolean(true));
            t->setInternal("Material", Value::enumItem("Grass", 1280));
            t->setParent(ws);
        }
    dm.takeChanges();   // the initial skeleton is not a change
    if (opts.isServer) loadDataStores();
}

// ---- DataStoreService's file: {"name": {"scope": {"key": value}}} -----------------
static void jsonQuote(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
            else out += (char)c;
        }
    }
    out += '"';
}

// A store keeps each value as JSON text: a Lua value would need a ref.
void Runtime::Impl::loadDataStores() {
    if (opts.dataStorePath.empty()) return;
    FILE* f = std::fopen(opts.dataStorePath.c_str(), "rb");
    if (!f) return;
    std::string text; char buf[4096]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    std::fclose(f);
    lua_pushcfunction(L, json_decode, "JSONDecode");
    lua_pushlstring(L, text.data(), text.size());
    if (lua_pcall(L, 1, 1, 0) != LUA_OK || !lua_istable(L, -1)) {
        std::string why = lua_isstring(L, -1) ? lua_tostring(L, -1) : "not a JSON object";
        if (cb.error) cb.error("", "DataStoreService: " + opts.dataStorePath + ": " + why + " (starting empty; it will be overwritten)");
        lua_pop(L, 1);
        return;
    }
    auto asText = [&](int idx, std::string& out) {
        lua_pushcfunction(L, json_encode, "JSONEncode");
        lua_pushvalue(L, idx < 0 ? idx - 1 : idx);
        bool ok = lua_pcall(L, 1, 1, 0) == LUA_OK;
        if (ok) out = lua_tostring(L, -1);
        lua_pop(L, 1);
        return ok;
    };
    auto readStores = [&](int table) {
        lua_pushnil(L);
        while (lua_next(L, table)) {                    // name -> scopes
            if (lua_type(L, -2) == LUA_TSTRING && lua_istable(L, -1)) {
                std::string name = lua_tostring(L, -2);
                lua_pushnil(L);
                while (lua_next(L, -2)) {               // scope -> keys
                    if (lua_type(L, -2) == LUA_TSTRING && lua_istable(L, -1)) {
                        std::string scope = lua_tostring(L, -2);
                        lua_pushnil(L);
                        while (lua_next(L, -2)) {       // key -> value
                            std::string text;
                            if (lua_type(L, -2) == LUA_TSTRING && asText(-1, text)) dataStores[name][scope][lua_tostring(L, -2)] = text;
                            lua_pop(L, 1);
                        }
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
    };
    // { name: { scope: { key: [ {v, t, d, j} ] } } }: the versions of every write
    auto readVersions = [&](int table) {
        lua_pushnil(L);
        while (lua_next(L, table)) {
            if (lua_type(L, -2) == LUA_TSTRING && lua_istable(L, -1)) {
                std::string name = lua_tostring(L, -2);
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    if (lua_type(L, -2) == LUA_TSTRING && lua_istable(L, -1)) {
                        std::string scope = lua_tostring(L, -2);
                        lua_pushnil(L);
                        while (lua_next(L, -2)) {
                            if (lua_type(L, -2) == LUA_TSTRING && lua_istable(L, -1)) {
                                std::string key = lua_tostring(L, -2);
                                auto& list = dataStoreVersions[name][scope][key];
                                for (int i = 1;; i++) {
                                    lua_rawgeti(L, -1, i);
                                    if (!lua_istable(L, -1)) { lua_pop(L, 1); break; }
                                    DsVersion v;
                                    lua_getfield(L, -1, "v"); if (lua_isstring(L, -1)) v.version = lua_tostring(L, -1); lua_pop(L, 1);
                                    lua_getfield(L, -1, "t"); v.time = lua_tonumber(L, -1); lua_pop(L, 1);
                                    lua_getfield(L, -1, "d"); v.deleted = lua_toboolean(L, -1); lua_pop(L, 1);
                                    lua_getfield(L, -1, "j"); asText(-1, v.json); lua_pop(L, 1);
                                    lua_getfield(L, -1, "u"); if (lua_istable(L, -1)) asText(-1, v.users); lua_pop(L, 1);
                                    lua_getfield(L, -1, "m"); if (lua_istable(L, -1)) asText(-1, v.meta); lua_pop(L, 1);
                                    if (!v.version.empty()) list.push_back(v);
                                    lua_pop(L, 1);
                                }
                            }
                            lua_pop(L, 1);
                        }
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
    };
    // Without the "pulseblockz" key the whole object is the stores, with no versions.
    lua_getfield(L, -1, "pulseblockz");
    bool versioned = lua_isnumber(L, -1);
    lua_pop(L, 1);
    if (versioned) {
        lua_getfield(L, -1, "stores");
        if (lua_istable(L, -1)) readStores(lua_gettop(L));
        lua_pop(L, 1);
        lua_getfield(L, -1, "versions");
        if (lua_istable(L, -1)) readVersions(lua_gettop(L));
        lua_pop(L, 1);
    } else {
        readStores(lua_gettop(L));
    }
    lua_pop(L, 1);
}

std::string Runtime::Impl::recordVersion(const std::string& name, const std::string& scope, const std::string& key,
                                         const std::string& json, bool deleted) {
    auto& list = dataStoreVersions[name][scope][key];
    DsVersion v;
    v.version = std::to_string(list.size() + 1);
    v.json = json;
    v.time = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
    v.deleted = deleted;
    list.push_back(v);
    dataStoresDirty = true;
    return v.version;
}

void Runtime::Impl::saveDataStores() {
    if (!dataStoresDirty || opts.dataStorePath.empty()) return;
    dataStoresDirty = false;
    std::string out = "{\"pulseblockz\":2,\"stores\":{";
    bool f1 = true;
    for (auto& [name, scopes] : dataStores) {
        if (!f1) out += ','; f1 = false;
        jsonQuote(out, name); out += ":{";
        bool f2 = true;
        for (auto& [scope, keys] : scopes) {
            if (!f2) out += ','; f2 = false;
            jsonQuote(out, scope); out += ":{";
            bool f3 = true;
            for (auto& [k, v] : keys) { if (!f3) out += ','; f3 = false; jsonQuote(out, k); out += ':'; out += v; }
            out += '}';
        }
        out += '}';
    }
    out += "},\"versions\":{";
    f1 = true;
    for (auto& [name, scopes] : dataStoreVersions) {
        if (!f1) out += ','; f1 = false;
        jsonQuote(out, name); out += ":{";
        bool f2 = true;
        for (auto& [scope, keys] : scopes) {
            if (!f2) out += ','; f2 = false;
            jsonQuote(out, scope); out += ":{";
            bool f3 = true;
            for (auto& [k, list] : keys) {
                if (!f3) out += ','; f3 = false;
                jsonQuote(out, k); out += ":[";
                bool f4 = true;
                for (const DsVersion& v : list) {
                    if (!f4) out += ','; f4 = false;
                    out += "{\"v\":"; jsonQuote(out, v.version);
                    out += ",\"t\":" + std::to_string(v.time);
                    out += v.deleted ? ",\"d\":true" : ",\"d\":false";
                    out += ",\"j\":"; out += v.json.empty() ? "null" : v.json;
                    if (!v.users.empty()) { out += ",\"u\":"; out += v.users; }
                    if (!v.meta.empty()) { out += ",\"m\":"; out += v.meta; }
                    out += '}';
                }
                out += ']';
            }
            out += '}';
        }
        out += '}';
    }
    out += "}}\n";
    std::string tmp = opts.dataStorePath + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) { if (cb.error) cb.error("", "DataStoreService: cannot write " + opts.dataStorePath); return; }
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    std::remove(opts.dataStorePath.c_str());   // rename does not replace on Windows
    std::rename(tmp.c_str(), opts.dataStorePath.c_str());
}

Runtime::Impl::~Impl() {
    // Releases below (Lua closing, the placeholders) would report into maps being destroyed.
    dm.onReleased = nullptr;
    saveDataStores();
    tearingDown = true;
    // Lua closes after these maps are gone (sb is declared first): no thread deaths into threadCtx.
    lua_callbacks(L)->userthread = nullptr;
    for (auto& [co, t] : byThread) delete t;
    byThread.clear();
}

// ---- scheduler ---------------------------------------------------------------------
std::shared_ptr<ScriptCtx> Runtime::Impl::ctxOf(lua_State* L) {
    auto it = byThread.find(L);
    if (it != byThread.end() && it->second->ctx) return it->second->ctx;
    auto ct = threadCtx.find(L);
    return ct != threadCtx.end() ? ct->second : nullptr;
}

bool Runtime::Impl::hasPluginCapability(lua_State* L) {
    const auto ctx = ctxOf(L);
    if (!ctx) return false;
    if (ctx->host) return true;
    const Instance* script = ctx->scriptId ? dm.find(ctx->scriptId) : nullptr;
    return script && isPluginScript(*script);
}

Task* Runtime::Impl::newTask(lua_State* from, std::shared_ptr<ScriptCtx> ctx) {
    lua_State* src = from ? from : L;
    lua_State* co = lua_newthread(src);
    Task* t = new Task();
    t->co = co;
    t->ref = lua_ref(src, -1);
    lua_pop(src, 1);
    t->ctx = std::move(ctx);
    byThread[co] = t;
    stats.threadsLive++;
    return t;
}

Task* Runtime::Impl::taskFor(lua_State* co) {
    auto it = byThread.find(co);
    if (it != byThread.end()) return it->second;
    // Adopt a coroutine a script made itself (coroutine.wrap / create) so the scheduler can resume it.
    Task* t = new Task();
    t->co = co;
    lua_pushthread(co);
    t->ref = lua_ref(co, -1);
    lua_pop(co, 1);
    if (auto ct = threadCtx.find(co); ct != threadCtx.end()) t->ctx = ct->second;
    byThread[co] = t;
    stats.threadsLive++;
    return t;
}

void Runtime::Impl::kill(Task* t) {
    if (pausedTask == t) pausedTask = nullptr;   // its script was stopped while it stood at a breakpoint
    byThread.erase(t->co);
    for (auto it = pendingInvokes.begin(); it != pendingInvokes.end();) it = (it->second == t) ? pendingInvokes.erase(it) : std::next(it);
    // In-flight requests too: a stale entry would resume this deleted Task when the host answers.
    for (auto it = pendingHttp.begin(); it != pendingHttp.end();) it = (it->second == t) ? pendingHttp.erase(it) : std::next(it);
    for (auto it = pendingSolid.begin(); it != pendingSolid.end();) it = (it->second.task == t) ? pendingSolid.erase(it) : std::next(it);
    for (auto it = pendingMeshPart.begin(); it != pendingMeshPart.end();) it = (it->second.task == t) ? pendingMeshPart.erase(it) : std::next(it);
    for (auto it = pendingImages.begin(); it != pendingImages.end();) it = (it->second == t) ? pendingImages.erase(it) : std::next(it);
    for (auto it = queueWaiters.begin(); it != queueWaiters.end();) it = (it->task == t) ? queueWaiters.erase(it) : std::next(it);
    // A PreloadAsync it was waiting in: the group goes, with its callback and the assets still to come.
    for (auto it = preloadGroups.begin(); it != preloadGroups.end();) {
        if (it->second.task != t) { ++it; continue; }
        const uint64_t group = it->first;
        if (it->second.cbRef != LUA_NOREF) lua_unref(L, it->second.cbRef);
        it = preloadGroups.erase(it);
        for (auto p = pendingPreload.begin(); p != pendingPreload.end();) p = (p->second.group == group) ? pendingPreload.erase(p) : std::next(p);
    }
    if (t->ref != LUA_NOREF) lua_unref(L, t->ref);
    t->state = Task::Dead;
    stats.threadsLive--;
    delete t;
}

void Runtime::Impl::report(const RunResult& r, const std::shared_ptr<ScriptCtx>& ctx) {
    std::string who = ctx ? ctx->name : "";
    if (r.killed) { stats.kills++; if (cb.killed) cb.killed(who, r.error); return; }
    stats.errors++;
    if (cb.error) cb.error(who, r.error);
    logLine(r.error, 3);
    // ScriptContext.Error(message, stackTrace, script): the stack is not kept here, so the trace is empty.
    if (Instance* sc = dm.find(DataModel::serviceId("ScriptContext")))
        if (Signal* s = findSignal(*sc, "Error")) if (s->hasConns()) {
            Instance* script = ctx ? dm.find(ctx->scriptId) : nullptr;
            std::string msg = r.error;
            fire(*s, [&](lua_State* co) { lua_pushstring(co, msg.c_str()); lua_pushstring(co, ""); pushInstance(co, script); return 3; });
        }
}

// A line of Output: kept for GetLogHistory (the last 500) and fired as LogService.MessageOut.
void Runtime::Impl::logLine(const std::string& text, int type) {
    logHistory.push_back({text, type, std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count()});
    if (logHistory.size() > 500) logHistory.pop_front();
    Instance* ls = dm.find(DataModel::serviceId("LogService"));
    if (!ls) return;
    if (Signal* s = findSignal(*ls, "MessageOut")) if (s->hasConns()) {
        const EnumDef* e = findEnum("MessageType");
        fire(*s, [&](lua_State* co) { lua_pushstring(co, text.c_str()); pushEnumItem(co, e, e->findValue(type)); return 2; });
    }
}

void Runtime::Impl::resume(Task* t, int nargs, bool asError) {
    if (pausedTask && t != pausedTask) {
        // The debugger holds the game still: this runs, args and all, when it lets go.
        t->pendingArgs = nargs;
        t->state = Task::Ready;
        ready.push_back(t);
        return;
    }
    Task::State prev = t->state;
    t->state = Task::Running;
    stats.resumes++;
    RunResult r;
    int64_t outerScript = dm.currentScript();
    dm.setCurrentScript(t->ctx ? t->ctx->scriptId : 0);   // what this task changes is this script's doing
    const auto resumedAt = std::chrono::steady_clock::now();
    depth++;
    if (depth == 1) {
        r = sb.resumeUnderBudget(t->co, nargs, asError, true);
    } else {
        // Nested (task.spawn, a signal from script code): raw lua_resume on purpose. The
        // VM-wide interrupt keeps charging the outer slice's deadline; resumeUnderBudget here
        // would arm a fresh budget and hand the nested call a slice of its own.
        int status = asError ? lua_resumeerror(t->co, nullptr) : lua_resume(t->co, nullptr, nargs);
        r.ok = status == LUA_OK || status == LUA_YIELD || status == LUA_BREAK;
        r.yielded = status != LUA_OK;
        r.broke = status == LUA_BREAK;
        if (!r.ok) {
            const char* msg = lua_tostring(t->co, -1);
            r.error = msg ? msg : "unknown error";
            r.killed = r.error.rfind("script killed:", 0) == 0;
            lua_pop(t->co, 1);
        }
    }
    depth--;
    if (depth == 0) stepScriptMs[t->ctx ? t->ctx->name : std::string("(host)")] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resumedAt).count();
    dm.setCurrentScript(outerScript);
    auto sink = t->sink; t->sink = nullptr;   // callSync hears it only if it ran through now
    if (r.skipped) {
        // Frame budget spent before it ran: its args are still on the stack, so requeue for a retry.
        stats.skipped++;
        t->pendingArgs = nargs;
        switch (prev) {
        case Task::Sleeping: t->state = prev; sleepers.push_back(t); break;
        case Task::Deferred: t->state = prev; deferred.push_front(t); break;
        case Task::WaitChild: t->state = prev; childWaiters.push_back(t); break;
        default: t->state = Task::Ready; ready.push_front(t); break;
        }
        return;
    }
    if (t->doomed) { unqueue(t); lua_resetthread(t->co); kill(t); return; }
    if (r.broke) { t->state = Task::Parked; return; }   // stopped at a breakpoint: the debugger has it
    if (!r.ok) { report(r, t->ctx); kill(t); return; }
    if (!r.yielded) {
        if (sink) (*sink)(t->co, lua_gettop(t->co));
        lua_settop(t->co, 0); kill(t); return;
    }
    if (t->state == Task::Running) {
        // Yielded outside the scheduler (coroutine.yield at top level): only a script's own
        // coroutine.resume can resume it, and that holds its own reference.
        kill(t);
    }
}

// ---- the debugger ------------------------------------------------------------------
void Runtime::Impl::rememberMain(int64_t id, const std::string& name, lua_State* co) {
    if (auto it = mainRefs.find(id); it != mainRefs.end()) lua_unref(L, it->second);
    lua_pushvalue(co, -1);
    mainRefs[id] = lua_ref(co, -1);
    lua_pop(co, 1);
    mainByName[name] = id;
    if (auto b = breakpoints.find(name); b != breakpoints.end())
        for (int line : b->second) lua_breakpoint(co, -1, line, 1);
}

// The task parks with LUA_BREAK. The stack, with each frame's locals and upvalues as tostring
// shows them, is taken now, while the stopped coroutine still holds it.
void Runtime::Impl::pauseAt(lua_State* co, int line) {
    Task* t = taskFor(co);
    pausedTask = t;
    stepMode = 0;
    lua_singlestep(co, 0);
    pause = Runtime::DebugPause();
    pause.script = t->ctx ? t->ctx->name : "";
    pause.line = line;
    const void* mainFn = nullptr;   // the script's chunk function, so its frame reads "(main)"
    if (t->ctx) if (auto m = mainRefs.find(t->ctx->scriptId); m != mainRefs.end()) { lua_getref(co, m->second); mainFn = lua_topointer(co, -1); lua_pop(co, 1); }
    for (int level = 0; level < 32; level++) {
        lua_Debug ar;
        if (!lua_getinfo(co, level, "sln", &ar)) break;
        Runtime::DebugFrame f;
        bool isMain = false;
        if (lua_getinfo(co, level, "f", &ar)) { isMain = lua_topointer(co, -1) == mainFn; lua_pop(co, 1); }
        f.function = ar.name ? ar.name : (isMain ? "(main)" : "(anonymous)");
        f.script = ar.source ? (ar.source[0] == '=' ? ar.source + 1 : ar.source) : "";
        f.line = ar.currentline;
        lua_checkstack(co, 8);
        for (int n = 1; n <= 64; n++) {
            const char* nm = lua_getlocal(co, level, n);
            if (!nm) break;
            if (nm[0] != '(') { size_t len; const char* s = luaL_tolstring(co, -1, &len); f.locals.push_back({nm, std::string(s, len)}); lua_pop(co, 1); }
            lua_pop(co, 1);
        }
        if (lua_getinfo(co, level, "f", &ar)) {
            for (int n = 1; n <= 64; n++) {
                const char* nm = lua_getupvalue(co, -1, n);
                if (!nm) break;
                size_t len; const char* s = luaL_tolstring(co, -1, &len);
                f.locals.push_back({nm, std::string(s, len)});
                lua_pop(co, 2);
            }
            lua_pop(co, 1);
        }
        pause.frames.push_back(std::move(f));
    }
    pauseFresh = true;
    lua_break(co);
}

// The VM resumes a broken coroutine on the BREAK instruction itself, so the
// first hit after a continue is the stop it just left: let that one through.
void Runtime::Impl::onDebugBreak(lua_State* co, lua_Debug* ar) {
    int line = ar ? ar->currentline : 0;
    if (co == skipBreakCo && line == skipBreakLine) { skipBreakCo = nullptr; return; }
    skipBreakCo = nullptr;
    pausedAtBreak = true;
    pauseAt(co, line);
}

// After debugStep: into stops at the next line anywhere (a call's first line included),
// over at the next line of this frame or a caller, out in a caller.
void Runtime::Impl::onDebugStep(lua_State* co, lua_Debug* ar) {
    if (!stepMode || !ar) return;
    int depth = lua_stackdepth(co), line = ar->currentline;
    bool stop = false;
    if (stepMode == 1) stop = line != stepLine || depth != stepDepth;
    else if (stepMode == 2) stop = depth < stepDepth || (depth == stepDepth && line != stepLine);
    else stop = depth < stepDepth;
    if (stop) { pausedAtBreak = false; pauseAt(co, line); }
}

int Runtime::setBreakpoint(const std::string& script, int line, bool on) {
    Impl& r = *impl_;
    if (on) r.breakpoints[script].insert(line);
    else if (auto it = r.breakpoints.find(script); it != r.breakpoints.end()) { it->second.erase(line); if (it->second.empty()) r.breakpoints.erase(it); }
    auto n = r.mainByName.find(script);
    if (n == r.mainByName.end()) return 0;
    auto m = r.mainRefs.find(n->second);
    if (m == r.mainRefs.end()) return 0;
    lua_getref(r.L, m->second);
    int landed = lua_breakpoint(r.L, -1, line, on ? 1 : 0);
    lua_pop(r.L, 1);
    return landed;
}
void Runtime::clearBreakpoints() {
    auto all = impl_->breakpoints;
    for (auto& [script, lines] : all) for (int line : lines) setBreakpoint(script, line, false);
    impl_->breakpoints.clear();
}
bool Runtime::debugPaused() const { return impl_->pausedTask != nullptr; }
bool Runtime::takeDebugPause(DebugPause& out) {
    if (!impl_->pauseFresh) return false;
    out = impl_->pause;
    impl_->pauseFresh = false;
    return true;
}
void Runtime::debugContinue() {
    Impl& r = *impl_;
    Task* t = r.pausedTask;
    if (!t) return;
    r.pausedTask = nullptr;
    r.stepMode = 0;
    lua_singlestep(t->co, 0);
    if (r.pausedAtBreak) { r.skipBreakCo = t->co; r.skipBreakLine = r.pause.line; }
    r.resume(t, 0);
}
void Runtime::debugStep(int kind) {
    Impl& r = *impl_;
    Task* t = r.pausedTask;
    if (!t) return;
    r.stepMode = kind < 1 || kind > 3 ? 1 : kind;
    r.stepDepth = lua_stackdepth(t->co);
    r.stepLine = r.pause.line;
    lua_singlestep(t->co, 1);
    r.pausedTask = nullptr;
    if (r.pausedAtBreak) { r.skipBreakCo = t->co; r.skipBreakLine = r.pause.line; }
    r.resume(t, 0);
}

void Runtime::Impl::unqueue(Task* t) {
    auto drop = [t](auto& q) { for (auto it = q.begin(); it != q.end();) it = (*it == t) ? q.erase(it) : it + 1; };
    drop(ready); drop(sleepers); drop(deferred); drop(childWaiters);
    for (Instance* i : bound) for (auto& [n, s] : static_cast<InstBinding*>(i->binding)->signals) drop(s->waiters);
}

// Roblox semantics: disabling or destroying a script ends every thread it started and drops
// every connection it made. A thread doing the stopping finishes its slice first (doomed).
void Runtime::Impl::stopScript(Instance& s) {
    int64_t id = s.id();
    started.erase(id);
    for (auto it = pendingStart.begin(); it != pendingStart.end();) it = (*it == id) ? pendingStart.erase(it) : it + 1;
    std::vector<Task*> victims;
    for (auto& [co, t] : byThread) if (t->ctx && t->ctx->scriptId == id) victims.push_back(t);
    for (Task* t : victims) {
        if (t->state == Task::Running) { t->doomed = true; continue; }
        unqueue(t);
        lua_resetthread(t->co);
        kill(t);
    }
    for (Instance* i : bound)
        for (auto& [n, sig] : static_cast<InstBinding*>(i->binding)->signals)
            for (auto& c : sig->conns)
                if (c.connected && c.ctx && c.ctx->scriptId == id) { c.connected = false; lua_unref(L, c.fnRef); }
    for (auto it = actionBindings.begin(); it != actionBindings.end();) {
        if (it->ctx && it->ctx->scriptId == id) { lua_unref(L, it->fnRef); it = actionBindings.erase(it); }
        else ++it;
    }
}

void Runtime::Impl::unbindAction(const std::string& name) {
    for (auto it = actionBindings.begin(); it != actionBindings.end();) {
        if (it->name == name) { lua_unref(L, it->fnRef); it = actionBindings.erase(it); }
        else ++it;
    }
}

// ---- input -------------------------------------------------------------------------
static int actionSink(lua_State* L) {
    const EnumDef* e; const EnumItem* i;
    rtOf(L).actionPassed = readEnumItem(L, 1, e, i) && !std::strcmp(e->name, "ContextActionResult") && !std::strcmp(i->name, "Pass");
    return 0;
}

void Runtime::windowFocus(bool focused) {
    Impl& r = *impl_;
    if (r.dm.isServer()) return;
    if (Instance* uis = r.dm.getService("UserInputService"))
        r.fireValues(*uis, focused ? "WindowFocused" : "WindowFocusReleased", {});
}

bool Runtime::lastInputSunk() const { return impl_->lastInputSunk; }

void Runtime::input(const UserInput& in) {
    Impl& r = *impl_;
    r.lastInputSunk = in.processed;
    if (r.dm.isServer()) return;
    const EnumDef* types = findEnum("UserInputType"); const EnumItem* ti = types->find(in.type);
    const EnumDef* states = findEnum("UserInputState"); const EnumItem* si = states->find(in.state);
    const EnumDef* keys = findEnum("KeyCode"); const EnumItem* ki = keys->find(in.key);
    if (!ti || !si) return;
    if (!ki) ki = keys->find("Unknown");
    bool keyboard = in.type == "Keyboard", mouseButton = in.type.rfind("MouseButton", 0) == 0;
    Instance* uis = r.dm.getService("UserInputService");
    Instance::Ptr& obj = r.inputObjects[in.type + "\n" + ki->name];
    if (!obj) { obj = r.dm.createInternal("InputObject"); obj->setName("InputObject"); }
    obj->setInternal("UserInputType", Value::enumItem(ti->name, ti->value));
    obj->setInternal("KeyCode", Value::enumItem(ki->name, ki->value));
    obj->setInternal("UserInputState", Value::enumItem(si->name, si->value));
    obj->setInternal("Position", Value::vector3(in.position));
    obj->setInternal("Delta", Value::vector3(in.delta));
    if (keyboard) { if (in.state == "Begin") r.keysDown.insert(ki->name); else if (in.state != "Change") r.keysDown.erase(ki->name); }
    if (mouseButton) { if (in.state == "Begin") r.mouseDown.insert(ti->name); else if (in.state != "Change") r.mouseDown.erase(ti->name); }
    if (in.type == "MouseMovement") { r.mousePos = in.position; r.mouseDelta = in.delta; }
    else if (mouseButton || in.type == "MouseWheel") r.mousePos = in.position;
    if (r.lastInputType != ti->name) {
        r.lastInputType = ti->name;
        if (Signal* s = r.findSignal(*uis, "LastInputTypeChanged"); s && s->hasConns())
            r.fire(*s, [&](lua_State* co) { pushEnumItem(co, types, ti); return 1; });
    }

    // A focused TextBox takes the keyboard, as on Roblox: no ContextActionService binding fires.
    // Clicks still go through, and UserInputService hears the key with gameProcessedEvent true.
    bool typing = keyboard && r.focusedTextBox() != nullptr;

    // ContextActionService first. Snapshot: a handler may bind / unbind.
    std::string keyTag = std::string("KeyCode.") + ki->name, typeTag = std::string("UserInputType.") + ti->name;
    std::vector<Impl::ActionBinding> hits;
    if (!typing)
        for (auto& b : r.actionBindings)
            for (auto& t : b.inputs)
                if (t == typeTag || (keyboard && t == keyTag)) { hits.push_back(b); break; }
    std::stable_sort(hits.begin(), hits.end(), [](const Impl::ActionBinding& a, const Impl::ActionBinding& b) {
        return a.priority != b.priority ? a.priority > b.priority : a.order > b.order;
    });
    bool processed = in.processed || typing;
    for (auto& b : hits) {
        bool live = false;
        for (auto& cur : r.actionBindings) if (cur.order == b.order) live = true;
        if (!live) continue;                    // unbound by an earlier handler
        r.actionPassed = false;
        Task* t = r.newTask(nullptr, b.ctx);
        lua_getref(t->co, r.actionWrapperRef);
        lua_pushcfunction(t->co, actionSink, "sink");
        lua_getref(t->co, b.fnRef);
        lua_pushstring(t->co, b.name.c_str());
        pushEnumItem(t->co, states, si);
        r.pushInstance(t->co, obj.get());
        r.resume(t, 5);
        if (!r.actionPassed) { processed = true; break; }
    }
    r.lastInputSunk = processed;
    const char* ev = in.state == "Begin" ? "InputBegan" : in.state == "Change" ? "InputChanged" : "InputEnded";
    if (Signal* s = r.findSignal(*uis, ev); s && s->hasConns())
        r.fire(*s, [&](lua_State* co) { r.pushInstance(co, obj.get()); lua_pushboolean(co, processed); return 2; });
    if (keyboard && in.state == "Begin" && !std::strcmp(ki->name, "Space")) r.fireValues(*uis, "JumpRequest", {});
    if (keyboard && !typing && in.state != "Change") inputActionsKey(r, ki->value, in.state == "Begin");

    // The classic Mouse (Player:GetMouse()) follows the pointer.
    if (r.mouse && !keyboard) {
        r.mouse->setInternal("X", Value::number(r.mousePos.x));
        r.mouse->setInternal("Y", Value::number(r.mousePos.y));
        const char* mev = in.type == "MouseMovement" ? "Move"
                        : in.type == "MouseWheel" ? (in.position.z > 0 ? "WheelForward" : "WheelBackward")
                        : in.type == "MouseButton1" ? (in.state == "Begin" ? "Button1Down" : "Button1Up")
                        : in.type == "MouseButton2" ? (in.state == "Begin" ? "Button2Down" : "Button2Up") : nullptr;
        if (mev && (!mouseButton || in.state != "Change")) r.fireValues(*r.mouse, mev, {});
    }
    // Out here because the block below runs only for a Begin or a MouseMovement: without the
    // release the prompt stays in triggeredPrompts and every later trigger is refused.
    if (mouseButton && in.state == "End" && r.clickedPrompt) {
        if (Instance* pr = r.dm.find(r.clickedPrompt); pr && !pr->destroyed()) r.promptRelease(*pr);
        r.clickedPrompt = 0;
    }
    // ClickDetectors on the part under the pointer (or a Model holding it), within
    // MaxActivationDistance of the character: hover and click fire here and go to the server.
    if (!keyboard && !processed && (in.type == "MouseMovement" || (mouseButton && in.state == "Begin"))) {
        int64_t detector = 0;
        if (Instance* cam = r.dm.find(r.dm.workspace()->get("CurrentCamera").ref)) {
            RayV ray = cameraRay(*cam, r.mousePos.x, r.mousePos.y);
            RaycastFilter f;
            Instance* character = r.localPlayer ? r.dm.findRef(r.localPlayer->get("Character").ref) : nullptr;
            if (character) f.ids.push_back(character->id());
            RayHit hit;
            if (raycastTree(r.dm, ray.origin, {ray.direction.x * 1000, ray.direction.y * 1000, ray.direction.z * 1000}, &f, hit)) {
                for (Instance* p = hit.part; p && !detector; p = p->parent()) {
                    if (p->isA("Workspace")) break;
                    if (Instance* cd = p->findFirstChildOfClass("ClickDetector")) {
                        Vec3 from = ray.origin;
                        if (character) if (Instance* root = character->findFirstChild("HumanoidRootPart")) from = root->get("Position").v;
                        Vec3 d{hit.position.x - from.x, hit.position.y - from.y, hit.position.z - from.z};
                        if (std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) <= cd->get("MaxActivationDistance").n) detector = cd->id();
                        break;
                    }
                }
            }
        }
        // ProximityPrompt.ClickablePrompt, on by default as on Roblox: a click triggers too.
        if (mouseButton && in.state == "Begin" && !r.holdingPrompt) {
            if (Instance* cam = r.dm.find(r.dm.workspace()->get("CurrentCamera").ref)) {
                RayV pray = cameraRay(*cam, r.mousePos.x, r.mousePos.y);
                RaycastFilter pf;
                Instance* who = r.localPlayer ? r.dm.findRef(r.localPlayer->get("Character").ref) : nullptr;
                if (who) pf.ids.push_back(who->id());
                RayHit phit;
                if (raycastTree(r.dm, pray.origin, {pray.direction.x * 1000, pray.direction.y * 1000, pray.direction.z * 1000}, &pf, phit)) {
                    // Only a prompt updatePrompts is showing: one out of range is not clickable.
                    for (int64_t id : r.shownPrompts) {
                        Instance* pr = r.dm.find(id);
                        if (!pr || pr->destroyed() || !pr->get("ClickablePrompt").b) continue;
                        Instance* on = r.promptPart(*pr);
                        for (Instance* q = phit.part; q; q = q->parent()) {
                            if (q->isA("Workspace")) break;
                            if (q == on) { r.promptPress(*pr); r.clickedPrompt = pr->id(); break; }
                        }
                        if (r.holdingPrompt || r.triggeredPrompts.count(id)) break;
                    }
                }
            }
        }
        auto tell = [&](int64_t id, const char* what) {
            Instance* cd = r.dm.find(id);
            if (!cd || cd->destroyed()) return;
            r.fireValues(*cd, what, {Value::instance(r.localPlayer ? r.localPlayer->id() : 0)});
            RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = id; msg.args.push_back(NetValue::string(what));
            r.outRemotes.push_back(std::move(msg));
        };
        if (detector != r.hoverDetector) {
            if (r.hoverDetector) tell(r.hoverDetector, "MouseHoverLeave");
            if (detector) tell(detector, "MouseHoverEnter");
            r.hoverDetector = detector;
            // The pointer wears ClickDetector.CursorIcon over one, and its own icon on the way out.
            if (r.mouse) {
                std::string want;
                if (Instance* cd = r.dm.find(detector)) want = cd->get("CursorIcon").s;
                if (!want.empty()) {
                    if (!r.cursorTaken) { r.iconBeforeHover = r.mouse->get("Icon").s; r.cursorTaken = true; }
                    r.mouse->setInternal("Icon", Value::string(want));
                } else if (r.cursorTaken) {
                    r.mouse->setInternal("Icon", Value::string(r.iconBeforeHover));
                    r.cursorTaken = false;
                }
            }
        }
        if (detector && mouseButton && in.type == "MouseButton1") tell(detector, "MouseClick");
        if (detector && mouseButton && in.type == "MouseButton2") tell(detector, "RightMouseClick");
    }
    // 1-9 equip the Backpack's tools, Backspace drops, the mouse activates the one in hand.
    // Moving a Tool is the server's job and comes back replicated; Activated fires here at once.
    if (r.localPlayer && !processed) {
        Instance* character = r.dm.findRef(r.localPlayer->get("Character").ref);
        Instance* backpack = r.localPlayer->findFirstChildOfClass("Backpack");
        Instance* held = character ? character->findFirstChildOfClass("Tool") : nullptr;
        auto send = [&](Instance* tool, const char* what) {
            RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = tool->id(); msg.args.push_back(NetValue::string(what));
            r.outRemotes.push_back(std::move(msg));
        };
        if (keyboard && in.state == "Begin" && backpack && character) {
            static const char* const digits[] = {"One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine"};
            int slot = -1;
            for (int k = 0; k < 9; k++) if (in.key == digits[k]) slot = k;
            if (slot >= 0) r.hotbarSelect(slot);
            else if (in.key == "Backspace" && held && held->get("CanBeDropped").b) send(held, "Drop");
        }
        if (mouseButton && in.type == "MouseButton1" && held && held->get("Enabled").b && !held->get("ManualActivationOnly").b) {
            // Humanoid.TargetPoint: where the click pointed, on this client
            if (in.state == "Begin") if (Instance* hum = character->findFirstChildOfClass("Humanoid")) { Vec3 at; if (mouseHit(r, at)) hum->set("TargetPoint", Value::vector3(at)); }
            if (in.state == "Begin") { r.fireValues(*held, "Activated", {}); send(held, "Activated"); }
            else if (in.state == "End") { r.fireValues(*held, "Deactivated", {}); send(held, "Deactivated"); }
        }
        // ProximityPrompts: the key of a shown one triggers it, or starts its hold.
        if (keyboard && in.state != "Change") r.promptKey(ki->name, in.state == "Begin");
    }
    r.runDeferred();
}

void Runtime::menu(bool open) { impl_->menu(open); impl_->runDeferred(); }
void Runtime::resetCharacter() { impl_->resetCharacter(); impl_->runDeferred(); }

void Runtime::savedQuality(int level) { impl_->savedQuality(level); impl_->runDeferred(); }
void Runtime::Impl::savedQuality(int level) {
    savedQualityLevel = std::clamp(level, 0, 10);
    if (!userSettings) return;                           // made on first ask, and reads this then
    Instance* ugs = userSettings->findFirstChildOfClass("UserGameSettings");
    if (!ugs) return;
    const std::string item = savedQualityLevel == 0 ? "Automatic" : "QualityLevel" + std::to_string(savedQualityLevel);
    if (ugs->get("SavedQualityLevel").s != item) ugs->setInternal("SavedQualityLevel", Value::enumItem(item, savedQualityLevel));
}
void Runtime::Impl::menu(bool open) {
    if (dm.isServer()) return;
    Instance* gs = dm.getService("GuiService");
    if (gs->get("MenuIsOpen").b == open) return;
    gs->setInternal("MenuIsOpen", Value::boolean(open));
    fireValues(*gs, open ? "MenuOpened" : "MenuClosed", {});
}
void Runtime::Impl::resetCharacter() {
    if (dm.isServer() || !localPlayer || !resetEnabled) return;
    if (resetCallback) {
        if (Instance* b = dm.find(resetCallback); b && !b->destroyed()) fireValues(*b, "Event", {});
        return;
    }
    RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = dm.getService("Players")->id();
    msg.args = {NetValue::string("ResetCharacter")};
    outRemotes.push_back(std::move(msg));
}

void Runtime::hotbarSelect(int slot) {
    impl_->hotbarSelect(slot);
    impl_->runDeferred();
}

// The engine's chat box (see rbx_chat.cpp).
void Runtime::chat(const std::string& text) { impl_->chatSend(text); }

void Runtime::queueTerrainOp(const TerrainOp& op) { impl_->terrainOps.push_back(op); }
std::vector<Runtime::Impulse> Runtime::takeImpulses() {
    std::vector<Impulse> out;
    out.swap(impl_->impulses);
    return out;
}
std::vector<Runtime::TerrainOp> Runtime::takeTerrainOps() {
    std::vector<TerrainOp> out;
    out.swap(impl_->terrainOps);
    return out;
}

std::vector<Runtime::Notification> Runtime::takeNotifications() {
    std::vector<Notification> out;
    out.swap(impl_->notifications);
    return out;
}

void Runtime::notificationButton(int64_t callback, const std::string& text) {
    Instance* b = impl_->dm.find(callback);
    if (!b || b->destroyed() || !b->isA("BindableFunction") || !b->binding) return;
    auto& callbacks = static_cast<InstBinding*>(b->binding)->callbacks;
    auto it = callbacks.find("OnInvoke");
    if (it == callbacks.end()) return;
    Task* t = impl_->newTask(nullptr, it->second.ctx);
    lua_getref(t->co, it->second.ref);
    lua_pushlstring(t->co, text.data(), text.size());
    impl_->resume(t, 1);
    impl_->runDeferred();
}

std::vector<Runtime::ChatLine> Runtime::takeChat() {
    std::vector<ChatLine> out;
    out.swap(impl_->chatLines);
    return out;
}

void Runtime::guiInput(int64_t id, const std::string& phase, int button, Vec2 pos) {
    Impl& r = *impl_;
    Instance* g = r.dm.find(id);
    if (!g || g->destroyed() || !g->isA("GuiObject")) return;
    if (r.dm.isServer()) {   // a plugin's GUI under CoreGui is live in the edit world; nothing else is a server's to press
        bool core = false;
        for (Instance* p = g->parent(); p; p = p->parent()) if (p->className() == "CoreGui") { core = true; break; }
        if (!core) return;
    }
    // Interactable false stops input for the object and its descendants, as on Roblox, so an
    // ancestor switches a whole panel off. MouseEnter and MouseLeave stop with it.
    for (Instance* a = g; a; a = a->parent()) {
        if (!a->isA("GuiObject")) break;
        Value v = a->get("Interactable");
        if (v.type == Value::Bool && !v.b) { g->setSilent("GuiState", Value::enumItem("NonInteractable", 3)); return; }
    }
    // GuiState follows the pointer, silently: the mirror is not told and Changed does not fire.
    auto state = [&](const char* name, int value) { g->setSilent("GuiState", Value::enumItem(name, value)); };
    std::string b = button == 2 ? "MouseButton2" : "MouseButton1";
    std::vector<Value> xy = {Value::number(pos.x), Value::number(pos.y)};
    if (phase == "Enter") { state("Hover", 1); r.fireValues(*g, "MouseEnter", xy); r.runDeferred(); return; }
    if (phase == "Leave") { state("Idle", 0); r.fireValues(*g, "MouseLeave", xy); r.runDeferred(); return; }
    if (phase == "WheelForward" || phase == "WheelBackward") { r.fireValues(*g, "MouseWheel" + phase.substr(5), xy); r.runDeferred(); return; }
    // the InputObject for that button, as UserInputService hands out
    const EnumDef* types = findEnum("UserInputType"); const EnumItem* ti = types->find(b);
    const EnumDef* states = findEnum("UserInputState"); const EnumItem* si = states->find(phase == "Up" ? "End" : "Begin");
    const EnumDef* keys = findEnum("KeyCode"); const EnumItem* ki = keys->find("Unknown");
    Instance::Ptr& obj = r.inputObjects[b + "\n" + ki->name];
    if (!obj) { obj = r.dm.createInternal("InputObject"); obj->setName("InputObject"); }
    obj->setInternal("UserInputType", Value::enumItem(ti->name, ti->value));
    obj->setInternal("KeyCode", Value::enumItem(ki->name, ki->value));
    obj->setInternal("UserInputState", Value::enumItem(si->name, si->value));
    obj->setInternal("Position", Value::vector3(pos.x, pos.y, 0));
    Value io = Value::instance(obj->id());
    if (phase == "Down") {
        state("Press", 2);
        if (g->isA("GuiButton")) r.fireValues(*g, b + "Down", xy);
        r.fireValues(*g, "InputBegan", {io});
    } else if (phase == "Up") {
        state("Hover", 1);
        if (g->isA("GuiButton")) r.fireValues(*g, b + "Up", xy);
        r.fireValues(*g, "InputEnded", {io});
    } else if (phase == "Click" && g->isA("GuiButton")) {
        r.fireValues(*g, b + "Click", {});
        if (button == 1) r.fireValues(*g, "Activated", {io, Value::number(1)});
        else if (button == 2) r.fireValues(*g, "SecondaryActivated", {io, Value::number(1)});
    }
    r.runDeferred();
}

void Runtime::guiText(int64_t id, const std::string& phase, const std::string& text, int caret, bool enter) {
    Impl& r = *impl_;
    if (r.dm.isServer()) return;
    Instance* g = r.dm.find(id);
    if (!g || g->destroyed() || !g->isA("TextBox")) return;
    if (phase == "Focus") r.focusBox(*g);
    else if (phase == "Blur") r.blurBox(*g, enter);
    else if (phase == "Change") {
        r.dm.setHostWriting(true);
        g->setInternal("CursorPosition", Value::number(caret));   // before Text, so its Changed sees the caret
        g->setInternal("Text", Value::string(text));
        r.dm.setHostWriting(false);
    }
    r.runDeferred();
}

// One TextBox at a time holds the keyboard. Focus clears the text if ClearTextOnFocus and puts
// the caret last; FocusLost carries whether Enter did it; CursorPosition is -1 unfocused.
Instance* Runtime::Impl::focusedTextBox() {
    Instance* b = focusedBox ? dm.find(focusedBox) : nullptr;
    if (!b || b->destroyed()) { focusedBox = 0; return nullptr; }
    return b;
}
void Runtime::Impl::focusBox(Instance& box) {
    if (focusedBox == box.id()) return;
    if (Instance* prev = focusedTextBox()) blurBox(*prev, false);
    focusedBox = box.id();
    if (box.get("ClearTextOnFocus").b) box.setInternal("Text", Value::string(""));
    box.setInternal("CursorPosition", Value::number((double)box.get("Text").s.size() + 1));
    fireValues(box, "Focused", {});
    fireValues(*dm.getService("UserInputService"), "TextBoxFocused", {Value::instance(box.id())});
}
void Runtime::Impl::blurBox(Instance& box, bool enter) {
    if (focusedBox != box.id()) return;
    focusedBox = 0;
    box.setInternal("CursorPosition", Value::number(-1));
    fireValues(box, "FocusLost", {Value::boolean(enter), Value::nil()});
    fireValues(*dm.getService("UserInputService"), "TextBoxFocusReleased", {Value::instance(box.id())});
}

// A task refused by the frame budget kept its args; the retry uses those.
static int retryArgs(Task* t, int fresh) {
    if (t->pendingArgs < 0) return fresh;
    int n = t->pendingArgs; t->pendingArgs = -1;
    return n;
}

void Runtime::Impl::runReady() {
    while (!ready.empty()) {
        Task* t = ready.front(); ready.pop_front();
        size_t before = ready.size();
        resume(t, retryArgs(t, 0));
        if (ready.size() > before && ready.front() == t) break;   // skipped: frame budget gone
    }
}

void Runtime::Impl::runDeferred() {
    // Deferred tasks queued while running deferred tasks run in the same pass.
    int guard = 0;
    while (!deferred.empty() && guard++ < 10000) {
        Task* t = deferred.front(); deferred.pop_front();
        if (t->state != Task::Deferred) continue;
        int nargs = lua_gettop(t->co) - 1;   // function + args were pushed at defer time
        if (nargs < 0) nargs = 0;
        resume(t, retryArgs(t, nargs));
    }
}

void Runtime::Impl::wakeSleepers() {
    std::vector<Task*> due;
    for (size_t i = 0; i < sleepers.size();) {
        if (sleepers[i]->wakeAt <= now) { due.push_back(sleepers[i]); sleepers[i] = sleepers.back(); sleepers.pop_back(); }
        else i++;
    }
    std::sort(due.begin(), due.end(), [](Task* a, Task* b) { return a->wakeAt < b->wakeAt; });
    for (Task* t : due) {
        if (t->state != Task::Sleeping) continue;
        if (t->pendingArgs >= 0) { resume(t, retryArgs(t, 0)); continue; }
        double elapsed = now - t->sleepStart;
        int nargs = lua_gettop(t->co);   // task.delay: function + args already on the stack
        if (nargs > 0) {
            // delay(fn, ...): fresh coroutine with fn + args -> resume runs fn(...)
            resume(t, nargs - 1);
        } else {
            lua_pushnumber(t->co, elapsed);
            resume(t, 1);
        }
    }
}

void Runtime::Impl::checkChildWaiters() {
    for (size_t i = 0; i < childWaiters.size();) {
        Task* t = childWaiters[i];
        if (t->pendingArgs >= 0) {   // found its child last step; the frame budget refused the resume
            childWaiters[i] = childWaiters.back(); childWaiters.pop_back();
            resume(t, retryArgs(t, 0));
            continue;
        }
        Instance* c = t->waitParent ? t->waitParent->findFirstChild(t->waitName) : nullptr;
        bool timeout = t->waitDeadline > 0 && now >= t->waitDeadline;
        if (!c && !timeout) {
            if (!t->waitWarned && t->waitDeadline <= 0 && now - t->sleepStart > 5 && cb.warn) {
                t->waitWarned = true;
                cb.warn(t->ctx ? t->ctx->name : "", "Infinite yield possible on '" + t->waitParent->fullName() + ":WaitForChild(\"" + t->waitName + "\")'");
            }
            i++; continue;
        }
        childWaiters[i] = childWaiters.back(); childWaiters.pop_back();
        t->waitParent.reset();
        pushInstance(t->co, c);
        resume(t, 1);
    }
}

// ---- signals -----------------------------------------------------------------------
InstBinding& Runtime::Impl::binding(Instance& i) {
    if (!i.binding) {
        auto* b = new InstBinding();
        i.binding = b;
        bound.insert(&i);
        Impl* me = this;
        Instance* owner = &i;
        i.bindingFree = [me, owner](void* p) {
            auto* b = static_cast<InstBinding*>(p);
            // Can run inside a GC sweep, where the Lua state must not be touched: refs go out next step.
            if (!me->tearingDown) {
                me->bound.erase(owner);
                for (auto& [n, s] : b->signals) for (auto& c : s->conns) if (c.connected) me->pendingUnref.push_back(c.fnRef);
                for (auto& [n, r] : b->callbacks) me->pendingUnref.push_back(r.ref);
            }
            delete b;
        };
    }
    return *static_cast<InstBinding*>(i.binding);
}

Signal* Runtime::Impl::findSignal(Instance& i, const std::string& name) {
    if (!i.binding) return nullptr;
    auto& sigs = static_cast<InstBinding*>(i.binding)->signals;
    auto it = sigs.find(name);
    return it == sigs.end() ? nullptr : it->second.get();
}

Signal& Runtime::Impl::signal(Instance& i, const std::string& name) {
    InstBinding& b = binding(i);
    auto& slot = b.signals[name];
    if (!slot) { slot = std::make_unique<Signal>(); slot->owner = &i; slot->name = name; }
    return *slot;
}

void Runtime::Impl::fire(Signal& s, const std::function<int(lua_State*)>& pushArgs) {
    // Snapshot: a handler may connect or disconnect during the iteration.
    std::vector<Conn> conns;
    for (auto& c : s.conns) if (c.connected) conns.push_back(c);
    for (auto& c : conns) {
        if (c.once) for (auto& live : s.conns) if (live.id == c.id) live.connected = false;
        Task* t = newTask(nullptr, c.ctx);
        lua_getref(t->co, c.fnRef);
        int n = pushArgs(t->co);
        resume(t, n);
        if (c.once) lua_unref(L, c.fnRef);
    }
    std::vector<Task*> waiters; waiters.swap(s.waiters);
    for (Task* t : waiters) {
        if (t->state != Task::WaitSignal) continue;
        int n = pushArgs(t->co);
        resume(t, n);
    }
    s.conns.erase(std::remove_if(s.conns.begin(), s.conns.end(), [](const Conn& c) { return !c.connected; }), s.conns.end());
}

void Runtime::Impl::fireValues(Instance& i, const std::string& event, const std::vector<Value>& args) {
    Signal* s = findSignal(i, event);
    if (!s || !s->hasConns()) return;
    fire(*s, [this, &args](lua_State* co) { for (auto& v : args) pushValue(co, v); return (int)args.size(); });
}

void Runtime::Impl::disconnectAll(Instance& i) {
    if (!i.binding) return;
    auto* b = static_cast<InstBinding*>(i.binding);
    for (auto& [n, s] : b->signals) {
        for (auto& c : s->conns) if (c.connected) { c.connected = false; lua_unref(L, c.fnRef); }
        s->conns.clear();
        s->waiters.clear();   // they stay parked; Roblox never resumes them either
    }
    for (auto& [n, r] : b->callbacks) lua_unref(L, r.ref);
    b->callbacks.clear();
}

// ---- scripts -----------------------------------------------------------------------
// A plugin is what the host put directly under the DataModel's one PluginDebugService
// (addPlugin / addPluginFile record it in pluginRoots) and whatever sits under that. Tree
// position alone is not enough: a place file, a replicated tree or a script's own Parent write
// could put a Script there, and none of them may run as a plugin.
const Instance* Runtime::Impl::pluginRootOf(const Instance& s) const {
    if (pluginRoots.empty()) return nullptr;
    const Instance* real = dm.root() ? dm.root()->findFirstChildOfClass("PluginDebugService") : nullptr;
    if (!real) return nullptr;
    for (const Instance* p = &s; p; p = p->parent())
        if (pluginRoots.count(p->id())) return p->parent() == real ? p : nullptr;
    return nullptr;
}
bool Runtime::Impl::isPluginScript(const Instance& s) const { return pluginRootOf(s) != nullptr; }

// Being under the service is what makes a plugin, so only a plugin (or the host) may put
// anything there: a place script that parented itself in would gain the capability.
bool Runtime::Impl::inPluginTree(const Instance* p) const {
    const Instance* real = dm.root() ? dm.root()->findFirstChildOfClass("PluginDebugService") : nullptr;
    for (; p && real; p = p->parent()) if (p == real) return true;
    return false;
}

// The `plugin` global: one Plugin per thing directly under PluginDebugService, named for it.
Instance* Runtime::Impl::pluginFor(Instance& s) {
    const Instance* rootChild = pluginRootOf(s);
    if (!rootChild) return nullptr;
    auto it = pluginOfRoot.find(rootChild->id());
    if (it != pluginOfRoot.end()) return it->second;
    Instance::Ptr pl = dm.createInternal("Plugin");
    pl->setName(rootChild->name());
    pluginObjects.push_back(pl);
    pluginOfRoot[rootChild->id()] = pl.get();
    return pl.get();
}

// DockWidgetPluginGuiInfo.new(dockState, enabled, override, w, h, minW, minH), as a table.
static int dockWidgetInfoNew(lua_State* L) {
    lua_newtable(L);
    lua_pushvalue(L, 1); lua_setfield(L, -2, "InitialDockState");
    lua_pushboolean(L, lua_isnoneornil(L, 2) ? 1 : lua_toboolean(L, 2)); lua_setfield(L, -2, "InitialEnabled");
    lua_pushboolean(L, lua_toboolean(L, 3)); lua_setfield(L, -2, "InitialEnabledShouldOverrideRestore");
    lua_pushnumber(L, luaL_optnumber(L, 4, 0)); lua_setfield(L, -2, "FloatingXSize");
    lua_pushnumber(L, luaL_optnumber(L, 5, 0)); lua_setfield(L, -2, "FloatingYSize");
    lua_pushnumber(L, luaL_optnumber(L, 6, 0)); lua_setfield(L, -2, "MinWidth");
    lua_pushnumber(L, luaL_optnumber(L, 7, 0)); lua_setfield(L, -2, "MinHeight");
    return 1;
}

bool Runtime::Impl::shouldRun(Instance& s) const {
    if (!s.isA("BaseScript") || s.destroyed() || !s.parent()) return false;
    if (!s.get("Enabled").b || s.get("Disabled").b) return false;
    if (isPluginScript(s)) return s.className() == "Script";   // a local plugin: runs in the edit world too, whatever its RunContext
    if (!opts.runScripts) return false;   // an edit world: the tree is there, nothing runs
    if (s.className() == "Script") {
        int rc = (int)s.get("RunContext").n;
        if (rc == 1) return dm.isServer();
        if (rc == 2) return !dm.isServer();
        return dm.isServer() && serverScriptRunsUnder(*s.parent());
    }
    if (s.className() == "LocalScript") return !dm.isServer() && clientScriptRunsUnder(*s.parent(), localPlayer);
    return false;
}

void Runtime::Impl::queueStart(Instance& i) {
    if (started.count(i.id())) return;
    if (std::find(pendingStart.begin(), pendingStart.end(), i.id()) == pendingStart.end()) pendingStart.push_back(i.id());
}

void Runtime::Impl::startScript(Instance& s) {
    started.insert(s.id());
    stats.scriptsStarted++;
    std::string name = s.fullName();
    std::string source = s.get("Source").s;

    lua_CompileOptions copts = userCompileOptions();
    size_t bcLen = 0;
    char* bc = luau_compile(source.c_str(), source.size(), &copts, &bcLen);

    auto ctx = std::make_shared<ScriptCtx>();
    ctx->scriptId = s.id(); ctx->name = name;
    Task* t = newTask(nullptr, ctx);
    luaL_sandboxthread(t->co);
    pushInstance(t->co, &s);
    lua_setglobal(t->co, "script");
    if (Instance* pl = pluginFor(s)) {   // a local plugin: its Plugin, and the Studio-only datatypes
        pushInstance(t->co, pl);
        lua_setglobal(t->co, "plugin");
        lua_newtable(t->co);
        lua_pushcfunction(t->co, dockWidgetInfoNew, "new");
        lua_setfield(t->co, -2, "new");
        lua_setglobal(t->co, "DockWidgetPluginGuiInfo");
    }
    std::string chunk = "=" + name;
    int status = luau_load(t->co, chunk.c_str(), bc, bcLen, 0);
    std::free(bc);
    if (status == 0) rememberMain(s.id(), name, t->co);   // for breakpoints, now and when they are set later
    if (status != 0) {
        std::string err = lua_tostring(t->co, -1);
        stats.errors++;
        if (cb.error) cb.error(name, err);
        kill(t);
        return;
    }
    resume(t, 0);
}

// ---- paths -------------------------------------------------------------------------
// Twice a second: a path whose waypoint is now occupied fires Blocked(index) once, Unblocked
// when it clears. Paths nothing holds any more are dropped.
void Runtime::Impl::stepPaths(double dt) {
    for (size_t i = 0; i < paths.size();)
        if (paths[i]->inst.use_count() == 1) { paths[i] = std::move(paths.back()); paths.pop_back(); } else i++;
    pathCheckIn -= dt;
    if (pathCheckIn > 0 || paths.empty()) return;
    pathCheckIn = 0.5;
    for (auto& pp : paths) {
        PathState& ps = *pp;
        if (ps.points.size() < 2) continue;
        int blocked = 0;
        for (size_t k = 1; k < ps.points.size() && !blocked; k++)
            if (pathOccupied(dm, ps.agent, ps.points[k].pos)) blocked = (int)k + 1;
        if (blocked == ps.blockedAt) continue;
        if (blocked) fireValues(*ps.inst, "Blocked", {Value::number(blocked)});
        else fireValues(*ps.inst, "Unblocked", {Value::number(ps.blockedAt)});
        ps.blockedAt = blocked;
    }
}

// ---- tweens ------------------------------------------------------------------------
static Value lerpValue(const Value& a, const Value& b, float t) {
    switch (a.type) {
    case Value::Number: return Value::number(a.n + (b.n - a.n) * t);
    case Value::Vector3: return Value::vector3(a.v.x + (b.v.x - a.v.x) * t, a.v.y + (b.v.y - a.v.y) * t, a.v.z + (b.v.z - a.v.z) * t);
    case Value::Vector2: return Value::vector2(a.v.x + (b.v.x - a.v.x) * t, a.v.y + (b.v.y - a.v.y) * t);
    case Value::UDim: return Value::udim(a.u[0] + (b.u[0] - a.u[0]) * t, a.u[1] + (b.u[1] - a.u[1]) * t);
    case Value::UDim2: return Value::udim2(a.u[0] + (b.u[0] - a.u[0]) * t, a.u[1] + (b.u[1] - a.u[1]) * t,
                                           a.u[2] + (b.u[2] - a.u[2]) * t, a.u[3] + (b.u[3] - a.u[3]) * t);
    case Value::Color3: return Value::color3(a.c.r + (b.c.r - a.c.r) * t, a.c.g + (b.c.g - a.c.g) * t, a.c.b + (b.c.b - a.c.b) * t);
    default: return t >= 1 ? b : a;
    }
}

void Runtime::Impl::stepTweens(double dt) {
    for (size_t i = 0; i < tweens.size();) {
        Tween& tw = *tweens[i];
        if (!tw.playing || tw.done || tw.obj->destroyed()) { if (tw.done || tw.obj->destroyed()) { tweens[i] = std::move(tweens.back()); tweens.pop_back(); } else i++; continue; }
        tw.elapsed += dt;
        double t = tw.elapsed - tw.info.delay;
        if (t < 0) { i++; continue; }
        float alpha = tw.info.time > 0 ? (float)(t / tw.info.time) : 1.f;
        bool cycleDone = alpha >= 1;
        if (cycleDone) alpha = 1;
        float e = ease(tw.info.style, tw.info.direction, tw.reversed ? 1 - alpha : alpha);
        for (auto& p : tw.props) {
            if (p.isCFrame) {
                Vec3 pos, o; cframeToPosOrient(p.cfFrom.lerp(p.cfTo, e), pos, o);
                tw.obj->set(p.cfPos, Value::vector3(pos)); tw.obj->set(p.cfOri, Value::vector3(o));
            } else tw.obj->set(p.name, lerpValue(p.from, p.to, e));
        }
        if (cycleDone) {
            bool finished = false;
            if (tw.info.reverses && !tw.reversed) tw.reversed = true;
            else {
                tw.reversed = false;
                if (tw.info.repeatCount < 0 || tw.cycle < tw.info.repeatCount) tw.cycle++;
                else finished = true;
            }
            tw.elapsed = tw.info.delay;
            if (finished) {
                tw.done = true; tw.playing = false;
                tw.tween->setInternal("PlaybackState", Value::enumItem("Completed", 4));
                Instance::Ptr keep = tw.tween;
                fireValues(*keep, "Completed", {Value::enumItem("PlaybackState.Completed", 4)});
            }
        }
        i++;
    }
}

// ---- players -----------------------------------------------------------------------
Instance* Runtime::Impl::teamByColor(const Value& color) {
    Instance* teams = dm.getService("Teams");
    for (auto& t : teams->children()) if (t->isA("Team") && t->get("TeamColor").c == color.c) return t.get();
    return nullptr;
}

// SpawnLocation.AllowTeamChangeOnTouch: the character that touched it joins the team of its colour.
void Runtime::Impl::teamTouch(Instance& spawn, Instance* hit) {
    if (!dm.isServer() || !hit || !spawn.get("AllowTeamChangeOnTouch").b || !spawn.get("Enabled").b) return;
    Instance* model = hit->parent();
    if (!model || !model->findFirstChildOfClass("Humanoid")) return;
    for (auto& p : dm.getService("Players")->children())
        if (p->isA("Player") && p->get("Character").ref == model->id()) {
            if (p->get("TeamColor").c == spawn.get("TeamColor").c && !p->get("Neutral").b) return;
            p->set("TeamColor", spawn.get("TeamColor"));
            p->set("Neutral", Value::boolean(false));
        }
}

Instance* Runtime::Impl::spawnCharacter(Instance& player) {
    Instance* ws = dm.workspace();
    // StarterPlayer.StarterCharacter (a Model with a Humanoid and a HumanoidRootPart) is cloned
    // for each spawn in place of the default rig, as on Roblox.
    Instance* custom = nullptr;
    if (Instance* spl = dm.getService("StarterPlayer"))
        if (Instance* sc = spl->findFirstChild("StarterCharacter"); sc && sc->isA("Model") && sc->findFirstChildOfClass("Humanoid"))
            if (Instance* r = sc->findFirstChild("HumanoidRootPart"); r && r->isA("BasePart"))
                custom = sc;
    Instance::Ptr ch = custom ? custom->clone() : nullptr;
    if (!ch) { ch = dm.create("Model"); custom = nullptr; }
    ch->setName(player.name());
    // StarterPlayer.CharacterRigType picks the rig, as in Studio. The engine paints its gradient
    // skin over limbs whose Color is still exactly kDefaultSkin; set one and that limb turns plain.
    Instance* starterPlayer = dm.getService("StarterPlayer");
    bool r15 = starterPlayer && starterPlayer->get("CharacterRigType").s == "R15";
    // A StarterCharacter is used as it was built, so the model says which rig it is and
    // CharacterRigType does not get to contradict it -- the limb set chosen below is the one
    // ADDED to the clone, and picking the wrong one bolts a Torso onto an UpperTorso. Its own
    // parts are the surest sign; failing those, its Humanoid's RigType.
    if (custom) {
        if (ch->findFirstChild("UpperTorso")) r15 = true;
        else if (ch->findFirstChild("Torso")) r15 = false;
        else if (Instance* h = ch->findFirstChildOfClass("Humanoid")) {
            const std::string& rig = h->get("RigType").s;
            if (rig == "R15" || rig == "R6") r15 = rig == "R15";
        }
    }
    struct Limb { const char* name; Vec3 size, offset; };
    // Roblox's R6 sizes and offsets, in studs, around the HumanoidRootPart.
    static const Limb kR6[] = {
        {"Head", {2, 1, 1}, {0, 1.5f, 0}}, {"Torso", {2, 2, 1}, {0, 0, 0}},
        {"Left Arm", {1, 2, 1}, {-1.5f, 0, 0}}, {"Right Arm", {1, 2, 1}, {1.5f, 0, 0}},
        {"Left Leg", {1, 2, 1}, {-0.5f, -2, 0}}, {"Right Leg", {1, 2, 1}, {0.5f, -2, 0}},
    };
    static const Limb kR15[] = {
        {"Head", {2, 1, 1}, {0, 2.3f, 0}},
        {"UpperTorso", {2, 1.6f, 1}, {0, 1, 0}},        {"LowerTorso", {2, 0.4f, 1}, {0, 0, 0}},
        {"LeftUpperArm", {1, 1.2f, 1}, {-1.5f, 1, 0}},  {"LeftLowerArm", {1, 1.2f, 1}, {-1.5f, -0.2f, 0}},
        {"LeftHand", {1, 0.4f, 1}, {-1.5f, -1, 0}},
        {"RightUpperArm", {1, 1.2f, 1}, {1.5f, 1, 0}},  {"RightLowerArm", {1, 1.2f, 1}, {1.5f, -0.2f, 0}},
        {"RightHand", {1, 0.4f, 1}, {1.5f, -1, 0}},
        {"LeftUpperLeg", {1, 1.4f, 1}, {-0.5f, -0.9f, 0}}, {"LeftLowerLeg", {1, 1.4f, 1}, {-0.5f, -2.3f, 0}},
        {"LeftFoot", {1, 0.2f, 1}, {-0.5f, -3.1f, 0}},
        {"RightUpperLeg", {1, 1.4f, 1}, {0.5f, -0.9f, 0}}, {"RightLowerLeg", {1, 1.4f, 1}, {0.5f, -2.3f, 0}},
        {"RightFoot", {1, 0.2f, 1}, {0.5f, -3.1f, 0}},
    };
    const Limb* limbs = r15 ? kR15 : kR6;
    size_t limbCount = r15 ? sizeof kR15 / sizeof *kR15 : sizeof kR6 / sizeof *kR6;
    Instance::Ptr rootMade;
    Instance* root = custom ? ch->findFirstChild("HumanoidRootPart") : nullptr;
    if (!root) {
        rootMade = dm.create("Part", ch.get());
        root = rootMade.get();
        root->setName("HumanoidRootPart");
        root->set("Size", Value::vector3(2, 2, 1));
        root->set("Transparency", Value::number(1));
        root->set("CanCollide", Value::boolean(false));
    }
    // The player's RespawnLocation, else the enabled SpawnLocations open to it (Neutral, or its
    // team's colour) in turn.
    Vec3 at{0, 5, 0};
    std::vector<Instance*> spawns;
    if (Instance* rl = dm.findRef(player.get("RespawnLocation").ref); rl && rl->isA("SpawnLocation") && rl->get("Enabled").b) spawns.push_back(rl);
    else {
        bool neutral = player.get("Neutral").b;
        Col3 mine = player.get("TeamColor").c;
        for (Instance* d : ws->getDescendants())
            if (d->isA("SpawnLocation") && d->get("Enabled").b && (d->get("Neutral").b || (!neutral && d->get("TeamColor").c == mine))) spawns.push_back(d);
    }
    float standing = r15 ? 3.7f : 3.5f;   // R15's feet hang a touch lower than R6's
    Instance* spawnedAt = nullptr;
    if (!spawns.empty()) {
        spawnedAt = spawns[spawnTurn++ % spawns.size()];
        Vec3 p = spawnedAt->get("Position").v;
        at = {p.x, p.y + standing, p.z};
    }
    // Not inside another character: the spot itself, then rings around it.
    std::vector<Vec3> taken;
    for (auto& c : ws->children())
        if (c->isA("Model") && c.get() != ch.get())
            if (Instance* r = c->findFirstChild("HumanoidRootPart")) if (c->findFirstChildOfClass("Humanoid")) taken.push_back(r->get("Position").v);
    auto free = [&](Vec3 p) { for (Vec3 t : taken) if (std::fabs(t.x - p.x) < 2.5f && std::fabs(t.z - p.z) < 2.5f && std::fabs(t.y - p.y) < 4) return false; return true; };
    if (!free(at)) {
        bool placed = false;
        for (float radius : {3.f, 6.f, 9.f}) {
            for (int i = 0; i < 8 && !placed; i++) {
                float a = (float)i * 0.785398f;
                Vec3 cand{at.x + std::cos(a) * radius, at.y, at.z + std::sin(a) * radius};
                if (free(cand)) { at = cand; placed = true; }
            }
            if (placed) break;
        }
    }
    if (custom) {
        // The whole copy moves with its root, keeping the shape it was built in.
        Vec3 was = root->get("Position").v;
        Vec3 by{at.x - was.x, at.y - was.y, at.z - was.z};
        for (Instance* d : ch->getDescendants())
            if (d->isA("BasePart")) { Vec3 p = d->get("Position").v; d->set("Position", Value::vector3(p.x + by.x, p.y + by.y, p.z + by.z)); }
    } else {
        root->set("Position", Value::vector3(at));
    }
    // SpawnLocation.Duration: seconds of ForceField after arriving. Server only -- in Play Solo
    // both runtimes spawn a character, and the client's field would be a second one with no clock.
    if (dm.isServer() && spawnedAt && spawnedAt->get("Duration").n > 0) {
        Instance::Ptr ff = dm.createInternal("ForceField");
        ff->setName("ForceField");
        ff->setParent(ch.get());
        forceFields.emplace_back(now + spawnedAt->get("Duration").n, ff->id());
    }
    // The attachment points a rig wears, where accessories hang off it.
    struct Point { const char* limb; const char* name; Vec3 at; };
    // The hat and hair points sit on top of the hexagon head (kHeadScale), not its box.
    static const Point kR6Points[] = {
        {"Head", "HatAttachment", {0, 1.24f, 0}},          {"Head", "HairAttachment", {0, 1.24f, 0}},
        {"Head", "FaceCenterAttachment", {0, 0, 0}},       {"Head", "FaceFrontAttachment", {0, 0, -0.5f}},
        {"Head", "NeckAttachment", {0, -0.5f, 0}},
        {"Torso", "NeckAttachment", {0, 1, 0}},            {"Torso", "BodyFrontAttachment", {0, 0, -0.5f}},
        {"Torso", "BodyBackAttachment", {0, 0, 0.5f}},     {"Torso", "LeftCollarAttachment", {-1, 1, 0}},
        {"Torso", "RightCollarAttachment", {1, 1, 0}},     {"Torso", "WaistCenterAttachment", {0, -1, 0}},
        {"Torso", "WaistFrontAttachment", {0, -1, -0.5f}}, {"Torso", "WaistBackAttachment", {0, -1, 0.5f}},
        {"Left Arm", "LeftShoulderAttachment", {0, 1, 0}},   {"Left Arm", "LeftGripAttachment", {0, -1, 0}},
        {"Right Arm", "RightShoulderAttachment", {0, 1, 0}}, {"Right Arm", "RightGripAttachment", {0, -1, 0}},
        {"Left Leg", "LeftFootAttachment", {0, -1, 0}},    {"Right Leg", "RightFootAttachment", {0, -1, 0}},
    };
    static const Point kR15Points[] = {
        {"Head", "HatAttachment", {0, 1.24f, 0}},             {"Head", "HairAttachment", {0, 1.24f, 0}},
        {"Head", "FaceCenterAttachment", {0, 0, 0}},          {"Head", "FaceFrontAttachment", {0, 0, -0.5f}},
        {"Head", "NeckRigAttachment", {0, -0.5f, 0}},
        {"UpperTorso", "NeckRigAttachment", {0, 0.8f, 0}},    {"UpperTorso", "BodyFrontAttachment", {0, 0, -0.5f}},
        {"UpperTorso", "BodyBackAttachment", {0, 0, 0.5f}},   {"UpperTorso", "LeftCollarAttachment", {-1, 0.8f, 0}},
        {"UpperTorso", "RightCollarAttachment", {1, 0.8f, 0}}, {"UpperTorso", "WaistRigAttachment", {0, -0.8f, 0}},
        {"LowerTorso", "WaistRigAttachment", {0, 0.2f, 0}},   {"LowerTorso", "WaistCenterAttachment", {0, 0.2f, 0}},
        {"LowerTorso", "WaistFrontAttachment", {0, 0.2f, -0.5f}}, {"LowerTorso", "WaistBackAttachment", {0, 0.2f, 0.5f}},
        {"LeftUpperArm", "LeftShoulderAttachment", {0, 0.6f, 0}}, {"LeftHand", "LeftGripAttachment", {0, -0.2f, 0}},
        {"RightUpperArm", "RightShoulderAttachment", {0, 0.6f, 0}}, {"RightHand", "RightGripAttachment", {0, -0.2f, 0}},
        {"LeftFoot", "LeftFootAttachment", {0, -0.1f, 0}},    {"RightFoot", "RightFootAttachment", {0, -0.1f, 0}},
    };
    const Point* points = r15 ? kR15Points : kR6Points;
    size_t pointCount = r15 ? sizeof kR15Points / sizeof *kR15Points : sizeof kR6Points / sizeof *kR6Points;
    // A StarterCharacter brought its own limbs, attachments and joints.
    if (!custom) {
    std::unordered_map<std::string, Instance*> built;   // the parts by name, for the joints
    built["HumanoidRootPart"] = root;
    for (size_t li = 0; li < limbCount; li++) {
        const Limb& l = limbs[li];
        Instance::Ptr part = dm.create("Part", ch.get());
        built[l.name] = part.get();
        part->setName(l.name);
        part->set("Size", Value::vector3(l.size));
        part->set("Color", Value::color3(kDefaultSkin.r, kDefaultSkin.g, kDefaultSkin.b));
        part->set("Position", Value::vector3(at.x + l.offset.x, at.y + l.offset.y, at.z + l.offset.z));
        if (!std::strcmp(l.name, "Head")) {
            // Head.Mesh with MeshType Head, as on Roblox. The shape is this project's own, not
            // Roblox's rounded head: hexagon_head_mesh() (pulseblockz_world.cpp) draws the
            // 2 x 1 x 1 box as a flat-topped hexagonal prism 1.73 tall, lifted onto the torso.
            Instance::Ptr mesh = dm.create("SpecialMesh", part.get());
            mesh->setName("Mesh");
            mesh->set("MeshType", Value::enumItem("Head", 0));
            mesh->set("Scale", Value::vector3(1.0f, 2.0f, 1.0f));
            mesh->set("Offset", Value::vector3(0, 0.37f, 0));
        }
        for (size_t pi = 0; pi < pointCount; pi++) {
            const Point& pt = points[pi];
            if (!std::strcmp(pt.limb, l.name)) {
                Instance::Ptr a = dm.create("Attachment", part.get());
                a->setName(pt.name);
                a->set("Position", Value::vector3(pt.at));
            }
        }
    }
    // Where Roblox puts them, because scripts reach for character.Torso["Right Shoulder"]: an
    // R6's Motor6Ds in the Torso (RootJoint in the root), with the C0 / C1 R6 animations were
    // authored against; an R15's in its own Part1. Part1 = Part0 * C0 * Transform * C1^-1, and
    // a rig with no Pose and an identity Transform is walked by the engine's own animation.
    struct Motor { const char* name; const char* in; const char* p0; const char* p1; Vec3 c0, c0r, c1, c1r; };
    static const Motor kR6Motors[] = {
        {"RootJoint", "HumanoidRootPart", "HumanoidRootPart", "Torso", {0, 0, 0}, {-90, 0, 180}, {0, 0, 0}, {-90, 0, 180}},
        {"Neck", "Torso", "Torso", "Head", {0, 1, 0}, {-90, 0, 180}, {0, -0.5f, 0}, {-90, 0, 180}},
        {"Left Shoulder", "Torso", "Torso", "Left Arm", {-1, 0.5f, 0}, {0, -90, 0}, {0.5f, 0.5f, 0}, {0, -90, 0}},
        {"Right Shoulder", "Torso", "Torso", "Right Arm", {1, 0.5f, 0}, {0, 90, 0}, {-0.5f, 0.5f, 0}, {0, 90, 0}},
        {"Left Hip", "Torso", "Torso", "Left Leg", {-1, -1, 0}, {0, -90, 0}, {-0.5f, 1, 0}, {0, -90, 0}},
        {"Right Hip", "Torso", "Torso", "Right Leg", {1, -1, 0}, {0, 90, 0}, {0.5f, 1, 0}, {0, 90, 0}},
    };
    static const Motor kR15Motors[] = {
        {"Root", "LowerTorso", "HumanoidRootPart", "LowerTorso", {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {"Waist", "UpperTorso", "LowerTorso", "UpperTorso", {0, 0.2f, 0}, {0, 0, 0}, {0, -0.8f, 0}, {0, 0, 0}},
        {"Neck", "Head", "UpperTorso", "Head", {0, 0.8f, 0}, {0, 0, 0}, {0, -0.5f, 0}, {0, 0, 0}},
        {"LeftShoulder", "LeftUpperArm", "UpperTorso", "LeftUpperArm", {-1.5f, 0.6f, 0}, {0, 0, 0}, {0, 0.6f, 0}, {0, 0, 0}},
        {"LeftElbow", "LeftLowerArm", "LeftUpperArm", "LeftLowerArm", {0, -0.6f, 0}, {0, 0, 0}, {0, 0.6f, 0}, {0, 0, 0}},
        {"LeftWrist", "LeftHand", "LeftLowerArm", "LeftHand", {0, -0.6f, 0}, {0, 0, 0}, {0, 0.2f, 0}, {0, 0, 0}},
        {"RightShoulder", "RightUpperArm", "UpperTorso", "RightUpperArm", {1.5f, 0.6f, 0}, {0, 0, 0}, {0, 0.6f, 0}, {0, 0, 0}},
        {"RightElbow", "RightLowerArm", "RightUpperArm", "RightLowerArm", {0, -0.6f, 0}, {0, 0, 0}, {0, 0.6f, 0}, {0, 0, 0}},
        {"RightWrist", "RightHand", "RightLowerArm", "RightHand", {0, -0.6f, 0}, {0, 0, 0}, {0, 0.2f, 0}, {0, 0, 0}},
        {"LeftHip", "LeftUpperLeg", "LowerTorso", "LeftUpperLeg", {-0.5f, -0.2f, 0}, {0, 0, 0}, {0, 0.7f, 0}, {0, 0, 0}},
        {"LeftKnee", "LeftLowerLeg", "LeftUpperLeg", "LeftLowerLeg", {0, -0.7f, 0}, {0, 0, 0}, {0, 0.7f, 0}, {0, 0, 0}},
        {"LeftAnkle", "LeftFoot", "LeftLowerLeg", "LeftFoot", {0, -0.7f, 0}, {0, 0, 0}, {0, 0.1f, 0}, {0, 0, 0}},
        {"RightHip", "RightUpperLeg", "LowerTorso", "RightUpperLeg", {0.5f, -0.2f, 0}, {0, 0, 0}, {0, 0.7f, 0}, {0, 0, 0}},
        {"RightKnee", "RightLowerLeg", "RightUpperLeg", "RightLowerLeg", {0, -0.7f, 0}, {0, 0, 0}, {0, 0.7f, 0}, {0, 0, 0}},
        {"RightAnkle", "RightFoot", "RightLowerLeg", "RightFoot", {0, -0.7f, 0}, {0, 0, 0}, {0, 0.1f, 0}, {0, 0, 0}},
    };
    const Motor* motorsOf = r15 ? kR15Motors : kR6Motors;
    size_t motorCount = r15 ? sizeof kR15Motors / sizeof *kR15Motors : sizeof kR6Motors / sizeof *kR6Motors;
    for (size_t mi = 0; mi < motorCount; mi++) {
        const Motor& mo = motorsOf[mi];
        Instance* in = built.count(mo.in) ? built[mo.in] : nullptr;
        Instance* p0 = built.count(mo.p0) ? built[mo.p0] : nullptr;
        Instance* p1 = built.count(mo.p1) ? built[mo.p1] : nullptr;
        if (!in || !p0 || !p1) continue;
        Instance::Ptr m = dm.create("Motor6D", in);
        m->setName(mo.name);
        m->set("Part0", Value::instance(p0->id()));
        m->set("Part1", Value::instance(p1->id()));
        m->set("C0Position", Value::vector3(mo.c0));   m->set("C0Orientation", Value::vector3(mo.c0r));
        m->set("C1Position", Value::vector3(mo.c1));   m->set("C1Orientation", Value::vector3(mo.c1r));
    }
    }   // !custom
    Instance::Ptr humMade;
    Instance* hum = custom ? ch->findFirstChildOfClass("Humanoid") : nullptr;
    if (!hum) {
        humMade = dm.createInternal("Humanoid", ch.get());
        hum = humMade.get();
        hum->setInternal("RigType", Value::enumItem(r15 ? "R15" : "R6", r15 ? 1 : 0));
    }
    if (!hum->findFirstChildOfClass("Animator"))
        dm.createInternal("Animator", hum)->setName("Animator");   // where animations are loaded, as in Studio
    hum->set("DisplayName", Value::string(player.name()));
    if (Instance* sp = dm.getService("StarterPlayer")) {
        hum->set("NameDisplayDistance", sp->get("NameDisplayDistance"));   // the name and health bar over the head
        hum->set("HealthDisplayDistance", sp->get("HealthDisplayDistance"));
    }
    ch->set("PrimaryPart", Value::instance(root->id()));
    // StarterCharacterScripts contents are copied into every character.
    if (Instance* scs = dm.getService("StarterPlayer")->findFirstChildOfClass("StarterCharacterScripts"))
        for (auto& c : scs->children()) { Instance::Ptr copy = c->clone(); if (copy) copy->setParent(ch.get()); }
    // The Backpack starts over each life: StarterPack, then the player's StarterGear.
    if (Instance* bp = player.findFirstChildOfClass("Backpack")) {
        for (auto& c : std::vector<Instance::Ptr>(bp->children())) c->destroy();
        for (Instance* from : {dm.getService("StarterPack"), player.findFirstChildOfClass("StarterGear")})
            if (from) for (auto& c : from->children()) { Instance::Ptr copy = c->clone(); if (copy) copy->setParent(bp); }
    }
    ch->setParent(ws);
    // After the character is in the Workspace, not before: the engine takes a Humanoid's
    // properties only for a character it already knows about.
    if (Instance* sp = dm.getService("StarterPlayer")) {
        hum->set("WalkSpeed", sp->get("CharacterWalkSpeed"));
        hum->set("JumpPower", sp->get("CharacterJumpPower"));
        hum->set("JumpHeight", sp->get("CharacterJumpHeight"));
        hum->set("UseJumpPower", sp->get("CharacterUseJumpPower"));
        hum->set("MaxSlopeAngle", sp->get("CharacterMaxSlopeAngle"));
    }
    player.set("Character", Value::instance(ch->id()));
    fireValues(player, "CharacterAdded", {Value::instance(ch->id())});
    // Nothing to fetch for the avatar: its appearance is loaded as soon as it stands.
    if (player.binding) if (Signal* s = findSignal(player, "CharacterAppearanceLoaded")) if (s->hasConns()) fireValues(player, "CharacterAppearanceLoaded", {Value::instance(ch->id())});
    return ch.get();
}

// ---- the character's dressing ---------------------------------------------------------
const char* limbGroupOf(const std::string& n) {
    static const struct { const char* part; const char* group; } kLimbs[] = {
        {"Head", "Head"}, {"Torso", "Torso"}, {"Left Arm", "LeftArm"}, {"Right Arm", "RightArm"}, {"Left Leg", "LeftLeg"}, {"Right Leg", "RightLeg"},
        {"UpperTorso", "Torso"}, {"LowerTorso", "Torso"},
        {"LeftUpperArm", "LeftArm"}, {"LeftLowerArm", "LeftArm"}, {"LeftHand", "LeftArm"}, {"RightUpperArm", "RightArm"}, {"RightLowerArm", "RightArm"}, {"RightHand", "RightArm"},
        {"LeftUpperLeg", "LeftLeg"}, {"LeftLowerLeg", "LeftLeg"}, {"LeftFoot", "LeftLeg"}, {"RightUpperLeg", "RightLeg"}, {"RightLowerLeg", "RightLeg"}, {"RightFoot", "RightLeg"},
    };
    for (auto& l : kLimbs) if (n == l.part) return l.group;
    return nullptr;
}
// `colours` carries <Group>Color3 (a BodyColors) or <Group>Color (a HumanoidDescription).
void Runtime::Impl::paintLimbs(Instance& character, const Instance& colours) {
    for (auto& c : character.children()) {
        if (!c->isA("BasePart")) continue;
        const char* group = limbGroupOf(c->name());
        if (!group) continue;
        Value v = colours.get(std::string(group) + "Color3");
        if (v.type != Value::Color3) v = colours.get(std::string(group) + "Color");
        if (v.type == Value::Color3) c->set("Color", v);
    }
}
// A BodyColors in a character paints it, on arrival and on every change; the server's paint
// replicates, so a copy applying replication leaves the parts alone.
void Runtime::Impl::bodyColorsChanged(Instance& bc) {
    if (dm.applyingRemote()) return;
    Instance* ch = bc.parent();
    if (!ch || !ch->isA("Model") || !ch->findFirstChildOfClass("Humanoid")) return;
    paintLimbs(*ch, bc);
}
// Humanoid:ApplyDescriptionAsync: the colours are painted; the rest of the description is kept
// for GetAppliedDescription and not worn.
void Runtime::Impl::applyDescription(Instance& hum, Instance& desc) {
    if (Instance* ch = hum.parent()) paintLimbs(*ch, desc);
    appliedDescriptions[hum.id()] = desc.clone();
    if (hum.binding) if (Signal* s = findSignal(hum, "ApplyDescriptionFinished")) if (s->hasConns()) fireValues(hum, "ApplyDescriptionFinished", {Value::instance(desc.id())});
}

// ---- tools -------------------------------------------------------------------------
Instance* Runtime::Impl::mouseObject() {
    if (dm.isServer()) return nullptr;
    if (!mouse) { mouse = dm.createInternal("Mouse"); mouse->setName("Mouse"); }
    return mouse.get();
}

// A Tool moving into a character is equipped, out of one unequipped. Both sides see the move,
// so both fire; the client's Equipped carries the Mouse, like Roblox's.
void Runtime::Impl::toolMoved(Instance& tool, Instance* oldP, Instance* newP) {
    auto isCharacter = [](Instance* p) { return p && p->isA("Model") && p->findFirstChildOfClass("Humanoid"); };
    bool was = heldTools.count(tool.id()) > 0, held = newP && isCharacter(newP);
    Instance* myCharacter = localPlayer ? dm.findRef(localPlayer->get("Character").ref) : nullptr;
    // ContextActionService's LocalToolEquipped / LocalToolUnequipped, when a script has asked for the service
    Instance* cas = myCharacter ? dm.find(DataModel::serviceId("ContextActionService")) : nullptr;
    if (was && (!held || newP != oldP)) {
        heldTools.erase(tool.id());
        if (tool.binding) if (Signal* s = findSignal(tool, "Unequipped")) if (s->hasConns()) fireValues(tool, "Unequipped", {});
        if (cas && oldP == myCharacter) if (Signal* s = findSignal(*cas, "LocalToolUnequipped")) if (s->hasConns()) fireValues(*cas, "LocalToolUnequipped", {Value::instance(tool.id())});
    }
    if (held && !tool.destroyed()) {
        heldTools.insert(tool.id());
        if (dm.isServer()) unequipTools(*newP, &tool);   // one tool in hand, like the Humanoid
        bool mine = myCharacter == newP;
        Instance* m = mine ? mouseObject() : nullptr;
        if (tool.binding) if (Signal* s = findSignal(tool, "Equipped")) if (s->hasConns()) fireValues(tool, "Equipped", {Value::instance(m ? m->id() : 0)});
        if (cas && mine) if (Signal* s = findSignal(*cas, "LocalToolEquipped")) if (s->hasConns()) fireValues(*cas, "LocalToolEquipped", {Value::instance(tool.id())});
    }
    if (!dm.isServer()) refreshHotbar();
}

// A tool keeps the slot it first appeared in, held or not; new ones go on the end. The engine
// draws the bar from each tool's HotbarSlot, 1-9, or 0 for one off the bar.
void Runtime::Impl::refreshHotbar() {
    if (dm.isServer() || !localPlayer) return;
    Instance* character = dm.findRef(localPlayer->get("Character").ref);
    Instance* backpack = localPlayer->findFirstChildOfClass("Backpack");
    std::vector<Instance*> present;
    if (backpack) for (auto& c : backpack->children()) if (c->isA("Tool") && !c->destroyed()) present.push_back(c.get());
    if (character) for (auto& c : character->children()) if (c->isA("Tool") && !c->destroyed()) present.push_back(c.get());
    std::vector<int64_t> bar;
    for (int64_t id : hotbar) for (Instance* t : present) if (t->id() == id) { bar.push_back(id); break; }
    for (Instance* t : present) if (std::find(bar.begin(), bar.end(), t->id()) == bar.end()) bar.push_back(t->id());
    for (int64_t id : hotbar)
        if (std::find(bar.begin(), bar.end(), id) == bar.end())
            if (Instance* t = dm.find(id)) if (!t->destroyed()) t->setInternal("HotbarSlot", Value::number(0));
    hotbar = bar;
    for (size_t k = 0; k < bar.size(); k++) dm.find(bar[k])->setInternal("HotbarSlot", Value::number(k < 9 ? (double)(k + 1) : 0));
}

void Runtime::Impl::hotbarSelect(int slot) {
    if (dm.isServer() || !localPlayer) return;
    refreshHotbar();
    if (slot < 0 || slot >= (int)hotbar.size() || slot >= 9) return;
    Instance* tool = dm.find(hotbar[slot]);
    Instance* character = dm.findRef(localPlayer->get("Character").ref);
    if (!tool || !character) return;
    RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = tool->id();
    msg.args.push_back(NetValue::string(tool->parent() == character ? "Unequip" : "Equip"));
    outRemotes.push_back(std::move(msg));
}

void Runtime::Impl::unequipTools(Instance& character, Instance* except) {
    Instance* backpack = nullptr;   // the character's player's Backpack; an NPC's tool just leaves the Model
    for (auto& p : dm.getService("Players")->children())
        if (p->isA("Player") && dm.findRef(p->get("Character").ref) == &character) { backpack = p->findFirstChildOfClass("Backpack"); break; }
    for (auto& c : std::vector<Instance::Ptr>(character.children())) if (c->isA("Tool") && c.get() != except) c->setParent(backpack);
}

void Runtime::Impl::equipTool(Instance& character, Instance& tool) {
    if (tool.parent() == &character) return;
    if (tool.get("RequiresHandle").b && !tool.findFirstChild("Handle")) return;
    tool.setParent(&character);   // toolMoved puts the one it was holding back
}

// Backspace: the tool lands in the Workspace, its Handle in front of the character.
void Runtime::Impl::dropTool(Instance& character, Instance& tool) {
    Instance* root = character.findFirstChild("HumanoidRootPart");
    Instance* handle = tool.findFirstChild("Handle");
    if (root && handle) {
        Vec3 p = root->get("Position").v, o = root->get("Orientation").v;
        CFrameV cf = cframeFromPosOrient(p, o);
        Vec3 at = cf * Vec3{0, -1, -3};
        Vec3 was = handle->get("Position").v, d{at.x - was.x, at.y - was.y, at.z - was.z};
        for (Instance* part : tool.getDescendants()) if (part->isA("BasePart")) {
            Vec3 q = part->get("Position").v;
            part->set("Position", Value::vector3(q.x + d.x, q.y + d.y, q.z + d.z));
            part->set("Anchored", Value::boolean(false));
        }
    }
    tool.setParent(dm.workspace());
}

// ---- ProximityPrompts --------------------------------------------------------------
// The part a prompt stands on: its parent, or a Model's PrimaryPart / first part.
Instance* Runtime::Impl::promptPart(Instance& prompt) {
    Instance* p = prompt.parent();
    if (!p) return nullptr;
    if (p->isA("BasePart")) return p;
    if (p->isA("Model")) {
        if (Instance* pp = dm.findRef(p->get("PrimaryPart").ref)) return pp;
        for (auto& c : p->children()) if (c->isA("BasePart")) return c.get();
    }
    return nullptr;
}

// The enabled prompts within MaxActivationDistance of the character's root (and in sight of its
// Head when RequiresLineOfSight), nearest first, under Exclusivity and MaxPromptsVisible. The
// hidden Shown property tells the engine which to draw.
void Runtime::Impl::updatePrompts(double dt) {
    Instance* svc = dm.getService("ProximityPromptService");
    Instance* character = localPlayer ? dm.findRef(localPlayer->get("Character").ref) : nullptr;
    Instance* root = character ? character->findFirstChild("HumanoidRootPart") : nullptr;
    struct Near { Instance* prompt; double dist; };
    std::vector<Near> near;
    if (root && svc->get("Enabled").b) {
        Vec3 from = root->get("Position").v;
        Instance* head = character->findFirstChild("Head");
        Vec3 eye = head ? head->get("Position").v : from;
        for (int64_t id : prompts) {
            Instance* pr = dm.find(id);
            if (!pr || pr->destroyed() || !pr->get("Enabled").b) continue;
            Instance* part = promptPart(*pr);
            if (!part) continue;
            Vec3 at = part->get("Position").v;
            Vec3 d{at.x - from.x, at.y - from.y, at.z - from.z};
            double dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (dist > pr->get("MaxActivationDistance").n) continue;
            if (pr->get("RequiresLineOfSight").b) {
                RaycastFilter f;
                f.ids.push_back(character->id());
                f.ids.push_back(pr->parent()->id());
                RayHit hit;
                if (raycastTree(dm, eye, {at.x - eye.x, at.y - eye.y, at.z - eye.z}, &f, hit)) continue;
            }
            near.push_back({pr, dist});
        }
    }
    std::stable_sort(near.begin(), near.end(), [](const Near& a, const Near& b) { return a.dist < b.dist; });
    std::vector<int64_t> show;
    std::set<std::string> keysTaken;
    bool allTaken = false;
    int room = std::max(0, (int)svc->get("MaxPromptsVisible").n);
    for (const Near& n : near) {
        std::string ex = n.prompt->get("Exclusivity").s;
        if (ex == "AlwaysShow") { show.push_back(n.prompt->id()); continue; }
        if (room <= 0 || allTaken) continue;
        std::string key = n.prompt->get("KeyboardKeyCode").s;
        if (ex == "OnePerButton" && keysTaken.count(key)) continue;
        if (ex == "OneGlobally") allTaken = true;
        keysTaken.insert(key);
        show.push_back(n.prompt->id());
        room--;
    }
    Value keyboard = Value::enumItem("ProximityPromptInputType.Keyboard", 0);
    for (int64_t id : shownPrompts) if (std::find(show.begin(), show.end(), id) == show.end()) {
        Instance* pr = dm.find(id);
        if (!pr || pr->destroyed()) continue;
        pr->setInternal("Shown", Value::boolean(false));
        if (holdingPrompt == id) {
            holdingPrompt = 0;
            fireValues(*pr, "PromptButtonHoldEnded", {});
            fireValues(*svc, "PromptButtonHoldEnded", {Value::instance(id)});
        }
        fireValues(*pr, "PromptHidden", {});
        fireValues(*svc, "PromptHidden", {Value::instance(id)});
    }
    for (int64_t id : show) if (std::find(shownPrompts.begin(), shownPrompts.end(), id) == shownPrompts.end()) {
        Instance* pr = dm.find(id);
        pr->setInternal("Shown", Value::boolean(true));
        fireValues(*pr, "PromptShown", {keyboard});
        fireValues(*svc, "PromptShown", {Value::instance(id), keyboard});
    }
    shownPrompts = std::move(show);
    if (holdingPrompt) {
        Instance* pr = dm.find(holdingPrompt);
        holdElapsed += dt;
        if (!pr || pr->destroyed()) holdingPrompt = 0;
        else if (holdElapsed >= pr->get("HoldDuration").n) {
            int64_t id = holdingPrompt; holdingPrompt = 0;
            fireValues(*pr, "PromptButtonHoldEnded", {});
            fireValues(*svc, "PromptButtonHoldEnded", {Value::instance(id)});
            triggerPrompt(*pr);
        }
    }
}

// Down: the nearest shown prompt on that key. Up: whatever it was holding or had triggered.
void Runtime::Impl::promptKey(const std::string& key, bool down) {
    if (down) {
        for (int64_t id : shownPrompts) {
            Instance* pr = dm.find(id);
            if (pr && !pr->destroyed() && pr->get("KeyboardKeyCode").s == key) { promptPress(*pr); break; }
        }
        return;
    }
    std::vector<int64_t> up(triggeredPrompts.begin(), triggeredPrompts.end());
    if (holdingPrompt) up.push_back(holdingPrompt);
    for (int64_t id : up) {
        Instance* pr = dm.find(id);
        if (pr && !pr->destroyed() && pr->get("KeyboardKeyCode").s == key) promptRelease(*pr);
    }
}

void Runtime::Impl::promptPress(Instance& prompt) {
    if (dm.isServer() || holdingPrompt || triggeredPrompts.count(prompt.id())) return;
    if (std::find(shownPrompts.begin(), shownPrompts.end(), prompt.id()) == shownPrompts.end()) return;
    if (prompt.get("HoldDuration").n > 0) {
        holdingPrompt = prompt.id(); holdElapsed = 0;
        fireValues(prompt, "PromptButtonHoldBegan", {});
        fireValues(*dm.getService("ProximityPromptService"), "PromptButtonHoldBegan", {Value::instance(prompt.id())});
    } else triggerPrompt(prompt);
}

void Runtime::Impl::promptRelease(Instance& prompt) {
    Instance* svc = dm.getService("ProximityPromptService");
    if (holdingPrompt == prompt.id()) {
        holdingPrompt = 0;
        fireValues(prompt, "PromptButtonHoldEnded", {});
        fireValues(*svc, "PromptButtonHoldEnded", {Value::instance(prompt.id())});
    }
    if (!triggeredPrompts.erase(prompt.id())) return;
    Value who = Value::instance(localPlayer ? localPlayer->id() : 0);
    fireValues(prompt, "TriggerEnded", {who});
    fireValues(*svc, "PromptTriggerEnded", {Value::instance(prompt.id()), who});
    RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = prompt.id(); msg.args.push_back(NetValue::string("TriggerEnded"));
    outRemotes.push_back(std::move(msg));
}

// Triggered(player) here at once and, as a message on the prompt, on the server
// after its own checks (Runtime::deliverRemote).
void Runtime::Impl::triggerPrompt(Instance& prompt) {
    Value who = Value::instance(localPlayer ? localPlayer->id() : 0);
    fireValues(prompt, "Triggered", {who});
    fireValues(*dm.getService("ProximityPromptService"), "PromptTriggered", {Value::instance(prompt.id()), who});
    triggeredPrompts.insert(prompt.id());
    RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = prompt.id(); msg.args.push_back(NetValue::string("Triggered"));
    outRemotes.push_back(std::move(msg));
}

void Runtime::Impl::promptGone(int64_t id) {
    prompts.erase(id);
    triggeredPrompts.erase(id);
    if (holdingPrompt == id) holdingPrompt = 0;
    shownPrompts.erase(std::remove(shownPrompts.begin(), shownPrompts.end(), id), shownPrompts.end());
}

// ---- sounds ------------------------------------------------------------------------
// The region a Sound or AudioPlayer plays inside, clamped to what the asset is: [0, TimeLength]
// when unset, and the whole asset when its two ends are equal. LoopRegion likewise, inside it.
static void playbackRegion(const Instance& p, double len, double& lo, double& hi) {
    const Value r = p.get("PlaybackRegion");
    lo = std::max(0.0, (double)r.u[0]); hi = std::min(len, (double)r.u[1]);
    if (r.u[0] == r.u[1] || hi <= lo) { lo = 0; hi = len; }
}
static void loopRegion(const Instance& p, double lo, double hi, double& ls, double& le) {
    const Value lr = p.get("LoopRegion");
    ls = std::max((double)lr.u[0], lo); le = std::min((double)lr.u[1], hi);
    if (lr.u[0] == lr.u[1] || le <= ls) { ls = lo; le = hi; }
}

// The clock behind Playing. TimeLength 0 means the engine has not loaded the clip, so the
// position waits, as on Roblox. setSilent: the engine keeps its own clock, and a joining
// client gets the position in its snapshot.
void Runtime::Impl::updateSounds(double dt) {
    std::vector<int64_t> ids(sounds.begin(), sounds.end());
    for (int64_t id : ids) {
        Instance* s = dm.find(id);
        if (!s || s->destroyed() || !s->get("Playing").b) continue;
        double len = s->get("TimeLength").n;
        if (len <= 0) continue;
        double pos = s->get("TimePosition").n + dt * s->get("PlaybackSpeed").n;
        // PlaybackRegionsEnabled: the clock lives in [lo, hi) and, Looped, wraps inside LoopRegion.
        double lo = 0, hi = len, ls = 0, le = len;
        if (s->get("PlaybackRegionsEnabled").b) {
            playbackRegion(*s, len, lo, hi);
            loopRegion(*s, lo, hi, ls, le);
            if (pos < lo) pos = lo;
        }
        const double end = s->get("Looped").b ? le : hi;
        if (pos < end) { s->setSilent("TimePosition", Value::number(pos)); continue; }
        if (s->get("Looped").b) {
            int n = ++soundLoops[id];
            s->setSilent("TimePosition", Value::number(ls + std::fmod(pos - ls, std::max(le - ls, 1e-3))));
            fireValues(*s, "DidLoop", {s->get("SoundId"), Value::number(n)});
        } else {
            s->setSilent("TimePosition", Value::number(lo));
            soundVerb = SoundEnding;
            s->setInternal("Playing", Value::boolean(false));
            soundVerb = SoundNone;
        }
    }
}

// ---- the Audio API ------------------------------------------------------------------
// An AudioPlayer's clock, as a Sound's: TimePosition ticks inside PlaybackRegion, wraps inside
// LoopRegion when Looping, and at the end goes back to the region's start so the next Play is
// from the top. Stop leaves TimePosition where it is.
void Runtime::Impl::audioPlay(Instance& p, bool play) {
    if (p.get("IsPlaying").b == play) return;
    p.setInternal("IsPlaying", Value::boolean(play));
}

void Runtime::Impl::updateAudio(double dt) {
    // Play / Stop given a mixer time: due now.
    for (size_t k = 0; k < audioActions.size();) {
        if (audioActions[k].at > now) { k++; continue; }
        AudioAction a = audioActions[k];
        audioActions.erase(audioActions.begin() + k);
        if (Instance* p = dm.findRef(a.player)) if (!p->destroyed()) audioPlay(*p, a.play);
    }
    std::vector<int64_t> ids(audioPlayers.begin(), audioPlayers.end());
    for (int64_t id : ids) {
        Instance* p = dm.find(id);
        if (!p || p->destroyed() || !p->get("IsPlaying").b) continue;
        double len = p->get("TimeLength").n;
        if (len <= 0) continue;
        double lo, hi;
        playbackRegion(*p, len, lo, hi);
        double pos = p->get("TimePosition").n + dt * std::clamp(p->get("PlaybackSpeed").n, 0.0, 20.0);
        if (pos < lo) pos = lo;
        if (p->get("Looping").b) {
            double ls, le;
            loopRegion(*p, lo, hi, ls, le);
            if (pos >= le) {
                pos = ls + std::fmod(pos - le, std::max(le - ls, 1e-3));
                p->setSilent("TimePosition", Value::number(pos));
                fireValues(*p, "Looped", {});
                continue;
            }
            p->setSilent("TimePosition", Value::number(pos));
            continue;
        }
        if (pos < hi) { p->setSilent("TimePosition", Value::number(pos)); continue; }
        p->setSilent("TimePosition", Value::number(lo));
        p->setInternal("IsPlaying", Value::boolean(false));
        fireValues(*p, "Ended", {});
    }
    refreshWires();
}

std::vector<std::string> audioChannelPins(const std::string& layout) { return audio::channelPins(layout); }
bool audioEffectClass(const std::string& c) { return audio::effectClass(c); }

// Which pins a class has. Producers have an Output only, effects both (a compressor a Sidechain
// too), and what a stream ends in an Input only; a splitter fans out and a mixer gathers by
// channel name.
std::vector<std::string> audioPins(const Instance& i, bool output) {
    const std::string& c = i.className();
    if (c == "AudioChannelSplitter") return output ? audioChannelPins(i.get("Layout").s) : std::vector<std::string>{"Input"};
    if (c == "AudioChannelMixer") return output ? std::vector<std::string>{"Output"} : audioChannelPins(i.get("Layout").s);
    if (output) {
        if (audioEffectClass(c) || c == "AudioPlayer" || c == "AudioListener" || c == "AudioDeviceInput" || c == "AudioTextToSpeech") return {"Output"};
        return {};
    }
    if (c == "AudioCompressor") return {"Input", "Sidechain"};
    if (audioEffectClass(c) || c == "AudioEmitter" || c == "AudioDeviceOutput" || c == "AudioAnalyzer" || c == "AudioSpeechToText" || c == "AudioRecorder")
        return {"Input"};
    return {};
}
bool audioHasPin(const Instance& i, const std::string& pin, bool output) {
    for (const std::string& p : audioPins(i, output)) if (p == pin) return true;
    return false;
}

// A Wire is Connected when both ends and both pins are real and it does not close a loop. Wires
// are taken in the order they were made, so the one closing the loop is the one refused.
void Runtime::Impl::refreshWires() {
    if (wires.empty()) return;
    std::vector<int64_t> order(wires.begin(), wires.end());
    // Made first, first: server ids count up from one end and a client's own count down from the other.
    std::sort(order.begin(), order.end(), [](int64_t a, int64_t b) { return std::llabs(a) < std::llabs(b); });
    std::unordered_map<int64_t, std::vector<int64_t>> feeds;   // what each instance feeds, by the wires accepted so far
    auto reaches = [&](int64_t from, int64_t to) {
        std::vector<int64_t> stack{from};
        std::unordered_set<int64_t> seen;
        while (!stack.empty()) {
            int64_t at = stack.back(); stack.pop_back();
            if (at == to) return true;
            if (!seen.insert(at).second) continue;
            auto f = feeds.find(at);
            if (f != feeds.end()) for (int64_t next : f->second) stack.push_back(next);
        }
        return false;
    };
    for (int64_t id : order) {
        Instance* w = dm.find(id);
        if (!w || w->destroyed()) continue;
        Instance* src = dm.findRef(w->get("SourceInstance").ref);
        Instance* dst = dm.findRef(w->get("TargetInstance").ref);
        bool ok = src && dst && !src->destroyed() && !dst->destroyed() && src != dst
                  && audioHasPin(*src, w->get("SourceName").s, true) && audioHasPin(*dst, w->get("TargetName").s, false)
                  && !reaches(dst->id(), src->id());
        if (ok) feeds[src->id()].push_back(dst->id());
        if (w->get("Connected").b == ok) continue;
        w->setInternal("Connected", Value::boolean(ok));
        const Value wv = Value::instance(w->id());
        if (src) fireValues(*src, "WiringChanged", {Value::boolean(ok), w->get("SourceName"), wv, Value::instance(dst ? dst->id() : 0)});
        if (dst) fireValues(*dst, "WiringChanged", {Value::boolean(ok), w->get("TargetName"), wv, Value::instance(src ? src->id() : 0)});
    }
}

// A Motor6D's or Motor's CurrentAngle moves toward DesiredAngle by at most MaxVelocity per
// physics step (Roblox's 1/60 s); the server turns it and the change replicates.
void Runtime::Impl::stepMotors(double dt) {
    std::vector<int64_t> ids(motors.begin(), motors.end());
    for (int64_t id : ids) {
        Instance* m = dm.find(id);
        if (!m || m->destroyed()) continue;
        double cur = m->get("CurrentAngle").n, want = m->get("DesiredAngle").n;
        if (cur == want) continue;
        double step = std::fabs(m->get("MaxVelocity").n) * dt * 60.0;
        double next = want > cur ? std::min(cur + step, want) : std::max(cur - step, want);
        m->setInternal("CurrentAngle", Value::number(next));
    }
}

// Play restarts from the top, Resume goes on from TimePosition, and PlayLocalSound (anywhere)
// plays a Sound that is not in the tree.
void Runtime::Impl::soundPlay(Instance& s, bool resume, bool anywhere) {
    if (anywhere) sounds.insert(s.id());
    else if (!sounds.count(s.id())) return;                     // not in the DataModel: nothing to hear it
    // A write of the value an Instance already holds records nothing, and on a server the clip
    // never loads so TimePosition is always 0: touch() is what puts the restart on the wire,
    // for a client whose own clock has run the clip to its end.
    if (!resume) {
        soundLoops.erase(s.id());
        if (s.get("TimePosition").n == 0) s.touch("TimePosition"); else s.setInternal("TimePosition", Value::number(0));
    }
    soundVerb = resume ? SoundResuming : SoundNone;
    if (s.get("Playing").b) { if (!resume) s.touch("Playing"); }
    else s.setInternal("Playing", Value::boolean(true));
    soundVerb = SoundNone;
}

void Runtime::Impl::soundStop(Instance& s, bool pause) {
    if (!pause) { soundLoops.erase(s.id()); s.setInternal("TimePosition", Value::number(0)); }
    if (!s.get("Playing").b) return;
    soundVerb = pause ? SoundPausing : SoundNone;
    s.setInternal("Playing", Value::boolean(false));
    soundVerb = SoundNone;
}

// A part of a character (a Model with a live Humanoid) touched the seat: sit it, unless
// it is Disabled, taken, or the character sat down / stood up a moment ago.
void Runtime::Impl::seatTouch(Instance& seat, Instance* part) {
    if (!dm.isServer() || !part || seat.get("Disabled").b || seat.get("Occupant").ref) return;
    Instance* m = part->parent();
    Instance* hum = m ? m->findFirstChildOfClass("Humanoid") : nullptr;
    if (!hum || hum->get("Health").n <= 0 || hum->get("SeatPart").ref || hum->get("PlatformStand").b) return;
    if (!m->findFirstChild("HumanoidRootPart")) return;
    auto cit = seatCooldown.find(hum->id());
    if (cit != seatCooldown.end() && cit->second > now) return;
    sitOn(seat, *hum);
}

void Runtime::Impl::sitOn(Instance& seat, Instance& hum) {
    Instance* m = hum.parent();
    Instance* root = m ? m->findFirstChild("HumanoidRootPart") : nullptr;
    if (!root) return;
    if (hum.get("SeatPart").ref) unseat(hum);
    if (Instance* old = dm.findRef(seat.get("Occupant").ref)) unseat(*old);
    Instance::Ptr w = dm.create("Weld");
    w->setName("SeatWeld");
    w->set("Part0", Value::instance(seat.id()));
    w->set("Part1", Value::instance(root->id()));
    w->set("C0Position", Value::vector3(0, seat.get("Size").v.y / 2 + 1, 0));   // the torso's bottom on the seat's top
    w->setParent(&seat);
    seat.setInternal("Occupant", Value::instance(hum.id()));
    hum.setInternal("SeatPart", Value::instance(seat.id()));
    hum.setInternal("Sit", Value::boolean(true));
    seatCooldown[hum.id()] = now + 0.5;
    if (hum.binding) if (Signal* s = findSignal(hum, "Seated")) if (s->hasConns()) fireValues(hum, "Seated", {Value::boolean(true), Value::instance(seat.id())});
}

void Runtime::Impl::unseat(Instance& hum) {
    Instance* seat = dm.findRef(hum.get("SeatPart").ref);
    hum.setInternal("SeatPart", Value::instance(0));
    hum.setInternal("Sit", Value::boolean(false));
    seatCooldown[hum.id()] = now + 1.0;
    if (seat && !seat->destroyed()) {
        if (seat->get("Occupant").ref == hum.id()) seat->setInternal("Occupant", Value::instance(0));
        if (seat->isA("VehicleSeat")) { seat->setInternal("Throttle", Value::number(0)); seat->setInternal("Steer", Value::number(0)); seat->setInternal("ThrottleFloat", Value::number(0)); seat->setInternal("SteerFloat", Value::number(0)); }
        std::vector<Instance*> welds;
        for (auto& c : seat->children()) if (c->isA("Weld") && c->name() == "SeatWeld") if (Instance* p1 = dm.findRef(c->get("Part1").ref)) if (p1->parent() == hum.parent()) welds.push_back(c.get());
        for (Instance* w : welds) if (!w->destroyed()) w->destroy();
    }
    if (hum.binding) if (Signal* s = findSignal(hum, "Seated")) if (s->hasConns()) fireValues(hum, "Seated", {Value::boolean(false), Value::instance(0)});
}

// A VehicleSeat reads its occupant's controls: forward along the seat is Throttle 1,
// back -1, right is Steer 1 (the floats the same, unrounded).
void Runtime::Impl::steerVehicle(Instance& hum) {
    Instance* seat = dm.findRef(hum.get("SeatPart").ref);
    if (!seat || !seat->isA("VehicleSeat")) return;
    Vec3 md = hum.get("MoveDirection").v;
    CFrameV cf = cframeFromPosOrient(seat->get("Position").v, seat->get("Orientation").v);
    Vec3 look = cf.look(), right = cf.col(0);
    float t = md.x * look.x + md.y * look.y + md.z * look.z, st = md.x * right.x + md.y * right.y + md.z * right.z;
    auto sign = [](float v) { return v > 0.25f ? 1.0 : v < -0.25f ? -1.0 : 0.0; };
    seat->setInternal("ThrottleFloat", Value::number(t)); seat->setInternal("SteerFloat", Value::number(st));
    seat->setInternal("Throttle", Value::number(sign(t))); seat->setInternal("Steer", Value::number(sign(st)));
}

void Runtime::Impl::detonate(Instance& e) {
    for (auto& [t, id] : explosions) if (id == e.id()) return;   // once
    explosions.emplace_back(now + 2.0, e.id());
    Vec3 at = e.get("Position").v;
    float radius = (float)e.get("BlastRadius").n;
    float jointRadius = radius * (float)e.get("DestroyJointRadiusPercent").n;
    ShapeV s; s.cf = CFrameV::fromPos(at); s.sphere = true; s.radius = radius;
    std::vector<Instance*> parts = overlapTree(dm, s, nullptr, nullptr, true);
    std::vector<int64_t> near;                           // the parts within the joint radius
    for (Instance* p : parts) {
        Vec3 pp = p->get("Position").v;
        float dx = pp.x - at.x, dy = pp.y - at.y, dz = pp.z - at.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist <= jointRadius) near.push_back(p->id());
        if (e.binding) if (Signal* sig = findSignal(e, "Hit")) if (sig->hasConns()) fireValues(e, "Hit", {Value::instance(p->id()), Value::number(dist)});
    }
    if (near.empty() || !dm.isServer()) return;
    Instance* ws = dm.getService("Workspace");
    std::vector<Instance*> joints;
    for (Instance* d : ws->getDescendants())
        if (d->isA("JointInstance") || d->isA("WeldConstraint"))
            for (int64_t id : near) if (d->get("Part0").ref == id || d->get("Part1").ref == id) { joints.push_back(d); break; }
    for (Instance* j : joints) j->destroy();
    for (int64_t id : near)                              // a character's joints break: its Humanoid dies
        if (Instance* p = dm.find(id)) if (Instance* m = p->parent()) if (Instance* h = m->findFirstChildOfClass("Humanoid"))
            if (h->get("Health").n > 0) h->set("Health", Value::number(0));
}

void Runtime::Impl::checkRespawns() {
    for (size_t k = 0; k < explosions.size();) {
        if (explosions[k].first > now) { k++; continue; }
        int64_t id = explosions[k].second;
        explosions.erase(explosions.begin() + k);
        if (Instance* e = dm.find(id)) if (!e->destroyed()) e->destroy();
    }
    // Before the respawns: a character that dies with a shield and comes straight back must not
    // inherit the old one's clock.
    for (size_t k = 0; k < forceFields.size();) {
        if (forceFields[k].first > now) { k++; continue; }
        if (Instance* ff = dm.find(forceFields[k].second)) if (!ff->destroyed()) ff->destroy();
        forceFields.erase(forceFields.begin() + k);
    }
    for (size_t k = 0; k < respawns.size();) {
        if (respawns[k].first > now) { k++; continue; }
        int64_t pid = respawns[k].second;
        respawns.erase(respawns.begin() + k);
        Instance* p = dm.find(pid);
        if (!p || p->destroyed() || !p->parent()) continue;
        if (Instance* old = dm.findRef(p->get("Character").ref)) {
            fireValues(*p, "CharacterRemoving", {Value::instance(old->id())});
            old->destroy();
        }
        spawnCharacter(*p);
    }
}

// ---- task API (globals) ---------------------------------------------------------------
static int task_wait(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    double secs = luaL_optnumber(L, 1, 0);
    if (secs < 0) secs = 0;
    if (!lua_isyieldable(L)) luaL_error(L, "cannot yield from this context");
    Task* t = rt.taskFor(L);
    t->state = Task::Sleeping;
    t->sleepStart = rt.now;
    t->wakeAt = rt.now + secs;
    rt.sleepers.push_back(t);
    return lua_yield(L, 0);
}

// Pushes fn (or thread) + args as a new task; returns the Task with the callee's stack ready.
static Task* taskFromArgs(lua_State* L, int fnIdx, Runtime::Impl& rt) {
    int n = lua_gettop(L);
    Task* t;
    if (lua_isthread(L, fnIdx)) {
        lua_State* co = lua_tothread(L, fnIdx);
        t = rt.taskFor(co);
        for (int i = fnIdx + 1; i <= n; i++) { lua_pushvalue(L, i); lua_xmove(L, co, 1); }
    } else {
        luaL_checktype(L, fnIdx, LUA_TFUNCTION);
        t = rt.newTask(L, rt.ctxOf(L));
        for (int i = fnIdx; i <= n; i++) { lua_pushvalue(L, i); lua_xmove(L, t->co, 1); }
    }
    return t;
}
static int task_spawn(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Task* t = taskFromArgs(L, 1, rt);
    lua_State* co = t->co;
    int nargs = lua_gettop(co) - (lua_isthread(L, 1) ? 0 : 1);
    lua_pushthread(co); lua_xmove(co, L, 1);   // return the thread
    rt.resume(t, nargs);
    return 1;
}
static int task_defer(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Task* t = taskFromArgs(L, 1, rt);
    t->state = Task::Deferred;
    rt.deferred.push_back(t);
    lua_pushthread(t->co); lua_xmove(t->co, L, 1);
    return 1;
}
static int task_delay(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    double secs = luaL_checknumber(L, 1);
    Task* t = taskFromArgs(L, 2, rt);
    t->state = Task::Sleeping;
    t->sleepStart = rt.now;
    t->wakeAt = rt.now + (secs < 0 ? 0 : secs);
    rt.sleepers.push_back(t);
    lua_pushthread(t->co); lua_xmove(t->co, L, 1);
    return 1;
}
static int task_cancel(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    luaL_checktype(L, 1, LUA_TTHREAD);
    lua_State* co = lua_tothread(L, 1);
    auto it = rt.byThread.find(co);
    if (it != rt.byThread.end()) {
        Task* t = it->second;
        if (t->state == Task::Running) luaL_error(L, "cannot cancel a running thread");
        rt.sleepers.erase(std::remove(rt.sleepers.begin(), rt.sleepers.end(), t), rt.sleepers.end());
        rt.ready.erase(std::remove(rt.ready.begin(), rt.ready.end(), t), rt.ready.end());
        rt.deferred.erase(std::remove(rt.deferred.begin(), rt.deferred.end(), t), rt.deferred.end());
        rt.childWaiters.erase(std::remove(rt.childWaiters.begin(), rt.childWaiters.end(), t), rt.childWaiters.end());
        lua_resetthread(co);
        rt.kill(t);
    }
    return 0;
}
// Legacy globals: wait/spawn/delay behave like their task.* versions here.
static int legacy_spawn(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    Task* t = rt.newTask(L, rt.ctxOf(L));
    lua_xmove(L, t->co, 1);
    t->state = Task::Deferred;
    rt.deferred.push_back(t);
    return 0;
}
static int legacy_delay(lua_State* L) {
    lua_settop(L, 2);
    return task_delay(L);
}
static int g_tick(lua_State* L) {
    using namespace std::chrono;
    lua_pushnumber(L, duration<double>(system_clock::now().time_since_epoch()).count()); return 1;
}
static int g_time(lua_State* L) { lua_pushnumber(L, rtOf(L).now); return 1; }
static int g_print(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    std::string out;
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        size_t len; const char* s = luaL_tolstring(L, i, &len);
        if (i > 1) out += '\t';
        out.append(s, len);
        lua_pop(L, 1);
    }
    auto ctx = rt.ctxOf(L);
    if (rt.cb.print) rt.cb.print(ctx ? ctx->name : "", out);
    rt.logLine(out, 0);
    return 0;
}
static int g_warn(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    std::string out;
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        size_t len; const char* s = luaL_tolstring(L, i, &len);
        if (i > 1) out += '\t';
        out.append(s, len);
        lua_pop(L, 1);
    }
    auto ctx = rt.ctxOf(L);
    if (rt.cb.warn) rt.cb.warn(ctx ? ctx->name : "", out);
    rt.logLine(out, 2);
    return 0;
}
static int g_typeof(lua_State* L) {
    if (lua_isvector(L, 1)) { lua_pushstring(L, "Vector3"); return 1; }
    if (lua_istable(L, 1) && lua_getmetatable(L, 1)) {   // a table standing for a datatype (a Content): its __type
        lua_getfield(L, -1, "__type");
        if (lua_isstring(L, -1)) return 1;
        lua_pop(L, 2);
    }
    lua_pushstring(L, luaL_typename(L, 1));
    return 1;
}

// require(ModuleScript): run once, cache the result.
static int g_require(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance* m = rt.toInstance(L, 1);
    if (!m) luaL_error(L, "Attempted to call require with invalid argument(s).");
    if (!m->isA("ModuleScript")) luaL_error(L, "Attempted to call require with invalid argument(s). (%s is not a ModuleScript)", m->fullName().c_str());
    auto it = rt.moduleResults.find(m->id());
    if (it != rt.moduleResults.end()) { lua_getref(L, it->second); return 1; }
    if (rt.moduleLoading.count(m->id())) luaL_error(L, "Requested module was required recursively (%s)", m->fullName().c_str());

    std::string name = m->fullName();
    std::string source = m->get("Source").s;
    lua_CompileOptions copts = userCompileOptions();
    size_t bcLen = 0;
    char* bc = luau_compile(source.c_str(), source.size(), &copts, &bcLen);
    // A module runs on its own sandboxed thread, to completion: a yield mid-module is an error
    // here, though Roblox allows one.
    lua_State* T = lua_newthread(L);
    luaL_sandboxthread(T);
    rt.pushInstance(T, m);
    lua_setglobal(T, "script");
    std::string chunk = "=" + name;
    int status = luau_load(T, chunk.c_str(), bc, bcLen, 0);
    std::free(bc);
    if (status == 0) rt.rememberMain(m->id(), name, T);
    if (status != 0) { std::string e = lua_tostring(T, -1); lua_pop(L, 1); luaL_error(L, "%s", e.c_str()); }
    rt.moduleLoading.insert(m->id());
    auto ctx = std::make_shared<ScriptCtx>(); ctx->scriptId = m->id(); ctx->name = name;
    Task* t = rt.taskFor(T);          // so print()/errors inside attribute to the module
    t->ctx = ctx;
    rt.depth++;
    status = lua_resume(T, L, 0);
    rt.depth--;
    rt.moduleLoading.erase(m->id());
    if (status == LUA_YIELD) { rt.kill(t); lua_pop(L, 1); luaL_error(L, "Module %s yielded while loading; only synchronous module code is supported", name.c_str()); }
    if (status != LUA_OK) {
        std::string e = lua_tostring(T, -1) ? lua_tostring(T, -1) : "error";
        rt.kill(t); lua_pop(L, 1);
        luaL_error(L, "Requested module experienced an error while loading: %s", e.c_str());
    }
    if (lua_gettop(T) != 1) { rt.kill(t); lua_pop(L, 1); luaL_error(L, "Module code did not return exactly one value (%s)", name.c_str()); }
    lua_xmove(T, L, 1);
    rt.kill(t);
    lua_remove(L, -2);   // thread
    lua_pushvalue(L, -1);
    rt.moduleResults[m->id()] = lua_ref(L, -1);
    lua_pop(L, 1);
    return 1;
}

void openTaskApi(Runtime::Impl& rt) {
    lua_State* L = rt.L;
    lua_newtable(L);
    lua_pushcfunction(L, task_wait, "task.wait"); lua_setfield(L, -2, "wait");
    lua_pushcfunction(L, task_spawn, "task.spawn"); lua_setfield(L, -2, "spawn");
    lua_pushcfunction(L, task_defer, "task.defer"); lua_setfield(L, -2, "defer");
    lua_pushcfunction(L, task_delay, "task.delay"); lua_setfield(L, -2, "delay");
    lua_pushcfunction(L, task_cancel, "task.cancel"); lua_setfield(L, -2, "cancel");
    lua_pushcfunction(L, [](lua_State*) -> int { return 0; }, "task.synchronize"); lua_setfield(L, -2, "synchronize");
    lua_pushcfunction(L, [](lua_State*) -> int { return 0; }, "task.desynchronize"); lua_setfield(L, -2, "desynchronize");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "task");

    lua_pushcfunction(L, task_wait, "wait"); lua_setglobal(L, "wait");
    lua_pushcfunction(L, legacy_spawn, "spawn"); lua_setglobal(L, "spawn");
    lua_pushcfunction(L, legacy_delay, "delay"); lua_setglobal(L, "delay");
    lua_pushcfunction(L, g_tick, "tick"); lua_setglobal(L, "tick");
    lua_pushcfunction(L, g_time, "time"); lua_setglobal(L, "time");
    lua_pushcfunction(L, g_time, "elapsedTime"); lua_setglobal(L, "elapsedTime");
    lua_pushcfunction(L, g_print, "print"); lua_setglobal(L, "print");
    lua_pushcfunction(L, g_warn, "warn"); lua_setglobal(L, "warn");
    lua_pushcfunction(L, g_typeof, "typeof"); lua_setglobal(L, "typeof");
    lua_pushcfunction(L, g_require, "require"); lua_setglobal(L, "require");
    (void)kTaskCtx;
}

// ---- Runtime (public) --------------------------------------------------------------
Runtime::Runtime(Options opts, Callbacks cb) : impl_(std::make_unique<Impl>(this, std::move(opts), std::move(cb))) {}
Runtime::~Runtime() = default;
DataModel& Runtime::dataModel() { return impl_->dm; }
Sandbox& Runtime::sandbox() { return impl_->sb; }
const Runtime::Options& Runtime::options() const { return impl_->opts; }
const Runtime::Stats& Runtime::stats() const { return impl_->stats; }
double Runtime::now() const { return impl_->now; }
void Runtime::Impl::filterReplication() {
    const std::vector<Change>& log = dm.changes();
    if (repCursor < log.size()) {
        std::vector<Change> tail(log.begin() + repCursor, log.end());
        std::vector<Change> rep = replicator.filter(tail);
        if (replicating) pendingRep.insert(pendingRep.end(), rep.begin(), rep.end());
    }
    repCursor = log.size();
}

std::vector<Change> Runtime::takeChanges() {
    Impl& r = *impl_;
    if (r.dm.isServer()) r.filterReplication();
    r.repCursor = 0;
    return r.dm.takeChanges();
}

Instance* Runtime::localPlayer() const { return impl_->localPlayer; }

void Runtime::setReplicateAll(bool b) { impl_->replicator.setAll(b); }

// A placeholder draws as the cyan and magenta checkerboard Roblox uses for one.
void Runtime::Impl::placeholderFor(int64_t id, const std::string& className) {
    if (placeholders.count(id) || dm.find(id)) return;
    Instance::Ptr o = dm.createWithId(id, findClass(className) ? className : "EditableImage");
    if (!o) return;
    placeholders[id] = o;
    if (o->isA("EditableMesh")) {
        // a unit cube, its faces checkered cyan and magenta corner by corner
        EditableMeshData& m = editableMeshes[id];
        static const float c[8][3] = {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}, {-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}};
        static const int q[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {0, 4, 7, 3}, {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}};
        for (auto& p : c) { EditableMeshData::V v; v.p = {p[0], p[1], p[2]}; m.verts.push_back(v); }
        for (int f = 0; f < 6; f++) {
            EditableMeshData::N n; m.normals.push_back(n);
            uint32_t ns = (uint32_t)m.normals.size() - 1;
            for (int t = 0; t < 2; t++) {
                EditableMeshData::F tri;
                const int idx[3] = {q[f][0], q[f][t + 1], q[f][t + 2]};
                for (int k = 0; k < 3; k++) {
                    tri.v[k] = (uint32_t)idx[k]; tri.n[k] = ns;
                    EditableMeshData::U uv; m.uvs.push_back(uv); tri.u[k] = (uint32_t)m.uvs.size() - 1;
                    EditableMeshData::C col; col.c = ((idx[k] + f) % 2) ? Col3{1, 0, 1} : Col3{0, 1, 1}; m.colors.push_back(col); tri.c[k] = (uint32_t)m.colors.size() - 1;
                }
                m.faces.push_back(tri);
            }
        }
        m.placeholder = true;
        m.dirty = true;
    }
    if (o->isA("EditableImage")) {
        EditableImage& e = editableImages[id];
        e.w = e.h = 64;
        e.rgba.assign(64 * 64 * 4, 255);
        for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
            bool cyan = ((x / 8) + (y / 8)) % 2 == 0;
            uint8_t* p = &e.rgba[((size_t)y * 64 + x) * 4];
            p[0] = cyan ? 0 : 255; p[1] = cyan ? 255 : 0; p[2] = 255;
        }
        e.placeholder = true;
        e.dirty = true;
        o->setInternal("Size", Value::vector2(64, 64));
    }
}

std::vector<Runtime::MeshFrame> Runtime::takeEditableMeshes() {
    Impl& r = *impl_;
    std::vector<MeshFrame> out;
    for (auto& [id, m] : r.editableMeshes) {
        if (!m.dirty) continue;
        m.dirty = false;
        MeshFrame f; f.id = id;
        m.corners(f.pos, f.nrm, f.uv, f.rgba);
        out.push_back(std::move(f));
    }
    return out;
}

std::vector<Runtime::ImageFrame> Runtime::takeEditableImages() {
    Impl& r = *impl_;
    std::vector<ImageFrame> out;
    for (auto& [id, e] : r.editableImages) {
        if (!e.dirty) continue;
        e.dirty = false;
        ImageFrame f; f.id = id; f.w = e.w; f.h = e.h; f.rgba.assign((const char*)e.rgba.data(), e.rgba.size());
        if (e.destroyed) { f.w = f.h = 1; f.rgba.assign(4, '\0'); }   // what is left draws as nothing
        out.push_back(std::move(f));
    }
    return out;
}

void Runtime::setInProcess(bool b) { impl_->replicator.setInProcess(b); }


void Runtime::setReplicating(bool on) {
    Impl& r = *impl_;
    if (on && !r.replicating) r.filterReplication();   // the known set has to catch up first
    r.replicating = on;
    if (!on) r.pendingRep.clear();
}

std::vector<Change> Runtime::replicationJoin() {
    Impl& r = *impl_;
    r.filterReplication();
    return r.replicator.join();
}

std::vector<Change> Runtime::takeReplication() {
    Impl& r = *impl_;
    r.filterReplication();
    std::vector<Change> out;
    out.swap(r.pendingRep);
    return out;
}

void Runtime::applyReplication(const std::vector<Change>& changes) {
    Impl& r = *impl_;
    if (r.depth == 0) r.sb.beginFrame();
    std::string err;
    // WaitForChild wakes after the whole batch, as Roblox resumes a thread only
    // between its own steps: the new subtree is all there when the waiter looks.
    r.applyingBatch = true;
    // Properties a script rewrites with the value already held -- Play() on a playing Sound,
    // MoveTo to the point it is already walking to. setImpl drops such a write, so an arrival
    // that changes nothing is touched again here or the restart is lost.
    static const char* const kTouched[] = {"Playing", "TimePosition", "WalkToPoint", "MeshContent"};
    // One warning per batch: mid-motion poses ride the unreliable-ordered channel and routinely
    // land ahead of the part's Create, still in flight on the reliable one, so that is the
    // ordinary case and a warning per change would cost more than the loss.
    size_t unapplied = 0;
    std::set<int64_t> unknownIds;
    std::string firstErr;
    for (const Change& c : changes) {
        Instance* before = (c.kind == Change::Destroy || c.kind == Change::Parent) ? r.dm.find(c.id) : nullptr;
        Instance* parentBefore = before ? before->parent() : nullptr;
        if (c.kind == Change::Destroy && before && before->isA("Player") && parentBefore && parentBefore->className() == "Players")
            r.fireValues(*parentBefore, "PlayerRemoving", {Value::instance(c.id)});
        if (c.kind == Change::Destroy && before == r.localPlayer) r.localPlayer = nullptr;
        bool retouch = false;
        if (c.kind == Change::Property) {
            for (const char* n : kTouched) if (c.name == n) { retouch = true; break; }
            if (retouch) { Instance* held = r.dm.find(c.id); retouch = held && held->get(c.name) == c.value; }
        }
        if (!r.dm.apply(c, &err)) {
            unapplied++;
            unknownIds.insert(c.id);
            if (firstErr.empty()) firstErr = err + " (id " + std::to_string(c.id) + ", " + (c.kind == Change::Property ? c.name : c.kind == Change::Parent ? std::string("Parent") : c.kind == Change::Create ? std::string("Create") : std::string("Destroy")) + ")";
            continue;
        }
        Instance* i = r.dm.find(c.id);
        if (!i) continue;
        if (retouch) i->touch(c.name);
        // What Roblox fires on the client for things the server did.
        if (c.kind == Change::Parent && i->isA("Player") && i->parent() != parentBefore && i->parent() && i->parent()->className() == "Players")
            r.fireValues(*i->parent(), "PlayerAdded", {Value::instance(c.id)});
        if (c.kind == Change::Property && c.name == "Character" && i->isA("Player") && c.value.ref != 0) {
            if (i == r.localPlayer) {
                if (r.guiCharacter != 0 && r.guiCharacter != c.value.ref) r.resetGuis();   // a respawn, not the first spawn
                r.guiCharacter = c.value.ref;
            }
            // Only once the body it names is here: Character and the model it points at are
            // separate changes and need not arrive in one batch, so the handler would get nil.
            if (r.dm.find(c.value.ref)) r.fireValues(*i, "CharacterAdded", {c.value});
            else r.pendingCharacterAdded.push_back({c.id, c.value.ref});
        }
        if (c.kind == Change::Property && c.name == "Source" && i->isA("BaseScript") && r.started.count(c.id)) {
            r.stopScript(*i);                                   // the file changed on the server side: rerun
            if (r.shouldRun(*i)) r.queueStart(*i);
        }
    }
    if (unapplied && r.cb.warn)
        r.cb.warn("", "replication: " + std::to_string(unapplied) + " change(s) for " + std::to_string(unknownIds.size()) + " instance(s) could not be applied; first: " + firstErr);
    r.applyingBatch = false;
    // Any CharacterAdded that was waiting for its body, now that this batch has landed.
    for (size_t k = 0; k < r.pendingCharacterAdded.size();) {
        const auto [who, body] = r.pendingCharacterAdded[k];
        Instance* p = r.dm.find(who);
        auto forget = [&] { r.pendingCharacterAdded[k] = r.pendingCharacterAdded.back(); r.pendingCharacterAdded.pop_back(); };
        // Gone, or given a different body since: nobody is waiting for this one.
        if (!p || !p->isA("Player") || (int64_t)p->get("Character").ref != body) { forget(); continue; }
        if (!r.dm.find(body)) { k++; continue; }                 // still on its way
        forget();
        r.fireValues(*p, "CharacterAdded", {Value::instance(body)});
    }
    if (!r.childWaiters.empty()) r.checkChildWaiters();
    r.runDeferred();
}

std::vector<RemoteMsg> Runtime::takeRemotes() {
    std::vector<RemoteMsg> out;
    out.swap(impl_->outRemotes);
    return out;
}

// `reply` for the invoke wrapper: (ok, ...) -> a Result message. Upvalues: remote id, player id, call id.
static int remoteReply(lua_State* L) {
    Runtime::Impl& r = rtOf(L);
    RemoteMsg res; res.kind = RemoteMsg::Result;
    res.remote = (int64_t)lua_tonumber(L, lua_upvalueindex(1));
    res.player = (int64_t)lua_tonumber(L, lua_upvalueindex(2));
    res.call = (uint64_t)lua_tonumber(L, lua_upvalueindex(3));
    res.ok = lua_toboolean(L, 1);
    if (!res.ok) {
        const char* msg = lua_tostring(L, 2);
        res.args.push_back(NetValue::string(msg ? msg : "error"));
        Instance* remote = r.dm.find(res.remote);
        if (r.cb.error) r.cb.error(remote ? remote->fullName() : "RemoteFunction", msg ? msg : "error");
    } else {
        for (int i = 2; i <= lua_gettop(L); i++) res.args.push_back(r.toNetValue(L, i));
    }
    r.outRemotes.push_back(std::move(res));
    return 0;
}

void Runtime::deliverRemote(const RemoteMsg& m) {
    Impl& r = *impl_;
    if (r.depth == 0) r.sb.beginFrame();
    Instance* remote = r.dm.find(m.remote);
    bool server = r.dm.isServer();
    Instance* sender = server ? r.dm.find(m.player) : nullptr;
    switch (m.kind) {
    case RemoteMsg::Event: {
        if (!remote || remote->destroyed()) break;
        if (remote->isA("Players")) {
            // the menu's Reset Character: the sender's Humanoid dies, the server respawns it
            if (server && sender && !m.args.empty() && m.args[0].type == NetValue::String && m.args[0].s == "ResetCharacter") {
                Instance* character = r.dm.findRef(sender->get("Character").ref);
                Instance* h = character ? character->findFirstChildOfClass("Humanoid") : nullptr;
                if (h && h->get("Health").n > 0) h->set("Health", Value::number(0));
                break;
            }
            r.chatRemote(m, sender);                                    // chat (rbx_chat.cpp)
            break;
        }
        if (remote->isA("ClickDetector")) {            // a client's click / hover on it (see Runtime::input)
            if (server && sender && !m.args.empty() && m.args[0].type == NetValue::String && remote->cls().hasEvent(m.args[0].s))
                r.fireValues(*remote, m.args[0].s, {Value::instance(sender->id())});
            break;
        }
        if (remote->isA("ProximityPrompt")) {          // a client's key on it (see Runtime::Impl::triggerPrompt)
            if (!server || !sender || m.args.empty() || m.args[0].type != NetValue::String) break;
            const std::string& what = m.args[0].s;
            Instance* svc = r.dm.getService("ProximityPromptService");
            Value who = Value::instance(sender->id());
            if (what == "Triggered") {
                // The server's own check: enabled, and in range with 4 studs of slack for the
                // client's character running ahead of this copy.
                Instance* character = r.dm.findRef(sender->get("Character").ref);
                Instance* root = character ? character->findFirstChild("HumanoidRootPart") : nullptr;
                Instance* part = r.promptPart(*remote);
                if (!remote->get("Enabled").b || !root || !part || !svc->get("Enabled").b) break;
                Vec3 a = root->get("Position").v, b = part->get("Position").v;
                double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
                if (std::sqrt(dx * dx + dy * dy + dz * dz) > remote->get("MaxActivationDistance").n + 4) break;
                r.fireValues(*remote, "Triggered", {who});
                r.fireValues(*svc, "PromptTriggered", {Value::instance(remote->id()), who});
            } else if (what == "TriggerEnded") {
                r.fireValues(*remote, "TriggerEnded", {who});
                r.fireValues(*svc, "PromptTriggerEnded", {Value::instance(remote->id()), who});
            }
            break;
        }
        if (remote->isA("Tool")) {                     // the owning client equips / drops / activates it (see Runtime::input)
            if (!server || !sender || m.args.empty() || m.args[0].type != NetValue::String) break;
            const std::string& what = m.args[0].s;
            Instance* character = r.dm.findRef(sender->get("Character").ref);
            Instance* backpack = sender->findFirstChildOfClass("Backpack");
            bool inChar = character && remote->parent() == character, inPack = backpack && remote->parent() == backpack;
            if (what == "Equip" && inPack && character) r.equipTool(*character, *remote);
            else if (what == "Unequip" && inChar) r.unequipTools(*character);
            else if (what == "Drop" && inChar && remote->get("CanBeDropped").b) r.dropTool(*character, *remote);
            else if ((what == "Activated" || what == "Deactivated") && inChar) r.fireValues(*remote, what, {});
            break;
        }
        Signal* s = r.findSignal(*remote, server ? "OnServerEvent" : "OnClientEvent");
        if (!s || !s->hasConns()) {
            // Nothing listening yet. Roblox keeps 256 firings a remote and hands them over
            // when a handler arrives; so does this.
            if (!m.queued && remote->isA("RemoteEvent")) {
                auto& q = r.remoteQueue[remote->id()];
                if (q.size() < 256) { q.push_back(m); q.back().queued = true; }
                else if (r.remoteQueueWarned.insert(remote->id()).second)
                    reportWarn(remote->fullName(), "RemoteEvent invocation queue exhausted for " + remote->fullName() + "; did you forget to implement " + std::string(server ? "OnServerEvent" : "OnClientEvent") + "?");
            }
            break;
        }
        r.fire(*s, [&](lua_State* co) {
            int n = 0;
            if (server) { r.pushInstance(co, sender); n++; }
            for (auto& a : m.args) { r.pushNetValue(co, a); n++; }
            return n;
        });
        break;
    }
    case RemoteMsg::Invoke: {
        const char* cbName = server ? "OnServerInvoke" : "OnClientInvoke";
        const Callback* cb = nullptr;
        if (remote && !remote->destroyed() && remote->binding) {
            auto& cbs = r.binding(*remote).callbacks;
            auto it = cbs.find(cbName);
            if (it != cbs.end()) cb = &it->second;
        }
        if (!cb) {
            RemoteMsg res; res.kind = RemoteMsg::Result; res.remote = m.remote; res.player = m.player; res.call = m.call; res.ok = false;
            res.args.push_back(NetValue::string((remote ? remote->fullName() : std::string("RemoteFunction")) + ": " + cbName + " is not set"));
            r.outRemotes.push_back(std::move(res));
            break;
        }
        Task* t = r.newTask(nullptr, cb->ctx);
        lua_getref(t->co, r.invokeWrapperRef);
        lua_pushnumber(t->co, (double)m.remote); lua_pushnumber(t->co, (double)m.player); lua_pushnumber(t->co, (double)m.call);
        lua_pushcclosure(t->co, remoteReply, "reply", 3);
        lua_getref(t->co, cb->ref);
        int n = 2;
        if (server) { r.pushInstance(t->co, sender); n++; }
        for (auto& a : m.args) { r.pushNetValue(t->co, a); n++; }
        r.resume(t, n);
        break;
    }
    case RemoteMsg::Result: {
        auto it = r.pendingInvokes.find(m.call);
        if (it == r.pendingInvokes.end()) break;
        Task* t = it->second;
        r.pendingInvokes.erase(it);
        if (m.ok) {
            for (auto& a : m.args) r.pushNetValue(t->co, a);
            r.resume(t, (int)m.args.size());
        } else {
            const std::string& msg = m.args.empty() ? std::string("error") : m.args[0].s;
            lua_pushlstring(t->co, msg.data(), msg.size());
            r.resume(t, 1, true);
        }
        break;
    }
    }
    r.runDeferred();
}
// One content of a PreloadAsync list settled: the callback hears, and the caller goes on once
// the group's count reaches zero.
void Runtime::deliverPreload(uint64_t id, bool ok) {
    Impl& r = *impl_;
    auto it = r.pendingPreload.find(id);
    if (it == r.pendingPreload.end()) return;
    Impl::PendingPreload pending = std::move(it->second);
    r.pendingPreload.erase(it);
    Instance* cp = r.contentProviderId ? r.dm.find(r.contentProviderId) : nullptr;
    if (cp) cp->setInternal("RequestQueueSize", Value::number((double)r.pendingPreload.size()));
    // What GetAssetFetchStatus answers from now on; its changed signal and, on a failure, AssetFetchFailed hear it.
    {
        const EnumDef* e = findEnum("AssetFetchStatus");
        const EnumItem* item = e->find(ok ? "Success" : "Failure");
        r.fetchStatus[pending.uri] = item->value;
        if (cp) {
            if (Signal* s = r.findSignal(*cp, "Fetch:" + pending.uri)) if (s->hasConns())
                r.fire(*s, [&](lua_State* co) { pushEnumItem(co, e, item); return 1; });
            if (!ok) if (Signal* s = r.findSignal(*cp, "AssetFetchFailed")) if (s->hasConns())
                r.fire(*s, [&](lua_State* co) { lua_pushlstring(co, pending.uri.data(), pending.uri.size()); return 1; });
        }
    }
    auto git = r.preloadGroups.find(pending.group);
    if (git == r.preloadGroups.end()) return;
    Impl::PreloadGroup& g = git->second;
    if (g.cbRef != LUA_NOREF) {
        Task* ct = r.newTask(nullptr, g.ctx);
        lua_getref(ct->co, g.cbRef);
        lua_pushlstring(ct->co, pending.uri.data(), pending.uri.size());
        const EnumDef* e = findEnum("AssetFetchStatus");
        pushEnumItem(ct->co, e, e->find(ok ? "Success" : "Failure"));
        r.resume(ct, 2);
    }
    if (--g.left <= 0) {
        Task* t = g.task;
        if (g.cbRef != LUA_NOREF) lua_unref(r.L, g.cbRef);
        r.preloadGroups.erase(git);
        if (t && t->state == Task::Parked) r.resume(t, 0);
    }
    r.runDeferred();
}

// The host decoded the image CreateEditableImageAsync asked for, or could not.
void Runtime::deliverImage(uint64_t id, const ImageResult& result) {
    Impl& r = *impl_;
    auto it = r.pendingImages.find(id);
    if (it == r.pendingImages.end()) return;
    Task* t = it->second;
    r.pendingImages.erase(it);
    lua_State* L = t->co;
    if (!result.ok || result.w <= 0 || result.h <= 0 || result.rgba.size() < (size_t)result.w * result.h * 4) {
        const std::string msg = result.error.empty() ? "CreateEditableImageAsync: the image could not be loaded" : result.error;
        lua_pushlstring(L, msg.data(), msg.size());
        r.resume(t, 1, true);
        r.runDeferred();
        return;
    }
    Instance::Ptr img = r.dm.createInternal("EditableImage");
    img->setInternal("Size", Value::vector2((float)result.w, (float)result.h));
    auto& e = r.editableImages[img->id()];
    e.w = result.w; e.h = result.h;
    e.rgba.assign((const uint8_t*)result.rgba.data(), (const uint8_t*)result.rgba.data() + (size_t)result.w * result.h * 4);
    e.dirty = true;
    r.pushInstance(L, img.get());
    r.resume(t, 1);
    r.runDeferred();
}

// CreateMeshPartAsync / CreateEditableMeshAsync answered. The part comes back unparented and
// sized to the mesh's own bounds; a failure throws in the calling script, as on Roblox.
void Runtime::deliverMesh(uint64_t id, const MeshResult& result) {
    Impl& r = *impl_;
    auto it = r.pendingMeshPart.find(id);
    if (it == r.pendingMeshPart.end()) return;
    Impl::PendingMeshPart pending = std::move(it->second);
    r.pendingMeshPart.erase(it);
    Task* t = pending.task;
    lua_State* L = t->co;
    if (pending.editable) {
        EditableMeshData data;
        if (!result.ok || !data.deserialize(result.geometry)) {
            const std::string msg = result.error.empty() ? "CreateEditableMeshAsync: could not load the mesh " + pending.uri : result.error;
            lua_pushlstring(L, msg.data(), msg.size());
            r.resume(t, 1, true);
            r.runDeferred();
            return;
        }
        if (data.liveVerts() > 60000 || data.liveFaces() > 20000) {
            lua_pushstring(L, "CreateEditableMeshAsync: the mesh is past 60,000 vertices or 20,000 triangles");
            r.resume(t, 1, true);
            r.runDeferred();
            return;
        }
        Instance::Ptr mesh = r.dm.createInternal("EditableMesh");
        data.fixedSize = pending.fixedSize;
        data.dirty = true;
        r.editableMeshes[mesh->id()] = std::move(data);
        mesh->setInternal("FixedSize", Value::boolean(pending.fixedSize));
        r.pushInstance(L, mesh.get());
        r.resume(t, 1);
        r.runDeferred();
        return;
    }
    Instance::Ptr part = result.ok ? r.dm.createInternal("MeshPart") : nullptr;
    if (!part) {
        const std::string msg = result.error.empty() ? "CreateMeshPartAsync: could not load the mesh " + pending.uri : result.error;
        lua_pushlstring(L, msg.data(), msg.size());
        r.resume(t, 1, true);
        r.runDeferred();
        return;
    }
    part->setInternal("MeshContent", Value::contentUri(pending.uri));
    part->setInternal("Size", Value::vector3(result.size));
    part->setInternal("MeshSize", Value::vector3(result.size));
    part->setInternal("CollisionFidelity", Value::string(pending.collision));
    part->setInternal("RenderFidelity", Value::string(pending.render));
    part->setInternal("FluidFidelity", Value::string(pending.fluid));
    r.pushInstance(L, part.get());
    r.resume(t, 1);
    r.runDeferred();
}

// The host finished a boolean. The result comes back unparented and wearing the look of the part
// it was called on, as on Roblox; GeometryService's callers get a table, one entry per piece.
void Runtime::deliverSolid(uint64_t id, const SolidResult& result) {
    Impl& r = *impl_;
    auto it = r.pendingSolid.find(id);
    if (it == r.pendingSolid.end()) return;      // the script died while the host was cutting
    Impl::PendingSolid pending = std::move(it->second);
    r.pendingSolid.erase(it);
    Task* t = pending.task;
    lua_State* L = t->co;
    if (!result.ok || result.pieces.empty()) {
        const std::string msg = result.error.empty() ? std::string("The solid modeling operation failed") : result.error;
        lua_pushlstring(L, msg.data(), msg.size());
        r.resume(t, 1, true);
        r.runDeferred();
        return;
    }
    const bool meshPart = pending.resultClass == "MeshPart";
    std::vector<Instance::Ptr> made;
    for (const SolidResult::Piece& piece : result.pieces) {
        Instance::Ptr u = r.dm.createInternal(pending.resultClass);
        if (!u) continue;
        if (!pending.resultName.empty()) u->setInternal("Name", Value::string(pending.resultName));
        for (const auto& [prop, value] : pending.look) u->setInternal(prop, value);
        // White, as on Roblox: Color multiplies the mesh's own face colours.
        if (meshPart) u->setInternal("Color", Value::color3(1, 1, 1));
        u->setInternal("MeshData", Value::string(piece.meshData));
        u->setInternal("Size", Value::vector3(piece.size));
        u->setInternal("Position", Value::vector3(piece.pos));
        u->setInternal("Orientation", Value::vector3(piece.orient));
        u->setInternal("MeshSize", Value::vector3(piece.size));
        u->setInternal("CollisionFidelity", Value::string(pending.collision));
        u->setInternal("RenderFidelity", Value::string(pending.render));
        u->setInternal("FluidFidelity", Value::string(pending.fluid));
        if (!meshPart) u->setInternal("TriangleCount", Value::number(piece.triangles));
        made.push_back(u);
    }
    if (made.empty()) {
        lua_pushstring(L, "The solid modeling operation failed");
        r.resume(t, 1, true);
        r.runDeferred();
        return;
    }
    if (pending.wantsTable) {
        lua_createtable(L, (int)made.size(), 0);
        for (size_t k = 0; k < made.size(); k++) {
            r.pushInstance(L, made[k].get());
            lua_rawseti(L, -2, (int)k + 1);
        }
    } else {
        r.pushInstance(L, made[0].get());
    }
    r.resume(t, 1);
    r.runDeferred();
}

// Two shapes, as on Roblox: Get/PostAsync get the body as a string, RequestAsync a table with
// the status and headers. Task::httpWantsTable says which -- the parked coroutine is the same.
void Runtime::deliverHttp(uint64_t id, bool ok, int status, const std::string& statusText,
                          const std::string& body,
                          const std::vector<std::pair<std::string, std::string>>& headers) {
    Impl& r = *impl_;
    auto it = r.pendingHttp.find(id);
    if (it == r.pendingHttp.end()) return;      // the script died while it was in flight
    Task* t = it->second;
    r.pendingHttp.erase(it);
    if (!ok) {
        // Raised inside the parked script, as on Roblox, so pcall catches it and an uncaught
        // one names the calling line.
        const std::string msg = statusText.empty() ? std::string("HttpService: the request failed") : statusText;
        lua_pushlstring(t->co, msg.data(), msg.size());
        r.resume(t, 1, true);
        r.runDeferred();
        return;
    }
    if (t->httpWantsTable) {
        lua_State* L = t->co;
        lua_createtable(L, 0, 4);
        lua_pushboolean(L, status >= 200 && status < 300); lua_setfield(L, -2, "Success");
        lua_pushinteger(L, status);                        lua_setfield(L, -2, "StatusCode");
        lua_pushlstring(L, statusText.data(), statusText.size()); lua_setfield(L, -2, "StatusMessage");
        lua_pushlstring(L, body.data(), body.size());      lua_setfield(L, -2, "Body");
        lua_createtable(L, 0, (int)headers.size());
        for (const auto& h : headers) {
            lua_pushlstring(L, h.second.data(), h.second.size());
            lua_setfield(L, -2, h.first.c_str());
        }
        lua_setfield(L, -2, "Headers");
        r.resume(t, 1);
    } else {
        // The body is all that comes back, so a non-2xx is an error here as on Roblox:
        // otherwise a 404's HTML would pass for the answer.
        if (status < 200 || status >= 300) {
            const std::string msg = "HttpService: HTTP " + std::to_string(status)
                                  + (statusText.empty() ? std::string() : " (" + statusText + ")");
            lua_pushlstring(t->co, msg.data(), msg.size());
            r.resume(t, 1, true);
            r.runDeferred();
            return;
        }
        lua_pushlstring(t->co, body.data(), body.size());
        r.resume(t, 1);
    }
    r.runDeferred();
}

Runtime* Runtime::from(lua_State* L) { return static_cast<Runtime*>(Sandbox::from(L)->userData()); }

// ---- the Studio's plugins ------------------------------------------------------------
void Runtime::addPlugin(const std::string& name, const std::string& source) {
    Impl& r = *impl_;
    Instance* svc = r.dm.getService("PluginDebugService");
    Instance::Ptr s = r.dm.create("Script", svc);
    s->setName(name);
    s->set("Source", Value::string(source));
    r.pluginRoots.insert(s->id());
    r.queueStart(*s);
}

void Runtime::addPluginFile(const std::string& file, const std::string& source) {
    Impl& r = *impl_;
    Instance* svc = r.dm.getService("PluginDebugService");
    std::string err;
    r.pluginFilesAllowed = true;
    Instance* made = loadSourceFile(*this, "PluginDebugService/" + file, source, &err);
    r.pluginFilesAllowed = false;
    if (!made) { reportError("PluginDebugService/" + file, err); return; }
    for (Instance* p = made; p; p = p->parent())
        if (p->parent() == svc) { r.pluginRoots.insert(p->id()); break; }
}

void Runtime::unloadPlugins() {
    Impl& r = *impl_;
    for (auto& [root, pl] : r.pluginOfRoot)
        if (pl->binding) if (Signal* s = r.findSignal(*pl, "Unloading")) if (s->hasConns()) r.fireValues(*pl, "Unloading", {});
    if (Instance* svc = r.dm.getService("PluginDebugService")) {
        auto kids = svc->children();
        for (auto& c : kids) c->destroy();
    }
    r.pluginOfRoot.clear(); r.pluginRoots.clear(); r.toolbarPlugin.clear(); r.pluginMouse.clear(); r.pluginObjects.clear();
    r.pluginButtons.clear(); r.pluginButtonsDirty = true;
    if (r.activePlugin) { r.activePlugin = nullptr; r.pluginActiveDirty = true; }
}

void Runtime::setPluginSetting(const std::string& plugin, const std::string& key, const std::string& json) { impl_->pluginSettings[plugin][key] = json; }

void Runtime::pluginButtonClick(int64_t buttonId) {
    Impl& r = *impl_;
    for (auto& o : r.pluginObjects)
        if (o->id() == buttonId) { if (o->binding) if (Signal* s = r.findSignal(*o, "Click")) if (s->hasConns()) r.fireValues(*o, "Click", {}); return; }
}

void Runtime::setSelection(const std::vector<int64_t>& ids) {
    Impl& r = *impl_;
    if (ids == r.selection) return;
    r.selection = ids;
    Instance* sel = r.dm.getService("Selection");
    if (sel && sel->binding) if (Signal* s = r.findSignal(*sel, "SelectionChanged")) if (s->hasConns()) r.fireValues(*sel, "SelectionChanged", {});
}

void Runtime::pluginInput(const UserInput& in, Vec3 origin, Vec3 direction) {
    Impl& r = *impl_;
    r.pluginRay = {origin, direction}; r.pluginRayValid = true;
    if (!r.activePlugin) return;
    auto it = r.pluginMouse.find(r.activePlugin->id());
    if (it == r.pluginMouse.end()) return;
    Instance& m = *it->second;
    m.setInternal("X", Value::number(in.position.x));
    m.setInternal("Y", Value::number(in.position.y));
    const char* mev = in.type == "MouseMovement" ? "Move"
                    : in.type == "MouseWheel" ? (in.position.z > 0 ? "WheelForward" : "WheelBackward")
                    : in.type == "MouseButton1" ? (in.state == "Begin" ? "Button1Down" : "Button1Up")
                    : in.type == "MouseButton2" ? (in.state == "Begin" ? "Button2Down" : "Button2Up") : nullptr;
    if (mev && m.binding) if (Signal* s = r.findSignal(m, mev)) if (s->hasConns()) r.fireValues(m, mev, {});
}

bool Runtime::takePluginButtons(std::vector<PluginButton>& out) {
    Impl& r = *impl_;
    if (!r.pluginButtonsDirty) return false;
    r.pluginButtonsDirty = false;
    out.clear();
    for (auto& b : r.pluginButtons) {
        PluginButton pb;
        pb.id = b.id; pb.toolbar = b.toolbarName; pb.buttonId = b.buttonId; pb.tooltip = b.tooltip; pb.icon = b.icon; pb.text = b.text; pb.active = b.active; pb.enabled = b.enabled;
        auto pit = r.toolbarPlugin.find(b.toolbar);
        if (pit != r.toolbarPlugin.end()) pb.plugin = pit->second->name();
        out.push_back(pb);
    }
    return true;
}

bool Runtime::takeSelectionRequest(std::vector<int64_t>& out) {
    Impl& r = *impl_;
    if (!r.selectionRequested) return false;
    r.selectionRequested = false;
    out = r.selection;
    return true;
}

std::vector<Runtime::PluginSetting> Runtime::takePluginSettings() {
    Impl& r = *impl_;
    std::vector<PluginSetting> out;
    for (auto& w : r.pluginSettingWrites) out.push_back({w.plugin, w.key, w.json});
    r.pluginSettingWrites.clear();
    return out;
}

bool Runtime::takePluginActive(std::string& plugin, bool& active, bool& exclusive) {
    Impl& r = *impl_;
    if (!r.pluginActiveDirty) return false;
    r.pluginActiveDirty = false;
    plugin = r.activePlugin ? r.activePlugin->name() : "";
    active = r.activePlugin != nullptr;
    exclusive = r.activeExclusive;
    return true;
}

std::vector<std::string> Runtime::takePluginWaypoints() {
    std::vector<std::string> out = std::move(impl_->pluginWaypoints);
    impl_->pluginWaypoints.clear();
    return out;
}

std::vector<std::pair<int64_t, int>> Runtime::takeOpenScripts() {
    std::vector<std::pair<int64_t, int>> out = std::move(impl_->openScriptRequests);
    impl_->openScriptRequests.clear();
    return out;
}

int Runtime::startScripts() {
    Impl& r = *impl_;
    int n = 0;
    std::vector<int64_t> pending; pending.swap(r.pendingStart);
    for (Instance* d : r.dm.root()->getDescendants())
        if (d->isA("BaseScript") && !r.started.count(d->id()) && r.shouldRun(*d)) pending.push_back(d->id());
    for (int64_t id : pending) {
        Instance* s = r.dm.find(id);
        if (!s || r.started.count(id) || !r.shouldRun(*s)) continue;
        r.sb.beginFrame();
        r.startScript(*s);
        n++;
    }
    r.runDeferred();
    return n;
}

void Runtime::step(double dt) {
    Impl& r = *impl_;
    if (r.pausedTask) return;   // stopped in the debugger: nothing moves, time does not pass
    // Remote events kept for a handler go out the frame one is there, in order.
    if (!r.remoteQueue.empty()) {
        std::vector<int64_t> ready;
        for (auto& [id, q] : r.remoteQueue) {
            Instance* inst = r.dm.find(id);
            if (!inst || inst->destroyed()) { ready.push_back(id); continue; }
            Signal* s = r.findSignal(*inst, r.dm.isServer() ? "OnServerEvent" : "OnClientEvent");
            if (s && s->hasConns()) ready.push_back(id);
        }
        for (int64_t id : ready) {
            auto it = r.remoteQueue.find(id);
            if (it == r.remoteQueue.end()) continue;
            std::vector<RemoteMsg> q = std::move(it->second);
            r.remoteQueue.erase(it);
            for (const RemoteMsg& qm : q) deliverRemote(qm);
        }
    }
    auto t0 = std::chrono::steady_clock::now();
    r.now += dt;
    r.sb.beginFrame();
    r.stepScriptMs.clear();
    for (int ref : r.pendingUnref) lua_unref(r.L, ref);
    r.pendingUnref.clear();

    if (!r.pendingStart.empty()) startScripts();

    Instance* rs = r.dm.getService("RunService");
    if (Signal* s = r.findSignal(*rs, "Stepped")) if (s->hasConns())
        r.fire(*s, [&](lua_State* co) { lua_pushnumber(co, r.now); lua_pushnumber(co, dt); return 2; });
    if (!r.dm.isServer()) {
        for (auto& [name, ref] : std::vector<std::pair<std::string, int>>(r.renderSteps)) {
            Task* t = r.newTask(nullptr, nullptr);
            lua_getref(t->co, ref); lua_pushnumber(t->co, dt);
            r.resume(t, 1);
        }
        if (Signal* s = r.findSignal(*rs, "RenderStepped")) if (s->hasConns())
            r.fire(*s, [&](lua_State* co) { lua_pushnumber(co, dt); return 1; });
        if (Signal* s = r.findSignal(*rs, "PreRender")) if (s->hasConns())
            r.fire(*s, [&](lua_State* co) { lua_pushnumber(co, dt); return 1; });
    }
    r.runDeferred();
    r.wakeSleepers();
    r.runReady();
    r.runDeferred();
    r.checkChildWaiters();
    r.checkQueueWaiters();
    r.lastDt = dt;
    r.stepTweens(dt);
    r.stepPaths(dt);
    r.checkRespawns();
    r.updateSounds(dt);
    r.updateAudio(dt);
    if (r.dm.isServer()) r.stepMotors(dt);
    if (!r.dm.isServer()) r.updatePrompts(dt);
    if (Signal* s = r.findSignal(*rs, "Heartbeat")) if (s->hasConns())
        r.fire(*s, [&](lua_State* co) { lua_pushnumber(co, dt); return 1; });
    if (Signal* s = r.findSignal(*rs, "PostSimulation")) if (s->hasConns())
        r.fire(*s, [&](lua_State* co) { lua_pushnumber(co, dt); return 1; });
    r.runDeferred();
    r.dm.workspace()->setInternal("DistributedGameTime", Value::number(r.now));
    r.saveDataStores();
    r.stats.lastStepMillis = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    r.stats.memoryNow = r.sb.memoryNow();
    r.stats.memoryPeak = r.sb.memoryPeak();
    // A step past 30 ms names the scripts that spent the time, costliest first.
    if (r.stats.lastStepMillis >= 30.0 && r.cb.print) {
        std::vector<std::pair<std::string, double>> rows(r.stepScriptMs.begin(), r.stepScriptMs.end());
        std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.second > b.second; });
        std::string line = "a slow step: " + std::to_string((int)r.stats.lastStepMillis) + " ms; the costly scripts:";
        for (size_t i = 0; i < rows.size() && i < 4; i++)
            if (rows[i].second >= 1.0) line += " " + rows[i].first + " " + std::to_string((int)rows[i].second) + " ms;";
        r.cb.print(r.dm.isServer() ? "Runtime" : "Runtime (client)", line);
    }
}

void Runtime::reloadSource(Instance& s, const std::string& source) {
    Impl& r = *impl_;
    s.set("Source", Value::string(source));
    if (s.isA("BaseScript")) {
        r.stopScript(s);
        if (r.shouldRun(s)) r.queueStart(s);
    } else {
        // Every cached result, not just this module's: one that required the changed module
        // must be re-evaluated too. Running scripts keep the tables they already hold.
        for (auto& [id, ref] : r.moduleResults) lua_unref(r.L, ref);
        r.moduleResults.clear();
    }
}

void Runtime::reportError(const std::string& script, const std::string& error) {
    if (impl_->cb.error) impl_->cb.error(script, error);
}
void Runtime::reportWarn(const std::string& script, const std::string& text) {
    if (impl_->cb.warn) impl_->cb.warn(script, text);
}

bool Runtime::hostWrite(int64_t id, const std::string& prop, const Value& v) {
    Instance* i = impl_->dm.find(id);
    if (!i || i->destroyed()) return false;
    impl_->dm.setHostWriting(true);
    bool ok = i->setInternal(prop, v);
    impl_->dm.setHostWriting(false);
    return ok;
}

bool Runtime::hostWriteQuiet(int64_t id, const std::string& prop, const Value& v) {
    Instance* i = impl_->dm.find(id);
    if (!i || i->destroyed()) return false;
    impl_->dm.setHostWriting(true);
    impl_->dm.setQuiet(true);
    bool ok = i->setInternal(prop, v);
    impl_->dm.setQuiet(false);
    impl_->dm.setHostWriting(false);
    return ok;
}

void Runtime::fireEvent(Instance& inst, const std::string& event, const std::vector<Value>& args) {
    Impl& r = *impl_;
    if (r.depth == 0) r.sb.beginFrame();
    if (event == "Touched" && inst.isA("SpawnLocation") && !args.empty()) r.teamTouch(inst, r.dm.find(args[0].ref));
    if (event == "Touched" && (inst.isA("Seat") || inst.isA("VehicleSeat")) && !args.empty()) r.seatTouch(inst, r.dm.find(args[0].ref));
    r.fireValues(inst, event, args);
    // GetMarkerReachedSignal: every KeyframeMarker under the keyframe reached fires its own, with its Value
    if (event == "KeyframeReached" && inst.isA("AnimationTrack") && !args.empty() && inst.binding)
        if (Instance* seq = animationSequenceOf(r, r.dm.findRef(inst.get("Animation").ref)))
            for (auto& k : seq->children()) if (k->isA("Keyframe") && k->name() == args[0].s)
                for (auto& mk : k->children()) if (mk->isA("KeyframeMarker"))
                    if (Signal* s = r.findSignal(inst, "Marker:" + mk->name())) if (s->hasConns()) r.fireValues(inst, "Marker:" + mk->name(), {mk->get("Value")});
    r.runDeferred();
}

std::string Runtime::runChunk(const std::string& chunkname, const std::string& source) {
    Impl& r = *impl_;
    lua_CompileOptions copts = userCompileOptions();
    size_t bcLen = 0;
    char* bc = luau_compile(source.c_str(), source.size(), &copts, &bcLen);
    auto ctx = std::make_shared<ScriptCtx>(); ctx->name = chunkname; ctx->host = true;
    Task* t = r.newTask(nullptr, ctx);
    luaL_sandboxthread(t->co);
    std::string chunk = "=" + chunkname;
    int status = luau_load(t->co, chunk.c_str(), bc, bcLen, 0);
    std::free(bc);
    if (status != 0) { std::string e = lua_tostring(t->co, -1); r.kill(t); if (r.cb.error) r.cb.error(chunkname, e); return e; }
    std::string err;
    auto oldErr = r.cb.error;
    r.cb.error = [&](const std::string& who, const std::string& e) { err = e; if (oldErr) oldErr(who, e); };
    r.sb.beginFrame();
    r.resume(t, 0);
    r.runDeferred();
    r.cb.error = oldErr;
    return err;
}

std::string Runtime::eval(const std::string& expr, std::string* error) {
    Impl& r = *impl_;
    std::string src = "return tostring((" + expr + "))";
    lua_CompileOptions copts = userCompileOptions();
    size_t bcLen = 0;
    char* bc = luau_compile(src.c_str(), src.size(), &copts, &bcLen);
    lua_State* T = lua_newthread(r.L);
    luaL_sandboxthread(T);
    { auto ctx = std::make_shared<ScriptCtx>(); ctx->name = "eval"; ctx->host = true; r.threadCtx[T] = std::move(ctx); }   // the debugger's watch is the host's
    int status = luau_load(T, "=eval", bc, bcLen, 0);
    std::free(bc);
    std::string out;
    if (status != 0) { if (error) *error = lua_tostring(T, -1); lua_pop(r.L, 1); return ""; }
    r.sb.beginFrame();
    r.depth++;
    status = lua_resume(T, nullptr, 0);
    r.depth--;
    if (status != LUA_OK) { if (error) *error = lua_tostring(T, -1) ? lua_tostring(T, -1) : "error"; }
    else if (lua_gettop(T) >= 1) out = lua_tostring(T, -1) ? lua_tostring(T, -1) : "";
    lua_pop(r.L, 1);
    return out;
}

// A UserId nobody in the town holds: TextChannel:AddUserAsync hands back the existing TextSource
// for an id, so two players on one number would share it and each other's lines. A client that
// claims none gets 1, 2, 3 in join order; server only, a client takes its id off its Player.
static int64_t freeUserId(Instance* players, int64_t claimed) {
    auto taken = [&](int64_t id) {
        for (auto& c : players->children())
            if (c->isA("Player") && (int64_t)c->get("UserId").n == id) return true;
        return false;
    };
    int64_t id = claimed > 0 ? claimed : 1;
    while (taken(id)) id++;
    return id;
}

Instance* Runtime::addPlayer(const std::string& name, int64_t userId, int64_t playerId) {
    Impl& r = *impl_;
    Instance* players = r.dm.getService("Players");
    Instance::Ptr p;
    if (!r.dm.isServer()) {   // the server made it and replication delivered it; this side only becomes it
        if (playerId) { for (auto& c : players->children()) if (c->id() == playerId) { p = c; break; } }
        else for (auto& c : players->children()) if (c->isA("Player") && c->name() == name) { p = c; break; }
    }
    bool fresh = !p;
    if (fresh) {
        if (r.dm.isServer()) userId = freeUserId(players, userId);
        p = r.dm.createInternal("Player");
        p->setName(name);
        p->set("DisplayName", Value::string(name));
        p->setInternal("UserId", Value::number((double)userId));
        // StarterPlayer's defaults become the player's own.
        if (Instance* sp = r.dm.getService("StarterPlayer")) {
            p->set("DevComputerMovementMode", sp->get("DevComputerMovementMode"));
            static const char* const kSeeds[][2] = {
                {"AutoJumpEnabled", "AutoJumpEnabled"}, {"CameraMaxZoomDistance", "CameraMaxZoomDistance"}, {"CameraMinZoomDistance", "CameraMinZoomDistance"},
                {"CameraMode", "CameraMode"}, {"DevCameraOcclusionMode", "DevCameraOcclusionMode"}, {"DevComputerCameraMode", "DevComputerCameraMovementMode"},
                {"DevEnableMouseLock", "EnableMouseLockOption"}, {"DevTouchCameraMode", "DevTouchCameraMovementMode"}, {"DevTouchMovementMode", "DevTouchMovementMode"},
                {"HealthDisplayDistance", "HealthDisplayDistance"}, {"NameDisplayDistance", "NameDisplayDistance"}};
            for (auto& seed : kSeeds) p->set(seed[0], sp->get(seed[1]));
        }
        for (const char* c : {"PlayerScripts", "PlayerGui", "Backpack", "StarterGear"}) { auto k = r.dm.createInternal(c); k->setName(c); k->setParent(p.get()); }
    }
    if (!r.dm.isServer() && !r.localPlayer) {
        r.localPlayer = p.get();
        players->setInternal("LocalPlayer", Value::instance(p->id()));
        // StarterPlayerScripts -> PlayerScripts, StarterGui -> PlayerGui
        auto copyInto = [&](Instance* from, Instance* to) {
            if (!from || !to) return;
            for (auto& c : from->children()) { Instance::Ptr cp = c->clone(); if (cp) cp->setParent(to); }
        };
        copyInto(r.dm.getService("StarterPlayer")->findFirstChildOfClass("StarterPlayerScripts"), p->findFirstChildOfClass("PlayerScripts"));
        copyInto(r.dm.getService("StarterGui"), p->findFirstChildOfClass("PlayerGui"));
        r.guiCharacter = (int64_t)p->get("Character").ref;
        // StarterPack -> Backpack is the server's, on spawn; those Tools may already be here.
        r.refreshHotbar();
    }
    if (fresh) p->setParent(players);
    r.sb.beginFrame();
    if (fresh && r.dm.isServer()) {
        r.playerChannels(*p, true);   // a TextSource in each default TextChannel
        // Teams: the AutoAssignable one with the fewest players, as on Roblox.
        Instance* pick = nullptr; size_t fewest = 0;
        for (auto& t : r.dm.getService("Teams")->children()) {
            if (!t->isA("Team") || !t->get("AutoAssignable").b) continue;
            size_t n = 0;
            for (auto& q : players->children()) if (q->isA("Player") && q->get("Team").ref == t->id()) n++;
            if (!pick || n < fewest) { pick = t.get(); fewest = n; }
        }
        if (pick) p->set("Team", Value::instance(pick->id()));
    }
    if (fresh) r.fireValues(*players, "PlayerAdded", {Value::instance(p->id())});
    // No characters in an edit world hosting Team Create, and none with CharacterAutoLoads off:
    // the place calls LoadCharacterAsync when it is ready, as on Roblox.
    if (r.dm.isServer() && r.opts.runScripts && players->get("CharacterAutoLoads").b) r.spawnCharacter(*p);
    startScripts();
    return p.get();
}

// ScreenGui.ResetOnSpawn: on a respawn the PlayerGui's LayerCollectors that reset are destroyed,
// their LocalScripts with them, and StarterGui's copies made again.
void Runtime::Impl::resetGuis() {
    Instance* pg = localPlayer ? localPlayer->findFirstChildOfClass("PlayerGui") : nullptr;
    if (!pg) return;
    std::vector<Instance*> gone;
    for (auto& c : pg->children()) if (c->isA("LayerCollector") && c->get("ResetOnSpawn").b) gone.push_back(c.get());
    for (Instance* g : gone) g->destroy();
    for (auto& c : dm.getService("StarterGui")->children()) {
        if (!c->isA("LayerCollector") || !c->get("ResetOnSpawn").b) continue;
        if (Instance::Ptr cp = c->clone()) cp->setParent(pg);
    }
}

void Runtime::removePlayer(Instance* p) {
    Impl& r = *impl_;
    if (!p) return;
    Instance* players = r.dm.getService("Players");
    r.sb.beginFrame();
    r.fireValues(*players, "PlayerRemoving", {Value::instance(p->id())});
    if (r.dm.isServer()) r.playerChannels(*p, false);
    Value ch = p->get("Character");
    if (Instance* c = r.dm.findRef(ch.ref)) c->destroy();
    if (r.localPlayer == p) r.localPlayer = nullptr;
    p->destroy();
    r.runDeferred();
}

std::string valueToString(const Value& v) {
    char b[128];
    switch (v.type) {
    case Value::Nil: return "nil";
    case Value::Bool: return v.b ? "true" : "false";
    case Value::Number: std::snprintf(b, sizeof b, "%.14g", v.n); return b;
    case Value::String: return v.s;
    case Value::Vector3: std::snprintf(b, sizeof b, "%g, %g, %g", v.v.x, v.v.y, v.v.z); return b;
    case Value::Vector2: std::snprintf(b, sizeof b, "%g, %g", v.v.x, v.v.y); return b;
    case Value::UDim: std::snprintf(b, sizeof b, "%g, %g", v.u[0], v.u[1]); return b;
    case Value::UDim2: std::snprintf(b, sizeof b, "{%g, %g}, {%g, %g}", v.u[0], v.u[1], v.u[2], v.u[3]); return b;
    case Value::Color3: std::snprintf(b, sizeof b, "%g, %g, %g", v.c.r, v.c.g, v.c.b); return b;
    case Value::Ref: std::snprintf(b, sizeof b, "Instance#%lld", (long long)v.ref); return b;
    case Value::Enum: return v.s;
    case Value::Font: {
        const EnumItem* w = findEnum("FontWeight")->findValue((int)v.n);
        return "Font { Family = " + v.s + ", Weight = " + (w ? w->name : "Regular") + ", Style = " + (v.b ? "Italic" : "Normal") + " }";
    }
    case Value::NumberRange: std::snprintf(b, sizeof b, "%g %g", v.u[0], v.u[1]); return b;
    case Value::PhysProps: std::snprintf(b, sizeof b, "%g, %g, %g, %g, %g", v.u[0], v.u[1], v.u[2], v.u[3], v.n); return b;
    case Value::Content:
        if (v.n == Value::ContentUri) return v.s;
        if (v.n == Value::ContentObject) { std::snprintf(b, sizeof b, "Instance#%lld", (long long)v.ref); return b; }
        return "";
    case Value::NumberSequence: case Value::ColorSequence: {
        std::string s;
        for (float f : v.kp) { std::snprintf(b, sizeof b, "%g ", f); s += b; }
        return s;
    }
    }
    return "?";
}

} // namespace pulseblockz::rbx
