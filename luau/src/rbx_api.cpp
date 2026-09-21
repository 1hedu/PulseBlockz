// The Instance API as scripts see it: `game`, `workspace`, `Instance.new`, members on tagged
// userdata, signals and connections, and the service methods.
#include <unordered_set>
#include "rbx_internal.h"
#include "rbx_audio.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <string_view>
#include <random>
#include <chrono>

namespace pulseblockz::rbx {

static const char* kInstances = "pulseblockz.instances";   // id -> userdata, weak values

// ---- userdata plumbing --------------------------------------------------------------
void Runtime::Impl::pushInstance(lua_State* L, Instance* i) {
    if (!i) { lua_pushnil(L); return; }
    lua_getfield(L, LUA_REGISTRYINDEX, kInstances);
    lua_pushnumber(L, (double)i->id());
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        // The tag drives typeof: Object is one that is not an Instance, such as an EditableImage.
        void* p = lua_newuserdatataggedwithmetatable(L, sizeof(InstanceUD), i->isA("Instance") ? TAG_INSTANCE : TAG_OBJECT);
        new (p) InstanceUD{i->shared_from_this()};
        lua_pushnumber(L, (double)i->id());
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
    }
    lua_remove(L, -2);
}

Instance* Runtime::Impl::toInstance(lua_State* L, int idx) {
    if (lua_userdatatag(L, idx) != TAG_INSTANCE) return nullptr;
    return static_cast<InstanceUD*>(lua_touserdata(L, idx))->inst.get();
}

Instance& Runtime::Impl::checkInstance(lua_State* L, int idx) {
    Instance* i = toInstance(L, idx);
    if (!i) luaL_typeerror(L, idx, "Instance");
    return *i;
}

Instance* Runtime::Impl::toObject(lua_State* L, int idx) {
    int tag = lua_userdatatag(L, idx);
    if (tag != TAG_INSTANCE && tag != TAG_OBJECT) return nullptr;
    return static_cast<InstanceUD*>(lua_touserdata(L, idx))->inst.get();
}

Instance& Runtime::Impl::checkObject(lua_State* L, int idx) {
    Instance* i = toObject(L, idx);
    if (!i) luaL_typeerror(L, idx, "Object");
    return *i;
}

void Runtime::Impl::pushSignal(lua_State* L, Instance& owner, Signal& s) {
    void* p = lua_newuserdatataggedwithmetatable(L, sizeof(SignalUD), TAG_SIGNAL);
    new (p) SignalUD{owner.shared_from_this(), &s};
}

// The name Roblox puts in an error message for a value: "Vector3", not "userdata".
namespace m { struct RaycastParamsUD { RaycastFilter f; bool overlap = false; }; }
// the vector helpers, defined with the raycasts below
static Vec3 add(Vec3 a, Vec3 b);
static Vec3 sub(Vec3 a, Vec3 b);
static Vec3 mul(Vec3 a, float s);
static float dot(Vec3 a, Vec3 b);
static float len(Vec3 a);
static Vec3 unit(Vec3 a);
static const char* typeOfName(lua_State* L, int idx) {
    if (lua_isvector(L, idx)) return "Vector3";
    switch (lua_userdatatag(L, idx)) {
    case TAG_INSTANCE: return "Instance";
    case TAG_OBJECT: return "Object";
    case TAG_COLOR3: return "Color3";
    case TAG_BRICKCOLOR: return "BrickColor";
    case TAG_VECTOR2: return "Vector2";
    case TAG_UDIM: return "UDim";
    case TAG_UDIM2: return "UDim2";
    case TAG_FONT: return "Font";
    case TAG_CONTENT: return "Content";
    case TAG_CFRAME: return "CFrame";
    case TAG_ENUMITEM: return "EnumItem";
    case TAG_SIGNAL: return "RBXScriptSignal";
    case TAG_CONNECTION: return "RBXScriptConnection";
    case TAG_TWEENINFO: return "TweenInfo";
    case TAG_RAY: return "Ray";
    case TAG_REGION3: return "Region3";
    case TAG_RAYCASTPARAMS: return "RaycastParams";
    case TAG_OVERLAPPARAMS: return "OverlapParams";
    default: break;
    }
    return luaL_typename(L, idx);
}

void Runtime::Impl::pushValue(lua_State* L, const Value& v, const EnumDef* e) {
    switch (v.type) {
    case Value::Nil: lua_pushnil(L); break;
    case Value::Bool: lua_pushboolean(L, v.b); break;
    case Value::Number: lua_pushnumber(L, v.n); break;
    case Value::String: lua_pushlstring(L, v.s.data(), v.s.size()); break;
    case Value::Vector3: pushVec3(L, v.v); break;
    case Value::Vector2: pushVector2(L, v.v2()); break;
    case Value::UDim: pushUDim(L, v.udim()); break;
    case Value::UDim2: pushUDim2(L, v.udim2()); break;
    case Value::Font: pushFont(L, v); break;
    case Value::NumberRange: case Value::NumberSequence: case Value::ColorSequence: pushSequence(L, v); break;
    case Value::PhysProps: pushPhysicalProperties(L, v); break;
    case Value::Content: pushContent(L, v); break;
    case Value::Color3: pushColor3(L, v.c); break;
    case Value::Ref: pushInstance(L, v.ref ? dm.find(v.ref) : nullptr); break;
    case Value::Enum: {
        // Host-made values carry "Type.Item" or just "Item" (then `e` must be given).
        const EnumDef* ed = e;
        std::string item = v.s;
        if (size_t dot = v.s.find('.'); dot != std::string::npos) { ed = findEnum(v.s.substr(0, dot)); item = v.s.substr(dot + 1); }
        const EnumItem* it = ed ? ed->find(item) : nullptr;
        if (it) pushEnumItem(L, ed, it); else lua_pushstring(L, v.s.c_str());
        break;
    }
    }
}

void Runtime::Impl::pushNetValue(lua_State* L, const NetValue& v) {
    switch (v.type) {
    case NetValue::Nil: lua_pushnil(L); break;
    case NetValue::Bool: lua_pushboolean(L, v.b); break;
    case NetValue::Number: lua_pushnumber(L, v.n); break;
    case NetValue::String: lua_pushlstring(L, v.s.data(), v.s.size()); break;
    case NetValue::Vector3: pushVec3(L, v.v); break;
    case NetValue::Vector2: pushVector2(L, {v.v.x, v.v.y}); break;
    case NetValue::UDim: pushUDim(L, {v.u[0], v.u[1]}); break;
    case NetValue::UDim2: pushUDim2(L, {{v.u[0], v.u[1]}, {v.u[2], v.u[3]}}); break;
    case NetValue::Font: pushFont(L, Value::font(v.s, (int)v.n, v.b)); break;
    case NetValue::Content: pushContent(L, Value::content((int)v.n, v.s, v.ref)); break;
    case NetValue::NumberRange: pushSequence(L, Value::numberRange(v.u[0], v.u[1])); break;
    case NetValue::NumberSequence: pushSequence(L, Value::numberSequence(v.kp)); break;
    case NetValue::ColorSequence: pushSequence(L, Value::colorSequence(v.kp)); break;
    case NetValue::Color3: pushColor3(L, v.c); break;
    case NetValue::CFrame: { CFrameV c; c.p = v.v; std::copy(v.m, v.m + 9, c.m); pushCFrame(L, c); break; }
    case NetValue::Ref: pushInstance(L, v.ref ? dm.find(v.ref) : nullptr); break;
    case NetValue::Enum: pushValue(L, Value::enumItem(v.s, v.n)); break;
    case NetValue::Table: {
        lua_createtable(L, 0, v.t ? (int)v.t->size() : 0);
        if (v.t) for (auto& [k, val] : *v.t) { pushNetValue(L, k); pushNetValue(L, val); lua_rawset(L, -3); }
        break;
    }
    }
}

NetValue Runtime::Impl::toNetValue(lua_State* L, int idx, int depth) {
    idx = lua_absindex(L, idx);
    NetValue out;
    switch (lua_type(L, idx)) {
    case LUA_TBOOLEAN: return NetValue::boolean(lua_toboolean(L, idx));
    case LUA_TNUMBER: return NetValue::number(lua_tonumber(L, idx));
    case LUA_TSTRING: { size_t n; const char* s = lua_tolstring(L, idx, &n); return NetValue::string(std::string(s, n)); }
    case LUA_TVECTOR: return NetValue::vector3(checkVec3(L, idx));
    case LUA_TUSERDATA: {
        if (isColor3(L, idx)) { out.type = NetValue::Color3; out.c = checkColor3(L, idx); return out; }
        if (isVector2(L, idx)) { Vec2 v = checkVector2(L, idx); out.type = NetValue::Vector2; out.v = {v.x, v.y, 0}; return out; }
        if (isUDim(L, idx)) { UDim d = checkUDim(L, idx); out.type = NetValue::UDim; out.u[0] = d.scale; out.u[1] = d.offset; return out; }
        if (isUDim2(L, idx)) { UDim2 d = checkUDim2(L, idx); out.type = NetValue::UDim2; out.u[0] = d.x.scale; out.u[1] = d.x.offset; out.u[2] = d.y.scale; out.u[3] = d.y.offset; return out; }
        if (isFont(L, idx)) { Value f = checkFont(L, idx); out.type = NetValue::Font; out.s = f.s; out.n = f.n; out.b = f.b; return out; }
        if (isContent(L, idx)) { Value c = checkContent(L, idx); out.type = NetValue::Content; out.n = c.n; out.s = c.s; out.ref = c.ref; return out; }
        if (isNumberRange(L, idx) || isNumberSequence(L, idx) || isColorSequence(L, idx)) {
            Value v = checkSequence(L, idx);
            out.type = v.type == Value::NumberRange ? NetValue::NumberRange : v.type == Value::NumberSequence ? NetValue::NumberSequence : NetValue::ColorSequence;
            out.u[0] = v.u[0]; out.u[1] = v.u[1]; out.kp = v.kp;
            return out;
        }
        if (isCFrame(L, idx)) { const CFrameV& c = checkCFrame(L, idx); out.type = NetValue::CFrame; out.v = c.p; std::copy(c.m, c.m + 9, out.m); return out; }
        if (Instance* i = toInstance(L, idx)) return NetValue::instance(i->id());
        const EnumDef* e; const EnumItem* it;
        if (readEnumItem(L, idx, e, it)) { out.type = NetValue::Enum; out.s = std::string(e->name) + "." + it->name; out.n = it->value; return out; }
        return out;   // TweenInfo, Random...: nil, like Roblox for things it cannot serialize
    }
    case LUA_TTABLE: {
        out.type = NetValue::Table;
        out.t = std::make_shared<std::vector<std::pair<NetValue, NetValue>>>();
        if (depth > 32) return out;   // a cycle: Roblox errors, this sends what fits
        lua_pushnil(L);
        while (lua_next(L, idx)) {
            int kt = lua_type(L, -2);
            if (kt == LUA_TNUMBER || kt == LUA_TSTRING || kt == LUA_TBOOLEAN) {
                NetValue k = toNetValue(L, -2, depth + 1), val = toNetValue(L, -1, depth + 1);
                if (val.type != NetValue::Nil) out.t->emplace_back(std::move(k), std::move(val));
            }
            lua_pop(L, 1);
        }
        return out;
    }
    default: return out;   // nil, functions, threads
    }
}

bool Runtime::Impl::toValue(lua_State* L, int idx, Value::Type want, const EnumDef* e, Value& out, std::string& err) {
    int t = lua_type(L, idx);
    auto fail = [&]() { err = typeOfName(L, idx); return false; };
    if (want == Value::Nil) {
        // Nil as the wanted type means infer it, which is how attributes arrive.
        if (t == LUA_TNIL) { out = Value::nil(); return true; }
        if (t == LUA_TBOOLEAN) { out = Value::boolean(lua_toboolean(L, idx)); return true; }
        if (t == LUA_TNUMBER) { out = Value::number(lua_tonumber(L, idx)); return true; }
        if (t == LUA_TSTRING) { size_t n; const char* s = lua_tolstring(L, idx, &n); out = Value::string(std::string(s, n)); return true; }
        if (lua_isvector(L, idx)) { out = Value::vector3(checkVec3(L, idx)); return true; }
        if (isColor3(L, idx)) { Col3 c = checkColor3(L, idx); out = Value::color3(c.r, c.g, c.b); return true; }
        if (isVector2(L, idx)) { Vec2 v = checkVector2(L, idx); out = Value::vector2(v.x, v.y); return true; }
        if (isUDim(L, idx)) { UDim d = checkUDim(L, idx); out = Value::udim(d.scale, d.offset); return true; }
        if (isUDim2(L, idx)) { out = Value::udim2(checkUDim2(L, idx)); return true; }
        if (isFont(L, idx)) { out = checkFont(L, idx); return true; }
        if (isNumberRange(L, idx) || isNumberSequence(L, idx) || isColorSequence(L, idx)) { out = checkSequence(L, idx); return true; }
        if (isPhysicalProperties(L, idx)) { out = checkPhysicalProperties(L, idx); return true; }
        if (Instance* i = toInstance(L, idx)) { out = Value::instance(i->id()); return true; }
        return fail();
    }
    switch (want) {
    case Value::Bool: if (t != LUA_TBOOLEAN) return fail(); out = Value::boolean(lua_toboolean(L, idx)); return true;
    case Value::Number: if (t != LUA_TNUMBER) return fail(); out = Value::number(lua_tonumber(L, idx)); return true;
    case Value::String: {
        if (t != LUA_TSTRING && t != LUA_TNUMBER) return fail();
        size_t n; const char* s = lua_tolstring(L, idx, &n); out = Value::string(std::string(s, n)); return true;
    }
    case Value::Vector3: if (!lua_isvector(L, idx)) return fail(); out = Value::vector3(checkVec3(L, idx)); return true;
    case Value::Vector2: { if (!isVector2(L, idx)) return fail(); Vec2 v = checkVector2(L, idx); out = Value::vector2(v.x, v.y); return true; }
    case Value::UDim: { if (!isUDim(L, idx)) return fail(); UDim d = checkUDim(L, idx); out = Value::udim(d.scale, d.offset); return true; }
    case Value::UDim2: { if (!isUDim2(L, idx)) return fail(); out = Value::udim2(checkUDim2(L, idx)); return true; }
    case Value::Font: { if (!isFont(L, idx)) return fail(); out = checkFont(L, idx); return true; }
    case Value::Content: { if (!isContent(L, idx)) return fail(); out = checkContent(L, idx); return true; }
    case Value::PhysProps: {
        // nil puts CustomPhysicalProperties back to the material's
        if (t == LUA_TNIL) { out = Value::nil(); return true; }
        if (!isPhysicalProperties(L, idx)) return fail(); out = checkPhysicalProperties(L, idx); return true;
    }
    case Value::NumberRange: {
        // a number stands for a range of one value, as Studio's property panel takes it
        if (t == LUA_TNUMBER) { float x = (float)lua_tonumber(L, idx); out = Value::numberRange(x, x); return true; }
        if (!isNumberRange(L, idx)) return fail(); out = checkSequence(L, idx); return true;
    }
    case Value::NumberSequence: {
        if (t == LUA_TNUMBER) { float x = (float)lua_tonumber(L, idx); out = Value::numberSequence(x, x); return true; }
        if (!isNumberSequence(L, idx)) return fail(); out = checkSequence(L, idx); return true;
    }
    case Value::ColorSequence: {
        if (isColor3(L, idx)) { out = Value::colorSequence(checkColor3(L, idx)); return true; }
        if (!isColorSequence(L, idx)) return fail(); out = checkSequence(L, idx); return true;
    }
    case Value::Color3: { if (!isColor3(L, idx)) return fail(); Col3 c = checkColor3(L, idx); out = Value::color3(c.r, c.g, c.b); return true; }
    case Value::Ref: {
        if (t == LUA_TNIL) { out = Value::instance(0); return true; }
        Instance* i = toInstance(L, idx); if (!i) return fail();
        out = Value::instance(i->id()); return true;
    }
    case Value::Enum: {
        if (!e) return fail();
        const EnumItem* it = nullptr;
        if (isEnumItem(L, idx) || t == LUA_TSTRING || t == LUA_TNUMBER) {
            // checkEnumItem raises on mismatch; do the lookup by hand to return false instead.
            if (t == LUA_TSTRING) it = e->find(lua_tostring(L, idx));
            else if (t == LUA_TNUMBER) it = e->findValue((int)lua_tonumber(L, idx));
            else { lua_pushvalue(L, idx); it = checkEnumItem(L, -1, e); lua_pop(L, 1); }
        }
        if (!it) return fail();
        out = Value::enumItem(it->name, it->value); return true;
    }
    default: return fail();
    }
}

static Instance& self(lua_State* L) { return rtOf(L).checkObject(L, 1); }   // the method table already checked the class

// Args [from..top] copied onto `co`, which must share L's global state.
static int xcopy(lua_State* L, lua_State* co, int from) {
    int n = lua_gettop(L);
    for (int i = from; i <= n; i++) { lua_pushvalue(L, i); lua_xmove(L, co, 1); }
    return n >= from ? n - from + 1 : 0;
}

static int inst_index(lua_State* L);   // Instance:GetStyled reads a property the way a `.` does

// ---- methods ---------------------------------------------------------------------------
namespace m {
static bool cframeProp(Instance& i, const char* key, std::string& posP, std::string& oriP);

static Instance* userSettings(Runtime::Impl& rt) {
    if (!rt.userSettings) {
        rt.userSettings = rt.dm.createInternal("UserSettings");
        rt.userSettings->setName("UserSettings");
        Instance::Ptr game = rt.dm.createInternal("UserGameSettings");
        game->setName("UserGameSettings");
        game->setParent(rt.userSettings.get());
        rt.savedQuality(rt.savedQualityLevel);
    }
    return rt.userSettings.get();
}
static int UserSettingsFn(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    rt.pushInstance(L, userSettings(rt));
    return 1;
}
static int GetService(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& g = self(L);
    if (g.isA("UserSettings")) {
        const char* name = luaL_checkstring(L, 2);
        Instance* found = g.findFirstChildOfClass(name);
        if (!found) luaL_error(L, "'%s' is not a valid Service name", name);
        rt.pushInstance(L, found);
        return 1;
    }
    if (!g.isA("DataModel")) luaL_error(L, "GetService is not a valid member of %s", g.className().c_str());
    const char* name = luaL_checkstring(L, 2);
    const ClassDef* c = findClass(name);
    if (!c || !c->service) luaL_error(L, "'%s' is not a valid Service name", name);
    rt.pushInstance(L, rt.dm.getService(name));
    return 1;
}
static int FindService(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    const char* name = luaL_checkstring(L, 2);
    rt.pushInstance(L, rt.dm.root()->findFirstChildOfClass(name));
    return 1;
}
static int IsLoaded(lua_State* L) { lua_pushboolean(L, 1); return 1; }
static int BindToClose(lua_State* L) {
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    rtOf(L).bindToClose.push_back(lua_ref(L, -1));
    lua_pop(L, 1);
    return 0;
}
static int FindFirstChild(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    const char* name = luaL_checkstring(L, 2);
    rt.pushInstance(L, i.findFirstChild(name, lua_toboolean(L, 3)));
    return 1;
}
static int FindFirstChildOfClass(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, self(L).findFirstChildOfClass(luaL_checkstring(L, 2))); return 1; }
static int FindFirstChildWhichIsA(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, self(L).findFirstChildWhichIsA(luaL_checkstring(L, 2))); return 1; }
static int FindFirstAncestor(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    const char* name = luaL_checkstring(L, 2);
    Instance* a = self(L).parent();
    while (a && a->name() != name) a = a->parent();
    rt.pushInstance(L, a); return 1;
}
static int FindFirstAncestorOfClass(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, self(L).findFirstAncestorOfClass(luaL_checkstring(L, 2))); return 1; }
static int FindFirstAncestorWhichIsA(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    const char* cn = luaL_checkstring(L, 2);
    Instance* a = self(L).parent();
    while (a && !a->isA(cn)) a = a->parent();
    rt.pushInstance(L, a); return 1;
}
static int FindFirstDescendant(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, self(L).findFirstChild(luaL_checkstring(L, 2), true)); return 1; }
static int WaitForChild(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    const char* name = luaL_checkstring(L, 2);
    double timeout = luaL_optnumber(L, 3, 0);
    if (Instance* c = i.findFirstChild(name)) { rt.pushInstance(L, c); return 1; }
    if (!lua_isyieldable(L)) luaL_error(L, "cannot yield from this context");
    Task* t = rt.taskFor(L);
    t->state = Task::WaitChild;
    t->waitParent = i.shared_from_this();
    t->waitName = name;
    t->waitDeadline = timeout > 0 ? rt.now + timeout : 0;
    t->sleepStart = rt.now;
    t->waitWarned = false;
    rt.childWaiters.push_back(t);
    return lua_yield(L, 0);
}
static int GetChildren(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    lua_createtable(L, (int)i.children().size(), 0);
    int n = 0;
    for (auto& c : i.children()) { rt.pushInstance(L, c.get()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int GetDescendants(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    auto ds = self(L).getDescendants();
    lua_createtable(L, (int)ds.size(), 0);
    int n = 0;
    for (Instance* d : ds) { rt.pushInstance(L, d); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int IsA(lua_State* L) { lua_pushboolean(L, self(L).isA(luaL_checkstring(L, 2))); return 1; }
static int IsDescendantOf(lua_State* L) { Instance* o = rtOf(L).toInstance(L, 2); lua_pushboolean(L, o && self(L).isDescendantOf(o)); return 1; }
static int IsAncestorOf(lua_State* L) { Instance* o = rtOf(L).toInstance(L, 2); lua_pushboolean(L, o && self(L).isAncestorOf(o)); return 1; }
static int Destroy(lua_State* L) {
    Instance& i = self(L);
    if (i.isA("DataModel") || (i.cls().service && i.parent() && i.parent()->isA("DataModel"))) luaL_error(L, "The Parent property of %s is locked", i.name().c_str());
    Instance::Ptr keep = i.shared_from_this();
    i.destroy();
    return 0;
}
static int Remove(lua_State* L) { std::string err; if (!self(L).setParent(nullptr, &err)) luaL_error(L, "%s", err.c_str()); return 0; }
static int Clone(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance::Ptr c = self(L).clone();
    if (!c) { lua_pushnil(L); return 1; }
    rt.pushInstance(L, c.get());
    // The userdata's Ptr is what keeps the clone alive until the script parents or drops it.
    return 1;
}
static int ClearAllChildren(lua_State* L) {
    Instance& i = self(L);
    std::vector<Instance::Ptr> kids = i.children();
    for (auto& c : kids) c->destroy();
    return 0;
}
static int GetFullName(lua_State* L) { lua_pushstring(L, self(L).fullName().c_str()); return 1; }
static int GetAttribute(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushValue(L, self(L).getAttribute(luaL_checkstring(L, 2))); return 1; }
static int SetAttribute(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    const char* name = luaL_checkstring(L, 2);
    Value v; std::string err;
    if (!rt.toValue(L, 3, Value::Nil, nullptr, v, err)) luaL_error(L, "invalid argument #3 to 'SetAttribute' (%s is not a supported attribute type)", err.c_str());
    if (v.type == Value::Ref) luaL_error(L, "invalid argument #3 to 'SetAttribute' (Instance is not a supported attribute type)");
    i.setAttribute(name, v);
    return 0;
}
static int GetAttributes(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    lua_newtable(L);
    for (auto& [k, v] : self(L).attributes()) { rt.pushValue(L, v); lua_setfield(L, -2, k.c_str()); }
    return 1;
}
static int GetAttributeChangedSignal(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    rt.pushSignal(L, i, rt.signal(i, std::string("Attr:") + luaL_checkstring(L, 2)));
    return 1;
}
static int GetPropertyChangedSignal(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    const char* prop = luaL_checkstring(L, 2);
    if (i.isA("BasePart") && !std::strcmp(prop, "BrickColor")) prop = "Color";
    std::string cfPos, cfOri;
    if (!i.cls().findProp(prop) && std::strcmp(prop, "Parent") && !cframeProp(i, prop, cfPos, cfOri) && !(i.isA("RayValue") && !std::strcmp(prop, "Value")))
        luaL_error(L, "%s is not a valid property name.", prop);
    rt.pushSignal(L, i, rt.signal(i, std::string("Prop:") + prop));
    return 1;
}
static int AddTag(lua_State* L) { self(L).addTag(luaL_checkstring(L, 2)); return 0; }
static int RemoveTag(lua_State* L) { self(L).removeTag(luaL_checkstring(L, 2)); return 0; }
static int HasTag(lua_State* L) { lua_pushboolean(L, self(L).hasTag(luaL_checkstring(L, 2))); return 1; }
static int GetTags(lua_State* L) {
    auto& tags = self(L).tags();
    lua_createtable(L, (int)tags.size(), 0);
    int n = 0;
    for (auto& t : tags) { lua_pushstring(L, t.c_str()); lua_rawseti(L, -2, ++n); }
    return 1;
}

// Players
static int GetPlayers(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    lua_newtable(L);
    int n = 0;
    for (auto& c : p.children()) if (c->isA("Player")) { rt.pushInstance(L, c.get()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int GetPlayerByUserId(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    double id = luaL_checknumber(L, 2);
    for (auto& c : self(L).children()) if (c->isA("Player") && c->get("UserId").n == id) { rt.pushInstance(L, c.get()); return 1; }
    lua_pushnil(L); return 1;
}
static int GetPlayerFromCharacter(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance* ch = rt.toInstance(L, 2);
    if (ch) for (auto& c : self(L).children()) if (c->isA("Player") && c->get("Character").ref == ch->id()) { rt.pushInstance(L, c.get()); return 1; }
    lua_pushnil(L); return 1;
}
// Teams
static int GetTeams(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    lua_newtable(L);
    int n = 0;
    for (auto& c : self(L).children()) if (c->isA("Team")) { rt.pushInstance(L, c.get()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int TeamGetPlayers(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    lua_newtable(L);
    int n = 0;
    for (auto& c : rt.dm.getService("Players")->children())
        if (c->isA("Player") && c->get("Team").ref == t.id()) { rt.pushInstance(L, c.get()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int Kick(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    Instance::Ptr keep = p.shared_from_this();
    rt.self->removePlayer(&p);
    return 0;
}
// With Players.CharacterAutoLoads off, the only way a character ever arrives.
static int LoadCharacter(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    if (!rt.dm.isServer()) luaL_error(L, "LoadCharacter can only be called by the backend server");
    if (!p.parent() || !p.parent()->isA("Players")) return 0;   // left already
    if (Instance* old = rt.dm.findRef(p.get("Character").ref)) { rt.fireValues(p, "CharacterRemoving", {Value::instance(old->id())}); old->destroy(); }
    rt.spawnCharacter(p);
    return 0;
}
// LoadCharacter, then the description's body colours on the new character (the rest of a
// description is recorded and not worn: see HumanoidDescription).
static int LoadCharacterWithHumanoidDescriptionAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    Instance& desc = rt.checkInstance(L, 2);
    if (!desc.isA("HumanoidDescription")) luaL_error(L, "invalid argument #1 to 'LoadCharacterWithHumanoidDescriptionAsync' (HumanoidDescription expected, got %s)", desc.className().c_str());
    LoadCharacter(L);
    if (Instance* ch = rt.dm.findRef(p.get("Character").ref))
        if (Instance* hum = ch->findFirstChildOfClass("Humanoid")) rt.applyDescription(*hum, desc);
    return 0;
}
// Takes the dressing off the character: accessories, clothing, body colours, character meshes
// and the face decal, as on Roblox.
static int ClearCharacterAppearance(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance* ch = rt.dm.findRef(self(L).get("Character").ref);
    if (!ch) return 0;
    std::vector<Instance::Ptr> gone;
    for (auto& c : ch->children()) if (c->isA("Accoutrement") || c->isA("CharacterAppearance")) gone.push_back(c);
    if (Instance* head = ch->findFirstChild("Head")) for (auto& c : head->children()) if (c->isA("Decal")) gone.push_back(c);
    for (auto& g : gone) g->destroy();
    return 0;
}
// Nothing here fetches an avatar, so there is nothing cached to clear and nothing to wait for:
// an appearance is loaded as soon as there is a character.
static int ClearCachedAvatarAppearance(lua_State* L) { (void)self(L); return 0; }
static int HasAppearanceLoaded(lua_State* L) { lua_pushboolean(L, rtOf(L).dm.findRef(self(L).get("Character").ref) != nullptr); return 1; }
// Roblox's social graph and groups: no account here has friends or groups, and nobody is verified.
static int GetFriendsOnlineAsync(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
static int GetFriendsWhoPlayedAsync(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
static int IsFriendsWithAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); lua_pushboolean(L, 0); return 1; }
static int IsInGroupAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); lua_pushboolean(L, 0); return 1; }
static int IsVerified(lua_State* L) { lua_pushboolean(L, self(L).get("HasVerifiedBadge").b); return 1; }
// Nobody arrives by teleport here, so the join data is empty.
static int GetJoinData(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
// The round trip is not measured here: 0 in Play Solo and over the wire alike.
static int GetNetworkPing(lua_State* L) { (void)self(L); lua_pushnumber(L, 0); return 1; }
static int SetAccountAge(lua_State* L) { self(L).setInternal("AccountAge", Value::number(luaL_checknumber(L, 2))); return 0; }
// Streaming: there is none here, so a focus is nothing to add, remove or wait for.
static int AddReplicationFocus(lua_State* L) { (void)self(L); rtOf(L).checkInstance(L, 2); return 0; }
static int RemoveReplicationFocus(lua_State* L) { (void)self(L); rtOf(L).checkInstance(L, 2); return 0; }
static int RequestStreamAroundAsync(lua_State* L) { (void)self(L); checkVec3(L, 2); return 0; }
// Players: the users known here are the ones in the game.
static int GetNameFromUserIdAsync(lua_State* L) {
    double id = luaL_checknumber(L, 2);
    for (auto& c : self(L).children()) if (c->isA("Player") && c->get("UserId").n == id) { lua_pushstring(L, c->name().c_str()); return 1; }
    luaL_error(L, "Players:GetNameFromUserIdAsync() failed: Unknown user");
}
static int GetUserIdFromNameAsync(lua_State* L) {
    const char* name = luaL_checkstring(L, 2);
    for (auto& c : self(L).children()) if (c->isA("Player") && c->name() == name) { lua_pushnumber(L, c->get("UserId").n); return 1; }
    luaL_error(L, "Players:GetUserIdFromNameAsync() failed: Unknown user");
}
// Writes the two read-only chat flags; the chat drawn here follows TextChatService, not them.
static int SetChatStyle(lua_State* L) {
    Instance& p = self(L);
    int style = lua_isnoneornil(L, 2) ? 0 : checkEnumItem(L, 2, findEnum("ChatStyle"))->value;
    p.setInternal("ClassicChat", Value::boolean(style != 1));
    p.setInternal("BubbleChat", Value::boolean(style != 0));
    return 0;
}
// The Settings menu's movement and camera modes: there is no such menu here, so a mode
// registered is nothing to show and nothing to clear.
static int RegisterMovementMode(lua_State* L) { (void)self(L); rtOf(L).checkInstance(L, 2); return 0; }
static int ClearMovementModes(lua_State* L) { (void)self(L); return 0; }
// Moving a tool is the server's; a client asks for its own character (see Runtime::input).
static int EquipTool(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& hum = self(L);
    Instance* tool = rt.toInstance(L, 2);
    Instance* character = hum.parent();
    if (!tool || !tool->isA("Tool") || !character) return 0;
    if (rt.dm.isServer()) { rt.equipTool(*character, *tool); return 0; }
    if (rt.localPlayer && rt.dm.findRef(rt.localPlayer->get("Character").ref) == character) {
        RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = tool->id(); msg.args.push_back(NetValue::string("Equip"));
        rt.outRemotes.push_back(std::move(msg));
    }
    return 0;
}
static int UnequipTools(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance* character = self(L).parent();
    if (!character) return 0;
    if (rt.dm.isServer()) { rt.unequipTools(*character); return 0; }
    if (rt.localPlayer && rt.dm.findRef(rt.localPlayer->get("Character").ref) == character)
        for (auto& c : character->children()) if (c->isA("Tool")) {
            RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = c->id(); msg.args.push_back(NetValue::string("Unequip"));
            rt.outRemotes.push_back(std::move(msg));
        }
    return 0;
}
static int ToolActivate(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& tool = self(L);
    if (!rt.heldTools.count(tool.id())) return 0;
    rt.fireValues(tool, "Activated", {});
    if (!rt.dm.isServer()) { RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = tool.id(); msg.args.push_back(NetValue::string("Activated")); rt.outRemotes.push_back(std::move(msg)); }
    return 0;
}
static int ToolDeactivate(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& tool = self(L);
    if (!rt.heldTools.count(tool.id())) return 0;
    rt.fireValues(tool, "Deactivated", {});
    if (!rt.dm.isServer()) { RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = tool.id(); msg.args.push_back(NetValue::string("Deactivated")); rt.outRemotes.push_back(std::move(msg)); }
    return 0;
}
// A LocalScript's own prompt UI (Style = Custom) presses the prompt as the key would.
static int PromptInputHoldBegin(lua_State* L) { rtOf(L).promptPress(self(L)); return 0; }
static int PromptInputHoldEnd(lua_State* L) { rtOf(L).promptRelease(self(L)); return 0; }
// CoreGuiHidden is a bitmask over CoreGuiType; All (4) covers every bit.
static int SetCoreGuiEnabled(lua_State* L) {
    Instance& sg = self(L);
    int type = checkEnumItem(L, 2, findEnum("CoreGuiType"))->value;
    bool enabled = lua_toboolean(L, 3);
    if (rtOf(L).dm.isServer()) luaL_error(L, "SetCoreGuiEnabled can only be called from a LocalScript");
    int hidden = (int)sg.get("CoreGuiHidden").n;
    int bits = type == 4 ? 0xFF : (1 << type);
    hidden = enabled ? (hidden & ~bits) : (hidden | bits);
    sg.setInternal("CoreGuiHidden", Value::number(hidden));
    return 0;
}
// A ResetButtonCallback BindableEvent fires instead of the menu's Reset resetting. Any core
// name not handled below is accepted and ignored.
static int SetCore(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& sg = self(L);
    const char* name = luaL_checkstring(L, 2);
    if (rt.dm.isServer()) luaL_error(L, "SetCore can only be called from a LocalScript");
    if (!std::strcmp(name, "ResetButtonCallback")) {
        if (lua_type(L, 3) == LUA_TBOOLEAN) { rt.resetEnabled = lua_toboolean(L, 3); rt.resetCallback = 0; }
        else if (Instance* b = rt.toInstance(L, 3); b && b->isA("BindableEvent")) { rt.resetCallback = b->id(); rt.resetEnabled = true; }
        else luaL_error(L, "SetCore: ResetButtonCallback expects a BindableEvent or a boolean");
        return 0;
    }
    if (!std::strcmp(name, "TopbarEnabled") || !std::strcmp(name, "ChatActive")) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        sg.setInternal(name, Value::boolean(lua_toboolean(L, 3)));
        return 0;
    }
    if (!std::strcmp(name, "SendNotification")) {
        luaL_checktype(L, 3, LUA_TTABLE);
        Runtime::Notification n;
        auto str = [&](const char* key, std::string& into, bool required) {
            lua_getfield(L, 3, key);
            if (lua_type(L, -1) == LUA_TSTRING) into = lua_tostring(L, -1);
            else if (required) luaL_error(L, "SendNotification: %s (a string) is required", key);
            lua_pop(L, 1);
        };
        str("Title", n.title, true);
        str("Text", n.text, true);
        str("Icon", n.icon, false);
        str("Button1", n.button1, false);
        str("Button2", n.button2, false);
        lua_getfield(L, 3, "Duration");
        if (lua_type(L, -1) == LUA_TNUMBER) n.duration = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 3, "Callback");
        if (Instance* cb = rt.toInstance(L, -1); cb && cb->isA("BindableFunction")) n.callback = cb->id();
        lua_pop(L, 1);
        rt.notifications.push_back(std::move(n));
        return 0;
    }
    if (!std::strcmp(name, "ChatMakeSystemMessage")) {
        luaL_checktype(L, 3, LUA_TTABLE);
        Runtime::ChatLine line; line.system = true;
        lua_getfield(L, 3, "Text");
        if (lua_type(L, -1) != LUA_TSTRING) luaL_error(L, "ChatMakeSystemMessage: Text (a string) is required");
        line.text = lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 3, "Color");
        if (isColor3(L, -1)) line.color = checkColor3(L, -1);
        lua_pop(L, 1);
        rt.chatLines.push_back(std::move(line));
    }
    return 0;
}
static int GetCore(lua_State* L) {
    Instance& sg = self(L);
    const char* name = luaL_checkstring(L, 2);
    if (rtOf(L).dm.isServer()) luaL_error(L, "GetCore can only be called from a LocalScript");
    if (std::strcmp(name, "ChatActive") && std::strcmp(name, "TopbarEnabled")) luaL_error(L, "GetCore: %s is not a known core", name);
    lua_pushboolean(L, sg.get(name).b);
    return 1;
}
// ---- TextChatService (rbx_chat.cpp) ----
static std::string metadataArg(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TSTRING) return lua_tostring(L, idx);
    if (lua_type(L, idx) == LUA_TTABLE) {   // Roblox takes a string; a table's Metadata field is the common slip
        lua_getfield(L, idx, "Metadata");
        std::string s = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        return s;
    }
    return "";
}
static int SendAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ch = self(L);
    const char* text = luaL_checkstring(L, 2);
    if (rt.dm.isServer()) luaL_error(L, "SendAsync can only be called from a LocalScript");
    rt.pushInstance(L, rt.sendAsync(ch, text, metadataArg(L, 3)).get());
    return 1;
}
static int DisplaySystemMessage(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ch = self(L);
    const char* text = luaL_checkstring(L, 2);
    if (rt.dm.isServer()) luaL_error(L, "DisplaySystemMessage can only be called from a LocalScript");
    rt.pushInstance(L, rt.systemMessage(ch, text, metadataArg(L, 3)).get());
    return 1;
}
static int AddUserAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ch = self(L);
    int64_t userId = (int64_t)luaL_checknumber(L, 2);
    if (!rt.dm.isServer()) luaL_error(L, "AddUserAsync can only be called from a Script");
    Instance* player = nullptr;
    for (auto& p : rt.dm.getService("Players")->children()) if (p->isA("Player") && (int64_t)p->get("UserId").n == userId) player = p.get();
    if (!player) { lua_pushnil(L); lua_pushboolean(L, false); return 2; }
    rt.pushInstance(L, rt.addTextSource(ch, *player));
    lua_pushboolean(L, true);
    return 2;
}
static int SetDirectChatRequester(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ch = self(L);
    Instance* src = lua_isnoneornil(L, 2) ? nullptr : &rt.checkInstance(L, 2);
    if (src && !src->isA("TextSource")) luaL_typeerror(L, 2, "TextSource");
    ch.setInternal("DirectChatRequester", Value::instance(src ? src->id() : 0));
    return 0;
}
static int DisplayBubble(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Instance& at = rt.checkInstance(L, 2);
    const char* text = luaL_checkstring(L, 3);
    Instance* part = &at;
    if (at.isA("Model")) { part = at.findFirstChild("Head"); if (!part) part = at.findFirstChildOfClass("BasePart"); }
    if (!part || !part->isA("BasePart")) luaL_error(L, "DisplayBubble expects a part or a character");
    if (rt.dm.isServer()) luaL_error(L, "DisplayBubble can only be called from a LocalScript");
    Runtime::ChatLine line; line.part = part->id(); line.text = text;
    rt.chatLines.push_back(std::move(line));
    return 0;
}
static int CanUsersChat(lua_State* L) { (void)self(L); lua_pushboolean(L, true); return 1; }
// A bubble over the part: on every client from the server, on this one from a client.
static int ChatChat(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Instance& at = rt.checkInstance(L, 2);
    const char* text = luaL_checkstring(L, 3);
    Instance* part = &at;
    if (at.isA("Model")) { part = at.findFirstChild("Head"); if (!part) part = at.findFirstChildOfClass("BasePart"); }
    if (!part || !part->isA("BasePart")) luaL_error(L, "Chat:Chat expects a part or a character");
    Col3 color{1, 1, 1};
    if (lua_gettop(L) >= 4 && lua_type(L, 4) != LUA_TNIL) {
        if (isColor3(L, 4)) color = checkColor3(L, 4);
        else {
            int c = checkEnumItem(L, 4, findEnum("ChatColor"))->value;
            color = c == 0 ? Col3{0.5f, 0.75f, 1} : c == 1 ? Col3{0.5f, 1, 0.5f} : c == 2 ? Col3{1, 0.5f, 0.5f} : Col3{1, 1, 1};
        }
    }
    if (rt.dm.isServer()) {
        RemoteMsg down; down.kind = RemoteMsg::Event; down.remote = rt.dm.getService("Players")->id();
        NetValue col; col.type = NetValue::Color3; col.c = color;
        down.args = {NetValue::string("Bubble"), NetValue::instance(part->id()), NetValue::string(text), col};
        rt.outRemotes.push_back(std::move(down));
    } else {
        Runtime::ChatLine line; line.part = part->id(); line.text = text; line.color = color;
        rt.chatLines.push_back(std::move(line));
    }
    return 0;
}
// No filter here: the text comes back as it is.
static int FilterString(lua_State* L) {
    (void)self(L);
    lua_pushstring(L, luaL_checkstring(L, 2));
    return 1;
}
static int GetCoreGuiEnabled(lua_State* L) {
    Instance& sg = self(L);
    int type = checkEnumItem(L, 2, findEnum("CoreGuiType"))->value;
    int hidden = (int)sg.get("CoreGuiHidden").n;
    lua_pushboolean(L, type == 4 ? hidden == 0 : !(hidden & (1 << type)));
    return 1;
}
static int SoundPlay(lua_State* L) { rtOf(L).soundPlay(self(L), false, false); return 0; }
static int SeatSit(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& seat = self(L);
    Instance& hum = rt.checkInstance(L, 2);
    if (!hum.isA("Humanoid")) luaL_error(L, "invalid argument #1 to 'Sit' (Humanoid expected, got %s)", hum.className().c_str());
    if (rt.dm.isServer() && hum.get("Health").n > 0) rt.sitOn(seat, hum);
    return 0;
}
// The engine reads a burst off EmitBurst, so the write is pulsed back to 0; -1 is Clear.
static int EmitParticles(lua_State* L) {
    Instance& e = self(L);
    double n = luaL_optnumber(L, 2, 16);
    if (n > 0) { e.setInternal("EmitBurst", Value::number(std::floor(n))); e.setInternal("EmitBurst", Value::number(0)); }
    return 0;
}
static int ClearParticles(lua_State* L) {
    Instance& e = self(L);
    e.setInternal("EmitBurst", Value::number(-1)); e.setInternal("EmitBurst", Value::number(0));
    return 0;
}
// Trail:Clear() drops the ribbon; the engine reads the pulse off ClearTick.
static int ClearTrail(lua_State* L) {
    Instance& t = self(L);
    t.setInternal("ClearTick", Value::number(1)); t.setInternal("ClearTick", Value::number(0));
    return 0;
}
// Beam:SetTextureOffset(offset): where in its cycle the texture is now, in cycles; the scroll
// goes on from there. The same offset twice in a row is one write: the second does not restart.
static int SetTextureOffset(lua_State* L) { self(L).setInternal("TextureOffset", Value::number(luaL_optnumber(L, 2, 0))); return 0; }
// Lighting's clock as minutes, and the sun on the arc the host draws (pulseblockz_world.cpp,
// update_lighting): up in the east (+X) at 6, overhead at 12, down in the west at 18, whatever
// GeographicLatitude says; the moon is opposite it, as in Studio.
static int GetMinutesAfterMidnight(lua_State* L) { lua_pushnumber(L, self(L).get("ClockTime").n * 60.0); return 1; }
static int SetMinutesAfterMidnight(lua_State* L) {
    double m = std::fmod(luaL_checknumber(L, 2), 1440.0); if (m < 0) m += 1440;
    self(L).setInternal("ClockTime", Value::number(m / 60.0));
    return 0;
}
static Vec3 sunDirection(Instance& lighting) {
    double a = (lighting.get("ClockTime").n - 6.0) / 12.0 * 3.14159265358979323846;
    return Vec3{(float)std::cos(a), (float)std::sin(a), 0};
}
static int GetSunDirection(lua_State* L) { pushVec3(L, sunDirection(self(L))); return 1; }
static int GetMoonDirection(lua_State* L) { Vec3 d = sunDirection(self(L)); pushVec3(L, Vec3{-d.x, -d.y, -d.z}); return 1; }
// MaterialService's overrides live in its <Material>Name properties; a Material with no such
// property (Neon, Glass, ForceField, Air, Water) has none.
static int GetBaseMaterialOverride(lua_State* L) {
    Instance& ms = self(L);
    std::string key = std::string(checkEnumItem(L, 2, findEnum("Material"))->name) + "Name";
    lua_pushstring(L, ms.cls().findProp(key) ? ms.get(key).s.c_str() : "");
    return 1;
}
static int SetBaseMaterialOverride(lua_State* L) {
    Instance& ms = self(L);
    const EnumItem* mat = checkEnumItem(L, 2, findEnum("Material"));
    std::string key = std::string(mat->name) + "Name";
    if (!ms.cls().findProp(key)) luaL_error(L, "Material %s cannot have a base material override", mat->name);
    ms.setInternal(key, Value::string(luaL_checkstring(L, 3)));
    return 0;
}
// The MaterialVariant child of that name whose BaseMaterial is the material, or nil.
static int GetMaterialVariant(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ms = self(L);
    const EnumItem* mat = checkEnumItem(L, 2, findEnum("Material"));
    const char* name = luaL_checkstring(L, 3);
    for (auto& c : ms.children())
        if (c->isA("MaterialVariant") && c->name() == name && c->get("BaseMaterial").s == mat->name) { rt.pushInstance(L, c.get()); return 1; }
    lua_pushnil(L);
    return 1;
}
static int CaptureFocus(lua_State* L) { Runtime::Impl& rt = rtOf(L); Instance& b = self(L); if (!rt.dm.isServer()) rt.focusBox(b); return 0; }
static int ReleaseFocus(lua_State* L) { Runtime::Impl& rt = rtOf(L); Instance& b = self(L); rt.blurBox(b, luaL_optboolean(L, 2, 0)); return 0; }
static int IsFocused(lua_State* L) { lua_pushboolean(L, rtOf(L).focusedBox == self(L).id()); return 1; }
static int GetFocusedTextBox(lua_State* L) { (void)self(L); Runtime::Impl& rt = rtOf(L); if (Instance* b = rt.focusedTextBox()) rt.pushInstance(L, b); else lua_pushnil(L); return 1; }
static int SoundResume(lua_State* L) { rtOf(L).soundPlay(self(L), true, false); return 0; }
static int SoundStop(lua_State* L) { rtOf(L).soundStop(self(L), false); return 0; }
static int SoundPause(lua_State* L) { rtOf(L).soundStop(self(L), true); return 0; }
// Heard by this client only, wherever (or not) the sound is parented.
static int PlayLocalSound(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& s = rt.checkInstance(L, 2);
    if (!s.isA("Sound")) luaL_error(L, "PlayLocalSound: Sound expected, got %s", s.className().c_str());
    if (!rt.dm.isServer()) rt.soundPlay(s, false, true);
    return 0;
}
// ListenerType: 0 Camera (the engine's own ear), 1 CFrame, 2 ObjectPosition (a part's place with
// the camera's facing), 3 ObjectCFrame (a part's whole frame).
static int SetListener(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ss = self(L);
    const EnumDef* e = findEnum("ListenerType");
    const EnumItem* it = checkEnumItem(L, 2, e);
    int64_t object = 0;
    if (it->value == 1) {
        const CFrameV& c = checkCFrame(L, 3);
        rt.listenerCFrame = c;
        Vec3 pos, o; cframeToPosOrient(c, pos, o);
        ss.setInternal("ListenerPosition", Value::vector3(pos));
        ss.setInternal("ListenerOrientation", Value::vector3(o));
    } else if (it->value == 2 || it->value == 3) {
        Instance& o = rt.checkInstance(L, 3);
        if (!o.isA("BasePart")) luaL_error(L, "SetListener: BasePart expected, got %s", o.className().c_str());
        object = o.id();
    }
    ss.setInternal("ListenerObject", Value::instance(object));
    ss.setInternal("ListenerType", Value::enumItem(it->name, it->value));
    return 0;
}
static int GetListener(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ss = self(L);
    const EnumDef* e = findEnum("ListenerType");
    const int type = (int)ss.get("ListenerType").n;
    const EnumItem* it = e ? e->findValue(type) : nullptr;
    if (it) pushEnumItem(L, e, it); else lua_pushnil(L);
    if (type == 1) pushCFrame(L, rt.listenerCFrame);
    else if (type == 2 || type == 3) rt.pushInstance(L, rt.dm.find(ss.get("ListenerObject").ref));
    else lua_pushnil(L);
    return 2;
}
static int DistanceFromCharacter(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Vec3 pt = checkVec3(L, 2);
    Instance* ch = rt.dm.findRef(self(L).get("Character").ref);
    Instance* root = ch ? ch->findFirstChild("HumanoidRootPart") : nullptr;
    if (!root) { lua_pushnumber(L, 0); return 1; }
    Vec3 p = root->get("Position").v;
    lua_pushnumber(L, std::sqrt((p.x - pt.x) * (p.x - pt.x) + (p.y - pt.y) * (p.y - pt.y) + (p.z - pt.z) * (p.z - pt.z)));
    return 1;
}

// RunService
static int IsServer(lua_State* L) { lua_pushboolean(L, rtOf(L).dm.isServer()); return 1; }
static int IsClient(lua_State* L) { lua_pushboolean(L, !rtOf(L).dm.isServer()); return 1; }
static int IsStudio(lua_State* L) { lua_pushboolean(L, rtOf(L).opts.isStudio); return 1; }
static int IsRunning(lua_State* L) { lua_pushboolean(L, 1); return 1; }
static int IsEdit(lua_State* L) { lua_pushboolean(L, !rtOf(L).opts.runScripts); return 1; }   // the Studio's edit world
static int BindToRenderStep(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 4, LUA_TFUNCTION);
    for (auto& [n, ref] : rt.renderSteps) if (n == name) luaL_error(L, "BindToRenderStep: name '%s' is already bound", name);
    lua_pushvalue(L, 4);
    rt.renderSteps.push_back({name, lua_ref(L, -1)});
    lua_pop(L, 1);
    return 0;
}
static int UnbindFromRenderStep(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    const char* name = luaL_checkstring(L, 2);
    for (size_t i = 0; i < rt.renderSteps.size(); i++)
        if (rt.renderSteps[i].first == name) { lua_unref(L, rt.renderSteps[i].second); rt.renderSteps.erase(rt.renderSteps.begin() + i); break; }
    return 0;
}

// Bindables
static int Fire(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    Signal* s = rt.findSignal(i, "Event");
    if (!s || !s->hasConns()) return 0;
    rt.fire(*s, [L](lua_State* co) { return xcopy(L, co, 2); });
    return 0;
}
static int Invoke(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    InstBinding& b = rt.binding(i);
    auto it = b.callbacks.find("OnInvoke");
    if (it == b.callbacks.end()) luaL_error(L, "OnInvoke is not set on %s", i.fullName().c_str());
    int n = lua_gettop(L) - 1;
    lua_getref(L, it->second.ref);
    lua_insert(L, 2);
    lua_call(L, n, LUA_MULTRET);
    return lua_gettop(L) - 1;
}
// Arguments cross as NetValues (rbx_net.h): tables and CFrames included, functions become nil,
// instances go by id.
static void collectArgs(lua_State* L, int first, std::vector<NetValue>& out) {
    Runtime::Impl& rt = rtOf(L);
    for (int i = first; i <= lua_gettop(L); i++) out.push_back(rt.toNetValue(L, i));
}
static Instance& checkPlayerArg(lua_State* L, int idx, const char* method) {
    Instance* p = rtOf(L).toInstance(L, idx);
    if (!p || !p->isA("Player")) luaL_error(L, "Unable to cast value to Object: %s expects a Player, got %s", method, typeOfName(L, idx));
    return *p;
}
static int FireServer(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    if (rt.dm.isServer()) luaL_error(L, "FireServer can only be called from the client");
    RemoteMsg m; m.kind = RemoteMsg::Event; m.remote = self(L).id();
    collectArgs(L, 2, m.args);
    rt.outRemotes.push_back(std::move(m));
    return 0;
}
static int FireClient(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    if (!rt.dm.isServer()) luaL_error(L, "FireClient can only be called from the server");
    RemoteMsg m; m.kind = RemoteMsg::Event; m.remote = self(L).id(); m.player = checkPlayerArg(L, 2, "FireClient").id();
    collectArgs(L, 3, m.args);
    rt.outRemotes.push_back(std::move(m));
    return 0;
}
static int FireAllClients(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    if (!rt.dm.isServer()) luaL_error(L, "FireAllClients can only be called from the server");
    RemoteMsg m; m.kind = RemoteMsg::Event; m.remote = self(L).id();
    collectArgs(L, 2, m.args);
    rt.outRemotes.push_back(std::move(m));
    return 0;
}
// The caller parks until the other side's Result comes back through deliverRemote.
static int invokeRemote(lua_State* L, int64_t player, int firstArg) {
    Runtime::Impl& rt = rtOf(L);
    if (!lua_isyieldable(L)) luaL_error(L, "cannot yield from this context");
    RemoteMsg m; m.kind = RemoteMsg::Invoke; m.remote = self(L).id(); m.player = player; m.call = rt.nextCall++;
    collectArgs(L, firstArg, m.args);
    rt.outRemotes.push_back(std::move(m));
    Task* t = rt.taskFor(L);
    t->state = Task::Parked;
    rt.pendingInvokes[rt.nextCall - 1] = t;
    return lua_yield(L, 0);
}
static int InvokeServer(lua_State* L) {
    if (rtOf(L).dm.isServer()) luaL_error(L, "InvokeServer can only be called from the client");
    return invokeRemote(L, 0, 2);
}
static int InvokeClient(lua_State* L) {
    if (!rtOf(L).dm.isServer()) luaL_error(L, "InvokeClient can only be called from the server");
    return invokeRemote(L, checkPlayerArg(L, 2, "InvokeClient").id(), 3);
}

// Humanoid
// A ForceField stops TakeDamage and nothing else: writing Health directly still works, as on Roblox.
static int TakeDamage(lua_State* L) {
    Instance& h = self(L);
    double d = luaL_checknumber(L, 2);
    if (Instance* ch = h.parent()) if (ch->findFirstChildOfClass("ForceField")) return 0;
    double hp = h.get("Health").n - d;
    h.set("Health", Value::number(hp < 0 ? 0 : hp));
    return 0;
}
static int GetState(lua_State* L) {
    const EnumDef* e = findEnum("HumanoidStateType");
    Instance& h = self(L);
    const std::string& name = h.get("StateName").s;   // the engine writes it: Swimming, Freefall, Seated, Running
    auto item = h.get("Health").n > 0 ? e->find(name.empty() ? "Running" : name.c_str()) : e->find("Dead");
    if (!item) item = e->find("Running");
    pushEnumItem(L, e, item);
    return 1;
}
static int ChangeState(lua_State* L) { (void)self(L); return 0; }
// Parenting an Accessory into the character does the same as AddAccessory, as in Studio.
static int AddAccessory(lua_State* L) {
    Instance& h = self(L);
    Instance* a = rtOf(L).toInstance(L, 2);
    if (!a || !a->isA("Accoutrement")) luaL_error(L, "invalid argument #1 to 'AddAccessory' (Accessory expected, got %s)", typeOfName(L, 2));
    std::string err;
    if (h.parent() && !a->setParent(h.parent(), &err)) luaL_error(L, "%s", err.c_str());
    return 0;
}
static int GetAccessories(lua_State* L) {
    Instance& h = self(L);
    lua_newtable(L);
    int n = 0;
    if (Instance* ch = h.parent())
        for (auto& c : ch->children())
            if (c->isA("Accoutrement")) { rtOf(L).pushInstance(L, c.get()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int RemoveAccessories(lua_State* L) {
    Instance& h = self(L);
    if (Instance* ch = h.parent()) {
        std::vector<Instance::Ptr> worn;
        for (auto& c : ch->children()) if (c->isA("Accoutrement")) worn.push_back(c);
        for (auto& a : worn) a->destroy();
    }
    return 0;
}
static int GetLimb(lua_State* L) {
    Instance& h = self(L);
    Instance& part = rtOf(L).checkInstance(L, 2);
    const EnumDef* e = findEnum("Limb");
    const char* group = part.parent() && part.parent() == h.parent() ? limbGroupOf(part.name()) : nullptr;
    pushEnumItem(L, e, e->find(group ? group : "Unknown"));
    return 1;
}
// An R15 limb by its name; RootPart for the HumanoidRootPart; Unknown for anything else or a part of another rig.
static int GetBodyPartR15(lua_State* L) {
    Instance& h = self(L);
    Instance& part = rtOf(L).checkInstance(L, 2);
    const EnumDef* e = findEnum("BodyPartR15");
    const EnumItem* item = nullptr;
    if (part.parent() && part.parent() == h.parent()) item = e->find(part.name() == "HumanoidRootPart" ? "RootPart" : part.name());
    pushEnumItem(L, e, item ? item : e->find("Unknown"));
    return 1;
}
// The new part takes the old one's name and frame and the Motor6Ds that held it; the old one goes.
static int ReplaceBodyPartR15(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& h = self(L);
    const EnumItem* which = checkEnumItem(L, 2, findEnum("BodyPartR15"));
    Instance& part = rt.checkInstance(L, 3);
    Instance* ch = h.parent();
    std::string name = !std::strcmp(which->name, "RootPart") ? "HumanoidRootPart" : which->name;
    Instance* old = ch ? ch->findFirstChild(name) : nullptr;
    if (!old || !old->isA("BasePart") || !part.isA("BasePart") || &part == old) { lua_pushboolean(L, 0); return 1; }
    Instance::Ptr keep = old->shared_from_this();
    part.setName(name);
    part.set("Position", old->get("Position")); part.set("Orientation", old->get("Orientation"));
    std::vector<Instance::Ptr> moved;
    for (auto& mo : old->children()) if (mo->isA("Motor6D")) moved.push_back(mo);
    for (Instance* d : ch->getDescendants())
        if (d->isA("JointInstance")) for (const char* end : {"Part0", "Part1"})
            if (d->get(end).ref == old->id()) d->set(end, Value::instance(part.id()));
    for (auto& mo : moved) mo->setParent(&part);
    part.setParent(ch);
    old->destroy();
    lua_pushboolean(L, 1);
    return 1;
}
// The root's velocity: the body is moved as one, so that is the humanoid's.
static int GetMoveVelocity(lua_State* L) {
    Instance& h = self(L);
    Instance* root = h.parent() ? h.parent()->findFirstChild("HumanoidRootPart") : nullptr;
    pushVec3(L, root ? root->get("AssemblyLinearVelocity").v : Vec3{0, 0, 0});
    return 1;
}
// A bit per HumanoidStateType value in StatesDisabled. Recorded, not acted on: see the property.
static int GetStateEnabled(lua_State* L) {
    int v = checkEnumItem(L, 2, findEnum("HumanoidStateType"))->value;
    lua_pushboolean(L, !((int64_t)self(L).get("StatesDisabled").n & (int64_t(1) << v)));
    return 1;
}
static int SetStateEnabled(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& h = self(L);
    const EnumItem* st = checkEnumItem(L, 2, findEnum("HumanoidStateType"));
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    bool enabled = lua_toboolean(L, 3);
    int64_t bits = (int64_t)h.get("StatesDisabled").n, bit = int64_t(1) << st->value;
    int64_t next = enabled ? (bits & ~bit) : (bits | bit);
    if (next == bits) return 0;
    h.set("StatesDisabled", Value::number((double)next));
    rt.fireValues(h, "StateEnabledChanged", {Value::enumItem(std::string("HumanoidStateType.") + st->name, st->value), Value::boolean(enabled)});
    return 0;
}
// No emote is loaded here (a description's emotes are recorded, not worn), so none plays.
static int PlayEmoteAsync(lua_State* L) { (void)self(L); luaL_checkstring(L, 2); lua_pushboolean(L, 0); return 1; }
// The Motor6Ds an R15 rig is held by, from its RigAttachments: two parts sharing an attachment
// name that ends in RigAttachment are joined, the part nearer the root as Part0, in a Motor6D
// named for the attachment (NeckRigAttachment -> Neck) and parented to Part1, with C0 and C1
// the two attachments' own frames. A motor of that name already in Part1 is left alone.
static int BuildRigFromAttachments(lua_State* L) {
    Instance& h = self(L);
    Instance* ch = h.parent();
    Instance* root = ch ? ch->findFirstChild("HumanoidRootPart") : nullptr;
    if (!root) return 0;
    static const std::string kSuffix = "RigAttachment";
    auto rigPoints = [](Instance& part) {
        std::vector<Instance*> out;
        for (auto& c : part.children())
            if (c->isA("Attachment") && c->name().size() > kSuffix.size() && c->name().compare(c->name().size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0) out.push_back(c.get());
        return out;
    };
    std::vector<Instance*> queue{root};
    std::set<int64_t> seen{root->id()};
    for (size_t at = 0; at < queue.size(); at++) {
        Instance* p0 = queue[at];
        for (Instance* a0 : rigPoints(*p0)) {
            for (auto& p1 : ch->children()) {
                if (!p1->isA("BasePart") || seen.count(p1->id())) continue;
                Instance* a1 = p1->findFirstChild(a0->name());
                if (!a1 || !a1->isA("Attachment")) continue;
                seen.insert(p1->id());
                queue.push_back(p1.get());
                std::string name = a0->name().substr(0, a0->name().size() - kSuffix.size());
                if (Instance* have = p1->findFirstChild(name); have && have->isA("Motor6D")) continue;
                Instance::Ptr mo = rtOf(L).dm.create("Motor6D", p1.get());
                mo->setName(name);
                mo->set("Part0", Value::instance(p0->id()));
                mo->set("Part1", Value::instance(p1->id()));
                mo->set("C0Position", a0->get("Position")); mo->set("C0Orientation", a0->get("Orientation"));
                mo->set("C1Position", a1->get("Position")); mo->set("C1Orientation", a1->get("Orientation"));
            }
        }
    }
    return 0;
}
// ---- HumanoidDescription ------------------------------------------------------------
static int ApplyDescriptionAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& h = self(L);
    Instance& desc = rt.checkInstance(L, 2);
    if (!desc.isA("HumanoidDescription")) luaL_error(L, "invalid argument #1 to 'ApplyDescriptionAsync' (HumanoidDescription expected, got %s)", desc.className().c_str());
    rt.applyDescription(h, desc);
    return 0;
}
// A copy of the last one applied, else a fresh description: nothing here fetched an avatar to describe.
static int GetAppliedDescription(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& h = self(L);
    auto it = rt.appliedDescriptions.find(h.id());
    Instance::Ptr copy = it != rt.appliedDescriptions.end() ? it->second->clone() : rt.dm.create("HumanoidDescription");
    rt.pushInstance(L, copy.get());
    return 1;
}
// The emotes and the equipped ones are kept as JSON in EmotesBlob / EquippedEmotesBlob, so a
// clone or a saved place carries them. Roblox's shapes: emotes {name = {assetIds}}, equipped
// an array of {Name, Slot}.
static int GetEmotes(lua_State* L) {
    Instance& d = self(L);
    lua_settop(L, 0);
    lua_pushstring(L, d.get("EmotesBlob").s.c_str());
    return json_decode(L);
}
static int SetEmotes(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& d = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 2); lua_remove(L, 1);
    json_encode(L);
    std::string blob = lua_tostring(L, -1);
    if (blob == "[]") blob = "{}";   // an empty Lua table encodes as a list
    if (d.get("EmotesBlob").s == blob) return 0;
    d.set("EmotesBlob", Value::string(blob));
    rt.fireValues(d, "EmotesChanged", {});
    return 0;
}
static int AddEmote(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& d = self(L);
    std::string name = luaL_checkstring(L, 2);
    double id = luaL_checknumber(L, 3);
    lua_settop(L, 0);
    lua_pushstring(L, d.get("EmotesBlob").s.c_str()); json_decode(L);   // the table at 2
    lua_createtable(L, 1, 0); lua_pushnumber(L, id); lua_rawseti(L, -2, 1);
    lua_setfield(L, 2, name.c_str());
    lua_remove(L, 1); json_encode(L);
    d.set("EmotesBlob", Value::string(lua_tostring(L, -1)));
    rt.fireValues(d, "EmotesChanged", {});
    return 0;
}
static int RemoveEmote(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& d = self(L);
    std::string name = luaL_checkstring(L, 2);
    lua_settop(L, 0);
    lua_pushstring(L, d.get("EmotesBlob").s.c_str()); json_decode(L);
    lua_getfield(L, 2, name.c_str());
    bool had = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!had) return 0;
    lua_pushnil(L); lua_setfield(L, 2, name.c_str());
    lua_remove(L, 1); json_encode(L);
    std::string blob = lua_tostring(L, -1);
    if (blob == "[]") blob = "{}";
    d.set("EmotesBlob", Value::string(blob));
    rt.fireValues(d, "EmotesChanged", {});
    return 0;
}
static int GetEquippedEmotes(lua_State* L) {
    Instance& d = self(L);
    lua_settop(L, 0);
    lua_pushstring(L, d.get("EquippedEmotesBlob").s.c_str());
    return json_decode(L);
}
// Takes Roblox's two forms: an array of names (slots in order) or of {Name, Slot} tables.
static int SetEquippedEmotes(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& d = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 2);
    lua_newtable(L);   // 3: the normalised list
    int n = lua_objlen(L, 2);
    for (int k = 1; k <= n; k++) {
        lua_rawgeti(L, 2, k);
        lua_createtable(L, 0, 2);
        if (lua_type(L, -2) == LUA_TSTRING) { lua_pushvalue(L, -2); lua_setfield(L, -2, "Name"); lua_pushnumber(L, k); lua_setfield(L, -2, "Slot"); }
        else if (lua_type(L, -2) == LUA_TTABLE) {
            lua_getfield(L, -2, "Name"); lua_setfield(L, -2, "Name");
            lua_getfield(L, -2, "Slot"); if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushnumber(L, k); } lua_setfield(L, -2, "Slot");
        }
        lua_rawseti(L, 3, k);
        lua_pop(L, 1);
    }
    lua_settop(L, 3); lua_remove(L, 1); lua_remove(L, 1);
    json_encode(L);
    std::string blob = lua_tostring(L, -1);
    if (d.get("EquippedEmotesBlob").s == blob) return 0;
    d.set("EquippedEmotesBlob", Value::string(blob));
    rt.fireValues(d, "EquippedEmotesChanged", {});
    return 0;
}
// AccessoryBlob as a table of {AssetId, AccessoryType, IsLayered, Order, Puffiness}; the
// includeRigidAccessories flag (Roblox's own is over which kinds it lists) keeps them all.
static int DescGetAccessories(lua_State* L) {
    Instance& d = self(L);
    lua_settop(L, 0);
    lua_pushstring(L, d.get("AccessoryBlob").s.c_str());
    json_decode(L);
    // AccessoryType comes back as the enum item it was stored by name
    const EnumDef* e = findEnum("AccessoryType");
    int n = lua_objlen(L, -1);
    for (int k = 1; k <= n; k++) {
        lua_rawgeti(L, -1, k);
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "AccessoryType");
            const EnumItem* it = lua_type(L, -1) == LUA_TSTRING ? e->find(lua_tostring(L, -1)) : nullptr;
            lua_pop(L, 1);
            if (it) { pushEnumItem(L, e, it); lua_setfield(L, -2, "AccessoryType"); }
        }
        lua_pop(L, 1);
    }
    return 1;
}
static int DescSetAccessories(lua_State* L) {
    Instance& d = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 2);
    int n = lua_objlen(L, 2);
    for (int k = 1; k <= n; k++) {   // the enum item is stored by name
        lua_rawgeti(L, 2, k);
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "AccessoryType");
            if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TSTRING) { lua_pushstring(L, checkEnumItem(L, -1, findEnum("AccessoryType"))->name); lua_setfield(L, -3, "AccessoryType"); }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    lua_remove(L, 1);
    json_encode(L);
    std::string blob = lua_tostring(L, -1);
    if (blob == "{}") blob = "[]";
    d.set("AccessoryBlob", Value::string(blob));
    return 0;
}
// ---- animations --------------------------------------------------------------------
// AnimationId is a tree path to a KeyframeSequence ("ReplicatedStorage.Animations.Wave") in
// place of an asset id.
static Instance* animationSequence(Runtime::Impl& rt, Instance* anim) {
    if (!anim) return nullptr;
    std::string path = anim->get("AnimationId").s;
    if (path.empty()) return nullptr;
    Instance* at = rt.dm.root();
    size_t start = 0;
    while (start <= path.size() && at) {
        size_t dot = path.find('.', start);
        std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!part.empty()) at = at->findFirstChild(part) ? at->findFirstChild(part) : rt.dm.getService(part);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return at && at->isA("KeyframeSequence") ? at : nullptr;
}
static double sequenceLength(Instance* seq) {
    double len = 0;
    if (seq) for (auto& k : seq->children()) if (k->isA("Keyframe")) len = std::max(len, k->get("Time").n);
    return len;
}
static Instance& animator(lua_State* L, Instance& i) {
    if (i.isA("Animator")) return i;
    if (Instance* a = i.findFirstChildOfClass("Animator")) return *a;
    luaL_error(L, "no Animator");
}
static int LoadAnimation(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& holder = animator(L, self(L));
    Instance* anim = rt.toInstance(L, 2);
    if (!anim || !anim->isA("Animation")) luaL_error(L, "invalid argument #1 to 'LoadAnimation' (Animation expected, got %s)", typeOfName(L, 2));
    Instance* seq = animationSequence(rt, anim);
    // Roblox leaves an AnimationTrack parentless; under the Animator it replicates to the engine
    // and to clients through the ordinary change log.
    Instance::Ptr track = rt.dm.createInternal("AnimationTrack", &holder);
    track->setName(anim->name());
    track->setInternal("Animation", Value::instance(anim->id()));
    track->setInternal("Length", Value::number(sequenceLength(seq)));
    track->set("Looped", Value::boolean(seq ? seq->get("Loop").b : false));
    if (seq) track->set("Priority", seq->get("Priority"));
    rt.pushInstance(L, track.get());
    return 1;
}
static int GetPlayingAnimationTracks(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& holder = animator(L, self(L));
    lua_newtable(L);
    int n = 0;
    for (auto& c : holder.children())
        if (c->isA("AnimationTrack") && c->get("IsPlaying").b) { rt.pushInstance(L, c.get()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int TrackPlay(lua_State* L) {
    Instance& t = self(L);
    double fade = luaL_optnumber(L, 2, 0.1), weight = luaL_optnumber(L, 3, 1), speed = luaL_optnumber(L, 4, 1);
    t.setInternal("Speed", Value::number(speed));
    t.setInternal("FadeTime", Value::number(fade));
    t.setInternal("Stopping", Value::boolean(false));
    t.setInternal("WeightTarget", Value::number(weight));
    t.setInternal("WeightCurrent", Value::number(fade > 0 ? 0 : weight));   // faded in by the engine
    t.set("TimePosition", Value::number(0));
    t.setInternal("IsPlaying", Value::boolean(true));
    if (Instance* a = t.parent()) rtOf(L).fireValues(*a, "AnimationPlayed", {Value::instance(t.id())});
    return 0;
}
static int TrackStop(lua_State* L) {
    Instance& t = self(L);
    double fade = luaL_optnumber(L, 2, 0.1);
    if (!t.get("IsPlaying").b) return 0;
    if (fade > 0) {   // fade out: the engine ends it when the weight runs out
        t.setInternal("FadeTime", Value::number(fade));
        t.setInternal("WeightTarget", Value::number(0));
        t.setInternal("Stopping", Value::boolean(true));
        return 0;
    }
    t.setInternal("WeightCurrent", Value::number(0));
    t.setInternal("IsPlaying", Value::boolean(false));
    rtOf(L).fireValues(t, "Stopped", {});
    rtOf(L).fireValues(t, "Ended", {});
    return 0;
}
static int AdjustSpeed(lua_State* L) { self(L).setInternal("Speed", Value::number(luaL_optnumber(L, 2, 1))); return 0; }
static int AdjustWeight(lua_State* L) {
    Instance& t = self(L);
    double w = luaL_optnumber(L, 2, 1), fade = luaL_optnumber(L, 3, 0.1);
    t.setInternal("WeightTarget", Value::number(w));
    t.setInternal("FadeTime", Value::number(fade));
    if (fade <= 0) t.setInternal("WeightCurrent", Value::number(w));
    return 0;
}
static int GetTimeOfKeyframe(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    const char* name = luaL_checkstring(L, 2);
    Instance* seq = animationSequence(rt, rt.dm.findRef(t.get("Animation").ref));
    if (seq) for (auto& k : seq->children())
        if (k->isA("Keyframe") && k->name() == name) { lua_pushnumber(L, k->get("Time").n); return 1; }
    luaL_error(L, "There is no keyframe named '%s' in the animation", name);
}
// Fires with the marker's Value when the track passes a Keyframe holding a KeyframeMarker of
// that name (Runtime::fireEvent, on KeyframeReached: the engine reports named keyframes only).
static int GetMarkerReachedSignal(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    rt.pushSignal(L, t, rt.signal(t, std::string("Marker:") + luaL_checkstring(L, 2)));
    return 1;
}
// Animation parameters (a curve clip's inputs). Recorded, not acted on: nothing here blends
// on them, so a parameter reads back what was set and there are no defaults.
static int SetParameter(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    const char* name = luaL_checkstring(L, 2);
    Value v; std::string err;
    if (!rt.toValue(L, 3, Value::Nil, nullptr, v, err)) luaL_error(L, "invalid argument #2 to 'SetParameter' (%s)", err.c_str());
    rt.trackParams[t.id()][name] = v;
    return 0;
}
static int GetParameter(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    auto it = rt.trackParams.find(self(L).id());
    const char* name = luaL_checkstring(L, 2);
    if (it == rt.trackParams.end() || !it->second.count(name)) { lua_pushnil(L); return 1; }
    rt.pushValue(L, it->second[name], nullptr);
    return 1;
}
static int GetParameterDefaults(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
// Sets the parts' velocities from the motors' motion for a rig going limp. Declared and not
// acted on: the physics here takes no velocity from a joint.
static int ApplyJointVelocities(lua_State* L) { (void)self(L); luaL_checktype(L, 2, LUA_TTABLE); return 0; }
// KeyframeSequence / Keyframe / Pose: the children of each kind, Roblox's methods over Parent.
static int childrenOfClass(lua_State* L, const char* cls) {
    Runtime::Impl& rt = rtOf(L);
    lua_newtable(L);
    int n = 0;
    for (auto& c : self(L).children()) if (c->isA(cls)) { rt.pushInstance(L, c.get()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int addOfClass(lua_State* L, const char* cls, const char* method) {
    Instance& i = rtOf(L).checkInstance(L, 2);
    if (!i.isA(cls)) luaL_error(L, "invalid argument #1 to '%s' (%s expected, got %s)", method, cls, i.className().c_str());
    std::string err;
    if (!i.setParent(&self(L), &err)) luaL_error(L, "%s", err.c_str());
    return 0;
}
static int removeOfClass(lua_State* L, const char* cls, const char* method) {
    Instance& i = rtOf(L).checkInstance(L, 2);
    if (!i.isA(cls)) luaL_error(L, "invalid argument #1 to '%s' (%s expected, got %s)", method, cls, i.className().c_str());
    if (i.parent() == &self(L)) { Instance::Ptr keep = i.shared_from_this(); i.setParent(nullptr); }
    return 0;
}
static int GetKeyframes(lua_State* L) { return childrenOfClass(L, "Keyframe"); }
static int AddKeyframe(lua_State* L) { return addOfClass(L, "Keyframe", "AddKeyframe"); }
static int RemoveKeyframe(lua_State* L) { return removeOfClass(L, "Keyframe", "RemoveKeyframe"); }
static int GetPoses(lua_State* L) { return childrenOfClass(L, "Pose"); }
static int AddPose(lua_State* L) { return addOfClass(L, "Pose", "AddPose"); }
static int RemovePose(lua_State* L) { return removeOfClass(L, "Pose", "RemovePose"); }
static int GetMarkers(lua_State* L) { return childrenOfClass(L, "KeyframeMarker"); }
static int AddMarker(lua_State* L) { return addOfClass(L, "KeyframeMarker", "AddMarker"); }
static int RemoveMarker(lua_State* L) { return removeOfClass(L, "KeyframeMarker", "RemoveMarker"); }
static int GetSubPoses(lua_State* L) { return childrenOfClass(L, "Pose"); }
static int AddSubPose(lua_State* L) { return addOfClass(L, "Pose", "AddSubPose"); }
static int RemoveSubPose(lua_State* L) { return removeOfClass(L, "Pose", "RemoveSubPose"); }

static int MoveTo(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    Vec3 target = checkVec3(L, 2);
    if (i.isA("Humanoid")) {
        // The engine walks there and fires MoveToFinished(true), or false after 8 s. Every call
        // restarts the walk, so the same point over again must still record a change.
        Instance* part = lua_isnoneornil(L, 3) ? nullptr : rt.toInstance(L, 3);
        i.set("WalkToPart", Value::instance(part ? part->id() : 0));
        if (part) { Vec3 p = part->get("Position").v; target = {p.x + target.x, p.y + target.y, p.z + target.z}; }
        if (i.get("WalkToPoint").v == target) i.touch("WalkToPoint");
        else i.set("WalkToPoint", Value::vector3(target));
        return 0;
    }
    Instance* prim = rt.dm.findRef(i.get("PrimaryPart").ref);
    if (!prim) for (Instance* d : i.getDescendants()) if (d->isA("BasePart")) { prim = d; break; }
    if (!prim) return 0;
    Vec3 from = prim->get("Position").v;
    Vec3 delta{target.x - from.x, target.y - from.y, target.z - from.z};
    for (Instance* d : i.getDescendants()) if (d->isA("BasePart")) { Vec3 p = d->get("Position").v; d->set("Position", Value::vector3(p.x + delta.x, p.y + delta.y, p.z + delta.z)); }
    return 0;
}
static int Move(lua_State* L) {
    // With relativeToCamera, (0, 0, -1) is forward from the camera rather than north.
    Instance& i = self(L);
    Vec3 d = checkVec3(L, 2);
    if (lua_isboolean(L, 3) && lua_toboolean(L, 3)) {
        Runtime::Impl& rt = rtOf(L);
        if (Instance* cam = rt.dm.find(rt.dm.workspace()->get("CurrentCamera").ref)) {
            // Yaw alone: a character walks on the ground however far the camera is tipped.
            double yaw = cam->get("Orientation").v.y * 3.14159265358979323846 / 180.0;
            double sy = std::sin(yaw), cy = std::cos(yaw);
            Vec3 fwd{(float)-sy, 0, (float)-cy};
            Vec3 right{(float)cy, 0, (float)-sy};
            // The direction arrives in the camera's own axes: +X is its right, -Z its front.
            d = {right.x * d.x + fwd.x * -d.z, d.y, right.z * d.x + fwd.z * -d.z};
        }
    }
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len > 1) d = {d.x / len, d.y / len, d.z / len};
    i.setInternal("MoveDirection", Value::vector3(d));
    return 0;
}
// The lowercase spellings Roblox still takes: part.position, obj:findFirstChild(), part.cframe.
static std::string legacyName(const char* k) {
    std::string s = k;
    if (s.empty() || s[0] < 'a' || s[0] > 'z') return s;
    if (s == "cframe") return "CFrame";
    s[0] = (char)(s[0] - 'a' + 'A');
    return s;
}
static CFrameV partCFrame(Instance& p) { return cframeFromPosOrient(p.get("Position").v, p.get("Orientation").v); }
// A CFrame-valued property is stored as a position + orientation pair; these are the names.
static bool cframeProp(Instance& i, const char* key, std::string& posP, std::string& oriP) {
    if (!std::strcmp(key, "CFrame") && (i.isA("BasePart") || i.isA("Camera") || i.isA("Attachment"))) { posP = "Position"; oriP = "Orientation"; return true; }
    if ((!std::strcmp(key, "C0") || !std::strcmp(key, "C1")) && i.isA("JointInstance")) { posP = std::string(key) + "Position"; oriP = std::string(key) + "Orientation"; return true; }
    if (!std::strcmp(key, "CFrame") && i.isA("Pose")) { posP = "CFramePosition"; oriP = "CFrameOrientation"; return true; }
    if (!std::strcmp(key, "Transform") && (i.isA("Motor6D") || i.isA("Bone") || i.isA("AnimationConstraint"))) { posP = "TransformPosition"; oriP = "TransformOrientation"; return true; }
    if (!std::strcmp(key, "PivotOffset") && i.isA("BasePart")) { posP = "PivotOffsetPosition"; oriP = "PivotOffsetOrientation"; return true; }
    if (!std::strcmp(key, "Focus") && i.isA("Camera")) { posP = "FocusPosition"; oriP = "FocusOrientation"; return true; }
    if (!std::strcmp(key, "DragFrame") && i.isA("DragDetector")) { posP = "DragFramePosition"; oriP = "DragFrameOrientation"; return true; }
    if ((!std::strcmp(key, "CageOrigin") || !std::strcmp(key, "ImportOrigin")) && i.isA("BaseWrap")) { posP = std::string(key) + "Position"; oriP = std::string(key) + "Orientation"; return true; }
    if ((!std::strcmp(key, "BindOffset") || !std::strcmp(key, "ReferenceOrigin")) && i.isA("WrapLayer")) { posP = std::string(key) + "Position"; oriP = std::string(key) + "Orientation"; return true; }
    if (!std::strcmp(key, "AttachmentPoint") && i.isA("Accoutrement")) { posP = "AttachmentPointPosition"; oriP = "AttachmentPointOrientation"; return true; }
    if (!std::strcmp(key, "CFrame") && i.isA("AlignOrientation")) { posP = "CFramePosition"; oriP = "CFrameOrientation"; return true; }   // the OneAttachment goal
    if (!std::strcmp(key, "CFrame") && (i.isA("BodyGyro") || i.isA("HandleAdornment"))) { posP = "CFramePosition"; oriP = "CFrameOrientation"; return true; }
    if (!std::strcmp(key, "Value") && i.isA("CFrameValue")) { posP = "ValuePosition"; oriP = "ValueOrientation"; return true; }
    // the rest family: IKControl's two offsets, a ControllerPartSensor's HitFrame, the rig descriptions' T-pose adjustments
    {
        std::string_view k = key;
        bool pair = (i.isA("IKControl") && (k == "Offset" || k == "EndEffectorOffset")) || (i.isA("ControllerPartSensor") && k == "HitFrame")
                 || (i.isA("HumanoidRigDescription") && (k == "OriginOffset" || (k.size() > 15 && k.substr(k.size() - 15) == "TposeAdjustment")))
                 || (i.isA("DigitsRigDescription") && k.size() > 15 && k.substr(k.size() - 15) == "TposeAdjustment");
        if (pair && i.cls().findProp(std::string(key) + "Position")) { posP = std::string(key) + "Position"; oriP = std::string(key) + "Orientation"; return true; }
    }
    return false;
}
static CFrameV getCFrameProp(Instance& i, const std::string& posP, const std::string& oriP) { return cframeFromPosOrient(i.get(posP).v, i.get(oriP).v); }
static void setCFrameProp(Instance& i, const std::string& posP, const std::string& oriP, const CFrameV& c) {
    Vec3 pos, o; cframeToPosOrient(c, pos, o);
    if (i.isA("CFrameValue")) {
        const bool oriChanges = i.get(oriP) != Value::vector3(o);
        g_cframeValueLastHalf = oriChanges ? oriP.c_str() : posP.c_str();
        i.set(posP, Value::vector3(pos)); i.set(oriP, Value::vector3(o));
        g_cframeValueLastHalf = nullptr;
        return;
    }
    i.set(posP, Value::vector3(pos)); i.set(oriP, Value::vector3(o));
}
static CFrameV attachmentWorld(Instance& a) {
    Instance* p = a.parent();
    CFrameV own = partCFrame(a);
    return p && p->isA("BasePart") ? partCFrame(*p) * own : own;
}
static void setPartCFrame(Instance& p, const CFrameV& c) {
    Vec3 pos, o; cframeToPosOrient(c, pos, o);
    p.set("Position", Value::vector3(pos)); p.set("Orientation", Value::vector3(o));
}
// A Bone's CFrame * Transform, in its parent's frame and in the world: a parent Bone's own transformed frame carries it.
static CFrameV boneTransformed(Instance& b) { return partCFrame(b) * getCFrameProp(b, "TransformPosition", "TransformOrientation"); }
static CFrameV boneTransformedWorld(Instance& b) {
    Instance* p = b.parent();
    CFrameV own = boneTransformed(b);
    if (p && p->isA("Bone")) return boneTransformedWorld(*p) * own;
    return p && p->isA("BasePart") ? partCFrame(*p) * own : own;
}

// ---- the Audio API ------------------------------------------------------------------
// Argument 2 is a time on SoundService:GetMixerTime(), this runtime's clock, and the id handed
// back is what Cancel takes. Stop keeps TimePosition, so the next Play carries on from it.
static int audioSchedule(lua_State* L, bool play) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    if (lua_isnoneornil(L, 2)) { rt.audioPlay(p, play); return 0; }
    const double at = luaL_checknumber(L, 2);
    const int64_t id = rt.nextAudioAction++;
    rt.audioActions.push_back({id, p.id(), at, play});
    lua_pushnumber(L, (double)id);
    return 1;
}
static int AudioPlayerPlay(lua_State* L) { return audioSchedule(L, true); }
static int AudioPlayerStop(lua_State* L) { return audioSchedule(L, false); }
static int AudioPlayerCancel(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    const int64_t id = (int64_t)luaL_checknumber(L, 2);
    for (size_t k = 0; k < rt.audioActions.size(); k++)
        if (rt.audioActions[k].id == id && rt.audioActions[k].player == p.id()) {
            rt.audioActions.erase(rt.audioActions.begin() + k);
            lua_pushboolean(L, 1);
            return 1;
        }
    lua_pushboolean(L, 0);
    return 1;
}
static int GetMixerTime(lua_State* L) { lua_pushnumber(L, rtOf(L).now); return 1; }

static int audioPushPins(lua_State* L, bool output) {
    const std::vector<std::string> pins = audioPins(self(L), output);
    lua_createtable(L, (int)pins.size(), 0);
    for (size_t k = 0; k < pins.size(); k++) { lua_pushstring(L, pins[k].c_str()); lua_rawseti(L, -2, (int)k + 1); }
    return 1;
}
static int AudioGetInputPins(lua_State* L) { return audioPushPins(L, false); }
static int AudioGetOutputPins(lua_State* L) { return audioPushPins(L, true); }
static int AudioGetConnectedWires(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    const std::string pin = luaL_checkstring(L, 2);
    lua_newtable(L);
    int n = 0;
    std::vector<int64_t> ids(rt.wires.begin(), rt.wires.end());
    std::sort(ids.begin(), ids.end());
    for (int64_t id : ids) {
        Instance* w = rt.dm.find(id);
        if (!w || w->destroyed() || !w->get("Connected").b) continue;
        const bool out = (int64_t)w->get("SourceInstance").ref == i.id() && w->get("SourceName").s == pin;
        const bool in = (int64_t)w->get("TargetInstance").ref == i.id() && w->get("TargetName").s == pin;
        if (out || in) { rt.pushInstance(L, w); lua_rawseti(L, -2, ++n); }
    }
    return 1;
}

// GetWaveformAsync(timeRange, samples): the runtime holds no audio data (the host mixer decodes
// the asset), so the table is empty rather than a shape of `samples` zeros.
static int AudioGetWaveformAsync(lua_State* L) {
    self(L);
    if (!lua_isnoneornil(L, 2) && !isNumberRange(L, 2)) luaL_error(L, "GetWaveformAsync: timeRange must be a NumberRange");
    if (!lua_isnoneornil(L, 3)) luaL_checknumber(L, 3);
    lua_newtable(L);
    return 1;
}
// AudioAnalyzer: nothing here measures a stream, so the spectrum is empty.
static int AudioGetSpectrum(lua_State* L) { self(L); lua_newtable(L); return 1; }

// AudioTextToSpeech: nothing here synthesizes speech. The state moves as Roblox's does short of
// a clip: IsPlaying follows Play / Pause, IsLoaded stays false through LoadAsync and Unload.
static int TtsPlay(lua_State* L) { self(L).setInternal("IsPlaying", Value::boolean(true)); return 0; }
static int TtsPause(lua_State* L) { self(L).setInternal("IsPlaying", Value::boolean(false)); return 0; }
static int TtsLoadAsync(lua_State* L) { self(L); return 0; }
static int TtsUnload(lua_State* L) { self(L).setInternal("IsLoaded", Value::boolean(false)); return 0; }

// AudioDeviceInput's access list: user ids, kept comma-joined in the hidden AccessList.
static int AudioGetUserIdAccessList(lua_State* L) {
    const std::string list = self(L).get("AccessList").s;
    lua_newtable(L);
    int n = 0;
    size_t at = 0;
    while (at < list.size()) {
        size_t end = list.find(',', at);
        if (end == std::string::npos) end = list.size();
        if (end > at) { lua_pushnumber(L, std::atof(list.substr(at, end - at).c_str())); lua_rawseti(L, -2, ++n); }
        at = end + 1;
    }
    return 1;
}
static int AudioSetUserIdAccessList(lua_State* L) {
    Instance& i = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::string list;
    char buf[32];
    for (int k = 1;; k++) {
        lua_rawgeti(L, 2, k);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        if (!lua_isnumber(L, -1)) luaL_error(L, "SetUserIdAccessList: user ids must be numbers");
        std::snprintf(buf, sizeof buf, "%.0f", lua_tonumber(L, -1));
        if (!list.empty()) list += ',';
        list += buf;
        lua_pop(L, 1);
    }
    i.set("AccessList", Value::string(list));
    return 0;
}

// AudioRecorder: nothing here records. CanRecordAsync is false, so RecordAsync refuses as Roblox
// does when recording is not permitted; the rest is the empty state.
static int RecorderCanRecordAsync(lua_State* L) { self(L); lua_pushboolean(L, 0); return 1; }
static int RecorderRecordAsync(lua_State* L) { self(L); luaL_error(L, "AudioRecorder:RecordAsync: recording is not permitted here (CanRecordAsync is false)"); }
static int RecorderStop(lua_State* L) { self(L).setInternal("IsRecording", Value::boolean(false)); return 0; }
static int RecorderClear(lua_State* L) { self(L).setInternal("TimeLength", Value::number(0)); return 0; }
static int RecorderGetTemporaryContent(lua_State* L) { self(L); pushContent(L, Value::content(Value::ContentNone)); return 1; }
static int RecorderGetUnrecordableInstancesAsync(lua_State* L) { self(L); lua_newtable(L); return 1; }

static int AudioFilterGetGainAt(lua_State* L) {
    Instance& f = self(L);
    const double at = luaL_checknumber(L, 2);
    if (f.get("Bypass").b) { lua_pushnumber(L, 0); return 1; }
    const std::string type = f.get("FilterType").s;
    lua_pushnumber(L, audio::filterGainAt(type, f.get("Frequency").n, f.get("Gain").n, f.get("Q").n, at));
    return 1;
}

// A curve is a table of key = volume, kept on the instance as a formatted string property.
static audio::Curve readCurve(lua_State* L, int idx, double maxKey, const char* what) {
    audio::Curve c;
    if (lua_isnoneornil(L, idx)) return c;
    luaL_checktype(L, idx, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (!lua_isnumber(L, -2) || !lua_isnumber(L, -1)) luaL_error(L, "%s: keys and values must be numbers", what);
        const double k = lua_tonumber(L, -2), v = lua_tonumber(L, -1);
        if (k < 0 || k > maxKey) luaL_error(L, "%s: key %g is out of range", what, k);
        if (v < 0 || v > 1) luaL_error(L, "%s: volume %g is outside 0 to 1", what, v);
        c.push_back({k, v});
        lua_pop(L, 1);
    }
    if (c.size() > 400) luaL_error(L, "%s: at most 400 points", what);
    std::sort(c.begin(), c.end());
    return c;
}
static int pushCurve(lua_State* L, const std::string& s) {
    const audio::Curve c = audio::parseCurve(s);
    lua_createtable(L, 0, (int)c.size());
    for (auto& [k, v] : c) { lua_pushnumber(L, k); lua_pushnumber(L, v); lua_settable(L, -3); }
    return 1;
}
static int AudioSetDistanceAttenuation(lua_State* L) {
    Instance& i = self(L);
    i.set("DistanceAttenuation", Value::string(audio::formatCurve(readCurve(L, 2, 1e12, "SetDistanceAttenuation"))));
    return 0;
}
static int AudioGetDistanceAttenuation(lua_State* L) { return pushCurve(L, self(L).get("DistanceAttenuation").s); }
static int AudioSetAngleAttenuation(lua_State* L) {
    Instance& i = self(L);
    i.set("AngleAttenuation", Value::string(audio::formatCurve(readCurve(L, 2, 180, "SetAngleAttenuation"))));
    return 0;
}
static int AudioGetAngleAttenuation(lua_State* L) { return pushCurve(L, self(L).get("AngleAttenuation").s); }

// An emitter or listener anywhere but an Attachment, a part, a Camera or a Model has no place
// in the world, and is not heard.
static bool audioPlace(Runtime::Impl& rt, Instance& i, CFrameV& at) {
    Instance* from = i.parent();
    if (i.get("PositionType").s == "Instance") from = rt.dm.findRef(i.get("PositionInstance").ref);
    if (!from) return false;
    if (from->isA("Attachment")) { at = attachmentWorld(*from); return true; }
    if (from->isA("BasePart") || from->isA("Camera")) { at = partCFrame(*from); return true; }
    if (from->isA("Model")) if (Instance* pp = rt.dm.findRef(from->get("PrimaryPart").ref)) { at = partCFrame(*pp); return true; }
    return false;
}
// How loud `emitter` is to `listener`, 0 to 1: distance and angle, on both ends, multiplied.
static double audibility(Runtime::Impl& rt, Instance& emitter, Instance& listener) {
    if (emitter.get("AudioInteractionGroup").s != listener.get("AudioInteractionGroup").s) return 0;
    CFrameV e, l;
    if (!audioPlace(rt, emitter, e) || !audioPlace(rt, listener, l)) return 0;
    const Vec3 d = {l.p.x - e.p.x, l.p.y - e.p.y, l.p.z - e.p.z};
    const double dist = std::sqrt((double)d.x * d.x + (double)d.y * d.y + (double)d.z * d.z);
    const Value bounds = emitter.get("DistanceAttenuationBounds");
    double gain = audio::emitterDistanceGain(emitter.get("DistanceAttenuationMode").s, bounds.u[0], bounds.u[1],
                                             audio::parseCurve(emitter.get("DistanceAttenuation").s), dist);
    gain *= audio::sampleCurve(audio::parseCurve(listener.get("DistanceAttenuation").s), dist, 1);
    // Angles off each end's LookVector, towards the other.
    auto angleOff = [&](const CFrameV& from, double sx, double sy, double sz) {
        const Vec3 look = from.look();
        const double len = std::sqrt(sx * sx + sy * sy + sz * sz);
        if (len < 1e-6) return 0.0;
        const double c = std::clamp((look.x * sx + look.y * sy + look.z * sz) / len, -1.0, 1.0);
        return std::acos(c) * 180.0 / audio::kPi;
    };
    gain *= audio::sampleCurve(audio::parseCurve(emitter.get("AngleAttenuation").s), angleOff(e, d.x, d.y, d.z), 1);
    gain *= audio::sampleCurve(audio::parseCurve(listener.get("AngleAttenuation").s), angleOff(l, -d.x, -d.y, -d.z), 1);
    return std::clamp(gain, 0.0, 1.0);
}
static int AudioGetAudibilityFor(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    Instance& other = rt.checkInstance(L, 2);
    const bool emitter = i.isA("AudioEmitter");
    if (!other.isA(emitter ? "AudioListener" : "AudioEmitter"))
        luaL_error(L, "GetAudibilityFor expects an %s", emitter ? "AudioListener" : "AudioEmitter");
    lua_pushnumber(L, emitter ? audibility(rt, i, other) : audibility(rt, other, i));
    return 1;
}
static int audioInteracting(lua_State* L, const char* otherClass) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    lua_newtable(L);
    int n = 0;
    for (Instance* d : rt.dm.root()->getDescendants())
        if (d->isA(otherClass) && d->get("AudioInteractionGroup").s == i.get("AudioInteractionGroup").s) {
            CFrameV at;
            if (!audioPlace(rt, *d, at)) continue;
            rt.pushInstance(L, d);
            lua_rawseti(L, -2, ++n);
        }
    return 1;
}
static int AudioGetInteractingListeners(lua_State* L) { return audioInteracting(L, "AudioListener"); }
static int AudioGetInteractingEmitters(lua_State* L) { return audioInteracting(L, "AudioEmitter"); }

// Tool.Grip: the CFrame the four Grip vectors make (RightGrip's C1).
static CFrameV toolGrip(Instance& t) {
    CFrameV c; c.p = t.get("GripPos").v;
    Vec3 r = t.get("GripRight").v, u = t.get("GripUp").v, f = t.get("GripForward").v;
    c.m[0] = r.x; c.m[3] = r.y; c.m[6] = r.z;
    c.m[1] = u.x; c.m[4] = u.y; c.m[7] = u.z;
    c.m[2] = -f.x; c.m[5] = -f.y; c.m[8] = -f.z;
    return c;
}
static void setToolGrip(Instance& t, const CFrameV& c) {
    auto clean = [](Vec3 v) { return Vec3{v.x + 0.f, v.y + 0.f, v.z + 0.f}; };   // no -0
    t.set("GripPos", Value::vector3(clean(c.p)));
    t.set("GripRight", Value::vector3(clean(c.col(0))));
    t.set("GripUp", Value::vector3(clean(c.col(1))));
    t.set("GripForward", Value::vector3(clean(c.look())));
}
static Instance* pivotPart(Runtime::Impl& rt, Instance& m) {
    if (m.isA("BasePart")) return &m;
    Instance* prim = rt.dm.findRef(m.get("PrimaryPart").ref);
    if (!prim) for (Instance* d : m.getDescendants()) if (d->isA("BasePart")) return d;
    return prim;
}
static CFrameV pivotOffset(Instance& part) { return getCFrameProp(part, "PivotOffsetPosition", "PivotOffsetOrientation"); }
static CFrameV partPivot(Instance& part) { return partCFrame(part) * pivotOffset(part); }
// A model with no PrimaryPart pivots on the WorldPivot a script wrote, or, never written, on its first part.
static bool storedWorldPivot(Runtime::Impl& rt, Instance& m) {
    return m.isA("Model") && !rt.dm.findRef(m.get("PrimaryPart").ref) && m.get("WorldPivotSet").b;
}
static CFrameV pivotOf(Runtime::Impl& rt, Instance& m) {
    if (m.isA("BasePart")) return partPivot(m);
    if (storedWorldPivot(rt, m)) return getCFrameProp(m, "WorldPivotPosition", "WorldPivotOrientation");
    Instance* p = pivotPart(rt, m);
    return p ? partPivot(*p) : CFrameV::identity();
}
static int GetPivot(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    pushCFrame(L, pivotOf(rt, self(L)));
    return 1;
}
// Model.WorldPivot written: with a PrimaryPart it becomes that part's PivotOffset; without one it is held on the model.
static void setWorldPivot(Runtime::Impl& rt, Instance& m, const CFrameV& target) {
    if (Instance* prim = rt.dm.findRef(m.get("PrimaryPart").ref)) {
        setCFrameProp(*prim, "PivotOffsetPosition", "PivotOffsetOrientation", partCFrame(*prim).inverse() * target);
        return;
    }
    setCFrameProp(m, "WorldPivotPosition", "WorldPivotOrientation", target);
    m.set("WorldPivotSet", Value::boolean(true));
}
// ScaleFactor is measured against the size the model was built at, not the last ScaleTo.
// Rotations are untouched, and a primitive SpecialMesh is already sized by its part, so only a
// file mesh's Scale moves.
static int GetScale(lua_State* L) {
    Instance& m = self(L);
    lua_pushnumber(L, m.get("ScaleFactor").n);
    return 1;
}
static int ScaleTo(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& m = self(L);
    double want = luaL_checknumber(L, 2);
    if (!(want > 0)) luaL_error(L, "ScaleTo: the scale must be greater than 0");
    double had = m.get("ScaleFactor").n;
    if (!(had > 0)) had = 1;
    float k = (float)(want / had);
    Instance* p = pivotPart(rt, m);
    Vec3 pivot = p ? partCFrame(*p).p : Vec3{0, 0, 0};
    auto scaled = [k](Vec3 v) { return Vec3{v.x * k, v.y * k, v.z * k}; };
    for (Instance* d : m.getDescendants()) {
        if (d->isA("BasePart")) {
            d->set("Size", Value::vector3(scaled(d->get("Size").v)));
            Vec3 at = d->get("Position").v;
            d->set("Position", Value::vector3(Vec3{pivot.x + (at.x - pivot.x) * k, pivot.y + (at.y - pivot.y) * k, pivot.z + (at.z - pivot.z) * k}));
        } else if (d->isA("Attachment")) {
            d->set("Position", Value::vector3(scaled(d->get("Position").v)));
        } else if (d->isA("JointInstance")) {
            d->set("C0Position", Value::vector3(scaled(d->get("C0Position").v)));
            d->set("C1Position", Value::vector3(scaled(d->get("C1Position").v)));
        } else if (d->isA("DataModelMesh")) {
            d->set("Offset", Value::vector3(scaled(d->get("Offset").v)));
            const bool file = d->isA("FileMesh") && (!d->cls().findProp("MeshType") || d->get("MeshType").s == "FileMesh");
            if (file) d->set("Scale", Value::vector3(scaled(d->get("Scale").v)));
        } else if (d->isA("Beam")) {
            for (const char* n : {"Width0", "Width1", "CurveSize0", "CurveSize1"})
                d->set(n, Value::number(d->get(n).n * k));
        }
    }
    // A WeldConstraint holds the spacing it took hold at, so it must take hold again or it pulls
    // the scaled parts back.
    for (Instance* d : m.getDescendants()) {
        if (d->isA("WeldConstraint") && d->get("Enabled").b) {
            d->set("Enabled", Value::boolean(false));
            d->set("Enabled", Value::boolean(true));
        }
    }
    m.set("ScaleFactor", Value::number(want));
    return 0;
}
static void pivotTo(Runtime::Impl& rt, Instance& m, const CFrameV& target) {
    if (m.isA("BasePart")) { setPartCFrame(m, target * pivotOffset(m).inverse()); return; }
    const bool stored = storedWorldPivot(rt, m);
    if (!stored && !pivotPart(rt, m)) return;
    CFrameV delta = target * pivotOf(rt, m).inverse();
    for (Instance* d : m.getDescendants()) if (d->isA("BasePart")) setPartCFrame(*d, delta * partCFrame(*d));
    if (stored) setCFrameProp(m, "WorldPivotPosition", "WorldPivotOrientation", target);
}
static int PivotTo(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    pivotTo(rt, self(L), checkCFrame(L, 2));
    return 0;
}
// The pivot moved by a vector, the orientation kept.
static int TranslateBy(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& m = self(L);
    CFrameV pivot = pivotOf(rt, m);
    pivot.p = add(pivot.p, checkVec3(L, 2));
    pivotTo(rt, m, pivot);
    return 0;
}
// The voxel field lives on the host, so terrain edits queue as ops for the end of the frame and
// the host writes Heights back as one property change. A script sees the result the frame after
// it asks, as it would on a Roblox server.
static int terrainMaterial(lua_State* L, int idx) {
    if (lua_isnoneornil(L, idx)) return 1280;                      // Grass
    return checkEnumItem(L, idx, findEnum("Material"))->value;
}
// kind: 0 impulse, 1 impulse at a position, 2 angular. Applied to the assembly in the host's
// physics at the end of the frame; an anchored part takes none, as on Roblox.
static int applyImpulse(lua_State* L, int kind) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    Runtime::Impulse im;
    im.part = i.id();
    im.kind = kind;
    im.v = checkVec3(L, 2);
    if (kind == 1) im.at = checkVec3(L, 3);
    if (!i.get("Anchored").b) rt.impulses.push_back(im);
    return 0;
}
static int ApplyImpulse(lua_State* L) { return applyImpulse(L, 0); }
static int ApplyImpulseAtPosition(lua_State* L) { return applyImpulse(L, 1); }
static int ApplyAngularImpulse(lua_State* L) { return applyImpulse(L, 2); }
static int FillBlock(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Runtime::TerrainOp op;
    op.kind = 0;
    const CFrameV& cf = checkCFrame(L, 2);
    op.pos = cf.p;
    for (int i = 0; i < 9; i++) op.rot[i] = cf.m[i];
    op.size = checkVec3(L, 3);
    op.material = terrainMaterial(L, 4);
    rt.terrainOps.push_back(op);
    return 0;
}
static int FillBall(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Runtime::TerrainOp op;
    op.kind = 1;
    op.pos = checkVec3(L, 2);
    op.radius = (float)luaL_checknumber(L, 3);
    op.material = terrainMaterial(L, 4);
    rt.terrainOps.push_back(op);
    return 0;
}
static int ClearTerrain(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Runtime::TerrainOp op;
    op.kind = 2;
    rt.terrainOps.push_back(op);
    return 0;
}

// volume x density: the material's, or CustomPhysicalProperties' when it has one
static double massOf(Instance& p) {
    Vec3 s = p.get("Size").v;
    MaterialPhysics m = physicsOf(p.get("Material").s, p.get("CustomPhysicalProperties"));
    return s.x * s.y * s.z * m.density;
}
// Everything reached over enabled Welds, Motor6Ds, Snaps, ManualWelds and WeldConstraints with
// both parts set; a HingeConstraint or a rope joins nothing. The host builds the body it
// simulates from the same joints (weld_root), so the two agree.
static std::vector<Instance*> assemblyOf(Runtime::Impl& rt, Instance& part) {
    std::vector<Instance*> out{&part};
    std::unordered_set<int64_t> seen{part.id()};
    Instance* ws = rt.dm.getService("Workspace");
    if (!ws) return out;
    std::vector<std::pair<int64_t, int64_t>> links;
    for (Instance* d : ws->getDescendants())
        if ((d->isA("JointInstance") || d->isA("WeldConstraint")) && d->get("Enabled").b && !d->isA("Rotate") && !d->isA("DynamicRotate")) {   // a Rotate turns: not rigid
            int64_t a = d->get("Part0").ref, b = d->get("Part1").ref;
            if (a && b && a != b) links.push_back({a, b});
        }
    for (size_t k = 0; k < out.size(); k++) {
        int64_t id = out[k]->id();
        for (auto& [a, b] : links) {
            int64_t other = a == id ? b : b == id ? a : 0;
            if (!other || seen.count(other)) continue;
            Instance* o = rt.dm.find(other);
            if (!o || o->destroyed() || !o->isA("BasePart")) continue;
            seen.insert(other);
            out.push_back(o);
        }
    }
    return out;
}
// Roblox's root: the highest RootPriority wins outright, then an anchored part, then the
// heaviest. The priority comes first because it is the one a place sets to say which part an
// assembly should be steered and positioned by, and it would be pointless if mass outvoted it.
static Instance* assemblyRoot(const std::vector<Instance*>& parts) {
    Instance* chosen = nullptr;
    double best = 0;
    for (Instance* p : parts) {
        const double pri = p->get("RootPriority").n;
        if (!chosen || pri > best) { chosen = p; best = pri; }
    }
    if (chosen && best != 0) return chosen;
    for (Instance* p : parts) if (p->get("Anchored").b) return p;
    Instance* heaviest = nullptr;
    double bestMass = -1;
    for (Instance* p : parts) { double m = massOf(*p); if (m > bestMass) { bestMass = m; heaviest = p; } }
    return heaviest;
}
// Massless parts weigh nothing in an assembly, unless the assembly is only them.
static double assemblyMass(const std::vector<Instance*>& parts) {
    double sum = 0;
    int counted = 0;
    for (Instance* p : parts) if (!p->get("Massless").b) { sum += massOf(*p); counted++; }
    if (!counted && !parts.empty()) sum = massOf(*parts[0]);
    return sum;
}
static Vec3 assemblyCentre(const std::vector<Instance*>& parts, Instance& self) {
    double x = 0, y = 0, z = 0, tm = 0;
    for (Instance* p : parts) {
        double w = p->get("Massless").b ? 0 : massOf(*p);
        Vec3 pp = p->get("Position").v;
        x += pp.x * w; y += pp.y * w; z += pp.z * w; tm += w;
    }
    if (tm <= 0) return self.get("Position").v;
    return Vec3{(float)(x / tm), (float)(y / tm), (float)(z / tm)};
}
static int GetMass(lua_State* L) { lua_pushnumber(L, massOf(self(L))); return 1; }
// The rest of the assembly; the recursive argument makes no difference here.
static int GetConnectedParts(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    std::vector<Instance*> parts = assemblyOf(rt, i);
    lua_createtable(L, (int)parts.size(), 0);
    int n = 0;
    for (Instance* p : parts) { if (p == &i) continue; rt.pushInstance(L, p); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int GetRootPart(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    rt.pushInstance(L, assemblyRoot(assemblyOf(rt, i)));
    return 1;
}
static int Noop(lua_State* L) { (void)self(L); return 0; }
// The world-aligned box round the model's parts (their Positions and Sizes, rotation ignored).
static bool modelExtents(Instance& m, Vec3& centre, Vec3& size) {
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    for (Instance* d : m.getDescendants()) if (d->isA("BasePart")) {
        Vec3 p = d->get("Position").v, s = d->get("Size").v;
        float pp[3] = {p.x, p.y, p.z}, ss[3] = {s.x, s.y, s.z};
        for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], pp[k] - ss[k] / 2); hi[k] = std::max(hi[k], pp[k] + ss[k] / 2); }
        any = true;
    }
    if (!any) { centre = {0, 0, 0}; size = {0, 0, 0}; return false; }
    centre = {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
    size = {hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]};
    return true;
}
static int GetBoundingBox(lua_State* L) {
    Vec3 c, sz;
    modelExtents(self(L), c, sz);
    pushCFrame(L, CFrameV::fromPos(c));
    pushVec3(L, sz);
    return 2;
}
static int GetExtentsSize(lua_State* L) {
    Vec3 c, sz;
    modelExtents(self(L), c, sz);
    pushVec3(L, sz);
    return 1;
}

// TweenService / Tween
static Tween* findTween(Runtime::Impl& rt, Instance& t) {
    for (auto& tw : rt.tweens) if (tw->tween.get() == &t) return tw.get();
    return nullptr;
}
static int TweenCreate(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& obj = rt.checkInstance(L, 2);
    TweenInfoV info = checkTweenInfo(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    auto tw = std::make_unique<Tween>();
    tw->obj = obj.shared_from_this();
    tw->info = info;
    lua_pushnil(L);
    while (lua_next(L, 4)) {
        const char* prop = lua_tostring(L, -2);
        if (!prop) luaL_error(L, "TweenService:Create property table must use string keys");
        Tween::Prop p; p.name = prop;
        if (cframeProp(obj, prop, p.cfPos, p.cfOri)) {
            if (!isCFrame(L, -1)) luaL_error(L, "TweenService:Create property '%s' expects a CFrame", prop);
            p.isCFrame = true; p.cfTo = checkCFrame(L, -1);
        } else {
            const PropDef* d = obj.cls().findProp(prop);
            if (!d) luaL_error(L, "%s is not a valid member of %s", prop, obj.className().c_str());
            if (d->type != Value::Number && d->type != Value::Vector3 && d->type != Value::Vector2 && d->type != Value::Color3 &&
                d->type != Value::UDim && d->type != Value::UDim2)
                luaL_error(L, "TweenService:Create property '%s' cannot be tweened", prop);
            std::string err;
            if (!rt.toValue(L, -1, d->type, d->enumType, p.to, err))
                luaL_error(L, "TweenService:Create property '%s' expects %s, got %s", prop, Value::typeName(d->type), err.c_str());
        }
        tw->props.push_back(std::move(p));
        lua_pop(L, 1);
    }
    Instance::Ptr ti = rt.dm.createInternal("Tween");
    ti->setName("Tween");
    tw->tween = ti;
    rt.pushInstance(L, ti.get());
    rt.tweens.push_back(std::move(tw));
    return 1;
}
// ---- PathfindingService ------------------------------------------------------------
static PathState* findPath(Runtime::Impl& rt, Instance& i) {
    for (auto& p : rt.paths) if (p->inst.get() == &i) return p.get();
    return nullptr;
}
static PathAgent readAgent(lua_State* L, int idx) {
    PathAgent a;
    if (lua_isnoneornil(L, idx)) return a;
    luaL_checktype(L, idx, LUA_TTABLE);
    auto num = [&](const char* k, float& v) { lua_getfield(L, idx, k); if (lua_isnumber(L, -1)) v = (float)lua_tonumber(L, -1); lua_pop(L, 1); };
    auto flag = [&](const char* k, bool& v) { lua_getfield(L, idx, k); if (lua_isboolean(L, -1)) v = lua_toboolean(L, -1); lua_pop(L, 1); };
    num("AgentRadius", a.radius); num("AgentHeight", a.height); num("WaypointSpacing", a.spacing);
    flag("AgentCanJump", a.canJump); flag("AgentCanClimb", a.canClimb);
    lua_getfield(L, idx, "Costs");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_isstring(L, -2) && lua_isnumber(L, -1)) a.costs[lua_tostring(L, -2)] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    a.radius = std::max(0.5f, a.radius); a.height = std::max(1.f, a.height);
    return a;
}
static int PathCreate(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    auto ps = std::make_unique<PathState>();
    ps->agent = readAgent(L, 2);
    Instance::Ptr pi = rt.dm.createInternal("Path");
    pi->setName("Path");
    ps->inst = pi;
    rt.pushInstance(L, pi.get());
    rt.paths.push_back(std::move(ps));
    return 1;
}
static const char* kPathStatus[] = {"Success", "ClosestNoPath", "ClosestOutOfRange", "FailStartNotEmpty", "FailFinishNotEmpty", "NoPath"};
static void computeInto(Runtime::Impl& rt, PathState& ps, Vec3 start, Vec3 goal) {
    int status = computePath(rt.dm, ps.agent, start, goal, ps.points);
    ps.blockedAt = 0;
    ps.inst->setInternal("Status", Value::enumItem(kPathStatus[status], status));
}
static Vec3 pathPoint(lua_State* L, int idx) {
    Runtime::Impl& rt = rtOf(L);
    if (Instance* i = rt.toInstance(L, idx); i && i->isA("BasePart")) return i->get("Position").v;
    return checkVec3(L, idx);
}
static int PathComputeAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    PathState* ps = findPath(rt, self(L));
    if (!ps) return 0;
    Vec3 s = pathPoint(L, 2), g = pathPoint(L, 3);
    computeInto(rt, *ps, s, g);
    return 0;
}
static int PathGetWaypoints(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    PathState* ps = findPath(rt, self(L));
    lua_createtable(L, ps ? (int)ps->points.size() : 0, 0);
    if (!ps) return 1;
    const EnumDef* actions = findEnum("PathWaypointAction");
    int n = 0;
    for (auto& w : ps->points) {
        lua_createtable(L, 0, 3);
        pushVec3(L, w.pos); lua_setfield(L, -2, "Position");
        rt.pushValue(L, Value::enumItem(w.action == 1 ? "Jump" : w.action == 2 ? "Custom" : "Walk", w.action), actions); lua_setfield(L, -2, "Action");
        lua_pushstring(L, w.label.c_str()); lua_setfield(L, -2, "Label");
        lua_setreadonly(L, -1, true);
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}
static int PathCheckOccupancy(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    PathState* ps = findPath(rt, self(L));
    PathAgent def;
    lua_pushboolean(L, pathOccupied(rt.dm, ps ? ps->agent : def, checkVec3(L, 2)));
    return 1;
}
// The older one-shot form of CreatePath plus ComputeAsync.
static int PathFind(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Vec3 s = pathPoint(L, 2), g = pathPoint(L, 3);
    auto ps = std::make_unique<PathState>();
    Instance::Ptr pi = rt.dm.createInternal("Path");
    pi->setName("Path");
    ps->inst = pi;
    computeInto(rt, *ps, s, g);
    rt.pushInstance(L, pi.get());
    rt.paths.push_back(std::move(ps));
    return 1;
}
static int TweenGetValue(lua_State* L) {
    float a = (float)luaL_checknumber(L, 2);
    int style = checkEnumItem(L, 3, findEnum("EasingStyle"))->value;
    int dir = checkEnumItem(L, 4, findEnum("EasingDirection"))->value;
    lua_pushnumber(L, ease(style, dir, a));
    return 1;
}
static int TweenPlay(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    Tween* tw = findTween(rt, t);
    if (!tw) return 0;
    // A newer tween on the same property wins: cancel the older ones.
    for (auto& o : rt.tweens) if (o.get() != tw && o->obj == tw->obj && o->playing)
        for (auto& p : o->props) for (auto& q : tw->props) if (p.name == q.name) { o->playing = false; o->done = true; o->tween->setInternal("PlaybackState", Value::enumItem("Cancelled", 5)); }
    if (tw->done || !tw->playing) {
        if (tw->done) { tw->elapsed = 0; tw->cycle = 0; tw->reversed = false; tw->done = false; }
        // The start values are whatever the object holds at Play, not at Create.
        for (auto& p : tw->props) {
            if (p.isCFrame) p.cfFrom = getCFrameProp(*tw->obj, p.cfPos, p.cfOri);
            else p.from = tw->obj->get(p.name);
        }
    }
    tw->playing = true;
    t.setInternal("PlaybackState", Value::enumItem(tw->info.delay > 0 ? "Delayed" : "Playing", tw->info.delay > 0 ? 1 : 2));
    return 0;
}
static int TweenPause(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    if (Tween* tw = findTween(rt, self(L))) { tw->playing = false; tw->tween->setInternal("PlaybackState", Value::enumItem("Paused", 3)); }
    return 0;
}
static int TweenCancel(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    if (Tween* tw = findTween(rt, t)) {
        tw->playing = false; tw->done = true; tw->elapsed = 0;
        t.setInternal("PlaybackState", Value::enumItem("Cancelled", 5));
        rt.fireValues(t, "Completed", {Value::enumItem("PlaybackState.Cancelled", 5)});
    }
    return 0;
}

// Debris
static int debrisDestroy(lua_State* L) {
    if (Instance* i = rtOf(L).toInstance(L, 1)) i->destroy();
    return 0;
}
static int AddItem(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& item = rt.checkInstance(L, 2);
    double life = luaL_optnumber(L, 3, 10);
    Task* t = rt.newTask(L, rt.ctxOf(L));
    lua_pushcfunction(t->co, debrisDestroy, "Debris");
    rt.pushInstance(t->co, &item);
    t->state = Task::Sleeping;
    t->sleepStart = rt.now;
    t->wakeAt = rt.now + life;
    rt.sleepers.push_back(t);
    return 0;
}

// CollectionService
static int GetTagged(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    const char* tag = luaL_checkstring(L, 2);
    lua_newtable(L);
    int n = 0;
    for (Instance* d : rt.dm.root()->getDescendants()) if (d->hasTag(tag)) { rt.pushInstance(L, d); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int CsAddTag(lua_State* L) { rtOf(L).checkInstance(L, 2).addTag(luaL_checkstring(L, 3)); return 0; }
static int CsRemoveTag(lua_State* L) { rtOf(L).checkInstance(L, 2).removeTag(luaL_checkstring(L, 3)); return 0; }
static int CsHasTag(lua_State* L) { lua_pushboolean(L, rtOf(L).checkInstance(L, 2).hasTag(luaL_checkstring(L, 3))); return 1; }
static int CsGetTags(lua_State* L) {
    auto& tags = rtOf(L).checkInstance(L, 2).tags();
    lua_createtable(L, (int)tags.size(), 0);
    int n = 0;
    for (auto& t : tags) { lua_pushstring(L, t.c_str()); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int GetInstanceAddedSignal(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& cs = self(L);
    rt.pushSignal(L, cs, rt.signal(cs, std::string("Tag+:") + luaL_checkstring(L, 2))); return 1;
}
static int GetInstanceRemovedSignal(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& cs = self(L);
    rt.pushSignal(L, cs, rt.signal(cs, std::string("Tag-:") + luaL_checkstring(L, 2))); return 1;
}

// HttpService
static int JSONEncode(lua_State* L) { lua_remove(L, 1); return json_encode(L); }
static int JSONDecode(lua_State* L) { lua_remove(L, 1); return json_decode(L); }
static int GenerateGUID(lua_State* L) {
    bool wrap = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
    static std::mt19937_64 rng{std::random_device{}()};
    uint64_t a = rng(), b = rng();
    char buf[64];
    std::snprintf(buf, sizeof buf, "%08X-%04X-4%03X-%04X-%012llX", (unsigned)(a >> 32), (unsigned)(a >> 16) & 0xFFFF,
                  (unsigned)a & 0xFFF, (unsigned)((b >> 48) & 0x3FFF) | 0x8000, (unsigned long long)(b & 0xFFFFFFFFFFFFull));
    if (wrap) { std::string s = std::string("{") + buf + "}"; lua_pushstring(L, s.c_str()); }
    else lua_pushstring(L, buf);
    return 1;
}
// Stores live on this machine, persisted to Options::dataStorePath (the Godot node's
// data_store_path). The *Async methods return at once: no request budget, no throttling.
static std::map<std::string, std::string>& storeOf(lua_State* L, Instance& ds) {
    Runtime::Impl& rt = rtOf(L);
    auto it = rt.dataStoreKeys.find(ds.id());
    if (it == rt.dataStoreKeys.end()) luaL_error(L, "%s is not a DataStore", ds.className().c_str());
    return rt.dataStores[it->second.first][it->second.second];
}
// mode: 0 a DataStore, 1 an OrderedDataStore, 2 the legacy global store (GetGlobalDataStore)
static int getStore(lua_State* L, int mode) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    if (!rt.dm.isServer()) luaL_error(L, "DataStore can't be accessed from client");
    bool ordered = mode == 1;
    size_t n = 0; const char* name = mode == 2 ? "" : luaL_checklstring(L, 2, &n);
    if (mode != 2 && (n == 0 || n > 50)) luaL_error(L, "DataStore name can't be empty or longer than 50 characters");
    std::string scope = mode == 2 ? "global" : luaL_optstring(L, 3, "global");
    // An ordered store is its own namespace: "name" and "name" ordered do not meet; so is the global default.
    std::string stored = ordered ? "ordered:" + std::string(name, n) : mode == 2 ? "default:" : std::string(name, n);
    std::string key = stored + '\n' + scope;
    auto it = rt.dataStoreObjects.find(key);
    if (it == rt.dataStoreObjects.end()) {
        Instance::Ptr ds = rt.dm.createInternal(ordered ? "OrderedDataStore" : mode == 2 ? "GlobalDataStore" : "DataStore");
        ds->setName(mode == 2 ? "GlobalDataStore" : name);
        rt.dataStoreKeys[ds->id()] = {stored, scope};
        it = rt.dataStoreObjects.emplace(key, ds).first;
    }
    rt.pushInstance(L, it->second.get());
    return 1;
}
static int GetDataStore(lua_State* L) { return getStore(L, 0); }
static int GetOrderedDataStore(lua_State* L) { return getStore(L, 1); }
// An OrderedDataStore holds integers only, so GetSortedAsync can sort them.
static void checkOrderedValue(lua_State* L, Instance& ds, int idx) {
    if (!ds.isA("OrderedDataStore")) return;
    if (!lua_isnumber(L, idx) || lua_tonumber(L, idx) != std::floor(lua_tonumber(L, idx)))
        luaL_error(L, "OrderedDataStore values must be integers");
}
static int GetRequestBudgetForRequestType(lua_State* L) { lua_pushinteger(L, 100); return 1; }
static const char* dsKey(lua_State* L) {
    size_t n; const char* k = luaL_checklstring(L, 2, &n);
    if (n == 0 || n > 50) luaL_error(L, "DataStore key can't be empty or longer than 50 characters");
    return k;
}
static void pushJson(lua_State* L, const std::string& text) {
    lua_pushcfunction(L, json_decode, "JSONDecode");
    lua_pushlstring(L, text.data(), text.size());
    lua_call(L, 1, 1);
}
static std::string toJson(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);
    lua_pushcfunction(L, json_encode, "JSONEncode");
    lua_pushvalue(L, idx);
    lua_call(L, 1, 1);
    size_t n; const char* s = lua_tolstring(L, -1, &n);
    std::string r(s, n);
    lua_pop(L, 1);
    return r;
}
static std::vector<Runtime::Impl::DsVersion>& versionsOf(lua_State* L, Instance& ds, const std::string& key) {
    Runtime::Impl& rt = rtOf(L);
    auto it = rt.dataStoreKeys.find(ds.id());
    if (it == rt.dataStoreKeys.end()) luaL_error(L, "%s is not a DataStore", ds.className().c_str());
    return rt.dataStoreVersions[it->second.first][it->second.second][key];
}
static std::string writeVersion(lua_State* L, Instance& ds, const std::string& key, const std::string& json, bool deleted) {
    Runtime::Impl& rt = rtOf(L);
    auto it = rt.dataStoreKeys.find(ds.id());
    if (it == rt.dataStoreKeys.end()) return "";
    return rt.recordVersion(it->second.first, it->second.second, key, json, deleted);
}
static void pushKeyInfo(lua_State* L, Instance& ds, const std::string& key) {
    auto& list = versionsOf(L, ds, key);
    if (list.empty()) { lua_pushnil(L); return; }
    Runtime::Impl& rt = rtOf(L);
    Instance::Ptr info = rt.dm.createInternal("DataStoreKeyInfo");
    info->setName("DataStoreKeyInfo");
    info->setInternal("Version", Value::string(list.back().version));
    info->setInternal("CreatedTime", Value::number(list.front().time));
    info->setInternal("UpdatedTime", Value::number(list.back().time));
    if (!list.back().users.empty()) info->setInternal("UserIds", Value::string(list.back().users));
    if (!list.back().meta.empty()) info->setInternal("Metadata", Value::string(list.back().meta));
    rt.pushInstance(L, info.get());
}
// The user ids (an array) and options (a DataStoreSetOptions / DataStoreIncrementOptions, or a
// table with the metadata) a write named, kept on the version just recorded.
static void tagVersion(lua_State* L, Instance& ds, const std::string& key, int usersIdx, int optionsIdx) {
    auto& list = versionsOf(L, ds, key);
    if (list.empty()) return;
    if (usersIdx && lua_istable(L, usersIdx)) list.back().users = toJson(L, usersIdx);
    if (optionsIdx && !lua_isnoneornil(L, optionsIdx)) {
        if (Instance* o = rtOf(L).toInstance(L, optionsIdx)) { if (o->cls().findProp("Metadata")) list.back().meta = o->get("Metadata").s; }
        else if (lua_istable(L, optionsIdx)) list.back().meta = toJson(L, optionsIdx);
    }
}
static int DsGetAsync(lua_State* L) {
    Instance& ds = self(L);
    auto& st = storeOf(L, ds);
    std::string k = dsKey(L);
    auto it = st.find(k);
    if (it == st.end()) lua_pushnil(L); else pushJson(L, it->second);
    pushKeyInfo(L, ds, k);   // Roblox hands the DataStoreKeyInfo back with the value
    return 2;
}
static int DsSetAsync(lua_State* L) {
    Instance& ds = self(L);
    auto& st = storeOf(L, ds);
    std::string k = dsKey(L);
    if (lua_isnoneornil(L, 3)) luaL_error(L, "Argument 2 missing or nil");
    checkOrderedValue(L, ds, 3);
    st[k] = toJson(L, 3);
    rtOf(L).dataStoresDirty = true;
    lua_pushstring(L, writeVersion(L, ds, k, st[k], false).c_str());
    tagVersion(L, ds, k, 4, 5);
    return 1;
}
static int DsUpdateAsync(lua_State* L) {
    // The transform sees the current value and returns the new one; returning nil cancels.
    auto& st = storeOf(L, self(L));
    std::string k = dsKey(L);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    auto it = st.find(k);
    if (it == st.end()) lua_pushnil(L); else pushJson(L, it->second);
    lua_call(L, 1, 3);   // (value, userIds, metadata)
    if (lua_isnil(L, -3)) { lua_pop(L, 2); pushKeyInfo(L, self(L), k); return 2; }
    checkOrderedValue(L, self(L), -3);
    st[k] = toJson(L, -3);
    rtOf(L).dataStoresDirty = true;
    writeVersion(L, self(L), k, st[k], false);
    tagVersion(L, self(L), k, lua_gettop(L) - 1, lua_gettop(L));
    lua_pop(L, 2);
    pushKeyInfo(L, self(L), k);
    return 2;
}
static int DsIncrementAsync(lua_State* L) {
    auto& st = storeOf(L, self(L));
    std::string k = dsKey(L);
    double delta = luaL_optnumber(L, 3, 1);
    double cur = 0;
    auto it = st.find(k);
    if (it != st.end()) {
        pushJson(L, it->second);
        if (!lua_isnumber(L, -1)) luaL_error(L, "IncrementAsync: the value at key %s is not a number", k.c_str());
        cur = lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    lua_pushnumber(L, cur + delta);
    checkOrderedValue(L, self(L), -1);
    st[k] = toJson(L, -1);
    rtOf(L).dataStoresDirty = true;
    writeVersion(L, self(L), k, st[k], false);   // IncrementAsync hands back the value alone, as Roblox's does
    tagVersion(L, self(L), k, 4, 5);
    return 1;
}
static int DsRemoveAsync(lua_State* L) {
    Instance& ds = self(L);
    auto& st = storeOf(L, ds);
    std::string k = dsKey(L);
    auto it = st.find(k);
    if (it == st.end()) { lua_pushnil(L); lua_pushnil(L); return 2; }
    pushJson(L, it->second);
    std::string was = it->second;
    st.erase(it);
    rtOf(L).dataStoresDirty = true;
    writeVersion(L, ds, k, was, true);   // a removal is a version of its own, marked deleted
    pushKeyInfo(L, ds, k);
    return 2;
}

// ---- versions and listings ----------------------------------------------------------
// A Pages whose rows are instances: DataStoreKey, DataStoreInfo, DataStoreObjectVersionInfo.
static Instance::Ptr makePages(lua_State* L, const char* className, std::vector<Instance::Ptr> rows, size_t pageSize) {
    Runtime::Impl& rt = rtOf(L);
    Runtime::Impl::Pages pg;
    pg.pageSize = pageSize;
    pg.objects = std::move(rows);
    Instance::Ptr pages = rt.dm.createInternal(className);
    pages->setName(className);
    pages->setInternal("IsFinished", Value::boolean(pg.objects.size() <= pg.pageSize));
    rt.dataStorePages[pages->id()] = std::move(pg);
    return pages;
}
static size_t checkPageSize(lua_State* L, int idx) {
    lua_Integer n = luaL_optinteger(L, idx, 0);
    if (n == 0) return 50;
    if (n < 1 || n > 100) luaL_error(L, "pageSize must be between 1 and 100");
    return (size_t)n;
}
// A cursor is a Cursor read off an earlier listing's pages: the position to resume from.
static void seekPages(lua_State* L, Instance& pages, int cursorIdx) {
    if (lua_isnoneornil(L, cursorIdx)) return;
    const char* c = luaL_checkstring(L, cursorIdx);
    if (!*c) return;
    auto& pg = rtOf(L).dataStorePages[pages.id()];
    pg.pos = std::min(pg.count(), (size_t)std::strtoull(c, nullptr, 10));
    pages.setInternal("Cursor", Value::string(pg.pos ? std::to_string(pg.pos) : ""));
    pages.setInternal("IsFinished", Value::boolean(pg.pos + pg.pageSize >= pg.count()));
}
static int DsListKeysAsync(lua_State* L) {   // (prefix, pageSize, cursor, excludeDeleted)
    Runtime::Impl& rt = rtOf(L);
    Instance& ds = self(L);
    auto& st = storeOf(L, ds);
    std::string prefix = luaL_optstring(L, 2, "");
    size_t pageSize = checkPageSize(L, 3);
    std::vector<Instance::Ptr> rows;
    for (auto& [k, v] : st) {
        if (k.compare(0, prefix.size(), prefix) != 0) continue;
        Instance::Ptr key = rt.dm.createInternal("DataStoreKey");
        key->setName("DataStoreKey");
        key->setInternal("KeyName", Value::string(k));
        rows.push_back(key);
    }
    Instance::Ptr pages = makePages(L, "DataStoreKeyPages", std::move(rows), pageSize);
    seekPages(L, *pages, 4);
    rt.pushInstance(L, pages.get());
    return 1;
}
static int DsListVersionsAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ds = self(L);
    std::string k = dsKey(L);
    bool ascending = true;
    if (isEnumItem(L, 3) || lua_type(L, 3) == LUA_TSTRING) {
        const EnumItem* dir = checkEnumItem(L, 3, findEnum("SortDirection"));
        ascending = !dir || dir->name == "Ascending";
    }
    double minDate = luaL_optnumber(L, 4, 0), maxDate = luaL_optnumber(L, 5, 0);
    size_t pageSize = checkPageSize(L, 6);
    std::vector<Instance::Ptr> rows;
    for (auto& v : versionsOf(L, ds, k)) {
        if (minDate > 0 && v.time < minDate) continue;
        if (maxDate > 0 && v.time > maxDate) continue;
        Instance::Ptr info = rt.dm.createInternal("DataStoreObjectVersionInfo");
        info->setName("DataStoreObjectVersionInfo");
        info->setInternal("Version", Value::string(v.version));
        info->setInternal("CreatedTime", Value::number(v.time));
        info->setInternal("IsDeleted", Value::boolean(v.deleted));
        rows.push_back(info);
    }
    if (!ascending) std::reverse(rows.begin(), rows.end());
    rt.pushInstance(L, makePages(L, "DataStoreVersionPages", std::move(rows), pageSize).get());
    return 1;
}
static int DsGetVersionAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ds = self(L);
    std::string k = dsKey(L);
    std::string want = luaL_checkstring(L, 3);
    for (auto& v : versionsOf(L, ds, k))
        if (v.version == want) {
            if (v.deleted) lua_pushnil(L); else pushJson(L, v.json);
            Instance::Ptr info = rt.dm.createInternal("DataStoreKeyInfo");
            info->setName("DataStoreKeyInfo");
            info->setInternal("Version", Value::string(v.version));
            info->setInternal("CreatedTime", Value::number(v.time));
            info->setInternal("UpdatedTime", Value::number(v.time));
            rt.pushInstance(L, info.get());
            return 2;
        }
    luaL_error(L, "No version %s for key %s", want.c_str(), k.c_str());
}
static int DsRemoveVersionAsync(lua_State* L) {
    Instance& ds = self(L);
    std::string k = dsKey(L);
    std::string want = luaL_checkstring(L, 3);
    auto& list = versionsOf(L, ds, k);
    for (auto it = list.begin(); it != list.end(); ++it)
        if (it->version == want) { list.erase(it); rtOf(L).dataStoresDirty = true; return 0; }
    luaL_error(L, "No version %s for key %s", want.c_str(), k.c_str());
}
static int ListDataStoresAsync(lua_State* L) {   // (prefix, pageSize, cursor)
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    if (!rt.dm.isServer()) luaL_error(L, "DataStore can't be accessed from client");
    std::string prefix = luaL_optstring(L, 2, "");
    size_t pageSize = checkPageSize(L, 3);
    std::vector<Instance::Ptr> rows;
    for (auto& [name, scopes] : rt.dataStores) {
        if (name.rfind("ordered:", 0) == 0 || name == "default:") continue;   // the "ordered:" namespace and the global default are not stores of their own
        if (name.compare(0, prefix.size(), prefix) != 0) continue;
        double created = 0, updated = 0;
        auto vit = rt.dataStoreVersions.find(name);
        if (vit != rt.dataStoreVersions.end())
            for (auto& [scope, keys] : vit->second)
                for (auto& [key, list] : keys)
                    if (!list.empty()) {
                        if (created == 0 || list.front().time < created) created = list.front().time;
                        if (list.back().time > updated) updated = list.back().time;
                    }
        Instance::Ptr info = rt.dm.createInternal("DataStoreInfo");
        info->setName("DataStoreInfo");
        info->setInternal("DataStoreName", Value::string(name));
        info->setInternal("CreatedTime", Value::number(created));
        info->setInternal("UpdatedTime", Value::number(updated));
        rows.push_back(info);
    }
    Instance::Ptr pages = makePages(L, "DataStoreListingPages", std::move(rows), pageSize);
    seekPages(L, *pages, 4);
    rt.pushInstance(L, pages.get());
    return 1;
}

// A DataStorePages page is an array of {key = ..., value = ...}; a listing's page is its rows.
static void pushPage(lua_State* L, Runtime::Impl& rt, const Runtime::Impl::Pages& pg) {
    lua_newtable(L);
    if (!pg.objects.empty()) {
        size_t last = std::min(pg.objects.size(), pg.pos + pg.pageSize);
        for (size_t i = pg.pos; i < last; i++) { rt.pushInstance(L, pg.objects[i].get()); lua_rawseti(L, -2, (int)(i - pg.pos) + 1); }
        return;
    }
    if (!pg.jsonItems.empty()) {   // a MemoryStoreHashMap listing: {key, value} rows
        size_t last = std::min(pg.jsonItems.size(), pg.pos + pg.pageSize);
        for (size_t i = pg.pos; i < last; i++) {
            lua_createtable(L, 0, 2);
            lua_pushstring(L, pg.jsonItems[i].first.c_str()); lua_setfield(L, -2, "key");
            pushJson(L, pg.jsonItems[i].second); lua_setfield(L, -2, "value");
            lua_rawseti(L, -2, (int)(i - pg.pos) + 1);
        }
        return;
    }
    size_t end = std::min(pg.items.size(), pg.pos + pg.pageSize);
    for (size_t i = pg.pos; i < end; i++) {
        lua_newtable(L);
        lua_pushstring(L, pg.items[i].first.c_str()); lua_setfield(L, -2, "key");
        lua_pushnumber(L, pg.items[i].second); lua_setfield(L, -2, "value");
        lua_rawseti(L, -2, (int)(i - pg.pos) + 1);
    }
}
static int GetSortedAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ds = self(L);
    if (!ds.isA("OrderedDataStore")) luaL_error(L, "GetSortedAsync is not a valid member of %s", ds.className().c_str());
    auto& st = storeOf(L, ds);
    bool ascending = lua_toboolean(L, 2);
    int pageSize = (int)luaL_checkinteger(L, 3);
    if (pageSize < 1 || pageSize > 100) luaL_error(L, "pageSize must be between 1 and 100");
    double lo = luaL_optnumber(L, 4, -HUGE_VAL), hi = luaL_optnumber(L, 5, HUGE_VAL);
    Runtime::Impl::Pages pg;
    pg.pageSize = (size_t)pageSize;
    for (auto& [k, v] : st) {
        pushJson(L, v);
        double n = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
        lua_pop(L, 1);
        if (n >= lo && n <= hi) pg.items.push_back({k, n});
    }
    std::stable_sort(pg.items.begin(), pg.items.end(), [&](auto& a, auto& b) { return ascending ? a.second < b.second : a.second > b.second; });
    Instance::Ptr pages = rt.dm.createInternal("DataStorePages");
    pages->setName("DataStorePages");
    pages->setInternal("IsFinished", Value::boolean(pg.items.size() <= pg.pageSize));
    rt.dataStorePages[pages->id()] = std::move(pg);
    rt.pushInstance(L, pages.get());
    return 1;
}
static Runtime::Impl::Pages& pagesOf(lua_State* L, Instance& i) {
    auto it = rtOf(L).dataStorePages.find(i.id());
    if (it == rtOf(L).dataStorePages.end()) luaL_error(L, "%s is not DataStorePages", i.className().c_str());
    return it->second;
}
static int GetCurrentPage(lua_State* L) { pushPage(L, rtOf(L), pagesOf(L, self(L))); return 1; }
static int AdvanceToNextPageAsync(lua_State* L) {
    Instance& i = self(L);
    auto& pg = pagesOf(L, i);
    if (pg.pos + pg.pageSize >= pg.count()) luaL_error(L, "No more pages to advance to");
    pg.pos += pg.pageSize;
    i.setInternal("IsFinished", Value::boolean(pg.pos + pg.pageSize >= pg.count()));
    if (i.cls().findProp("Cursor")) i.setInternal("Cursor", Value::string(std::to_string(pg.pos)));
    return 0;
}

// ---- HttpService -------------------------------------------------------------------
// Server only and off until the host opts in, as on Roblox. The runtime owns no socket: it
// parks the calling script, the host makes the request, and deliverHttp wakes the script.
static void httpGuard(lua_State* L, const char* what) {
    Runtime::Impl& rt = rtOf(L);
    if (!rt.dm.isServer())
        luaL_error(L, "%s can only be called from the server", what);
    if (!rt.opts.httpEnabled)
        luaL_error(L, "Http requests are not enabled. Enable HTTP Requests in game settings.");
    if (!rt.cb.httpRequest)
        luaL_error(L, "Http requests are not enabled on this host.");
    if (!lua_isyieldable(L))
        luaL_error(L, "%s cannot yield from this context", what);
}

static int httpSend(lua_State* L, const std::string& method, const std::string& url,
                    const std::string& body, const std::string& contentType,
                    std::vector<std::pair<std::string, std::string>> headers,
                    bool wantsTable) {
    Runtime::Impl& rt = rtOf(L);
    const uint64_t id = rt.nextHttp++;
    Task* t = rt.taskFor(L);
    t->state = Task::Parked;
    t->httpWantsTable = wantsTable;
    rt.pendingHttp[id] = t;
    rt.cb.httpRequest(id, method, url, body, contentType, headers);
    return lua_yield(L, 0);
}

// A non-string key or value is dropped rather than stringified into the header.
static std::vector<std::pair<std::string, std::string>> headerTable(lua_State* L, int idx) {
    std::vector<std::pair<std::string, std::string>> out;
    if (lua_isnoneornil(L, idx)) return out;
    luaL_checktype(L, idx, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING)
            out.emplace_back(lua_tostring(L, -2), lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return out;
}

static int HttpGetAsync(lua_State* L) {
    httpGuard(L, "GetAsync");
    const char* url = luaL_checkstring(L, 2);
    return httpSend(L, "GET", url, "", "", {}, false);
}

static int HttpPostAsync(lua_State* L) {
    httpGuard(L, "PostAsync");
    const char* url = luaL_checkstring(L, 2);
    const char* body = luaL_optstring(L, 3, "");
    // Enum.HttpContentType is an index into these five; anything else falls back to JSON.
    static const char* kTypes[] = {"application/json", "application/xml",
                                   "application/x-www-form-urlencoded", "text/plain", "text/xml"};
    int which = 0;
    if (lua_isnumber(L, 4)) which = (int)lua_tointeger(L, 4);
    if (which < 0 || which > 4) which = 0;
    return httpSend(L, "POST", url, body, kTypes[which], {}, false);
}

// The only form that reaches a response header or status code: Get/PostAsync keep the body alone.
static int HttpRequestAsync(lua_State* L) {
    httpGuard(L, "RequestAsync");
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "Url");
    if (!lua_isstring(L, -1)) luaL_error(L, "RequestAsync needs a Url");
    std::string url = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "Method");
    std::string method = lua_isstring(L, -1) ? lua_tostring(L, -1) : "GET";
    lua_pop(L, 1);
    lua_getfield(L, 2, "Body");
    std::string body = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    lua_getfield(L, 2, "Headers");
    auto headers = headerTable(L, lua_gettop(L));
    lua_pop(L, 1);
    for (auto& c : method) c = (char)toupper((unsigned char)c);
    return httpSend(L, method, url, body, "", headers, true);
}

// ---- solid modelling ----------------------------------------------------------------
// Behaviour follows creator-docs: BasePart.yaml, GeometryService.yaml, PartOperation.yaml,
// MeshPart.yaml, parts/solid-modeling.md. The runtime does no geometry -- it describes the
// parts to the host, parks, and wakes holding the result, as an HttpService call does.

enum class SolidFamily { BasePart, Geometry };

// Copied from the main part onto every result. CustomPhysicalProperties carries Density,
// Elasticity, ElasticityWeight, Friction and FrictionWeight with it.
static const char* const kSolidLook[] = {
    "Color", "Material", "MaterialVariant", "Reflectance", "Transparency",
    "AudioCanCollide", "CanCollide", "Anchored", "CustomPhysicalProperties",
};

static bool solidOperand(Runtime::Impl& rt, Instance& part, SolidFamily family, bool cut,
                         Runtime::SolidOperand& out, std::string& why) {
    if (!part.isA("BasePart")) { why = part.className() + " is not a part"; return false; }
    if (part.isA("Terrain")) { why = "Terrain is not supported by solid modeling"; return false; }
    if (family == SolidFamily::BasePart && part.isA("MeshPart")) {
        why = "MeshParts are not supported by BasePart solid modeling; use GeometryService";
        return false;
    }
    if (family == SolidFamily::BasePart && !part.isDescendantOf(rt.dm.workspace())) {
        why = part.fullName() + " must be in the Workspace to take part in BasePart solid modeling";
        return false;
    }
    // A client-made id is negative, the server's positive, so a client may only model its own.
    if (!rt.dm.isServer() && part.id() > 0) {
        why = part.fullName() + " was not created on this client, and a client can only solid model its own parts";
        return false;
    }
    out.className = part.className();
    out.shape = part.className() == "Part" ? part.get("Shape").s : std::string();
    out.size = part.get("Size").v;
    out.pos = part.get("Position").v;
    out.orient = part.get("Orientation").v;
    out.color = part.get("Color").c;
    if (part.isA("PartOperation") || part.isA("MeshPart")) out.meshData = part.get("MeshData").s;
    if (part.isA("MeshPart")) out.meshId = part.get("MeshId").s;
    out.subtract = cut;
    return true;
}

static void solidParts(lua_State* L, Runtime::Impl& rt, int idx, std::vector<Instance*>& out) {
    luaL_checktype(L, idx, LUA_TTABLE);
    const int n = lua_objlen(L, idx);
    for (int k = 1; k <= n; k++) {
        lua_rawgeti(L, idx, k);
        Instance* i = rt.toInstance(L, -1);
        lua_pop(L, 1);
        if (!i) luaL_error(L, "the parts table must hold only parts (item %d is not an Instance)", k);
        out.push_back(i);
    }
}

static std::string solidEnum(lua_State* L, Runtime::Impl& rt, int idx, const char* enumName, const char* fallback) {
    if (lua_isnoneornil(L, idx)) return fallback;
    Value v;
    std::string err;
    if (!rt.toValue(L, idx, Value::Enum, findEnum(enumName), v, err)) luaL_error(L, "%s", err.c_str());
    return v.s;
}

// A NegateOperation is Studio's Negate button; rbxNegate is the tag a script adds for the same.
static bool negated(const Instance& part) {
    return part.className() == "NegateOperation" || part.hasTag("rbxNegate");
}

static int solidAsk(lua_State* L, Runtime::SolidRequest::Op op, SolidFamily family, Instance& from,
                    const std::vector<Instance*>& others, bool splitApart,
                    const std::string& collision, const std::string& render, const std::string& fluid) {
    Runtime::Impl& rt = rtOf(L);
    if (!rt.cb.solidRequest) luaL_error(L, "solid modeling is not available in this runtime");
    Runtime::SolidRequest req;
    req.op = op;
    req.splitApart = splitApart;
    req.keepOrigin = family == SolidFamily::Geometry;
    std::string why;
    Runtime::SolidOperand first;
    if (!solidOperand(rt, from, family, false, first, why)) luaL_error(L, "%s", why.c_str());
    req.operands.push_back(first);
    bool anyMesh = from.isA("MeshPart");
    for (Instance* i : others) {
        if (i == &from) continue;                                  // listed again: once is enough
        Runtime::SolidOperand o;
        const bool cut = op == Runtime::SolidRequest::Subtract || (op == Runtime::SolidRequest::Union && negated(*i));
        if (!solidOperand(rt, *i, family, cut, o, why)) luaL_error(L, "%s", why.c_str());
        if (i->isA("MeshPart")) anyMesh = true;
        req.operands.push_back(o);
    }
    Runtime::Impl::PendingSolid pending;
    for (const char* prop : kSolidLook)
        if (from.cls().findProp(prop)) pending.look.emplace_back(prop, from.get(prop));
    pending.wantsTable = family == SolidFamily::Geometry;
    // BasePart names its result Union or Intersect; GeometryService leaves the class's default
    // name, and gives MeshParts if any input was one.
    const bool intersect = op == Runtime::SolidRequest::Intersect;
    if (family == SolidFamily::Geometry && anyMesh) pending.resultClass = "MeshPart";
    else pending.resultClass = intersect ? "IntersectOperation" : "UnionOperation";
    if (family == SolidFamily::BasePart) pending.resultName = intersect ? "Intersect" : "Union";
    pending.collision = collision;
    // A PartOperation's RenderFidelity cannot be Performance; asked for, it falls to Automatic.
    pending.render = (render == "Performance" && pending.resultClass != "MeshPart") ? "Automatic" : render;
    pending.fluid = fluid;
    const uint64_t id = rt.nextSolid++;
    Task* t = rt.taskFor(L);
    t->state = Task::Parked;
    pending.task = t;
    rt.pendingSolid[id] = std::move(pending);
    rt.cb.solidRequest(id, req);
    return lua_yield(L, 0);
}

// A negated part in a UnionAsync is cut out rather than added: Roblox has one Union button.
static int solidMethod(lua_State* L, Runtime::SolidRequest::Op op) {
    Runtime::Impl& rt = rtOf(L);
    Instance& from = self(L);
    std::vector<Instance*> others;
    solidParts(L, rt, 2, others);
    const std::string collision = solidEnum(L, rt, 3, "CollisionFidelity", "Default");
    const std::string render = solidEnum(L, rt, 4, "RenderFidelity", "Automatic");
    return solidAsk(L, op, SolidFamily::BasePart, from, others, false, collision, render, "Automatic");
}
static int UnionAsync(lua_State* L) { return solidMethod(L, Runtime::SolidRequest::Union); }
static int SubtractAsync(lua_State* L) { return solidMethod(L, Runtime::SolidRequest::Subtract); }
static int IntersectAsync(lua_State* L) { return solidMethod(L, Runtime::SolidRequest::Intersect); }

// GeometryService:UnionAsync(part, parts, options?) hands back a table of results, not one part.
static int geometryMethod(lua_State* L, Runtime::SolidRequest::Op op) {
    Runtime::Impl& rt = rtOf(L);
    Instance& from = rt.checkInstance(L, 2);
    std::vector<Instance*> others;
    solidParts(L, rt, 3, others);
    std::string collision = "Default", render = "Automatic", fluid = "Automatic";
    bool split = true;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua_getfield(L, 4, "CollisionFidelity");
        collision = solidEnum(L, rt, lua_gettop(L), "CollisionFidelity", "Default");
        lua_pop(L, 1);
        lua_getfield(L, 4, "RenderFidelity");
        render = solidEnum(L, rt, lua_gettop(L), "RenderFidelity", "Automatic");
        lua_pop(L, 1);
        lua_getfield(L, 4, "FluidFidelity");
        fluid = solidEnum(L, rt, lua_gettop(L), "FluidFidelity", "Automatic");
        lua_pop(L, 1);
        lua_getfield(L, 4, "SplitApart");
        if (!lua_isnil(L, -1)) split = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }
    return solidAsk(L, op, SolidFamily::Geometry, from, others, split, collision, render, fluid);
}
// An entry is a content string or an Instance, whose content-bearing properties are read out of
// it. Yields until all have settled, calling callback(contentId, Enum.AssetFetchStatus) for each.
static int PreloadAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& svc = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    if (!lua_isnoneornil(L, 3)) luaL_checktype(L, 3, LUA_TFUNCTION);
    if (!rt.cb.preloadRequest) luaL_error(L, "PreloadAsync is not available in this runtime");
    static const char* const kContentProps[] = {"Image", "HoverImage", "PressedImage", "Texture", "TextureID", "TextureId", "MeshId", "SoundId", "SkyboxBk", "SkyboxDn", "SkyboxFt", "SkyboxLf", "SkyboxRt", "SkyboxUp"};
    std::vector<std::string> uris;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        if (lua_isstring(L, -1)) { std::string s = lua_tostring(L, -1); if (!s.empty()) uris.push_back(s); }
        else if (Instance* inst = rt.toInstance(L, -1)) {
            for (const char* p : kContentProps) {
                if (!inst->cls().findProp(p)) continue;
                const Value v = inst->get(p);
                std::string s = v.type == Value::String ? v.s : (v.type == Value::Content && v.n == Value::ContentUri ? v.s : std::string());
                if (!s.empty()) uris.push_back(s);
            }
        }
        lua_pop(L, 1);
    }
    rt.contentProviderId = svc.id();
    if (uris.empty()) return 0;
    Task* t = rt.taskFor(L);
    const uint64_t gid = rt.nextPreloadGroup++;
    Runtime::Impl::PreloadGroup& g = rt.preloadGroups[gid];
    g.task = t;
    g.left = (int)uris.size();
    g.ctx = rt.ctxOf(L);
    if (!lua_isnoneornil(L, 3)) { lua_pushvalue(L, 3); g.cbRef = lua_ref(L, -1); lua_pop(L, 1); }
    t->state = Task::Parked;
    for (const std::string& uri : uris) {
        const uint64_t id = rt.nextPreload++;
        rt.pendingPreload[id] = {gid, uri};
        rt.cb.preloadRequest(id, uri);
    }
    svc.setInternal("RequestQueueSize", Value::number((double)rt.pendingPreload.size()));
    return lua_yield(L, 0);
}

// MeshPart.MeshId is read only, so this is the only way a script makes a MeshPart of a mesh.
static int CreateMeshPartAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    if (!isContent(L, 2)) luaL_error(L, "invalid argument #2 to 'CreateMeshPartAsync' (Content expected, got %s)", typeOfName(L, 2));
    Value content = checkContent(L, 2);
    std::string collision = "Default", render = "Automatic", fluid = "Automatic";
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua_getfield(L, 3, "CollisionFidelity"); collision = solidEnum(L, rt, lua_gettop(L), "CollisionFidelity", "Default"); lua_pop(L, 1);
        lua_getfield(L, 3, "RenderFidelity"); render = solidEnum(L, rt, lua_gettop(L), "RenderFidelity", "Automatic"); lua_pop(L, 1);
        lua_getfield(L, 3, "FluidFidelity"); fluid = solidEnum(L, rt, lua_gettop(L), "FluidFidelity", "Automatic"); lua_pop(L, 1);
    }
    // Linked, not copied: the part is sized to the mesh's bounds and redrawn as the mesh changes.
    if (content.n == Value::ContentObject || content.n == Value::ContentOpaque) {
        Instance* src = rt.dm.find(content.ref);
        Vec3 size{0, 0, 0};
        if (src && src->isA("EditableMesh")) {
            EditableMeshData& m = em::meshOf(L, *src, "CreateMeshPartAsync");
            Vec3 lo, hi; m.bounds(lo, hi);
            if (lo.x > hi.x || m.liveFaces() == 0) luaL_error(L, "CreateMeshPartAsync: the EditableMesh has no triangles");
            size = {hi.x - lo.x, hi.y - lo.y, hi.z - lo.z};
        } else if (src && src->className() == "DataModelContent" && src->get("Kind").s == "Mesh") {
            size = src->get("MeshBounds").v;
        } else luaL_error(L, "CreateMeshPartAsync: the Content must name a mesh");
        Instance::Ptr part = rt.dm.createInternal("MeshPart");
        part->setInternal("MeshContent", content);
        part->setInternal("Size", Value::vector3(size));
        part->setInternal("MeshSize", Value::vector3(size));
        part->setInternal("CollisionFidelity", Value::string(collision));
        part->setInternal("RenderFidelity", Value::string(render));
        part->setInternal("FluidFidelity", Value::string(fluid));
        rt.pushInstance(L, part.get());
        return 1;
    }
    if (content.n != Value::ContentUri) luaL_error(L, "CreateMeshPartAsync: the Content names no mesh");
    if (!rt.cb.meshRequest) luaL_error(L, "CreateMeshPartAsync is not available in this runtime");
    Runtime::Impl::PendingMeshPart pending;
    pending.uri = content.s;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua_getfield(L, 3, "CollisionFidelity");
        pending.collision = solidEnum(L, rt, lua_gettop(L), "CollisionFidelity", "Default");
        lua_pop(L, 1);
        lua_getfield(L, 3, "RenderFidelity");
        pending.render = solidEnum(L, rt, lua_gettop(L), "RenderFidelity", "Automatic");
        lua_pop(L, 1);
        lua_getfield(L, 3, "FluidFidelity");
        pending.fluid = solidEnum(L, rt, lua_gettop(L), "FluidFidelity", "Automatic");
        lua_pop(L, 1);
    }
    const uint64_t id = rt.nextMesh++;
    Task* t = rt.taskFor(L);
    t->state = Task::Parked;
    pending.task = t;
    rt.pendingMeshPart[id] = std::move(pending);
    rt.cb.meshRequest(id, content.s, false);
    return lua_yield(L, 0);
}

static int GeometryUnionAsync(lua_State* L) { return geometryMethod(L, Runtime::SolidRequest::Union); }
static int GeometrySubtractAsync(lua_State* L) { return geometryMethod(L, Runtime::SolidRequest::Subtract); }
static int GeometryIntersectAsync(lua_State* L) { return geometryMethod(L, Runtime::SolidRequest::Intersect); }

// The source's geometry, this one's properties, attributes, tags and children. GeometryService
// results keep the main part's coordinate space, so the shape lands where the old one was.
static int SubstituteGeometry(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& target = self(L);
    Instance* source = rt.toInstance(L, 2);
    if (!source || !source->isA("PartOperation"))
        luaL_error(L, "SubstituteGeometry needs a PartOperation to take the geometry from");
    target.setInternal("MeshData", source->get("MeshData"));
    target.setInternal("Size", source->get("Size"));
    target.setInternal("TriangleCount", source->get("TriangleCount"));
    target.setInternal("MeshSize", source->get("MeshSize"));
    return 0;
}

// ---- GeometryService:CalculateConstraintsToPreserve -----------------------------------------
// The Constraints and Attachments a caller may keep, each with the parent to give it. An
// Attachment goes to the result whose surface it is nearest; past `tolerance` -- its distance
// to the original's surface against the result's -- it and its constraints get a nil parent.
// WeldConstraints and NoCollisionConstraints naming the original are re-pointed at each result.
// Left out, neither option drops anything: no Roblox default to follow.

// A part's surface in world space, for distances to it.
struct SolidSurface {
    enum Kind { Box, Ball, Cylinder, Triangles } kind = Box;
    CFrameV cf;
    Vec3 size;
    std::vector<Vec3> tri;          // world-space corners, three a triangle
};

static bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int val = 0, bits = -8;
    out.clear();
    out.reserve(in.size() * 3 / 4);
    for (unsigned char c : in) {
        if (c == '=') break;
        const size_t k = chars.find((char)c);
        if (k == std::string::npos) { if (c == '\n' || c == '\r') continue; return false; }
        val = (val << 6) + (int)k;
        bits += 6;
        if (bits >= 0) { out.push_back((uint8_t)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return true;
}

static SolidSurface surfaceOf(const Instance& part) {
    SolidSurface out;
    out.cf = cframeFromPosOrient(part.get("Position").v, part.get("Orientation").v);
    out.size = part.get("Size").v;
    // A PartOperation's or MeshPart's own triangles, scaled to Size about the origin as drawn.
    if (part.isA("PartOperation") || part.isA("MeshPart")) {
        std::vector<uint8_t> raw;
        if (base64Decode(part.get("MeshData").s, raw) && raw.size() >= 16 && std::memcmp(raw.data(), "PBOP", 4) == 0) {
            auto u32 = [&](size_t at) { uint32_t v; std::memcpy(&v, raw.data() + at, 4); return v; };
            auto f32 = [&](size_t at) { float v; std::memcpy(&v, raw.data() + at, 4); return v; };
            const uint32_t version = u32(4), nv = u32(8), ni = u32(12);
            const size_t stride = version == 3 ? 44 : version == 2 ? 36 : 24;   // 2 adds a face colour, 3 a UV
            if (version >= 1 && version <= 3 && raw.size() >= 16 + (size_t)nv * stride + (size_t)ni * 4) {
                std::vector<Vec3> v(nv);
                Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
                for (uint32_t i = 0; i < nv; i++) {
                    const size_t at = 16 + i * stride;
                    v[i] = {f32(at), f32(at + 4), f32(at + 8)};
                    lo = {std::min(lo.x, v[i].x), std::min(lo.y, v[i].y), std::min(lo.z, v[i].z)};
                    hi = {std::max(hi.x, v[i].x), std::max(hi.y, v[i].y), std::max(hi.z, v[i].z)};
                }
                const Vec3 span{hi.x - lo.x, hi.y - lo.y, hi.z - lo.z};
                const Vec3 sc{span.x > 1e-6f ? out.size.x / span.x : 1, span.y > 1e-6f ? out.size.y / span.y : 1, span.z > 1e-6f ? out.size.z / span.z : 1};
                const size_t base = 16 + (size_t)nv * stride;
                for (uint32_t i = 0; i < ni; i++) {
                    const uint32_t k = u32(base + i * 4);
                    if (k >= nv) { out.tri.clear(); break; }
                    out.tri.push_back(out.cf * Vec3{v[k].x * sc.x, v[k].y * sc.y, v[k].z * sc.z});
                }
                if (out.tri.size() >= 3) { out.kind = SolidSurface::Triangles; return out; }
            }
        }
    }
    const std::string shape = part.className() == "Part" ? part.get("Shape").s : std::string();
    if (shape == "Ball") out.kind = SolidSurface::Ball;
    else if (shape == "Cylinder") out.kind = SolidSurface::Cylinder;
    else out.kind = SolidSurface::Box;   // Block; a wedge is measured to its box
    return out;
}

static Vec3 vsub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 vadd(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 vmul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float vdot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float vlen(Vec3 a) { return std::sqrt(vdot(a, a)); }

// The closest point on triangle abc to p (Ericson, Real-Time Collision Detection 5.1.5).
static Vec3 closestOnTriangle(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
    const Vec3 ab = vsub(b, a), ac = vsub(c, a), ap = vsub(p, a);
    const float d1 = vdot(ab, ap), d2 = vdot(ac, ap);
    if (d1 <= 0 && d2 <= 0) return a;
    const Vec3 bp = vsub(p, b);
    const float d3 = vdot(ab, bp), d4 = vdot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) return vadd(a, vmul(ab, d1 / (d1 - d3)));
    const Vec3 cp = vsub(p, c);
    const float d5 = vdot(ab, cp), d6 = vdot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) return vadd(a, vmul(ac, d2 / (d2 - d6)));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return vadd(b, vmul(vsub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    const float den = 1.0f / (va + vb + vc);
    return vadd(a, vadd(vmul(ab, vb * den), vmul(ac, vc * den)));
}

// Distance from a point to the part's SURFACE -- from inside as well as out.
static float distanceToSurface(const SolidSurface& s, Vec3 p) {
    if (s.kind == SolidSurface::Triangles) {
        float best = 1e30f;
        for (size_t t = 0; t + 2 < s.tri.size(); t += 3) best = std::min(best, vlen(vsub(p, closestOnTriangle(p, s.tri[t], s.tri[t + 1], s.tri[t + 2]))));
        return best;
    }
    const Vec3 l = s.cf.inverse() * p;
    if (s.kind == SolidSurface::Ball) {
        const float r = std::min(std::min(s.size.x, s.size.y), s.size.z) * 0.5f;
        return std::fabs(vlen(l) - r);
    }
    if (s.kind == SolidSurface::Cylinder) {
        const float r = std::min(s.size.y, s.size.z) * 0.5f, hx = s.size.x * 0.5f;
        const float dr = std::sqrt(l.y * l.y + l.z * l.z) - r, dx = std::fabs(l.x) - hx;
        if (dr <= 0 && dx <= 0) return std::min(-dr, -dx);
        const float a = std::max(dr, 0.0f), b = std::max(dx, 0.0f);
        return std::sqrt(a * a + b * b);
    }
    const Vec3 d{std::fabs(l.x) - s.size.x * 0.5f, std::fabs(l.y) - s.size.y * 0.5f, std::fabs(l.z) - s.size.z * 0.5f};
    if (d.x <= 0 && d.y <= 0 && d.z <= 0) return std::min(std::min(-d.x, -d.y), -d.z);
    return vlen({std::max(d.x, 0.0f), std::max(d.y, 0.0f), std::max(d.z, 0.0f)});
}

static void boundsOf(const SolidSurface& s, Vec3& lo, Vec3& hi) {
    lo = {1e30f, 1e30f, 1e30f};
    hi = {-1e30f, -1e30f, -1e30f};
    auto grow = [&](Vec3 v) {
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    };
    if (s.kind == SolidSurface::Triangles) { for (const Vec3& v : s.tri) grow(v); return; }
    for (int x : {-1, 1}) for (int y : {-1, 1}) for (int z : {-1, 1})
        grow(s.cf * Vec3{x * s.size.x * 0.5f, y * s.size.y * 0.5f, z * s.size.z * 0.5f});
}

static int CalculateConstraintsToPreserve(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& source = rt.checkInstance(L, 2);
    std::vector<Instance*> dest;
    solidParts(L, rt, 3, dest);
    double tolerance = -1;
    std::string weldRule = "All";
    bool dropLonely = true;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua_getfield(L, 4, "tolerance");
        if (!lua_isnil(L, -1)) tolerance = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 4, "weldConstraintPreserve");
        weldRule = solidEnum(L, rt, lua_gettop(L), "WeldConstraintPreserve", "All");
        lua_pop(L, 1);
        lua_getfield(L, 4, "dropAttachmentsWithoutConstraints");
        if (!lua_isnil(L, -1)) dropLonely = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }
    const SolidSurface from = surfaceOf(source);
    std::vector<SolidSurface> to;
    for (Instance* d : dest) to.push_back(surfaceOf(*d));

    std::vector<Instance*> everything = rt.dm.root()->getDescendants();
    // Detached parts can hold constraints too; the source and the results may not be in the place.
    for (Instance* i : source.getDescendants()) everything.push_back(i);

    lua_newtable(L);
    int n = 0;
    auto push = [&](Instance* i) { if (i) rt.pushInstance(L, i); else lua_pushnil(L); };
    auto field = [&](const char* key, Instance* i) { push(i); lua_setfield(L, -2, key); };

    for (const Instance::Ptr& kid : source.children()) {
        if (!kid->isA("Attachment")) continue;
        Instance* att = kid.get();
        const Vec3 at = (from.cf * cframeFromPosOrient(att->get("Position").v, att->get("Orientation").v)).p;
        const float d0 = distanceToSurface(from, at);
        Instance* best = nullptr;
        float bestD = 1e30f;
        for (size_t k = 0; k < dest.size(); k++) {
            const float d = distanceToSurface(to[k], at);
            if (d < bestD) { bestD = d; best = dest[k]; }
        }
        Instance* parent = best;
        if (tolerance >= 0 && best && std::fabs(bestD - d0) > tolerance) parent = nullptr;
        std::vector<Instance*> uses;
        for (Instance* c : everything)
            if (c->isA("Constraint") && (c->get("Attachment0").ref == att->id() || c->get("Attachment1").ref == att->id()))
                if (std::find(uses.begin(), uses.end(), c) == uses.end()) uses.push_back(c);
        if (uses.empty()) {
            lua_createtable(L, 0, 4);
            field("Attachment", att);
            field("Constraint", nullptr);
            field("AttachmentParent", dropLonely ? nullptr : parent);
            field("ConstraintParent", nullptr);
            lua_rawseti(L, -2, ++n);
            continue;
        }
        for (Instance* c : uses) {
            lua_createtable(L, 0, 4);
            field("Attachment", att);
            field("Constraint", c);
            field("AttachmentParent", parent);
            // A constraint parented to the original moves with its attachment; one parented
            // elsewhere stays put, unless the attachment is dropped and takes it along.
            field("ConstraintParent", parent ? (c->parent() == &source ? parent : c->parent()) : nullptr);
            lua_rawseti(L, -2, ++n);
        }
    }

    std::vector<Instance*> seen;
    for (Instance* w : everything) {
        const bool weld = w->className() == "WeldConstraint", noCollide = w->className() == "NoCollisionConstraint";
        if ((!weld && !noCollide) || std::find(seen.begin(), seen.end(), w) != seen.end()) continue;
        seen.push_back(w);
        const int64_t p0 = w->get("Part0").ref, p1 = w->get("Part1").ref;
        if (p0 != source.id() && p1 != source.id()) continue;
        if (weld && weldRule == "None") continue;
        Instance* other = rt.dm.find(p0 == source.id() ? p1 : p0);
        for (size_t k = 0; k < dest.size(); k++) {
            if (weld && weldRule == "Touching") {
                if (!other || !other->isA("BasePart")) continue;
                Vec3 alo, ahi, blo, bhi;
                boundsOf(to[k], alo, ahi);
                boundsOf(surfaceOf(*other), blo, bhi);
                const float e = 0.05f;
                const bool touch = alo.x <= bhi.x + e && ahi.x >= blo.x - e && alo.y <= bhi.y + e && ahi.y >= blo.y - e
                                && alo.z <= bhi.z + e && ahi.z >= blo.z - e;
                if (!touch) continue;
            }
            Instance* part0 = p0 == source.id() ? dest[k] : rt.dm.find(p0);
            Instance* part1 = p1 == source.id() ? dest[k] : rt.dm.find(p1);
            lua_createtable(L, 0, 4);
            if (weld) {
                field("WeldConstraint", w);
                field("WeldConstraintParent", dest[k]);
                field("WeldConstraintPart0", part0);
                field("WeldConstraintPart1", part1);
            } else {
                field("NoCollisionConstraint", w);
                field("NoCollisionConstraintParent", dest[k]);
                field("NoCollisionConstraintPart0", part0);
                field("NoCollisionConstraintPart1", part1);
            }
            lua_rawseti(L, -2, ++n);
        }
    }
    return 1;
}

static int ApplyMesh(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& target = self(L);
    Instance* source = rt.toInstance(L, 2);
    if (!source || !source->isA("MeshPart")) luaL_error(L, "ApplyMesh needs a MeshPart");
    for (const char* prop : {"MeshContent", "TextureContent", "MeshData", "RenderFidelity", "CollisionFidelity", "FluidFidelity", "MeshSize"})
        target.setInternal(prop, source->get(prop));
    // Re-applying the same EditableMesh is how its collision is refreshed, so the change is
    // recorded even when the Content did not change.
    target.touch("MeshContent");
    return 0;
}

} // namespace m

// ---- raycasting --------------------------------------------------------------------
static Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float len(Vec3 a) { return std::sqrt(dot(a, a)); }
static Vec3 unit(Vec3 a) { float l = len(a); return l > 0 ? mul(a, 1 / l) : Vec3{0, 0, 0}; }
static float comp(Vec3 v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }

// t comes back in units of `d`, so t <= 1 is a hit within the ray. A ray starting inside the
// box sees no surface.
static bool rayBox(const CFrameV& cf, Vec3 half, Vec3 o, Vec3 d, float& t, Vec3& normal) {
    CFrameV inv = cf.inverse();
    Vec3 lo = inv * o, ld = inv.rotate(d);
    float tmin = 0, tmax = 1e30f;
    int axis = -1; float sign = 0;
    for (int i = 0; i < 3; i++) {
        float oi = comp(lo, i), di = comp(ld, i), h = comp(half, i);
        if (std::fabs(di) < 1e-9f) { if (oi < -h || oi > h) return false; continue; }
        float t1 = (-h - oi) / di, t2 = (h - oi) / di, s = -1;
        if (t1 > t2) { std::swap(t1, t2); s = 1; }
        if (t1 > tmin) { tmin = t1; axis = i; sign = s; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    if (axis < 0 || tmin > 1) return false;
    t = tmin;
    Vec3 n{0, 0, 0};
    if (axis == 0) n.x = sign; else if (axis == 1) n.y = sign; else n.z = sign;
    normal = cf.rotate(n);
    return true;
}
static bool raySphere(Vec3 c, float r, Vec3 o, Vec3 d, float& t, Vec3& normal) {
    Vec3 m = sub(o, c);
    float a = dot(d, d), b = dot(m, d), cc = dot(m, m) - r * r;
    if (cc < 0 || a <= 0) return false;                 // starts inside
    float disc = b * b - a * cc;
    if (disc < 0) return false;
    float tt = (-b - std::sqrt(disc)) / a;
    if (tt < 0 || tt > 1) return false;
    t = tt;
    normal = unit(sub(add(o, mul(d, tt)), c));
    return true;
}
bool raycastTree(DataModel& dm, Vec3 origin, Vec3 direction, const RaycastFilter* filter, RayHit& out) {
    bool found = false;
    float best = 2;
    queryParts(dm, filter, [&](Instance& i) {
        ShapeV s = partShape(i);
        float t; Vec3 n;
        bool hit = s.sphere ? raySphere(s.cf.p, s.radius, origin, direction, t, n) : rayBox(s.cf, s.half, origin, direction, t, n);
        if (hit && t < best) { best = t; found = true; out.part = &i; out.normal = n; out.position = add(origin, mul(direction, t)); out.distance = t * len(direction); }
        return true;
    });
    return found;
}
// Roblox's camera: LookVector is -Z and FieldOfView is the vertical angle.
RayV cameraRay(Instance& camera, float x, float y, float depth) {
    CFrameV cf = cframeFromPosOrient(camera.get("Position").v, camera.get("Orientation").v);
    Vec2 vs = camera.get("ViewportSize").v2();
    float w = vs.x > 0 ? vs.x : 1, h = vs.y > 0 ? vs.y : 1;
    float tanHalf = std::tan((float)camera.get("FieldOfView").n * 3.14159265f / 360);
    Vec3 local{(2 * x / w - 1) * tanHalf * (w / h), (1 - 2 * y / h) * tanHalf, -1};
    return {cf * mul(local, depth), unit(cf.rotate(local))};      // depth: studs in front of the camera, not along the ray
}
static bool worldToViewport(Instance& camera, Vec3 p, Vec3& out) {
    CFrameV cf = cframeFromPosOrient(camera.get("Position").v, camera.get("Orientation").v);
    Vec2 vs = camera.get("ViewportSize").v2();
    float w = vs.x > 0 ? vs.x : 1, h = vs.y > 0 ? vs.y : 1;
    float tanHalf = std::tan((float)camera.get("FieldOfView").n * 3.14159265f / 360);
    Vec3 l = cf.inverse() * p;
    float depth = -l.z, dz = depth != 0 ? depth : 1e-6f;
    out = {(l.x / (dz * tanHalf * (w / h)) + 1) / 2 * w, (1 - l.y / (dz * tanHalf)) / 2 * h, depth};
    return depth > 0 && out.x >= 0 && out.x <= w && out.y >= 0 && out.y <= h;
}

static int connect(lua_State* L, bool once);   // a signal's Connect, below: MessagingService:SubscribeAsync hands back one
namespace m {
// RaycastParams and OverlapParams share a userdata; `overlap` is which one it is.
static RaycastParamsUD* toParams(lua_State* L, int idx) {
    int tag = lua_userdatatag(L, idx);
    return tag == TAG_RAYCASTPARAMS || tag == TAG_OVERLAPPARAMS ? static_cast<RaycastParamsUD*>(lua_touserdata(L, idx)) : nullptr;
}
static RaycastFilter& checkParams(lua_State* L, int idx, bool overlap) {
    RaycastParamsUD* p = toParams(L, idx);
    if (!p || p->overlap != overlap) luaL_typeerror(L, idx, overlap ? "OverlapParams" : "RaycastParams");
    return p->f;
}
static RaycastFilter& checkRaycastParams(lua_State* L, int idx) { return checkParams(L, idx, false); }
static const char* paramsName(lua_State* L, int idx) { RaycastParamsUD* p = toParams(L, idx); return p && p->overlap ? "OverlapParams" : "RaycastParams"; }
static void addToFilter(lua_State* L, int idx, RaycastFilter& f) {
    Runtime::Impl& rt = rtOf(L);
    if (Instance* i = rt.toInstance(L, idx)) { f.ids.push_back(i->id()); return; }
    if (lua_type(L, idx) != LUA_TTABLE) luaL_error(L, "invalid argument #%d (Instance or table of Instances expected, got %s)", idx - 1, typeOfName(L, idx));
    idx = lua_absindex(L, idx);
    for (int n = 1;; n++) {
        lua_rawgeti(L, idx, n);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        if (Instance* i = rt.toInstance(L, -1)) f.ids.push_back(i->id());
        lua_pop(L, 1);
    }
}
static int rp_new(lua_State* L) { new (lua_newuserdatataggedwithmetatable(L, sizeof(RaycastParamsUD), TAG_RAYCASTPARAMS)) RaycastParamsUD(); return 1; }
static int op_new(lua_State* L) { auto* p = new (lua_newuserdatataggedwithmetatable(L, sizeof(RaycastParamsUD), TAG_OVERLAPPARAMS)) RaycastParamsUD(); p->overlap = true; return 1; }
static int rp_index(lua_State* L) {
    RaycastParamsUD* ud = toParams(L, 1);
    if (!ud) luaL_typeerror(L, 1, "RaycastParams");
    RaycastFilter& f = ud->f;
    const char* k = luaL_checkstring(L, 2);
    if (ud->overlap && !std::strcmp(k, "MaxParts")) { lua_pushnumber(L, f.maxParts); return 1; }
    if (ud->overlap && !std::strcmp(k, "BruteForceAllSlow")) { lua_pushboolean(L, f.bruteForce); return 1; }
    if (!ud->overlap && !std::strcmp(k, "IgnoreWater")) { lua_pushboolean(L, f.ignoreWater); return 1; }
    if (!std::strcmp(k, "FilterDescendantsInstances")) {
        Runtime::Impl& rt = rtOf(L);
        lua_newtable(L);
        int n = 0;
        for (int64_t id : f.ids) if (Instance* i = rt.dm.find(id)) { rt.pushInstance(L, i); lua_rawseti(L, -2, ++n); }
        return 1;
    }
    if (!std::strcmp(k, "FilterType")) { const EnumDef* e = findEnum("RaycastFilterType"); pushEnumItem(L, e, e->find(f.include ? "Include" : "Exclude")); return 1; }
    if (!std::strcmp(k, "RespectCanCollide")) { lua_pushboolean(L, f.respectCanCollide); return 1; }
    if (!std::strcmp(k, "CollisionGroup")) { lua_pushstring(L, f.collisionGroup.c_str()); return 1; }
    luaL_error(L, "%s is not a valid member of %s", k, paramsName(L, 1));
}
static int rp_newindex(lua_State* L) {
    RaycastParamsUD* ud = toParams(L, 1);
    if (!ud) luaL_typeerror(L, 1, "RaycastParams");
    RaycastFilter& f = ud->f;
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "FilterDescendantsInstances")) { f.ids.clear(); luaL_checktype(L, 3, LUA_TTABLE); addToFilter(L, 3, f); return 0; }
    if (!std::strcmp(k, "FilterType")) { f.include = checkEnumItem(L, 3, findEnum("RaycastFilterType"))->value == 1; return 0; }
    if (!ud->overlap && !std::strcmp(k, "IgnoreWater")) { f.ignoreWater = lua_toboolean(L, 3); return 0; }
    if (ud->overlap && !std::strcmp(k, "MaxParts")) { f.maxParts = (int)luaL_checknumber(L, 3); return 0; }
    if (ud->overlap && !std::strcmp(k, "BruteForceAllSlow")) { f.bruteForce = lua_toboolean(L, 3); return 0; }
    if (!std::strcmp(k, "RespectCanCollide")) { f.respectCanCollide = lua_toboolean(L, 3); return 0; }
    if (!std::strcmp(k, "CollisionGroup")) { f.collisionGroup = luaL_checkstring(L, 3); return 0; }
    luaL_error(L, "%s is not a valid member of %s", k, paramsName(L, 1));
}
static int rp_namecall(lua_State* L) {
    RaycastParamsUD* ud = toParams(L, 1);
    if (!ud) luaL_typeerror(L, 1, "RaycastParams");
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && !std::strcmp(m, "AddToFilter")) { addToFilter(L, 2, ud->f); return 0; }
    luaL_error(L, "%s is not a valid method of %s", m ? m : "?", paramsName(L, 1));
}
static int rp_tostring(lua_State* L) { lua_pushstring(L, paramsName(L, 1)); return 1; }

static void pushRayHit(lua_State* L, Runtime::Impl& rt, const RayHit& h) {
    lua_createtable(L, 0, 5);
    rt.pushInstance(L, h.part); lua_setfield(L, -2, "Instance");
    pushVec3(L, h.position); lua_setfield(L, -2, "Position");
    pushVec3(L, h.normal); lua_setfield(L, -2, "Normal");
    lua_pushnumber(L, h.distance); lua_setfield(L, -2, "Distance");
    rt.pushValue(L, h.part->get("Material"), findEnum("Material")); lua_setfield(L, -2, "Material");
    lua_setreadonly(L, -1, true);
}
static int Raycast(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Vec3 o = checkVec3(L, 2), d = checkVec3(L, 3);
    RaycastFilter* f = lua_isnoneornil(L, 4) ? nullptr : &checkRaycastParams(L, 4);
    RayHit h;
    if (!raycastTree(rt.dm, o, d, f, h)) { lua_pushnil(L); return 1; }
    pushRayHit(L, rt, h);
    return 1;
}
static int pushParts(lua_State* L, Runtime::Impl& rt, const std::vector<Instance*>& parts) {
    lua_createtable(L, (int)parts.size(), 0);
    int n = 0;
    for (Instance* p : parts) { rt.pushInstance(L, p); lua_rawseti(L, -2, ++n); }
    return 1;
}
static const RaycastFilter* overlapParams(lua_State* L, int idx) { return lua_isnoneornil(L, idx) ? nullptr : &checkParams(L, idx, true); }
// Bounding boxes, not shapes: a part counts when its box overlaps.
static int GetPartBoundsInBox(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    ShapeV s; s.cf = checkCFrame(L, 2); s.half = mul(checkVec3(L, 3), 0.5f);
    return pushParts(L, rt, overlapTree(rt.dm, s, overlapParams(L, 4), nullptr, true));
}
static int GetPartBoundsInRadius(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    ShapeV s; s.cf = CFrameV::fromPos(checkVec3(L, 2)); s.sphere = true; s.radius = (float)luaL_checknumber(L, 3);
    return pushParts(L, rt, overlapTree(rt.dm, s, overlapParams(L, 4), nullptr, true));
}
static int GetPartsInPart(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Instance& part = rt.checkInstance(L, 2);
    if (!part.isA("BasePart")) luaL_error(L, "invalid argument #1 to 'GetPartsInPart' (BasePart expected, got %s)", part.className().c_str());
    return pushParts(L, rt, overlapTree(rt.dm, partShape(part), overlapParams(L, 3), &part, false));
}
static int GetTouchingParts(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& part = self(L);
    return pushParts(L, rt, overlapTree(rt.dm, partShape(part), nullptr, &part, false));
}
static int pushShapecast(lua_State* L, Runtime::Impl& rt, const ShapeV& s, Vec3 d, Instance* skip, int paramsIdx) {
    RaycastFilter* f = lua_isnoneornil(L, paramsIdx) ? nullptr : &checkRaycastParams(L, paramsIdx);
    RayHit h;
    if (!shapecastTree(rt.dm, s, d, f, skip, h)) { lua_pushnil(L); return 1; }
    pushRayHit(L, rt, h);
    return 1;
}
static int Blockcast(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    ShapeV s; s.cf = checkCFrame(L, 2); s.half = mul(checkVec3(L, 3), 0.5f);
    return pushShapecast(L, rt, s, checkVec3(L, 4), nullptr, 5);
}
static int Spherecast(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    ShapeV s; s.cf = CFrameV::fromPos(checkVec3(L, 2)); s.sphere = true; s.radius = (float)luaL_checknumber(L, 3);
    return pushShapecast(L, rt, s, checkVec3(L, 4), nullptr, 5);
}
// The part's own shape swept; the part itself is not counted as a hit.
static int Shapecast(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Instance& part = rt.checkInstance(L, 2);
    if (!part.isA("BasePart")) luaL_error(L, "invalid argument #1 to 'Shapecast' (BasePart expected, got %s)", part.className().c_str());
    return pushShapecast(L, rt, partShape(part), checkVec3(L, 3), &part, 4);
}
// The classic form: part, position, normal, material, with reach capped at 5000 as on Roblox.
static int FindPartOnRay(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    RayV ray = checkRay(L, 2);
    RaycastFilter f;
    if (!lua_isnoneornil(L, 3)) addToFilter(L, 3, f);
    Vec3 d = ray.direction;
    if (len(d) > 5000) d = mul(unit(d), 5000);
    RayHit h;
    if (!raycastTree(rt.dm, ray.origin, d, &f, h)) {
        lua_pushnil(L); pushVec3(L, add(ray.origin, d)); pushVec3(L, {0, 0, 0}); rt.pushValue(L, Value::enumItem("Air", 1792), findEnum("Material"));
        return 4;
    }
    rt.pushInstance(L, h.part); pushVec3(L, h.position); pushVec3(L, h.normal); rt.pushValue(L, h.part->get("Material"), findEnum("Material"));
    return 4;
}
// Camera: pixels <-> rays
static int ViewportPointToRay(lua_State* L) {
    pushRay(L, cameraRay(self(L), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_optnumber(L, 4, 0)));
    return 1;
}
static int WorldToViewportPoint(lua_State* L) {
    Vec3 out;
    bool in = worldToViewport(self(L), checkVec3(L, 2), out);
    pushVec3(L, out); lua_pushboolean(L, in);
    return 2;
}
static int GetRenderCFrame(lua_State* L) { pushCFrame(L, partCFrame(self(L))); return 1; }

// ---- EditableImage -----------------------------------------------------------------
// A bitmap in RGBA8, at most 1024 a side, Size fixed at creation. An Object, not an Instance:
// AssetService makes it, a Content property shows it, and it lives while something holds it.
// A frame's drawing reaches the engine as one texture update per image.
// No WritePixels/ReadPixels, Resize, Rotate, Crop or Copy: Roblox has none of them either.
static Runtime::Impl::EditableImage& imageOf(lua_State* L, Runtime::Impl& rt, Instance& img, const char* method) {
    auto it = rt.editableImages.find(img.id());
    if (it == rt.editableImages.end() || it->second.destroyed) luaL_error(L, "%s: the EditableImage has been destroyed", method);
    if (it->second.placeholder) luaL_error(L, "%s: this EditableImage is a placeholder for one on the other side, and its contents cannot be read or written", method);
    return it->second;
}
static Instance& imageArg(lua_State* L, Runtime::Impl& rt, int idx, const char* method) {
    Instance* o = rt.toObject(L, idx);
    if (!o || !o->isA("EditableImage")) luaL_error(L, "%s: argument #%d must be an EditableImage", method, idx - 1);
    return *o;
}
static Instance::Ptr newEditableImage(Runtime::Impl& rt, int w, int h) {
    Instance::Ptr img = rt.dm.createInternal("EditableImage");
    img->setInternal("Size", Value::vector2((float)w, (float)h));
    auto& e = rt.editableImages[img->id()];
    e.w = w; e.h = h; e.rgba.assign((size_t)w * h * 4, 0); e.dirty = true;
    return img;
}
static int CreateEditableImage(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Vec2 size{512, 512};
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_getfield(L, 2, "Size");
        if (!lua_isnil(L, -1)) size = checkVector2(L, -1);
        lua_pop(L, 1);
    }
    if (size.x < 1 || size.y < 1 || size.x > 1024 || size.y > 1024)
        luaL_error(L, "CreateEditableImage: Size must be between 1 and 1024 on each side");
    rt.pushInstance(L, newEditableImage(rt, (int)size.x, (int)size.y).get());
    return 1;
}
// `cover` scales the source's alpha: an anti-aliased edge, or a transparency folded in by the
// caller. `mode` is an Enum.ImageCombineType value.
static void blendPixel(uint8_t* d, const uint8_t* s, int mode, float cover = 1) {
    float sa = s[3] / 255.f * cover, da = d[3] / 255.f;
    switch (mode) {
    case 2: {   // Overwrite: the source replaces the destination, alpha included
        for (int k = 0; k < 3; k++) d[k] = s[k];
        d[3] = (uint8_t)std::clamp(sa * 255.f + 0.5f, 0.f, 255.f);
        return;
    }
    case 3: for (int k = 0; k < 4; k++) d[k] = (uint8_t)std::min(255.f, d[k] + (k < 3 ? s[k] * cover : s[3] * cover)); return;   // Add
    case 4: for (int k = 0; k < 4; k++) { float m = (k < 3 ? s[k] : s[3]) / 255.f; d[k] = (uint8_t)std::clamp(d[k] * (1 - cover + cover * m) + 0.5f, 0.f, 255.f); } return;   // Multiply
    case 7: for (int k = 0; k < 4; k++) d[k] = (uint8_t)std::max(0.f, d[k] - (k < 3 ? s[k] * cover : s[3] * cover)); return;   // Subtract
    case 5: case 6: {
        // AlphaBlend (5): the destination colour counts whatever its own alpha. NormalMapBlend
        // (6) blends the same, then renormalises the result as a tangent-space normal.
        float c[3];
        for (int k = 0; k < 3; k++) c[k] = s[k] * sa + d[k] * (1 - sa);
        if (mode == 6) {
            float n[3], len = 0;
            for (int k = 0; k < 3; k++) { n[k] = c[k] / 255.f * 2 - 1; len += n[k] * n[k]; }
            len = std::sqrt(len);
            if (len > 1e-6f) for (int k = 0; k < 3; k++) c[k] = (n[k] / len * 0.5f + 0.5f) * 255;
        }
        for (int k = 0; k < 3; k++) d[k] = (uint8_t)std::clamp(c[k] + 0.5f, 0.f, 255.f);
        d[3] = (uint8_t)std::clamp((sa + da * (1 - sa)) * 255 + 0.5f, 0.f, 255.f);
        return;
    }
    default: {   // BlendSourceOver
        float oa = sa + da * (1 - sa);
        for (int k = 0; k < 3; k++) d[k] = oa > 0 ? (uint8_t)std::clamp((s[k] * sa + d[k] * da * (1 - sa)) / oa + 0.5f, 0.f, 255.f) : 0;
        d[3] = (uint8_t)std::clamp(oa * 255 + 0.5f, 0.f, 255.f);
    }
    }
}
static int combineArg(lua_State* L, int idx, int fallback = 1) {
    if (lua_isnoneornil(L, idx)) return fallback;
    const EnumItem* it = checkEnumItem(L, idx, findEnum("ImageCombineType"));
    return it ? it->value : fallback;
}
static bool antiAliasArg(lua_State* L, int idx) {
    if (lua_isnoneornil(L, idx)) return true;     // default: Enabled
    const EnumItem* it = checkEnumItem(L, idx, findEnum("AntiAliasing"));
    return it && it->value == 1;
}
static void colorArg(lua_State* L, int idx, uint8_t out[4]) {
    Col3 c = checkColor3(L, idx);
    double tr = luaL_optnumber(L, idx + 1, 0);
    out[0] = (uint8_t)std::clamp(c.r * 255.f + 0.5f, 0.f, 255.f); out[1] = (uint8_t)std::clamp(c.g * 255.f + 0.5f, 0.f, 255.f); out[2] = (uint8_t)std::clamp(c.b * 255.f + 0.5f, 0.f, 255.f);
    out[3] = (uint8_t)std::clamp((1 - tr) * 255 + 0.5, 0.0, 255.0);
}
static int WritePixelsBuffer(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& e = imageOf(L, rt, img, "WritePixelsBuffer");
    Vec2 pos = checkVector2(L, 2), sz = checkVector2(L, 3);
    size_t len = 0;
    const uint8_t* src = (const uint8_t*)lua_tobuffer(L, 4, &len);
    if (!src) luaL_error(L, "WritePixelsBuffer: expected a buffer");
    int x0 = (int)pos.x, y0 = (int)pos.y, w = (int)sz.x, h = (int)sz.y;
    if (x0 < 0 || y0 < 0 || w <= 0 || h <= 0 || x0 + w > e.w || y0 + h > e.h) luaL_error(L, "WritePixelsBuffer: the region is outside the image");
    if (len < (size_t)w * h * 4) luaL_error(L, "WritePixelsBuffer: the buffer holds fewer than %d bytes", w * h * 4);
    for (int y = 0; y < h; y++) std::memcpy(&e.rgba[((size_t)(y0 + y) * e.w + x0) * 4], src + (size_t)y * w * 4, (size_t)w * 4);
    e.dirty = true;
    return 0;
}
static int ReadPixelsBuffer(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& e = imageOf(L, rt, img, "ReadPixelsBuffer");
    Vec2 pos = checkVector2(L, 2), sz = checkVector2(L, 3);
    int x0 = (int)pos.x, y0 = (int)pos.y, w = (int)sz.x, h = (int)sz.y;
    if (x0 < 0 || y0 < 0 || w <= 0 || h <= 0 || x0 + w > e.w || y0 + h > e.h) luaL_error(L, "ReadPixelsBuffer: the region is outside the image");
    uint8_t* dst = (uint8_t*)lua_newbuffer(L, (size_t)w * h * 4);
    for (int y = 0; y < h; y++) std::memcpy(dst + (size_t)y * w * 4, &e.rgba[((size_t)(y0 + y) * e.w + x0) * 4], (size_t)w * 4);
    return 1;
}
static int DrawRectangle(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& e = imageOf(L, rt, img, "DrawRectangle");
    Vec2 pos = checkVector2(L, 2), sz = checkVector2(L, 3);
    uint8_t c[4]; colorArg(L, 4, c);
    int mode = combineArg(L, 6);
    // Alone among the drawing methods, this one will not take a position off the canvas.
    if (pos.x < 0 || pos.y < 0 || pos.x >= e.w || pos.y >= e.h) luaL_error(L, "DrawRectangle: the position is outside the image");
    int x0 = (int)pos.x, y0 = (int)pos.y, x1 = std::min(e.w, (int)(pos.x + sz.x)), y1 = std::min(e.h, (int)(pos.y + sz.y));
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) blendPixel(&e.rgba[((size_t)y * e.w + x) * 4], c, mode);
    e.dirty = true;
    return 0;
}
static int DrawLine(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& e = imageOf(L, rt, img, "DrawLine");
    Vec2 a = checkVector2(L, 2), b = checkVector2(L, 3);
    uint8_t c[4]; colorArg(L, 4, c);
    int mode = combineArg(L, 6);
    if (antiAliasArg(L, 7)) {
        // one pixel thick, covered by how far each pixel's centre is from the segment
        float ax = a.x + 0.5f, ay = a.y + 0.5f, bx = b.x + 0.5f, by = b.y + 0.5f;
        float vx = bx - ax, vy = by - ay, len2 = vx * vx + vy * vy;
        int x0 = std::max(0, (int)std::floor(std::min(ax, bx) - 1)), x1 = std::min(e.w - 1, (int)std::ceil(std::max(ax, bx) + 1));
        int y0 = std::max(0, (int)std::floor(std::min(ay, by) - 1)), y1 = std::min(e.h - 1, (int)std::ceil(std::max(ay, by) + 1));
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float t = len2 > 0 ? std::clamp(((px - ax) * vx + (py - ay) * vy) / len2, 0.f, 1.f) : 0;
            float dx = px - (ax + vx * t), dy = py - (ay + vy * t);
            float cover = std::clamp(1.0f - std::max(std::sqrt(dx * dx + dy * dy) - 0.5f, 0.f), 0.f, 1.f);
            if (cover > 0) blendPixel(&e.rgba[((size_t)y * e.w + x) * 4], c, mode, cover);
        }
    } else {
        int x0 = (int)a.x, y0 = (int)a.y, x1 = (int)b.x, y1 = (int)b.y;
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
        for (int guard = 0; guard < 1 << 22; guard++) {
            if (x0 >= 0 && y0 >= 0 && x0 < e.w && y0 < e.h) blendPixel(&e.rgba[((size_t)y0 * e.w + x0) * 4], c, mode);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
    e.dirty = true;
    return 0;
}
static int DrawCircle(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& e = imageOf(L, rt, img, "DrawCircle");
    Vec2 ctr = checkVector2(L, 2);
    double r = luaL_checknumber(L, 3);
    uint8_t c[4]; colorArg(L, 4, c);
    int mode = combineArg(L, 6);
    bool aa = antiAliasArg(L, 7);
    int x0 = std::max(0, (int)(ctr.x - r - 1)), y0 = std::max(0, (int)(ctr.y - r - 1)), x1 = std::min(e.w - 1, (int)(ctr.x + r + 1)), y1 = std::min(e.h - 1, (int)(ctr.y + r + 1));
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        double ddx = x + 0.5 - ctr.x, ddy = y + 0.5 - ctr.y, dist = std::sqrt(ddx * ddx + ddy * ddy);
        float cover = aa ? (float)std::clamp(r - dist + 0.5, 0.0, 1.0) : (dist <= r ? 1.f : 0.f);
        if (cover > 0) blendPixel(&e.rgba[((size_t)y * e.w + x) * 4], c, mode, cover);
    }
    e.dirty = true;
    return 0;
}
static int DrawImage(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& e = imageOf(L, rt, img, "DrawImage");
    Vec2 pos = checkVector2(L, 2);
    auto& s = imageOf(L, rt, imageArg(L, rt, 3, "DrawImage"), "DrawImage");
    int mode = combineArg(L, 4);
    for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) {
        int dx = (int)pos.x + x, dy = (int)pos.y + y;
        if (dx < 0 || dy < 0 || dx >= e.w || dy >= e.h) continue;
        blendPixel(&e.rgba[((size_t)dy * e.w + dx) * 4], &s.rgba[((size_t)y * s.w + x) * 4], mode);
    }
    e.dirty = true;
    return 0;
}
// `position` is where the pivot lands, not a corner. Options and their defaults: CombineType
// AlphaBlend, SamplingMode Default (bilinear; Pixelated is nearest), PivotPoint the source's middle.
static int DrawImageTransformed(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& e = imageOf(L, rt, img, "DrawImageTransformed");
    Vec2 pos = checkVector2(L, 2), scale = checkVector2(L, 3);
    double degrees = luaL_checknumber(L, 4);
    Instance& srcObj = imageArg(L, rt, 5, "DrawImageTransformed");
    auto& s = imageOf(L, rt, srcObj, "DrawImageTransformed");
    int mode = 5;
    bool nearest = false;
    Vec2 pivot{s.w / 2.f, s.h / 2.f};
    if (!lua_isnoneornil(L, 6)) {
        luaL_checktype(L, 6, LUA_TTABLE);
        lua_getfield(L, 6, "CombineType"); mode = combineArg(L, lua_gettop(L), 5); lua_pop(L, 1);
        lua_getfield(L, 6, "SamplingMode");
        if (!lua_isnil(L, -1)) nearest = checkEnumItem(L, lua_gettop(L), findEnum("ResamplerMode"))->value == 1;
        lua_pop(L, 1);
        lua_getfield(L, 6, "PivotPoint"); if (!lua_isnil(L, -1)) pivot = checkVector2(L, -1); lua_pop(L, 1);
    }
    if (std::abs(scale.x) < 1e-6f || std::abs(scale.y) < 1e-6f) return 0;
    const double rad = degrees * 3.14159265358979323846 / 180, cs = std::cos(rad), sn = std::sin(rad);
    // where the source's corners land, bounding the destination pixels worth visiting
    double lox = 1e18, loy = 1e18, hix = -1e18, hiy = -1e18;
    for (int k = 0; k < 4; k++) {
        double ux = ((k & 1) ? s.w : 0) - pivot.x, uy = ((k & 2) ? s.h : 0) - pivot.y;
        ux *= scale.x; uy *= scale.y;
        double px = pos.x + ux * cs - uy * sn, py = pos.y + ux * sn + uy * cs;
        lox = std::min(lox, px); hix = std::max(hix, px); loy = std::min(loy, py); hiy = std::max(hiy, py);
    }
    int x0 = std::max(0, (int)std::floor(lox)), x1 = std::min(e.w - 1, (int)std::ceil(hix));
    int y0 = std::max(0, (int)std::floor(loy)), y1 = std::min(e.h - 1, (int)std::ceil(hiy));
    auto at = [&](int x, int y) { x = std::clamp(x, 0, s.w - 1); y = std::clamp(y, 0, s.h - 1); return &s.rgba[((size_t)y * s.w + x) * 4]; };
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        double dx = x + 0.5 - pos.x, dy = y + 0.5 - pos.y;
        double sx = (dx * cs + dy * sn) / scale.x + pivot.x, sy = (-dx * sn + dy * cs) / scale.y + pivot.y;
        if (sx < 0 || sy < 0 || sx >= s.w || sy >= s.h) continue;
        uint8_t px[4];
        if (nearest) std::memcpy(px, at((int)sx, (int)sy), 4);
        else {
            double fx = sx - 0.5, fy = sy - 0.5;
            int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
            double tx = fx - ix, ty = fy - iy;
            for (int k = 0; k < 4; k++) {
                double top = at(ix, iy)[k] * (1 - tx) + at(ix + 1, iy)[k] * tx;
                double bot = at(ix, iy + 1)[k] * (1 - tx) + at(ix + 1, iy + 1)[k] * tx;
                px[k] = (uint8_t)std::clamp(top * (1 - ty) + bot * ty + 0.5, 0.0, 255.0);
            }
        }
        blendPixel(&e.rgba[((size_t)y * e.w + x) * 4], px, mode);
    }
    e.dirty = true;
    return 0;
}
// From a uri the host loads, or from an EditableImage or baked Opaque content, copied here.
static int CreateEditableImageAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    if (!isContent(L, 2)) luaL_error(L, "invalid argument #2 to 'CreateEditableImageAsync' (Content expected, got %s)", typeOfName(L, 2));
    Value c = checkContent(L, 2);
    Instance* from = c.n == Value::ContentObject || c.n == Value::ContentOpaque ? rt.dm.find(c.ref) : nullptr;
    if (from && from->isA("EditableImage")) {
        auto& s = imageOf(L, rt, *from, "CreateEditableImageAsync");
        Instance::Ptr copy = newEditableImage(rt, s.w, s.h);
        rt.editableImages[copy->id()].rgba = s.rgba;
        rt.pushInstance(L, copy.get());
        return 1;
    }
    if (from && from->className() == "DataModelContent") {
        if (from->get("Kind").s != "Image") luaL_error(L, "CreateEditableImageAsync: the content is not an image");
        Vec2 sz = from->get("Size").v2();
        const std::string& data = from->get("Data").s;
        Instance::Ptr copy = newEditableImage(rt, (int)sz.x, (int)sz.y);
        auto& e = rt.editableImages[copy->id()];
        if (data.size() >= e.rgba.size()) std::memcpy(e.rgba.data(), data.data(), e.rgba.size());
        rt.pushInstance(L, copy.get());
        return 1;
    }
    if (c.n != Value::ContentUri) luaL_error(L, "CreateEditableImageAsync: the Content names no image");
    if (!rt.cb.imageRequest) luaL_error(L, "CreateEditableImageAsync is not available in this runtime");
    const uint64_t id = rt.nextMesh++;
    Task* t = rt.taskFor(L);
    t->state = Task::Parked;
    rt.pendingImages[id] = t;
    rt.cb.imageRequest(id, c.s);
    return lua_yield(L, 0);
}

// A new, empty EditableMesh; FixedSize is always false.
static int CreateEditableMesh(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Instance::Ptr mesh = rt.dm.createInternal("EditableMesh");
    rt.editableMeshes[mesh->id()];
    rt.pushInstance(L, mesh.get());
    return 1;
}
// From a uri the host loads, or from an EditableMesh or baked mesh content, copied here.
// Fixed-size unless the options say FixedSize = false.
static int CreateEditableMeshAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    if (!isContent(L, 2)) luaL_error(L, "invalid argument #2 to 'CreateEditableMeshAsync' (Content expected, got %s)", typeOfName(L, 2));
    Value c = checkContent(L, 2);
    bool fixed = true;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua_getfield(L, 3, "FixedSize");
        if (!lua_isnil(L, -1)) fixed = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }
    Instance* from = c.n == Value::ContentObject || c.n == Value::ContentOpaque ? rt.dm.find(c.ref) : nullptr;
    auto made = [&](const EditableMeshData& data) {
        Instance::Ptr mesh = rt.dm.createInternal("EditableMesh");
        EditableMeshData& m = rt.editableMeshes[mesh->id()];
        m = data;
        m.fixedSize = fixed; m.destroyed = false; m.placeholder = false; m.dirty = true;
        mesh->setInternal("FixedSize", Value::boolean(fixed));
        rt.pushInstance(L, mesh.get());
        return 1;
    };
    if (from && from->isA("EditableMesh")) {
        EditableMeshData copy;
        copy.deserialize(em::meshOf(L, *from, "CreateEditableMeshAsync").serialize());
        return made(copy);
    }
    if (from && from->className() == "DataModelContent") {
        if (from->get("Kind").s != "Mesh") luaL_error(L, "CreateEditableMeshAsync: the content is not a mesh");
        EditableMeshData data;
        if (!data.deserialize(from->get("Data").s)) luaL_error(L, "CreateEditableMeshAsync: the content could not be read");
        return made(data);
    }
    if (c.n != Value::ContentUri) luaL_error(L, "CreateEditableMeshAsync: the Content names no mesh");
    if (!rt.cb.meshRequest) luaL_error(L, "CreateEditableMeshAsync is not available in this runtime");
    Runtime::Impl::PendingMeshPart pending;
    pending.uri = c.s;
    pending.editable = true;
    pending.fixedSize = fixed;
    const uint64_t id = rt.nextMesh++;
    Task* t = rt.taskFor(L);
    t->state = Task::Parked;
    pending.task = t;
    rt.pendingMeshPart[id] = std::move(pending);
    rt.cb.meshRequest(id, c.s, true);
    return lua_yield(L, 0);
}

// -> (CreateContentResult, Content). An EditableImage or EditableMesh baked into an Opaque
// Content that drives a property like an asset uri, replicating when the server made it and
// lasting while something holds it. Past the 200 MB Roblox budgets a server, the result is
// StorageLimitExceeded and an empty Content.
static int CreateDataModelContentAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    if (!isContent(L, 2)) luaL_error(L, "invalid argument #2 to 'CreateDataModelContentAsync' (Content expected, got %s)", typeOfName(L, 2));
    Value c = checkContent(L, 2);
    Instance* from = c.n == Value::ContentObject ? rt.dm.find(c.ref) : nullptr;
    if (!from || !(from->isA("EditableImage") || from->isA("EditableMesh")))
        luaL_error(L, "CreateDataModelContentAsync: the Content must hold an EditableImage or an EditableMesh");
    const EnumDef* result = findEnum("CreateContentResult");
    static constexpr size_t kBudget = 200u * 1024 * 1024;
    if (from->isA("EditableMesh")) {
        EditableMeshData& m = em::meshOf(L, *from, "CreateDataModelContentAsync");
        std::string data = m.serialize();
        if (rt.dataModelContentBytes + data.size() > kBudget) {
            pushEnumItem(L, result, result->find("StorageLimitExceeded"));
            pushContent(L, Value::content(Value::ContentNone));
            return 2;
        }
        Vec3 lo, hi; m.bounds(lo, hi);
        Instance::Ptr holder = rt.dm.createInternal("DataModelContent");
        holder->setInternal("Kind", Value::string("Mesh"));
        holder->setInternal("MeshBounds", lo.x > hi.x ? Value::vector3(0, 0, 0) : Value::vector3(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z));
        rt.dataModelContentBytes += data.size();
        holder->setInternal("Data", Value::string(std::move(data)));
        pushEnumItem(L, result, result->find("Success"));
        pushOpaqueContent(L, holder);
        return 2;
    }
    auto& s = imageOf(L, rt, *from, "CreateDataModelContentAsync");
    if (rt.dataModelContentBytes + s.rgba.size() > kBudget) {
        pushEnumItem(L, result, result->find("StorageLimitExceeded"));
        pushContent(L, Value::content(Value::ContentNone));
        return 2;
    }
    Instance::Ptr holder = rt.dm.createInternal("DataModelContent");
    holder->setInternal("Kind", Value::string("Image"));
    holder->setInternal("Size", Value::vector2((float)s.w, (float)s.h));
    holder->setInternal("Data", Value::string(std::string((const char*)s.rgba.data(), s.rgba.size())));
    rt.dataModelContentBytes += s.rgba.size();
    pushEnumItem(L, result, result->find("Success"));
    pushOpaqueContent(L, holder);
    return 2;
}

// ---- projection: EditableImage onto an EditableMesh's texture, and back -------------------------
// A projector: Position, Direction (where it faces), Up, and Size -- X and Y across, Z deep.
namespace {
struct Projector {
    Vec3 pos, fwd, up, right, size;
    // where a point lands: x, y across the projector (-0.5..0.5 inside), z along it (0..1 inside)
    bool local(Vec3 p, float& x, float& y, float& z) const {
        Vec3 d{p.x - pos.x, p.y - pos.y, p.z - pos.z};
        x = (d.x * right.x + d.y * right.y + d.z * right.z) / std::max(size.x, 1e-6f);
        y = (d.x * up.x + d.y * up.y + d.z * up.z) / std::max(size.y, 1e-6f);
        z = (d.x * fwd.x + d.y * fwd.y + d.z * fwd.z) / std::max(size.z, 1e-6f);
        return std::abs(x) <= 0.5f && std::abs(y) <= 0.5f && z >= 0 && z <= 1;
    }
};
Vec3 v3unit(Vec3 a) { float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); return l > 1e-9f ? Vec3{a.x / l, a.y / l, a.z / l} : Vec3{0, 0, 0}; }
Vec3 v3cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
}
static Projector projectorArg(lua_State* L, int idx, const char* method) {
    luaL_checktype(L, idx, LUA_TTABLE);
    Projector pr;
    auto vec = [&](const char* key, Vec3 fallback) {
        lua_getfield(L, idx, key);
        Vec3 v = lua_isnil(L, -1) ? fallback : checkVec3(L, -1);
        lua_pop(L, 1);
        return v;
    };
    pr.pos = vec("Position", {0, 0, 0});
    pr.fwd = v3unit(vec("Direction", {0, 0, -1}));
    Vec3 up = vec("Up", {0, 1, 0});
    pr.size = vec("Size", {1, 1, 1});
    // the caller's Up, made square to Direction
    float d = up.x * pr.fwd.x + up.y * pr.fwd.y + up.z * pr.fwd.z;
    pr.up = v3unit({up.x - pr.fwd.x * d, up.y - pr.fwd.y * d, up.z - pr.fwd.z * d});
    if (pr.up.x == 0 && pr.up.y == 0 && pr.up.z == 0) luaL_error(L, "%s: Up cannot be along Direction", method);
    pr.right = v3cross(pr.fwd, pr.up);
    return pr;
}
static void sampleBilinear(const Runtime::Impl::EditableImage& img, float u, float v, float px[4]) {
    float fx = u * img.w - 0.5f, fy = v * img.h - 0.5f;
    int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    float tx = fx - ix, ty = fy - iy;
    auto at = [&](int x, int y) { x = std::clamp(x, 0, img.w - 1); y = std::clamp(y, 0, img.h - 1); return &img.rgba[((size_t)y * img.w + x) * 4]; };
    for (int k = 0; k < 4; k++)
        px[k] = (at(ix, iy)[k] * (1 - tx) + at(ix + 1, iy)[k] * tx) * (1 - ty) + (at(ix, iy + 1)[k] * (1 - tx) + at(ix + 1, iy + 1)[k] * tx) * ty;
}
// FadeAngle: full strength while the surface faces the projector within it, falling to nothing at 90.
static float fadeWeight(Vec3 faceNormal, const Projector& pr, float fadeAngle) {
    Vec3 n = v3unit(faceNormal);
    float c = -(n.x * pr.fwd.x + n.y * pr.fwd.y + n.z * pr.fwd.z);
    if (c <= 0) return 0;
    if (fadeAngle >= 90) return 1;
    float angle = std::acos(std::min(1.f, c)) * 180 / 3.14159265f;
    if (angle <= fadeAngle) return 1;
    return std::clamp(1 - (angle - fadeAngle) / (90 - fadeAngle), 0.f, 1.f);
}
static void brushBlend(uint8_t* d, const float s[4], int colorMode, int alphaMode, float weight) {
    uint8_t src[4];
    for (int k = 0; k < 4; k++) src[k] = (uint8_t)std::clamp(s[k] + 0.5f, 0.f, 255.f);
    uint8_t before[4];
    std::memcpy(before, d, 4);
    blendPixel(d, src, colorMode, weight);
    if (alphaMode == 2) d[3] = before[3];                                  // LockCanvasAlpha
    if (alphaMode == 3) for (int k = 0; k < 3; k++) d[k] = before[k];      // LockCanvasColor
}
// The Decal projected onto the mesh and written here through the mesh's UVs: this image is the
// mesh's texture, not the view.
static int DrawImageProjected(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& dst = imageOf(L, rt, img, "DrawImageProjected");
    Instance* meshObj = rt.toObject(L, 2);
    if (!meshObj || !meshObj->isA("EditableMesh")) luaL_error(L, "DrawImageProjected: argument #1 must be an EditableMesh");
    EditableMeshData& mesh = em::meshOf(L, *meshObj, "DrawImageProjected");
    Projector pr = projectorArg(L, 3, "DrawImageProjected");
    luaL_checktype(L, 4, LUA_TTABLE);
    lua_getfield(L, 4, "Decal");
    Instance& decalObj = imageArg(L, rt, lua_gettop(L), "DrawImageProjected");
    lua_pop(L, 1);
    auto& decal = imageOf(L, rt, decalObj, "DrawImageProjected");
    lua_getfield(L, 4, "ColorBlendType"); int colorMode = combineArg(L, lua_gettop(L), 5); lua_pop(L, 1);
    lua_getfield(L, 4, "AlphaBlendType"); int alphaMode = lua_isnil(L, -1) ? 1 : checkEnumItem(L, lua_gettop(L), findEnum("ImageAlphaType"))->value; lua_pop(L, 1);
    lua_getfield(L, 4, "FadeAngle"); float fade = lua_isnil(L, -1) ? 90 : (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 4, "BlendIntensity"); float intensity = lua_isnil(L, -1) ? 1 : std::clamp((float)lua_tonumber(L, -1), 0.f, 1.f); lua_pop(L, 1);
    for (const auto& f : mesh.faces) {
        if (!f.alive) continue;
        Vec3 p[3]; float tu[3], tv[3];
        for (int k = 0; k < 3; k++) { p[k] = mesh.verts[f.v[k]].p; tu[k] = mesh.uvs[f.u[k]].u * dst.w; tv[k] = mesh.uvs[f.u[k]].v * dst.h; }
        float w = fadeWeight(mesh.faceNormal(f), pr, fade) * intensity;
        if (w <= 0) continue;
        int x0 = std::max(0, (int)std::floor(std::min({tu[0], tu[1], tu[2]}))), x1 = std::min(dst.w - 1, (int)std::ceil(std::max({tu[0], tu[1], tu[2]})));
        int y0 = std::max(0, (int)std::floor(std::min({tv[0], tv[1], tv[2]}))), y1 = std::min(dst.h - 1, (int)std::ceil(std::max({tv[0], tv[1], tv[2]})));
        float area = (tu[1] - tu[0]) * (tv[2] - tv[0]) - (tu[2] - tu[0]) * (tv[1] - tv[0]);
        if (std::abs(area) < 1e-9f) continue;
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float cx = x + 0.5f, cy = y + 0.5f;
            float b1 = ((cx - tu[0]) * (tv[2] - tv[0]) - (tu[2] - tu[0]) * (cy - tv[0])) / area;
            float b2 = ((tu[1] - tu[0]) * (cy - tv[0]) - (cx - tu[0]) * (tv[1] - tv[0])) / area;
            float b0 = 1 - b1 - b2;
            if (b0 < -1e-4f || b1 < -1e-4f || b2 < -1e-4f) continue;
            Vec3 at{p[0].x * b0 + p[1].x * b1 + p[2].x * b2, p[0].y * b0 + p[1].y * b1 + p[2].y * b2, p[0].z * b0 + p[1].z * b1 + p[2].z * b2};
            float lx, ly, lz;
            if (!pr.local(at, lx, ly, lz)) continue;
            float px[4];
            sampleBilinear(decal, lx + 0.5f, 0.5f - ly, px);
            brushBlend(&dst.rgba[((size_t)y * dst.w + x) * 4], px, colorMode, alphaMode, w);
        }
    }
    dst.dirty = true;
    return 0;
}
// The inverse: a ray per pixel of this image from the projector's face, and the nearest face
// towards it gives the UV to read sourceTexture at.
static int SampleImageProjected(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto& dst = imageOf(L, rt, img, "SampleImageProjected");
    Instance* meshObj = rt.toObject(L, 2);
    if (!meshObj || !meshObj->isA("EditableMesh")) luaL_error(L, "SampleImageProjected: argument #1 must be an EditableMesh");
    EditableMeshData& mesh = em::meshOf(L, *meshObj, "SampleImageProjected");
    auto& src = imageOf(L, rt, imageArg(L, rt, 3, "SampleImageProjected"), "SampleImageProjected");
    Projector pr = projectorArg(L, 4, "SampleImageProjected");
    int colorMode = 2, alphaMode = 1;
    float fade = 180;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua_getfield(L, 5, "ColorBlendType"); colorMode = combineArg(L, lua_gettop(L), 2); lua_pop(L, 1);
        lua_getfield(L, 5, "AlphaBlendType"); if (!lua_isnil(L, -1)) alphaMode = checkEnumItem(L, lua_gettop(L), findEnum("ImageAlphaType"))->value; lua_pop(L, 1);
        lua_getfield(L, 5, "FadeAngle"); if (!lua_isnil(L, -1)) fade = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    }
    for (int y = 0; y < dst.h; y++) for (int x = 0; x < dst.w; x++) {
        float lx = (x + 0.5f) / dst.w - 0.5f, ly = 0.5f - (y + 0.5f) / dst.h;
        Vec3 o{pr.pos.x + pr.right.x * lx * pr.size.x + pr.up.x * ly * pr.size.y,
               pr.pos.y + pr.right.y * lx * pr.size.x + pr.up.y * ly * pr.size.y,
               pr.pos.z + pr.right.z * lx * pr.size.x + pr.up.z * ly * pr.size.y};
        float best = pr.size.z; const EditableMeshData::F* hit = nullptr; float hb[3] = {0, 0, 0};
        for (const auto& f : mesh.faces) {
            if (!f.alive) continue;
            Vec3 a = mesh.verts[f.v[0]].p, b = mesh.verts[f.v[1]].p, c = mesh.verts[f.v[2]].p;
            Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z}, e2{c.x - a.x, c.y - a.y, c.z - a.z};
            Vec3 pv = v3cross(pr.fwd, e2);
            float det = e1.x * pv.x + e1.y * pv.y + e1.z * pv.z;
            if (std::abs(det) < 1e-12f) continue;
            Vec3 s{o.x - a.x, o.y - a.y, o.z - a.z};
            float u = (s.x * pv.x + s.y * pv.y + s.z * pv.z) / det; if (u < 0 || u > 1) continue;
            Vec3 q = v3cross(s, e1);
            float v = (pr.fwd.x * q.x + pr.fwd.y * q.y + pr.fwd.z * q.z) / det; if (v < 0 || u + v > 1) continue;
            float t = (e2.x * q.x + e2.y * q.y + e2.z * q.z) / det;
            if (t < 0 || t > best) continue;
            if (fadeWeight(mesh.faceNormal(f), pr, std::min(fade, 90.f)) <= 0) continue;   // facing the projector
            best = t; hit = &f; hb[0] = 1 - u - v; hb[1] = u; hb[2] = v;
        }
        if (!hit) continue;
        float su = 0, sv = 0;
        for (int k = 0; k < 3; k++) { su += mesh.uvs[hit->u[k]].u * hb[k]; sv += mesh.uvs[hit->u[k]].v * hb[k]; }
        float px[4];
        sampleBilinear(src, su, sv, px);
        float w = fade >= 180 ? 1 : fadeWeight(mesh.faceNormal(*hit), pr, fade);
        brushBlend(&dst.rgba[((size_t)y * dst.w + x) * 4], px, colorMode, alphaMode, w);
    }
    dst.dirty = true;
    return 0;
}

// Frees the pixels at once: anything showing the image shows nothing, and it takes no more drawing.
static int ImageDestroy(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& img = self(L);
    auto it = rt.editableImages.find(img.id());
    if (it == rt.editableImages.end() || it->second.destroyed) return 0;
    it->second.rgba.clear();
    it->second.rgba.shrink_to_fit();
    it->second.destroyed = true;
    it->second.dirty = true;
    return 0;
}

// ---- the Studio's plugins ---------------------------------------------------------
// `plugin` is a Plugin given to a script under PluginDebugService. These objects sit outside the
// tree, in rt.pluginObjects.
static Instance::Ptr pluginObject(Runtime::Impl& rt, const char* cls, const std::string& name) {
    Instance::Ptr o = rt.dm.createInternal(cls);
    o->setName(name);
    rt.pluginObjects.push_back(o);
    return o;
}
static int PluginCreateToolbar(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& pl = self(L);
    Instance::Ptr tb = pluginObject(rt, "PluginToolbar", luaL_checkstring(L, 2));
    rt.toolbarPlugin[tb->id()] = &pl;
    rt.pushInstance(L, tb.get());
    return 1;
}
static int ToolbarCreateButton(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& tb = self(L);
    std::string id = luaL_checkstring(L, 2), tip = luaL_optstring(L, 3, ""), icon = luaL_optstring(L, 4, ""), text = luaL_optstring(L, 5, "");
    Instance::Ptr b = pluginObject(rt, "PluginToolbarButton", id);
    b->setInternal("Icon", Value::string(icon));
    Runtime::Impl::PluginButtonInfo info;
    info.id = b->id(); info.toolbar = tb.id(); info.toolbarName = tb.name(); info.buttonId = id; info.tooltip = tip; info.icon = icon; info.text = text.empty() ? id : text;
    auto pit = rt.toolbarPlugin.find(tb.id());
    if (pit != rt.toolbarPlugin.end()) info.plugin = pit->second->id();
    rt.pluginButtons.push_back(info);
    rt.pluginButtonsDirty = true;
    rt.pushInstance(L, b.get());
    return 1;
}
static int ButtonSetActive(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& b = self(L);
    bool on = lua_toboolean(L, 2);
    for (auto& i : rt.pluginButtons) if (i.id == b.id() && i.active != on) { i.active = on; rt.pluginButtonsDirty = true; }
    return 0;
}
static void pluginDeactivation(Runtime::Impl& rt, Instance& pl) {
    if (pl.binding) if (Signal* s = rt.findSignal(pl, "Deactivation")) if (s->hasConns()) rt.fireValues(pl, "Deactivation", {});
}
static int PluginActivate(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& pl = self(L);
    if (rt.activePlugin && rt.activePlugin != &pl) pluginDeactivation(rt, *rt.activePlugin);
    rt.activePlugin = &pl; rt.activeExclusive = lua_toboolean(L, 2); rt.pluginActiveDirty = true;
    return 0;
}
static int PluginDeactivate(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& pl = self(L);
    if (rt.activePlugin == &pl) { rt.activePlugin = nullptr; rt.pluginActiveDirty = true; pluginDeactivation(rt, pl); }
    return 0;
}
static int PluginIsActivated(lua_State* L) { Runtime::Impl& rt = rtOf(L); lua_pushboolean(L, rt.activePlugin == &self(L)); return 1; }
static int PluginIsActivatedExclusive(lua_State* L) { Runtime::Impl& rt = rtOf(L); Instance& pl = self(L); lua_pushboolean(L, rt.activePlugin == &pl && rt.activeExclusive); return 1; }
static int PluginGetMouse(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& pl = self(L);
    auto it = rt.pluginMouse.find(pl.id());
    if (it == rt.pluginMouse.end()) { Instance::Ptr m = pluginObject(rt, "PluginMouse", "PluginMouse"); it = rt.pluginMouse.emplace(pl.id(), m.get()).first; }
    rt.pushInstance(L, it->second);
    return 1;
}
static int PluginGetSetting(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& pl = self(L);
    std::string key = luaL_checkstring(L, 2);
    auto pit = rt.pluginSettings.find(pl.name());
    if (pit == rt.pluginSettings.end()) { lua_pushnil(L); return 1; }
    auto kit = pit->second.find(key);
    if (kit == pit->second.end() || kit->second == "null") { lua_pushnil(L); return 1; }
    lua_settop(L, 0);
    lua_pushlstring(L, kit->second.data(), kit->second.size());
    return json_decode(L);
}
static int PluginSetSetting(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& pl = self(L);
    std::string key = luaL_checkstring(L, 2), json = "null";
    lua_settop(L, 3); lua_remove(L, 1); lua_remove(L, 1);   // the value alone
    if (!lua_isnil(L, 1)) { json_encode(L); size_t n = 0; const char* s = lua_tolstring(L, -1, &n); json.assign(s ? s : "", n); }
    rt.pluginSettings[pl.name()][key] = json;
    rt.pluginSettingWrites.push_back({pl.name(), key, json});
    return 0;
}
static int PluginOpenScript(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Instance& s = rt.checkInstance(L, 2);
    rt.openScriptRequests.push_back({s.id(), (int)luaL_optinteger(L, 3, 1)});
    return 0;
}
static int PluginCreateDockWidget(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    std::string id = luaL_checkstring(L, 2);
    Instance::Ptr g = rt.dm.createInternal("DockWidgetPluginGui", rt.dm.getService("CoreGui"));
    g->setName(id);
    g->setInternal("Title", Value::string(id));
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "InitialEnabled"); if (!lua_isnil(L, -1)) g->set("Enabled", Value::boolean(lua_toboolean(L, -1))); lua_pop(L, 1);
        lua_getfield(L, 3, "FloatingXSize"); if (lua_isnumber(L, -1) && lua_tonumber(L, -1) > 0) g->setInternal("FloatingXSize", Value::number(lua_tonumber(L, -1))); lua_pop(L, 1);
        lua_getfield(L, 3, "FloatingYSize"); if (lua_isnumber(L, -1) && lua_tonumber(L, -1) > 0) g->setInternal("FloatingYSize", Value::number(lua_tonumber(L, -1))); lua_pop(L, 1);
    }
    rt.pushInstance(L, g.get());
    return 1;
}
static int PluginCreateAction(lua_State* L) {   // (actionId, text, statusTip, iconName?, allowBinding?)
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Instance::Ptr a = pluginObject(rt, "PluginAction", luaL_checkstring(L, 2));
    a->setInternal("ActionId", Value::string(lua_tostring(L, 2)));
    a->setInternal("Text", Value::string(luaL_optstring(L, 3, "")));
    a->setInternal("StatusTip", Value::string(luaL_optstring(L, 4, "")));
    a->setInternal("AllowBinding", Value::boolean(lua_isnoneornil(L, 6) ? true : lua_toboolean(L, 6)));
    rt.pushInstance(L, a.get());
    return 1;
}
static int SelectionGet(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    lua_newtable(L);
    int n = 0;
    for (int64_t id : rt.selection) if (Instance* i = rt.dm.find(id)) { rt.pushInstance(L, i); lua_rawseti(L, -2, ++n); }
    return 1;
}
static std::vector<int64_t> instanceList(lua_State* L, int idx) {
    Runtime::Impl& rt = rtOf(L);
    std::vector<int64_t> ids;
    if (!lua_istable(L, idx)) return ids;
    int n = lua_objlen(L, idx);
    for (int i = 1; i <= n; i++) { lua_rawgeti(L, idx, i); if (Instance* inst = rt.toInstance(L, -1)) ids.push_back(inst->id()); lua_pop(L, 1); }
    return ids;
}
static void selectionSet(Runtime::Impl& rt, std::vector<int64_t> ids) {
    rt.selection = std::move(ids);
    rt.selectionRequested = true;
    Instance* sel = rt.dm.getService("Selection");
    if (sel && sel->binding) if (Signal* s = rt.findSignal(*sel, "SelectionChanged")) if (s->hasConns()) rt.fireValues(*sel, "SelectionChanged", {});
}
static int SelectionSet(lua_State* L) { (void)self(L); selectionSet(rtOf(L), instanceList(L, 2)); return 0; }
static int SelectionAdd(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    std::vector<int64_t> ids = rt.selection;
    for (int64_t id : instanceList(L, 2)) if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    selectionSet(rt, ids);
    return 0;
}
static int SelectionRemove(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    std::vector<int64_t> ids = rt.selection;
    for (int64_t id : instanceList(L, 2)) ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    selectionSet(rt, ids);
    return 0;
}
// A waypoint closes the undo entry the Studio is building from what plugins changed since the
// last one; a recording is a waypoint named at its start and closed at its finish.
static int HistorySetWaypoint(lua_State* L) { (void)self(L); rtOf(L).pluginWaypoints.push_back(luaL_optstring(L, 2, "plugin edit")); return 0; }
static int HistoryTryBegin(lua_State* L) {   // nil while one is in progress, as on Roblox
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    if (rt.historyRecording) { lua_pushnil(L); return 1; }
    rt.historyRecording = true;
    lua_pushstring(L, luaL_optstring(L, 2, "plugin edit"));
    return 1;
}
static int HistoryFinish(lua_State* L) { Runtime::Impl& rt = rtOf(L); (void)self(L); rt.historyRecording = false; rt.pluginWaypoints.push_back(luaL_optstring(L, 2, "plugin edit")); return 0; }
static int HistoryIsRecording(lua_State* L) { Runtime::Impl& rt = rtOf(L); (void)self(L); lua_pushboolean(L, rt.historyRecording); return 1; }
static int PushFalse(lua_State* L) { (void)self(L); lua_pushboolean(L, 0); return 1; }

// One Mouse per client, kept outside the tree.
static int GetMouse(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    if (!rt.mouse) { rt.mouse = rt.dm.createInternal("Mouse"); rt.mouse->setName("Mouse"); }
    rt.pushInstance(L, rt.mouse.get());
    return 1;
}
// The camera's ray through the pointer, cast past the local character and the TargetFilter.
static bool mouseRay(Runtime::Impl& rt, Instance& mouse, RayV& ray, RayHit& hit) {
    if (mouse.isA("PluginMouse")) {   // the Studio's own camera, through the pointer over its view
        if (!rt.pluginRayValid) return false;
        ray = rt.pluginRay;
    } else {
        Instance* cam = rt.dm.find(rt.dm.workspace()->get("CurrentCamera").ref);
        if (!cam) return false;
        ray = cameraRay(*cam, (float)mouse.get("X").n, (float)mouse.get("Y").n);
    }
    RaycastFilter f;
    if (rt.localPlayer) if (int64_t ch = rt.localPlayer->get("Character").ref) f.ids.push_back(ch);
    if (int64_t tf = mouse.get("TargetFilter").ref) f.ids.push_back(tf);
    return raycastTree(rt.dm, ray.origin, mul(ray.direction, 1000), &f, hit);
}

// UserInputService: what Runtime::input has been told (nothing, on a server).
static int IsKeyDown(lua_State* L) {
    (void)self(L);
    const EnumItem* k = checkEnumItem(L, 2, findEnum("KeyCode"));
    lua_pushboolean(L, rtOf(L).keysDown.count(k->name) > 0);
    return 1;
}
static int IsMouseButtonPressed(lua_State* L) {
    (void)self(L);
    const EnumItem* b = checkEnumItem(L, 2, findEnum("UserInputType"));
    lua_pushboolean(L, rtOf(L).mouseDown.count(b->name) > 0);
    return 1;
}
// Unix seconds with the fraction. Roblox keeps every client in step with the server's clock;
// here each process reads its own, so the sides agree only to their machines' drift.
static int GetServerTimeNow(lua_State* L) {
    (void)self(L);
    double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    lua_pushnumber(L, now);
    return 1;
}
static int GetMouseLocation(lua_State* L) { (void)self(L); Vec3 p = rtOf(L).mousePos; pushVector2(L, {p.x, p.y}); return 1; }
static int GetMouseDelta(lua_State* L) { (void)self(L); Vec3 d = rtOf(L).mouseDelta; pushVector2(L, {d.x, d.y}); return 1; }
static int GetKeysPressed(lua_State* L) {
    (void)self(L);
    Runtime::Impl& rt = rtOf(L);
    lua_newtable(L);
    int n = 0;
    for (const std::string& k : rt.keysDown) {
        auto it = rt.inputObjects.find("Keyboard\n" + k);
        if (it == rt.inputObjects.end()) continue;
        rt.pushInstance(L, it->second.get());
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}
static int GetLastInputType(lua_State* L) {
    (void)self(L);
    const EnumDef* e = findEnum("UserInputType");
    const EnumItem* i = e->find(rtOf(L).lastInputType);
    pushEnumItem(L, e, i ? i : e->find("None"));
    return 1;
}

// BindAction(name, handler, createTouchButton, ...inputs) calls handler(actionName, inputState,
// inputObject). Runtime::input runs the highest-priority, most recent binding first; returning
// Enum.ContextActionResult.Pass lets the next one, then UserInputService, see the input, and
// anything else sinks it.
static int bindAction(lua_State* L, bool atPriority) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    std::string name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    Runtime::Impl::ActionBinding b;
    b.name = name;
    int first = 5;
    if (atPriority) {
        const EnumDef* e; const EnumItem* it;
        b.priority = readEnumItem(L, 5, e, it) ? it->value : (int)luaL_checknumber(L, 5);
        first = 6;
    }
    for (int i = first; i <= lua_gettop(L); i++) {
        const EnumDef* e; const EnumItem* it;
        if (!readEnumItem(L, i, e, it) || (std::strcmp(e->name, "KeyCode") && std::strcmp(e->name, "UserInputType")))
            luaL_error(L, "%s: argument #%d must be an Enum.KeyCode or Enum.UserInputType item", atPriority ? "BindActionAtPriority" : "BindAction", i - 1);
        b.inputs.push_back(std::string(e->name) + "." + it->name);
    }
    rt.unbindAction(name);                     // binding a name again replaces the old binding
    b.fnRef = lua_ref(L, 3);
    b.ctx = rt.ctxOf(L);
    b.order = rt.nextActionOrder++;
    rt.actionBindings.push_back(std::move(b));
    return 0;
}
static int BindAction(lua_State* L) { return bindAction(L, false); }
static int BindActionAtPriority(lua_State* L) { return bindAction(L, true); }
static int UnbindAction(lua_State* L) { (void)self(L); rtOf(L).unbindAction(luaL_checkstring(L, 2)); return 0; }
static int UnbindAllActions(lua_State* L) {
    (void)self(L);
    Runtime::Impl& rt = rtOf(L);
    for (auto& b : rt.actionBindings) lua_unref(L, b.fnRef);
    rt.actionBindings.clear();
    return 0;
}
static int GetAllBoundActionInfo(lua_State* L) {
    (void)self(L);
    lua_newtable(L);
    for (auto& b : rtOf(L).actionBindings) {
        lua_newtable(L);
        lua_pushnumber(L, b.priority); lua_setfield(L, -2, "priorityLevel");
        lua_pushnumber(L, (double)b.order); lua_setfield(L, -2, "stackOrder");
        lua_newtable(L);
        int n = 0;
        for (auto& in : b.inputs) {
            size_t dot = in.find('.');
            const EnumDef* e = findEnum(in.substr(0, dot));
            const EnumItem* it = e ? e->find(in.substr(dot + 1)) : nullptr;
            if (it) { pushEnumItem(L, e, it); lua_rawseti(L, -2, ++n); }
        }
        lua_setfield(L, -2, "inputTypes");
        lua_setfield(L, -2, b.name.c_str());
    }
    return 1;
}

static int GetDebugId(lua_State* L) { lua_pushfstring(L, "%d", (int)self(L).id()); return 1; }

// ---- the gui family ------------------------------------------------------------------
// The topbar gap the engine keeps clear above every ScreenGui (kGuiInsetTop in pulseblockz_world.h).
static constexpr float kGuiInsetTopPx = 36;
static int GetGuiInset(lua_State* L) { (void)self(L); pushVector2(L, {0, kGuiInsetTopPx}); pushVector2(L, {0, 0}); return 2; }
// The GuiObjects under a point of the screen, in AbsolutePosition's space, front first: a higher
// ZIndex, then the deeper object. Only visible ones, under enabled LayerCollectors.
static int GetGuiObjectsAtPosition(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& gui = self(L);
    float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3);
    struct Hit { Instance* g; double z; int depth; };
    std::vector<Hit> hits;
    for (Instance* d : gui.getDescendants()) {
        if (!d->isA("GuiObject")) continue;
        bool shown = true;
        int depth = 0;
        for (Instance* a = d; a && a != &gui; a = a->parent(), depth++) {
            if (a->isA("GuiObject") && !a->get("Visible").b) { shown = false; break; }
            if (a->isA("LayerCollector") && !a->get("Enabled").b) { shown = false; break; }
        }
        if (!shown) continue;
        Vec2 p = d->get("AbsolutePosition").v2(), s = d->get("AbsoluteSize").v2();
        if (x < p.x || y < p.y || x > p.x + s.x || y > p.y + s.y) continue;
        hits.push_back({d, d->get("ZIndex").n, depth});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.z != b.z ? a.z > b.z : a.depth > b.depth; });
    std::vector<Instance*> out;
    for (auto& h : hits) out.push_back(h.g);
    return pushParts(L, rt, out);
}
// GuiService.SelectedObject: the old object loses, the new one gains, and every GuiBase2d over
// either hears SelectionChanged(amISelected, previous, new).
static void moveSelection(Runtime::Impl& rt, Instance& svc, Instance* to) {
    Instance* from = rt.dm.findRef(svc.get("SelectedObject").ref);
    if (from == to) return;
    svc.set("SelectedObject", Value::instance(to ? to->id() : 0));
    Value vFrom = Value::instance(from ? from->id() : 0), vTo = Value::instance(to ? to->id() : 0);
    if (from && !from->destroyed()) rt.fireValues(*from, "SelectionLost", {});
    if (to) rt.fireValues(*to, "SelectionGained", {});
    std::vector<Instance*> told;
    for (Instance* start : {from, to})
        for (Instance* a = start; a && a->isA("GuiBase2d"); a = a->parent()) {
            if (std::find(told.begin(), told.end(), a) != told.end()) continue;
            told.push_back(a);
            bool mine = false;
            for (Instance* b = to; b; b = b->parent()) if (b == a) { mine = true; break; }
            rt.fireValues(*a, "SelectionChanged", {Value::boolean(mine), vFrom, vTo});
        }
}
// Select(parent): the Selectable, visible GuiObject under it with the lowest SelectionOrder, the
// first in the tree among equals; nothing under it leaves the selection where it was.
static int GuiSelect(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& svc = self(L);
    Instance* parent = rt.toInstance(L, 2);
    if (!parent) luaL_error(L, "invalid argument #2 to 'Select' (Instance expected, got %s)", typeOfName(L, 2));
    Instance* best = nullptr;
    double bestOrder = 0;
    for (Instance* d : parent->getDescendants()) {
        if (!d->isA("GuiObject") || !d->get("Selectable").b) continue;
        bool shown = true;
        for (Instance* a = d; a && a->isA("GuiObject"); a = a->parent()) if (!a->get("Visible").b) { shown = false; break; }
        if (!shown) continue;
        double order = d->get("SelectionOrder").n;
        if (!best || order < bestOrder) { best = d; bestOrder = order; }
    }
    if (best) moveSelection(rt, svc, best);
    return 0;
}
// The Get / Set pairs over the hidden flags: no inspect menu, emotes menu or pause card opens here.
static int guiFlagGet(lua_State* L, const char* prop) { lua_pushboolean(L, self(L).get(prop).b); return 1; }
static int guiFlagSet(lua_State* L, const char* prop) { self(L).setInternal(prop, Value::boolean(lua_toboolean(L, 2))); return 0; }
static int GetInspectMenuEnabled(lua_State* L) { return guiFlagGet(L, "InspectMenuEnabled"); }
static int SetInspectMenuEnabled(lua_State* L) { return guiFlagSet(L, "InspectMenuEnabled"); }
static int GetEmotesMenuOpen(lua_State* L) { return guiFlagGet(L, "EmotesMenuOpen"); }
static int SetEmotesMenuOpen(lua_State* L) { return guiFlagSet(L, "EmotesMenuOpen"); }
static int GetGameplayPausedNotificationEnabled(lua_State* L) { return guiFlagGet(L, "GameplayPausedNotificationEnabled"); }
static int SetGameplayPausedNotificationEnabled(lua_State* L) { return guiFlagSet(L, "GameplayPausedNotificationEnabled"); }
static int CloseInspectMenu(lua_State* L) { (void)self(L); return 0; }

// UserInputService's gamepad and motion answers: nothing here is a gamepad or a sensor, so no
// gamepad is connected, none is for navigation, and the device readings are InputObjects at rest.
static int noGamepads(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
static int GetConnectedGamepads(lua_State* L) { return noGamepads(L); }
static int GetNavigationGamepads(lua_State* L) { return noGamepads(L); }
static int GetSupportedGamepadKeyCodes(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); lua_newtable(L); return 1; }
static int GetGamepadState(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); lua_newtable(L); return 1; }
static int GetGamepadConnected(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); lua_pushboolean(L, false); return 1; }
static int IsNavigationGamepad(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); lua_pushboolean(L, false); return 1; }
static int SetNavigationGamepad(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); return 0; }
static int GamepadSupports(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); checkEnumItem(L, 3, findEnum("KeyCode")); lua_pushboolean(L, false); return 1; }
static int IsGamepadButtonDown(lua_State* L) { return GamepadSupports(L); }
static Instance* restingInput(Runtime::Impl& rt, const char* type) {
    const EnumDef* types = findEnum("UserInputType"); const EnumItem* ti = types->find(type);
    const EnumDef* keys = findEnum("KeyCode"); const EnumItem* ki = keys->find("Unknown");
    Instance::Ptr& obj = rt.inputObjects[std::string(type) + "\n" + ki->name];
    if (!obj) {
        obj = rt.dm.createInternal("InputObject"); obj->setName("InputObject");
        obj->setInternal("UserInputType", Value::enumItem(ti->name, ti->value));
        obj->setInternal("KeyCode", Value::enumItem(ki->name, ki->value));
    }
    return obj.get();
}
static int GetDeviceAcceleration(lua_State* L) { (void)self(L); Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, restingInput(rt, "Accelerometer")); return 1; }
static int GetDeviceGravity(lua_State* L) { (void)self(L); Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, restingInput(rt, "Accelerometer")); return 1; }
static int GetDeviceRotation(lua_State* L) { (void)self(L); Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, restingInput(rt, "Gyro")); pushCFrame(L, CFrameV::identity()); return 2; }
// The InputObjects of the mouse buttons held now.
static int GetMouseButtonsPressed(lua_State* L) {
    (void)self(L);
    Runtime::Impl& rt = rtOf(L);
    lua_newtable(L);
    int n = 0;
    for (const std::string& b : rt.mouseDown) {
        auto it = rt.inputObjects.find(b + "\nUnknown");
        if (it == rt.inputObjects.end()) continue;
        rt.pushInstance(L, it->second.get());
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}
// The character a key puts in on a US layout: a printable KeyCode's own, letters upper-case; ""
// for the rest. Roblox's answers for other layouts and for gamepad buttons are not verified here.
static int GetStringForKeyCode(lua_State* L) {
    (void)self(L);
    const EnumItem* k = checkEnumItem(L, 2, findEnum("KeyCode"));
    if (k->value >= 32 && k->value < 127) { char c = (char)std::toupper(k->value); lua_pushlstring(L, &c, 1); }
    else lua_pushstring(L, "");
    return 1;
}
// InputObject:IsModifierKeyDown(Enum.ModifierKey): either side of the modifier is down.
static int IsModifierKeyDown(lua_State* L) {
    (void)self(L);
    const EnumItem* m = checkEnumItem(L, 2, findEnum("ModifierKey"));
    Runtime::Impl& rt = rtOf(L);
    const char* side = !std::strcmp(m->name, "Alt") ? "Alt" : !std::strcmp(m->name, "Ctrl") ? "Control" : !std::strcmp(m->name, "Meta") ? "Super" : "Shift";
    lua_pushboolean(L, rt.keysDown.count(std::string("Left") + side) || rt.keysDown.count(std::string("Right") + side));
    return 1;
}

// ContextActionService's touch-button settings are kept on the binding; nothing here draws a
// button, so GetButton has none to give. BindActivate is a gamepad's tool trigger: no gamepad here.
static Runtime::Impl::ActionBinding* bindingNamed(Runtime::Impl& rt, const std::string& name) {
    for (auto& b : rt.actionBindings) if (b.name == name) return &b;
    return nullptr;
}
static int actionButtonSet(lua_State* L, int which) {
    (void)self(L);
    Runtime::Impl& rt = rtOf(L);
    Runtime::Impl::ActionBinding* b = bindingNamed(rt, luaL_checkstring(L, 2));
    if (!b) return 0;
    if (which == 3) {
        if (!isUDim2(L, 3)) luaL_error(L, "invalid argument #3 to 'SetPosition' (UDim2 expected, got %s)", typeOfName(L, 3));
        b->buttonPosition = Value::udim2(checkUDim2(L, 3));
    } else (which == 0 ? b->title : which == 1 ? b->description : b->image) = luaL_checkstring(L, 3);
    return 0;
}
static int SetTitle(lua_State* L) { return actionButtonSet(L, 0); }
static int SetDescription(lua_State* L) { return actionButtonSet(L, 1); }
static int SetImage(lua_State* L) { return actionButtonSet(L, 2); }
static int SetPosition(lua_State* L) { return actionButtonSet(L, 3); }
static int GetButton(lua_State* L) { (void)self(L); luaL_checkstring(L, 2); lua_pushnil(L); return 1; }
static int BindActivate(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); return 0; }
static int UnbindActivate(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserInputType")); return 0; }
static void pushActionInfo(lua_State* L, const Runtime::Impl::ActionBinding& b) {
    lua_newtable(L);
    lua_pushnumber(L, b.priority); lua_setfield(L, -2, "priorityLevel");
    lua_pushnumber(L, (double)b.order); lua_setfield(L, -2, "stackOrder");
    if (!b.title.empty()) { lua_pushstring(L, b.title.c_str()); lua_setfield(L, -2, "title"); }
    if (!b.description.empty()) { lua_pushstring(L, b.description.c_str()); lua_setfield(L, -2, "description"); }
    if (!b.image.empty()) { lua_pushstring(L, b.image.c_str()); lua_setfield(L, -2, "image"); }
    if (b.buttonPosition.type == Value::UDim2) { pushUDim2(L, b.buttonPosition.udim2()); lua_setfield(L, -2, "position"); }
    lua_newtable(L);
    int n = 0;
    for (auto& in : b.inputs) {
        size_t dot = in.find('.');
        const EnumDef* e = findEnum(in.substr(0, dot));
        const EnumItem* it = e ? e->find(in.substr(dot + 1)) : nullptr;
        if (it) { pushEnumItem(L, e, it); lua_rawseti(L, -2, ++n); }
    }
    lua_setfield(L, -2, "inputTypes");
}
static int GetBoundActionInfo(lua_State* L) {
    (void)self(L);
    Runtime::Impl::ActionBinding* b = bindingNamed(rtOf(L), luaL_checkstring(L, 2));
    if (b) pushActionInfo(L, *b); else lua_newtable(L);
    return 1;
}
// The TextureId of the Tool the local character holds; nil with none.
static int GetCurrentLocalToolIcon(lua_State* L) {
    (void)self(L);
    Runtime::Impl& rt = rtOf(L);
    Instance* ch = rt.localPlayer ? rt.dm.findRef(rt.localPlayer->get("Character").ref) : nullptr;
    Instance* tool = ch ? ch->findFirstChildOfClass("Tool") : nullptr;
    if (tool) lua_pushstring(L, tool->get("TextureId").s.c_str()); else lua_pushnil(L);
    return 1;
}

// A ScrollingFrame here stops where the wheel or the bar leaves it: no inertia, so no velocity.
static int GetScrollVelocity(lua_State* L) { (void)self(L); pushVector2(L, {0, 0}); return 1; }
static int ResetScrollVelocity(lua_State* L) { (void)self(L); return 0; }

// UIPageLayout: the pages are the parent's GuiObject children in SortOrder (Name, or LayoutOrder).
static std::vector<Instance*> layoutPages(Instance& layout) {
    std::vector<Instance*> pages;
    Instance* p = layout.parent();
    if (!p) return pages;
    for (auto& c : p->children()) if (c->isA("GuiObject")) pages.push_back(c.get());
    bool byOrder = layout.get("SortOrder").s == "LayoutOrder";
    std::stable_sort(pages.begin(), pages.end(), [byOrder](Instance* a, Instance* b) {
        return byOrder ? a->get("LayoutOrder").n < b->get("LayoutOrder").n : a->name() < b->name();
    });
    return pages;
}
// PageLeave for the page left, PageEnter for the one reached, then Stopped for it: nothing here
// slides, so the move is at once.
static void turnPage(Runtime::Impl& rt, Instance& layout, Instance* page) {
    Instance* was = rt.dm.findRef(layout.get("CurrentPage").ref);
    if (was == page) return;
    layout.setInternal("CurrentPage", Value::instance(page ? page->id() : 0));
    if (was && !was->destroyed()) rt.fireValues(layout, "PageLeave", {Value::instance(was->id())});
    if (page) { rt.fireValues(layout, "PageEnter", {Value::instance(page->id())}); rt.fireValues(layout, "Stopped", {Value::instance(page->id())}); }
}
static int JumpTo(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& layout = self(L);
    Instance* page = rt.toInstance(L, 2);
    if (!page || page->parent() != layout.parent()) return 0;
    turnPage(rt, layout, page);
    return 0;
}
static int JumpToIndex(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& layout = self(L);
    std::vector<Instance*> pages = layoutPages(layout);
    int idx = (int)luaL_checkinteger(L, 2);   // zero-based, as on Roblox
    if (idx >= 0 && idx < (int)pages.size()) turnPage(rt, layout, pages[idx]);
    return 0;
}
static int pageStep(lua_State* L, int dir) {
    Runtime::Impl& rt = rtOf(L);
    Instance& layout = self(L);
    std::vector<Instance*> pages = layoutPages(layout);
    if (pages.empty()) return 0;
    Instance* cur = rt.dm.findRef(layout.get("CurrentPage").ref);
    int at = -1;
    for (size_t k = 0; k < pages.size(); k++) if (pages[k] == cur) at = (int)k;
    int n = (int)pages.size();
    int next = at < 0 ? (dir > 0 ? 0 : n - 1) : at + dir;
    if (next < 0 || next >= n) {
        if (!layout.get("Circular").b) return 0;
        next = (next + n) % n;
    }
    turnPage(rt, layout, pages[next]);
    return 0;
}
static int PageNext(lua_State* L) { return pageStep(L, 1); }
static int PagePrevious(lua_State* L) { return pageStep(L, -1); }

// VideoFrame: Playing and the two events; nothing here decodes the video.
static int VideoPlay(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& v = self(L);
    if (!v.get("Playing").b) { v.set("Playing", Value::boolean(true)); rt.fireValues(v, "Played", {v.get("TimePosition")}); }
    return 0;
}
static int VideoPause(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& v = self(L);
    if (v.get("Playing").b) { v.set("Playing", Value::boolean(false)); rt.fireValues(v, "Paused", {v.get("TimePosition")}); }
    return 0;
}

// WireframeHandleAdornment keeps its segments as point pairs; nothing here draws them.
static int AddLine(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& w = self(L);
    Vec3 a = checkVec3(L, 2), b = checkVec3(L, 3);
    auto& seg = rt.wireSegments[w.id()];
    seg.push_back(a); seg.push_back(b);
    return 0;
}
static std::vector<Vec3> checkPoints(lua_State* L, int idx) {
    luaL_checktype(L, idx, LUA_TTABLE);
    std::vector<Vec3> pts;
    for (int k = 1;; k++) {
        lua_rawgeti(L, idx, k);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        pts.push_back(checkVec3(L, -1));
        lua_pop(L, 1);
    }
    return pts;
}
static int AddLines(lua_State* L) {   // pairs: {a, b, c, d} is a-b and c-d
    Runtime::Impl& rt = rtOf(L);
    Instance& w = self(L);
    std::vector<Vec3> pts = checkPoints(L, 2);
    auto& seg = rt.wireSegments[w.id()];
    for (size_t k = 0; k + 1 < pts.size(); k += 2) { seg.push_back(pts[k]); seg.push_back(pts[k + 1]); }
    return 0;
}
static int AddPath(lua_State* L) {   // a polyline, closed back to its start when `loop`
    Runtime::Impl& rt = rtOf(L);
    Instance& w = self(L);
    std::vector<Vec3> pts = checkPoints(L, 2);
    bool loop = lua_toboolean(L, 3);
    auto& seg = rt.wireSegments[w.id()];
    for (size_t k = 0; k + 1 < pts.size(); k++) { seg.push_back(pts[k]); seg.push_back(pts[k + 1]); }
    if (loop && pts.size() > 2) { seg.push_back(pts.back()); seg.push_back(pts.front()); }
    return 0;
}
static int ClearWireframe(lua_State* L) { rtOf(L).wireSegments.erase(self(L).id()); return 0; }

// BillboardGui.CurrentDistance: CurrentCamera to the Adornee (the parent when nil), by its Position.
static double billboardDistance(Runtime::Impl& rt, Instance& gui) {
    Instance* cam = rt.dm.find(rt.dm.workspace()->get("CurrentCamera").ref);
    Instance* target = rt.dm.findRef(gui.get("Adornee").ref);
    if (!target) target = gui.parent();
    if (!cam || !target) return 0;
    Vec3 at;
    if (target->isA("BasePart") || target->isA("Attachment")) at = target->isA("Attachment") ? m::attachmentWorld(*target).p : target->get("Position").v;
    else if (target->isA("Model")) at = m::pivotOf(rt, *target).p;
    else return 0;
    Vec3 c = cam->get("Position").v;
    return std::sqrt((at.x - c.x) * (at.x - c.x) + (at.y - c.y) * (at.y - c.y) + (at.z - c.z) * (at.z - c.z));
}

// ---- the parts family ------------------------------------------------------------------
// Every Constraint anywhere in the tree with this Attachment at either end.
static int GetConstraints(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& a = self(L);
    std::vector<Instance*> out;
    for (Instance* d : rt.dm.root()->getDescendants())
        if (d->isA("Constraint") && (d->get("Attachment0").ref == a.id() || d->get("Attachment1").ref == a.id())) out.push_back(d);
    return pushParts(L, rt, out);
}
// The joints holding this part: JointInstances and WeldConstraints naming it as Part0 or Part1,
// NoCollisionConstraints too, and Constraints on one of its Attachments.
static void jointsOf(Runtime::Impl& rt, Instance& part, std::vector<Instance*>& out, bool constraintsToo) {
    for (Instance* d : rt.dm.root()->getDescendants()) {
        if (d->isA("JointInstance") || d->isA("WeldConstraint") || d->isA("NoCollisionConstraint")) {
            if (d->get("Part0").ref == part.id() || d->get("Part1").ref == part.id()) out.push_back(d);
        } else if (constraintsToo && d->isA("Constraint")) {
            for (const char* end : {"Attachment0", "Attachment1"}) {
                Instance* att = rt.dm.findRef(d->get(end).ref);
                if (att && att->parent() == &part) { out.push_back(d); break; }
            }
        }
    }
}
static int GetJoints(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    std::vector<Instance*> out;
    jointsOf(rt, self(L), out, true);
    return pushParts(L, rt, out);
}
static int GetNoCollisionConstraints(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& part = self(L);
    std::vector<Instance*> out;
    for (Instance* d : rt.dm.root()->getDescendants())
        if (d->isA("NoCollisionConstraint") && (d->get("Part0").ref == part.id() || d->get("Part1").ref == part.id())) out.push_back(d);
    return pushParts(L, rt, out);
}
// No collision-group table exists here (every group collides with every other), so two parts collide
// unless an enabled NoCollisionConstraint pairs them.
static int CanCollideWith(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& a = self(L);
    Instance& b = rt.checkInstance(L, 2);
    if (!b.isA("BasePart")) luaL_error(L, "invalid argument #1 to 'CanCollideWith' (BasePart expected, got %s)", b.className().c_str());
    bool can = true;
    for (Instance* d : rt.dm.root()->getDescendants())
        if (d->isA("NoCollisionConstraint") && d->get("Enabled").b) {
            int64_t p0 = d->get("Part0").ref, p1 = d->get("Part1").ref;
            if ((p0 == a.id() && p1 == b.id()) || (p0 == b.id() && p1 == a.id())) { can = false; break; }
        }
    lua_pushboolean(L, can);
    return 1;
}
// Network ownership: every part here is the server's own, and stays so (SetNetworkOwner records nothing).
static int CanSetNetworkOwnership(lua_State* L) {
    Instance& p = self(L);
    if (p.get("Anchored").b) { lua_pushboolean(L, 0); lua_pushstring(L, "Cannot set network ownership on anchored parts"); return 2; }
    lua_pushboolean(L, 1);
    lua_pushstring(L, "");
    return 2;
}
static int GetNetworkOwner(lua_State* L) { (void)self(L); lua_pushnil(L); return 1; }           // nil: the server
static int GetNetworkOwnershipAuto(lua_State* L) { (void)self(L); lua_pushboolean(L, 1); return 1; }
static int SetNetworkOwnershipAuto(lua_State* L) { (void)self(L); return 0; }
// The nearest point of the part's box to the one given: a point outside is clamped to the box, a point
// inside is pushed out to the closest face. A ball or a cylinder is treated as its box.
static int GetClosestPointOnSurface(lua_State* L) {
    Instance& p = self(L);
    CFrameV cf = partCFrame(p);
    Vec3 half = mul(p.get("Size").v, 0.5f);
    Vec3 l = cf.inverse() * checkVec3(L, 2);
    float c[3] = {l.x, l.y, l.z}, h[3] = {half.x, half.y, half.z};
    bool inside = true;
    for (int k = 0; k < 3; k++) { if (c[k] > h[k]) { c[k] = h[k]; inside = false; } else if (c[k] < -h[k]) { c[k] = -h[k]; inside = false; } }
    if (inside) {
        int best = 0; float gap = 1e30f;
        for (int k = 0; k < 3; k++) { float g = h[k] - std::fabs(c[k]); if (g < gap) { gap = g; best = k; } }
        c[best] = c[best] < 0 ? -h[best] : h[best];
    }
    pushVec3(L, cf * Vec3{c[0], c[1], c[2]});
    return 1;
}
// Rigid-body velocity at a world point: the assembly's linear velocity plus its spin about its centre of mass.
static int GetVelocityAtPosition(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    Vec3 at = checkVec3(L, 2);
    Vec3 v = p.get("AssemblyLinearVelocity").v, w = p.get("AssemblyAngularVelocity").v;
    Vec3 r = sub(at, assemblyCentre(assemblyOf(rt, p), p));
    pushVec3(L, add(v, Vec3{w.y * r.z - w.z * r.y, w.z * r.x - w.x * r.z, w.x * r.y - w.y * r.x}));
    return 1;
}
// Grounded: the assembly holds an anchored part.
static int IsGrounded(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    bool grounded = false;
    for (Instance* q : assemblyOf(rt, self(L))) if (q->get("Anchored").b) { grounded = true; break; }
    lua_pushboolean(L, grounded);
    return 1;
}
// The assembly's inertia tensor in world axes: each part a solid box about its own centre, carried to
// the assembly's centre of mass; a Massless part weighs nothing unless the assembly is only it.
static void assemblyInertia(Runtime::Impl& rt, Instance& part, float I[9]) {
    std::vector<Instance*> parts = assemblyOf(rt, part);
    Vec3 c = assemblyCentre(parts, part);
    for (int k = 0; k < 9; k++) I[k] = 0;
    bool anyMass = false;
    for (Instance* q : parts) if (!q->get("Massless").b) anyMass = true;
    for (Instance* q : parts) {
        if (anyMass && q->get("Massless").b) continue;
        float m = (float)massOf(*q);
        Vec3 s = q->get("Size").v;
        float b[3] = {m / 12 * (s.y * s.y + s.z * s.z), m / 12 * (s.x * s.x + s.z * s.z), m / 12 * (s.x * s.x + s.y * s.y)};
        CFrameV cf = partCFrame(*q);
        Vec3 d = sub(cf.p, c);
        float dd = dot(d, d), dv[3] = {d.x, d.y, d.z};
        for (int r = 0; r < 3; r++) for (int col = 0; col < 3; col++) {
            float rot = 0;   // R diag(b) R^T
            for (int k = 0; k < 3; k++) rot += cf.m[r * 3 + k] * b[k] * cf.m[col * 3 + k];
            I[r * 3 + col] += rot + m * ((r == col ? dd : 0) - dv[r] * dv[col]);
        }
    }
}
static void mat3Apply(const float M[9], Vec3 v, Vec3& out) {
    out = {M[0] * v.x + M[1] * v.y + M[2] * v.z, M[3] * v.x + M[4] * v.y + M[5] * v.z, M[6] * v.x + M[7] * v.y + M[8] * v.z};
}
static bool mat3Invert(const float M[9], float out[9]) {
    float det = M[0] * (M[4] * M[8] - M[5] * M[7]) - M[1] * (M[3] * M[8] - M[5] * M[6]) + M[2] * (M[3] * M[7] - M[4] * M[6]);
    if (std::fabs(det) < 1e-12f) return false;
    float inv = 1 / det;
    out[0] = (M[4] * M[8] - M[5] * M[7]) * inv; out[1] = (M[2] * M[7] - M[1] * M[8]) * inv; out[2] = (M[1] * M[5] - M[2] * M[4]) * inv;
    out[3] = (M[5] * M[6] - M[3] * M[8]) * inv; out[4] = (M[0] * M[8] - M[2] * M[6]) * inv; out[5] = (M[2] * M[3] - M[0] * M[5]) * inv;
    out[6] = (M[3] * M[7] - M[4] * M[6]) * inv; out[7] = (M[1] * M[6] - M[0] * M[7]) * inv; out[8] = (M[0] * M[4] - M[1] * M[3]) * inv;
    return true;
}
// Torque for an angular acceleration and back, through the assembly's inertia; the optional RelativeTo
// (Attachment0 by Roblox's default here means the part's own frame, World the world's) picks the axes.
static bool relativeToPart(lua_State* L, int idx) {
    if (lua_isnoneornil(L, idx)) return true;
    const EnumItem* it = checkEnumItem(L, idx, findEnum("ActuatorRelativeTo"));
    return it && it->name != "World";
}
static int AngularAccelerationToTorque(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    Vec3 a = checkVec3(L, 2);
    bool local = relativeToPart(L, 3);
    CFrameV cf = partCFrame(p);
    if (local) a = cf.rotate(a);
    float I[9]; assemblyInertia(rt, p, I);
    Vec3 t; mat3Apply(I, a, t);
    if (local) t = cf.inverse().rotate(t);
    pushVec3(L, t);
    return 1;
}
static int TorqueToAngularAcceleration(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& p = self(L);
    Vec3 t = checkVec3(L, 2);
    bool local = relativeToPart(L, 3);
    CFrameV cf = partCFrame(p);
    if (local) t = cf.rotate(t);
    float I[9], Iinv[9]; assemblyInertia(rt, p, I);
    Vec3 a{0, 0, 0};
    if (mat3Invert(I, Iinv)) mat3Apply(Iinv, t, a);
    if (local) a = cf.inverse().rotate(a);
    pushVec3(L, a);
    return 1;
}
// Grows the face by the studs given (shrinks for a negative amount), the part's centre moving half of
// it so the opposite face stays; false, and nothing moves, when a side would fall under 0.001 studs.
static int Resize(lua_State* L) {
    Instance& p = self(L);
    const EnumItem* face = checkEnumItem(L, 2, findEnum("NormalId"));
    float delta = (float)luaL_checknumber(L, 3);
    Vec3 size = p.get("Size").v;
    int axis = face->value == 0 || face->value == 3 ? 0 : face->value == 1 || face->value == 4 ? 1 : 2;   // Right/Left, Top/Bottom, Back/Front
    float sign = face->value < 3 ? 1.f : -1.f;
    float* dims[3] = {&size.x, &size.y, &size.z};
    if (*dims[axis] + delta < 0.001f) { lua_pushboolean(L, 0); return 1; }
    *dims[axis] += delta;
    CFrameV cf = partCFrame(p);
    Vec3 n = mul(cf.col(axis), sign);
    p.set("Size", Value::vector3(size));
    p.set("Position", Value::vector3(add(cf.p, mul(n, delta * 0.5f))));
    lua_pushboolean(L, 1);
    return 1;
}
// Every part between the camera and each cast point, the ignore list's instances and their descendants left out.
static int GetPartsObscuringTarget(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& cam = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    RaycastFilter f;
    if (!lua_isnoneornil(L, 3)) { luaL_checktype(L, 3, LUA_TTABLE); for (int64_t id : instanceList(L, 3)) f.ids.push_back(id); }
    Vec3 from = cam.get("Position").v;
    std::vector<Instance*> out;
    std::unordered_set<int64_t> seen;
    int n = (int)lua_objlen(L, 2);
    for (int k = 1; k <= n; k++) {
        lua_rawgeti(L, 2, k);
        Vec3 to = checkVec3(L, -1);
        lua_pop(L, 1);
        RaycastFilter step = f;
        Vec3 o = from;
        for (int hop = 0; hop < 64; hop++) {
            RayHit h;
            if (!raycastTree(rt.dm, o, sub(to, o), &step, h) || !h.part) break;
            if (seen.insert(h.part->id()).second) out.push_back(h.part);
            step.ids.push_back(h.part->id());
            o = h.position;
        }
    }
    return pushParts(L, rt, out);
}
// Streaming's per-player persistence, kept as the UserIds in PersistentPlayers; nothing here streams.
static std::vector<int64_t> persistentIds(Instance& m) {
    std::vector<int64_t> out;
    const std::string& s = m.get("PersistentPlayers").s;
    size_t at = 0;
    while (at < s.size()) {
        size_t end = s.find(',', at);
        if (end == std::string::npos) end = s.size();
        if (end > at) out.push_back(std::strtoll(s.c_str() + at, nullptr, 10));
        at = end + 1;
    }
    return out;
}
static void storePersistent(Instance& m, const std::vector<int64_t>& ids) {
    std::string s;
    for (int64_t id : ids) { if (!s.empty()) s += ','; s += std::to_string(id); }
    m.set("PersistentPlayers", Value::string(s));
}
static int AddPersistentPlayer(lua_State* L) {
    Instance& m = self(L);
    if (lua_isnoneornil(L, 2)) return 0;
    Instance& pl = checkPlayerArg(L, 2, "AddPersistentPlayer");
    int64_t id = (int64_t)pl.get("UserId").n;
    std::vector<int64_t> ids = persistentIds(m);
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) { ids.push_back(id); storePersistent(m, ids); }
    return 0;
}
static int RemovePersistentPlayer(lua_State* L) {
    Instance& m = self(L);
    if (lua_isnoneornil(L, 2)) return 0;
    Instance& pl = checkPlayerArg(L, 2, "RemovePersistentPlayer");
    int64_t id = (int64_t)pl.get("UserId").n;
    std::vector<int64_t> ids = persistentIds(m);
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    storePersistent(m, ids);
    return 0;
}
static int GetPersistentPlayers(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    std::vector<Instance*> out;
    for (int64_t id : persistentIds(self(L)))
        for (auto& p : rt.dm.getService("Players")->children()) if (p->isA("Player") && (int64_t)p->get("UserId").n == id) out.push_back(p.get());
    return pushParts(L, rt, out);
}
static int SetDesiredAngle(lua_State* L) { self(L).set("DesiredAngle", Value::number(luaL_checknumber(L, 2))); return 0; }

// WorldRoot: every part of the list touching a part outside it (bounding shapes, the overlap tolerance ignored).
static int ArePartsTouchingOthers(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<int64_t> ids = instanceList(L, 2);
    std::unordered_set<int64_t> set(ids.begin(), ids.end());
    for (int64_t id : ids) {
        Instance* p = rt.dm.find(id);
        if (!p || !p->isA("BasePart")) continue;
        for (Instance* o : overlapTree(rt.dm, partShape(*p), nullptr, p, false)) if (!set.count(o->id())) { lua_pushboolean(L, 1); return 1; }
    }
    lua_pushboolean(L, 0);
    return 1;
}
// Each part to its CFrame; the BulkMoveMode is accepted and every change fires as it always does.
static int BulkMoveTo(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);
    std::vector<int64_t> ids = instanceList(L, 2);
    int n = (int)lua_objlen(L, 3);
    if (n != (int)ids.size()) luaL_error(L, "BulkMoveTo: the part list and the CFrame list must be the same length");
    for (int k = 0; k < n; k++) {
        lua_rawgeti(L, 3, k + 1);
        if (!isCFrame(L, -1)) luaL_error(L, "BulkMoveTo: CFrame expected at index %d, got %s", k + 1, typeOfName(L, -1));
        CFrameV cf = checkCFrame(L, -1);
        lua_pop(L, 1);
        Instance* p = rt.dm.find(ids[k]);
        if (p && p->isA("BasePart")) setPartCFrame(*p, cf);
    }
    return 0;
}
// The unanchored parts under the Workspace: the host reports no sleepers, so every one counts as awake.
static int GetNumAwakeParts(lua_State* L) {
    int n = 0;
    for (Instance* d : self(L).getDescendants()) if (d->isA("BasePart") && !d->get("Anchored").b) n++;
    lua_pushinteger(L, n);
    return 1;
}
static int GetPhysicsThrottling(lua_State* L) { (void)self(L); lua_pushinteger(L, 100); return 1; }   // never throttled
static int GetRealPhysicsFPS(lua_State* L) { (void)self(L); lua_pushnumber(L, 60); return 1; }         // the host steps at its 60
static int PGSIsEnabled(lua_State* L) { (void)self(L); lua_pushboolean(L, 1); return 1; }

// ---- Terrain -------------------------------------------------------------------------
// The host's voxel field, read from Terrain.Heights (base64 PBVX, version 2 or 3; pulseblockz_world.cpp
// writes it): a density and a material index per sample on a 4-stud grid, x fastest, then y, then z.
// The material index is the host's palette, in this order.
static const int kTerrainPalette[] = {1280, 1284, 1296, 896, 1328, 1344, 1360, 1376, 788, 804, 1552, 1536, 820, 836, 1392, 912, 800, 880, 816, 848, 528, 2048};
static const int kTerrainPaletteCount = (int)(sizeof(kTerrainPalette) / sizeof(kTerrainPalette[0]));
struct VoxelField {
    int nx = 0, ny = 0, nz = 0; float cell = 4; Vec3 origin; std::vector<uint8_t> d, m;
    bool ok() const { return nx > 0; }
    size_t at(int x, int y, int z) const { return ((size_t)z * ny + y) * nx + x; }
};
static uint32_t u32At(const std::vector<uint8_t>& b, size_t at) { return (uint32_t)b[at] | ((uint32_t)b[at + 1] << 8) | ((uint32_t)b[at + 2] << 16) | ((uint32_t)b[at + 3] << 24); }
static float f32At(const std::vector<uint8_t>& b, size_t at) { uint32_t u = u32At(b, at); float f; std::memcpy(&f, &u, 4); return f; }
static VoxelField voxelField(Instance& terrain) {
    VoxelField f;
    std::vector<uint8_t> raw;
    if (!base64Decode(terrain.get("Heights").s, raw) || raw.size() < 40 || std::memcmp(raw.data(), "PBVX", 4) != 0) return f;
    uint32_t version = u32At(raw, 4);
    if (version != 2 && version != 3) return f;
    int nx = (int)u32At(raw, 8), ny = (int)u32At(raw, 12), nz = (int)u32At(raw, 16);
    if (nx < 2 || ny < 2 || nz < 2 || nx > 512 || ny > 257 || nz > 512) return f;
    const size_t n = (size_t)nx * ny * nz;
    uint32_t runs = u32At(raw, 36);
    size_t at = 40;
    std::vector<uint8_t> d; d.reserve(n);
    for (uint32_t r = 0; r < runs && d.size() < n && at + 3 <= raw.size(); r++) {
        uint32_t count = (uint32_t)raw[at] | ((uint32_t)raw[at + 1] << 8);
        uint8_t v = raw[at + 2];
        at += 3;
        if (d.size() + count > n) count = (uint32_t)(n - d.size());
        d.insert(d.end(), count, v);
    }
    if (d.size() != n) return f;
    std::vector<uint8_t> m(n, 0);
    if (version >= 3 && at + 4 <= raw.size()) {
        uint32_t mruns = u32At(raw, at);
        at += 4;
        size_t filled = 0;
        for (uint32_t r = 0; r < mruns && filled < n && at + 3 <= raw.size(); r++) {
            uint32_t count = (uint32_t)raw[at] | ((uint32_t)raw[at + 1] << 8);
            uint8_t v = raw[at + 2];
            at += 3;
            if (filled + count > n) count = (uint32_t)(n - filled);
            std::fill(m.begin() + filled, m.begin() + filled + count, v < kTerrainPaletteCount ? v : 0);
            filled += count;
        }
    }
    f.nx = nx; f.ny = ny; f.nz = nz;
    f.cell = f32At(raw, 20);
    if (!(f.cell > 0.01f)) f.cell = 4;
    f.origin = {f32At(raw, 24), f32At(raw, 28), f32At(raw, 32)};
    f.d = std::move(d); f.m = std::move(m);
    return f;
}
static void checkResolution(lua_State* L, int idx) {
    if ((int)luaL_checknumber(L, idx) != 4) luaL_error(L, "the resolution must be 4 (the voxel grid's)");
}
// Roblox's cells are 4 studs from the world's origin; the host's samples sit on the same grid.
static int CellCenterToWorld(lua_State* L) { (void)self(L); pushVec3(L, {(float)luaL_checknumber(L, 2) * 4 + 2, (float)luaL_checknumber(L, 3) * 4 + 2, (float)luaL_checknumber(L, 4) * 4 + 2}); return 1; }
static int CellCornerToWorld(lua_State* L) { (void)self(L); pushVec3(L, {(float)luaL_checknumber(L, 2) * 4, (float)luaL_checknumber(L, 3) * 4, (float)luaL_checknumber(L, 4) * 4}); return 1; }
// The three WorldToCell variants answer alike: the cell the point is in. Nothing here tells a boundary
// point's empty neighbour from its solid one.
static int WorldToCell(lua_State* L) {
    (void)self(L);
    Vec3 p = checkVec3(L, 2);
    pushVec3(L, {std::floor(p.x / 4), std::floor(p.y / 4), std::floor(p.z / 4)});
    return 1;
}
// Cells with any matter in them.
static int CountCells(lua_State* L) {
    VoxelField f = voxelField(self(L));
    int64_t n = 0;
    for (uint8_t v : f.d) if (v) n++;
    lua_pushinteger(L, (int)n);
    return 1;
}
static Runtime::TerrainOp regionOp(lua_State* L, int regionIdx, int kind) {
    Region3V r = checkRegion3(L, regionIdx);
    Runtime::TerrainOp op;
    op.kind = kind;
    op.pos = mul(add(r.lo, r.hi), 0.5f);
    op.size = sub(r.hi, r.lo);
    return op;
}
static int FillRegion(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Runtime::TerrainOp op = regionOp(L, 2, 0);
    checkResolution(L, 3);
    op.material = terrainMaterial(L, 4);
    rt.terrainOps.push_back(op);
    return 0;
}
// A cylinder of the height along the frame's Y axis, and of the radius.
static int FillCylinder(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Runtime::TerrainOp op;
    op.kind = 3;
    const CFrameV& cf = checkCFrame(L, 2);
    op.pos = cf.p;
    for (int i = 0; i < 9; i++) op.rot[i] = cf.m[i];
    op.size = {0, (float)luaL_checknumber(L, 3), 0};
    op.radius = (float)luaL_checknumber(L, 4);
    op.material = terrainMaterial(L, 5);
    rt.terrainOps.push_back(op);
    return 0;
}
// A WedgePart's shape: vertical at the frame's back (+Z), sloping down to its front.
static int FillWedge(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Runtime::TerrainOp op;
    op.kind = 4;
    const CFrameV& cf = checkCFrame(L, 2);
    op.pos = cf.p;
    for (int i = 0; i < 9; i++) op.rot[i] = cf.m[i];
    op.size = checkVec3(L, 3);
    op.material = terrainMaterial(L, 4);
    rt.terrainOps.push_back(op);
    return 0;
}
static int ReplaceMaterial(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Runtime::TerrainOp op = regionOp(L, 2, 5);
    checkResolution(L, 3);
    op.fromMaterial = checkEnumItem(L, 4, findEnum("Material"))->value;
    op.material = checkEnumItem(L, 5, findEnum("Material"))->value;
    rt.terrainOps.push_back(op);
    return 0;
}
// The region's cells (low corner and count, on the 4-stud grid).
struct CellBox { int x0, y0, z0, nx, ny, nz; };
static CellBox cellBox(const Region3V& r) {
    CellBox b;
    b.x0 = (int)std::floor(r.lo.x / 4 + 0.5f); b.y0 = (int)std::floor(r.lo.y / 4 + 0.5f); b.z0 = (int)std::floor(r.lo.z / 4 + 0.5f);
    b.nx = std::max(0, (int)std::floor(r.hi.x / 4 + 0.5f) - b.x0); b.ny = std::max(0, (int)std::floor(r.hi.y / 4 + 0.5f) - b.y0); b.nz = std::max(0, (int)std::floor(r.hi.z / 4 + 0.5f) - b.z0);
    return b;
}
// ReadVoxels: materials[x][y][z] and occupancies[x][y][z] for the region's cells, each outer table
// carrying Size; a cell outside the field, or with no matter, is Air at 0. The sample at a cell's low
// corner stands for the cell.
// which: 0 every material, 1 the ground only (water reads as Air), 2 the water only.
static void pushVoxelTables(lua_State* L, Instance& terrain, const CellBox& b, int which, bool materials, bool occupancy) {
    VoxelField f = voxelField(terrain);
    const EnumDef* e = findEnum("Material");
    const EnumItem* airItem = e->find("Air");
    auto sample = [&](int x, int y, int z, int& mat, float& occ) {
        mat = 1792; occ = 0;
        if (!f.ok()) return;
        int sx = (int)std::lround(((b.x0 + x) * 4.f - f.origin.x) / f.cell), sy = (int)std::lround(((b.y0 + y) * 4.f - f.origin.y) / f.cell), sz = (int)std::lround(((b.z0 + z) * 4.f - f.origin.z) / f.cell);
        if (sx < 0 || sy < 0 || sz < 0 || sx >= f.nx || sy >= f.ny || sz >= f.nz) return;
        size_t k = f.at(sx, sy, sz);
        if (!f.d[k]) return;
        mat = kTerrainPalette[f.m[k]];
        occ = f.d[k] / 255.f;
        if ((which == 2 && mat != 2048) || (which == 1 && mat == 2048)) { mat = 1792; occ = 0; }
    };
    for (int table = 0; table < 2; table++) {
        if (table == 0 && !materials) continue;
        if (table == 1 && !occupancy) continue;
        lua_createtable(L, b.nx, 1);
        for (int x = 0; x < b.nx; x++) {
            lua_createtable(L, b.ny, 0);
            for (int y = 0; y < b.ny; y++) {
                lua_createtable(L, b.nz, 0);
                for (int z = 0; z < b.nz; z++) {
                    int mat; float occ;
                    sample(x, y, z, mat, occ);
                    if (table == 0) { const EnumItem* it = e->findValue(mat); pushEnumItem(L, e, it ? it : airItem); }
                    else lua_pushnumber(L, occ);
                    lua_rawseti(L, -2, z + 1);
                }
                lua_rawseti(L, -2, y + 1);
            }
            lua_rawseti(L, -2, x + 1);
        }
        pushVec3(L, {(float)b.nx, (float)b.ny, (float)b.nz});
        lua_setfield(L, -2, "Size");
    }
}
static int ReadVoxels(lua_State* L) {
    Instance& t = self(L);
    Region3V r = checkRegion3(L, 2);
    checkResolution(L, 3);
    pushVoxelTables(L, t, cellBox(r), 0, true, true);
    return 2;
}
// The channels: SolidMaterial and SolidOccupancy for the ground, LiquidOccupancy for the water.
static int ReadVoxelChannels(lua_State* L) {
    Instance& t = self(L);
    Region3V r = checkRegion3(L, 2);
    checkResolution(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    CellBox b = cellBox(r);
    lua_newtable(L);
    int n = (int)lua_objlen(L, 4);
    for (int k = 1; k <= n; k++) {
        lua_rawgeti(L, 4, k);
        std::string ch = luaL_checkstring(L, -1);
        lua_pop(L, 1);
        if (ch == "SolidMaterial") pushVoxelTables(L, t, b, 1, true, false);
        else if (ch == "SolidOccupancy") pushVoxelTables(L, t, b, 1, false, true);
        else if (ch == "LiquidOccupancy") pushVoxelTables(L, t, b, 2, false, true);
        else luaL_error(L, "ReadVoxelChannels: unknown channel '%s'", ch.c_str());
        lua_setfield(L, -2, ch.c_str());
    }
    return 1;
}
// WriteVoxels: the tables the shape ReadVoxels gives, to the host as one op.
static void readVoxelTable(lua_State* L, int idx, const CellBox& b, bool material, std::vector<uint16_t>& mats, std::vector<uint8_t>& occs) {
    luaL_checktype(L, idx, LUA_TTABLE);
    const EnumDef* e = findEnum("Material");
    for (int x = 0; x < b.nx; x++) {
        lua_rawgeti(L, idx, x + 1);
        if (!lua_istable(L, -1)) luaL_error(L, "WriteVoxels: the table must be %d x %d x %d", b.nx, b.ny, b.nz);
        for (int y = 0; y < b.ny; y++) {
            lua_rawgeti(L, -1, y + 1);
            if (!lua_istable(L, -1)) luaL_error(L, "WriteVoxels: the table must be %d x %d x %d", b.nx, b.ny, b.nz);
            for (int z = 0; z < b.nz; z++) {
                lua_rawgeti(L, -1, z + 1);
                size_t k = (size_t)x + (size_t)b.nx * ((size_t)y + (size_t)b.ny * z);
                if (material) mats[k] = (uint16_t)checkEnumItem(L, -1, e)->value;
                else occs[k] = (uint8_t)std::lround(std::max(0.0, std::min(1.0, luaL_checknumber(L, -1))) * 255);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
}
static Runtime::TerrainOp voxelWriteOp(const CellBox& b) {
    Runtime::TerrainOp op;
    op.kind = 6;
    op.pos = {b.x0 * 4.f, b.y0 * 4.f, b.z0 * 4.f};
    op.nx = b.nx; op.ny = b.ny; op.nz = b.nz;
    op.materials.assign((size_t)b.nx * b.ny * b.nz, 1792);
    op.occupancy.assign((size_t)b.nx * b.ny * b.nz, 0);
    return op;
}
static int WriteVoxels(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Region3V r = checkRegion3(L, 2);
    checkResolution(L, 3);
    CellBox b = cellBox(r);
    Runtime::TerrainOp op = voxelWriteOp(b);
    readVoxelTable(L, 4, b, true, op.materials, op.occupancy);
    readVoxelTable(L, 5, b, false, op.materials, op.occupancy);
    rt.terrainOps.push_back(op);
    return 0;
}
// The channel tables written: SolidMaterial with SolidOccupancy; LiquidOccupancy alone lays water.
static int WriteVoxelChannels(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Region3V r = checkRegion3(L, 2);
    checkResolution(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    CellBox b = cellBox(r);
    Runtime::TerrainOp op = voxelWriteOp(b);
    lua_getfield(L, 4, "SolidMaterial");
    bool solid = lua_istable(L, -1);
    if (solid) readVoxelTable(L, lua_gettop(L), b, true, op.materials, op.occupancy);
    lua_pop(L, 1);
    lua_getfield(L, 4, "SolidOccupancy");
    if (lua_istable(L, -1)) readVoxelTable(L, lua_gettop(L), b, false, op.materials, op.occupancy);
    lua_pop(L, 1);
    lua_getfield(L, 4, "LiquidOccupancy");
    if (lua_istable(L, -1) && !solid) {
        readVoxelTable(L, lua_gettop(L), b, false, op.materials, op.occupancy);
        for (size_t k = 0; k < op.occupancy.size(); k++) op.materials[k] = op.occupancy[k] ? 2048 : 1792;
    }
    lua_pop(L, 1);
    rt.terrainOps.push_back(op);
    return 0;
}
// MaterialColors: Roblox's 69-byte blob, a 6-byte header then RGB for the 21 ground materials in this order.
static const int kRobloxColorOrder[21] = {1280, 800, 816, 848, 1296, 528, 896, 1552, 1328, 912, 1344, 788, 1360, 804, 1376, 880, 1536, 1284, 1392, 820, 836};
// The host's own colour for each (pulseblockz_world.cpp kTerrainMats), what GetMaterialColor answers with no blob.
static const uint8_t kDefaultMaterialRGB[21][3] = {
    {107, 143, 77}, {89, 97, 107}, {158, 158, 153}, {140, 77, 61}, {204, 186, 133}, {158, 122, 82}, {102, 102, 107}, {179, 217, 242}, {237, 242, 250},
    {199, 153, 107}, {92, 69, 51}, {51, 51, 56}, {140, 115, 89}, {153, 56, 26}, {61, 61, 64}, {128, 122, 115}, {191, 230, 255}, {92, 133, 66},
    {235, 235, 230}, {217, 209, 179}, {140, 140, 140}};
static std::string base64Encode(const std::vector<uint8_t>& in) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        uint32_t v = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];
        out += chars[(v >> 18) & 63]; out += chars[(v >> 12) & 63]; out += chars[(v >> 6) & 63]; out += chars[v & 63];
    }
    if (i < in.size()) {
        uint32_t v = in[i] << 16 | (i + 1 < in.size() ? in[i + 1] << 8 : 0);
        out += chars[(v >> 18) & 63]; out += chars[(v >> 12) & 63];
        out += i + 1 < in.size() ? chars[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}
static int colorSlot(lua_State* L, int idx) {
    int value = checkEnumItem(L, idx, findEnum("Material"))->value;
    for (int k = 0; k < 21; k++) if (kRobloxColorOrder[k] == value) return k;
    luaL_error(L, "that Material is not a terrain material with a colour");
    return 0;
}
static std::vector<uint8_t> materialColorBlob(Instance& terrain) {
    std::vector<uint8_t> raw;
    if (base64Decode(terrain.get("MaterialColors").s, raw) && raw.size() == 69) return raw;
    raw.assign(69, 0);
    for (int k = 0; k < 21; k++) for (int c = 0; c < 3; c++) raw[6 + k * 3 + c] = kDefaultMaterialRGB[k][c];
    return raw;
}
static int GetMaterialColor(lua_State* L) {
    Instance& t = self(L);
    int slot = colorSlot(L, 2);
    std::vector<uint8_t> raw = materialColorBlob(t);
    pushColor3(L, {raw[6 + slot * 3] / 255.f, raw[7 + slot * 3] / 255.f, raw[8 + slot * 3] / 255.f});
    return 1;
}
static int SetMaterialColor(lua_State* L) {
    Instance& t = self(L);
    int slot = colorSlot(L, 2);
    Col3 c = checkColor3(L, 3);
    std::vector<uint8_t> raw = materialColorBlob(t);
    raw[6 + slot * 3] = (uint8_t)std::lround(std::max(0.f, std::min(1.f, c.r)) * 255);
    raw[7 + slot * 3] = (uint8_t)std::lround(std::max(0.f, std::min(1.f, c.g)) * 255);
    raw[8 + slot * 3] = (uint8_t)std::lround(std::max(0.f, std::min(1.f, c.b)) * 255);
    t.set("MaterialColors", Value::string(base64Encode(raw)));
    return 0;
}


// ---- the services family --------------------------------------------------------------
// A signal fired on the next deferred pass, as a prompt's answer or a message's arrival would
// come later on Roblox: the task's stack is (instance, event, args...).
static int deferredFire(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance* i = rt.toInstance(L, 1);
    const char* ev = lua_tostring(L, 2);
    if (!i || !ev) return 0;
    Signal* s = rt.findSignal(*i, ev);
    if (!s || !s->hasConns()) return 0;
    int n = lua_gettop(L);
    rt.fire(*s, [&](lua_State* co) { for (int k = 3; k <= n; k++) { lua_pushvalue(L, k); lua_xmove(L, co, 1); } return n - 2; });
    return 0;
}
static void fireLater(lua_State* L, Instance& i, const char* ev, const std::function<void(lua_State*)>& pushArgs) {
    Runtime::Impl& rt = rtOf(L);
    Task* t = rt.newTask(L, rt.ctxOf(L));
    lua_pushcfunction(t->co, deferredFire, ev);
    rt.pushInstance(t->co, &i);
    lua_pushstring(t->co, ev);
    pushArgs(t->co);
    t->state = Task::Deferred;
    rt.deferred.push_back(t);
}
static void pushEmptyPages(lua_State* L, const char* className) {
    Runtime::Impl& rt = rtOf(L);
    Instance::Ptr pages = makePages(L, className, {}, 50);
    rt.pushInstance(L, pages.get());
}
static double unixNow() { return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); }

// BadgeService: no badge backend here. An award is kept for the session, so the checks agree with it.
static int AwardBadgeAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    if (!rt.dm.isServer()) luaL_error(L, "AwardBadgeAsync can only be called from the server");
    double user = luaL_checknumber(L, 2), badge = luaL_checknumber(L, 3);
    rt.badgeAwards[(int64_t)user].insert(badge);
    lua_pushboolean(L, 1);
    return 1;
}
static int UserHasBadgeAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    double user = luaL_checknumber(L, 2), badge = luaL_checknumber(L, 3);
    auto it = rt.badgeAwards.find((int64_t)user);
    lua_pushboolean(L, it != rt.badgeAwards.end() && it->second.count(badge));
    return 1;
}
// The ids asked about that the user holds
static int CheckUserBadgesAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    double user = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    auto it = rt.badgeAwards.find((int64_t)user);
    lua_newtable(L);
    int n = 0;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 3, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        if (lua_isnumber(L, -1) && it != rt.badgeAwards.end() && it->second.count(lua_tonumber(L, -1))) lua_rawseti(L, -2, ++n);
        else lua_pop(L, 1);
    }
    return 1;
}
static int GetBadgeInfoAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); luaL_error(L, "BadgeService:GetBadgeInfoAsync() failed: no badge backend here"); }

// HttpService:UrlEncode, as Roblox's: everything but letters, digits, '-' and '_' becomes %XX.
static int UrlEncode(lua_State* L) {
    (void)self(L);
    size_t n; const char* s = luaL_checklstring(L, 2, &n);
    std::string out;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (std::isalnum(c) || c == '-' || c == '_') out += (char)c;
        else { char buf[4]; std::snprintf(buf, sizeof buf, "%%%02X", c); out += buf; }
    }
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

// ---- DataStore additions ------------------------------------------------------------------
// The latest version written at or before `timestamp` (milliseconds, as the key infos count).
static int DsGetVersionAtTimeAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ds = self(L);
    std::string k = dsKey(L);
    double at = luaL_checknumber(L, 3);
    auto& list = versionsOf(L, ds, k);
    const Runtime::Impl::DsVersion* best = nullptr;
    for (auto& v : list) if (v.time <= at) best = &v;
    if (!best) { lua_pushnil(L); lua_pushnil(L); return 2; }
    if (best->deleted) lua_pushnil(L); else pushJson(L, best->json);
    Instance::Ptr info = rt.dm.createInternal("DataStoreKeyInfo");
    info->setName("DataStoreKeyInfo");
    info->setInternal("Version", Value::string(best->version));
    info->setInternal("CreatedTime", Value::number(best->time));
    info->setInternal("UpdatedTime", Value::number(best->time));
    if (!best->users.empty()) info->setInternal("UserIds", Value::string(best->users));
    if (!best->meta.empty()) info->setInternal("Metadata", Value::string(best->meta));
    rt.pushInstance(L, info.get());
    return 2;
}
static int KeyInfoGetUserIds(lua_State* L) { pushJson(L, self(L).get("UserIds").s); return 1; }
static int KeyInfoGetMetadata(lua_State* L) { pushJson(L, self(L).get("Metadata").s); return 1; }
static int OptionsGetMetadata(lua_State* L) { pushJson(L, self(L).get("Metadata").s); return 1; }
static int OptionsSetMetadata(lua_State* L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    self(L).set("Metadata", Value::string(toJson(L, 2)));
    return 0;
}
static int SetExperimentalFeatures(lua_State* L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    self(L).set("ExperimentalFeatures", Value::string(toJson(L, 2)));
    return 0;
}
// Roblox's legacy default store, apart from every named one.
static int GetGlobalDataStore(lua_State* L) { return getStore(L, 2); }

// ---- MemoryStoreService --------------------------------------------------------------------
// Server only, as on Roblox. now() is the runtime's clock, so an expiry is seconds from here.
static Runtime::Impl::MemStore& memStoreOf(lua_State* L, Instance& i) {
    Runtime::Impl& rt = rtOf(L);
    auto it = rt.memoryStoreKeys.find(i.id());
    if (it == rt.memoryStoreKeys.end()) luaL_error(L, "%s is not a memory store", i.className().c_str());
    Runtime::Impl::MemStore& st = rt.memoryStores[it->second];
    for (auto e = st.items.begin(); e != st.items.end();) { if (e->second.expires <= rt.now) e = st.items.erase(e); else ++e; }
    for (auto e = st.queue.begin(); e != st.queue.end();) { if (e->expires <= rt.now) e = st.queue.erase(e); else ++e; }
    return st;
}
static int getMemStore(lua_State* L, char kind) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    if (!rt.dm.isServer()) luaL_error(L, "MemoryStoreService can't be accessed from client");
    size_t n; const char* name = luaL_checklstring(L, 2, &n);
    if (n == 0 || n > 128) luaL_error(L, "MemoryStore name can't be empty or longer than 128 characters");
    std::string key = std::string(1, kind) + '\n' + std::string(name, n);
    auto it = rt.memoryStores.find(key);
    if (it == rt.memoryStores.end()) {
        Runtime::Impl::MemStore st;
        st.kind = kind;
        st.inst = rt.dm.createInternal(kind == 'h' ? "MemoryStoreHashMap" : kind == 's' ? "MemoryStoreSortedMap" : "MemoryStoreQueue");
        st.inst->setName(name);
        if (kind == 'q') st.invisibility = luaL_optnumber(L, 3, 30);
        rt.memoryStoreKeys[st.inst->id()] = key;
        it = rt.memoryStores.emplace(key, std::move(st)).first;
    }
    rt.pushInstance(L, it->second.inst.get());
    return 1;
}
static int GetHashMap(lua_State* L) { return getMemStore(L, 'h'); }
static int GetSortedMap(lua_State* L) { return getMemStore(L, 's'); }
static int GetQueue(lua_State* L) { return getMemStore(L, 'q'); }
static std::string memKey(lua_State* L, int idx) {
    size_t n; const char* k = luaL_checklstring(L, idx, &n);
    if (n == 0 || n > 128) luaL_error(L, "MemoryStore key can't be empty or longer than 128 characters");
    return std::string(k, n);
}
static double memExpiry(lua_State* L, int idx) {   // seconds, up to Roblox's 45 days
    double e = luaL_checknumber(L, idx);
    if (e <= 0 || e > 3888000) luaL_error(L, "expiration must be between 0 and 3888000 seconds");
    return rtOf(L).now + e;
}
static void readSortKey(lua_State* L, int idx, Runtime::Impl::MemItem& it) {
    it.hasSort = !lua_isnoneornil(L, idx);
    if (!it.hasSort) return;
    if (lua_type(L, idx) == LUA_TNUMBER) { it.sortIsNumber = true; it.sortNum = lua_tonumber(L, idx); }
    else if (lua_type(L, idx) == LUA_TSTRING) { it.sortIsNumber = false; it.sortStr = lua_tostring(L, idx); }
    else luaL_error(L, "sortKey must be a number or a string");
}
// The map's order: items without a sort key first, then numbers, then strings; ties by key.
static int memRank(const Runtime::Impl::MemItem& i) { return !i.hasSort ? 0 : i.sortIsNumber ? 1 : 2; }
static bool memBefore(const std::string& ka, const Runtime::Impl::MemItem& a, const std::string& kb, const Runtime::Impl::MemItem& b) {
    int ra = memRank(a), rb = memRank(b);
    if (ra != rb) return ra < rb;
    if (ra == 1 && a.sortNum != b.sortNum) return a.sortNum < b.sortNum;
    if (ra == 2 && a.sortStr != b.sortStr) return a.sortStr < b.sortStr;
    return ka < kb;
}
static void pushMemRow(lua_State* L, const std::string& key, const Runtime::Impl::MemItem& it) {
    lua_createtable(L, 0, 3);
    lua_pushstring(L, key.c_str()); lua_setfield(L, -2, "key");
    pushJson(L, it.json); lua_setfield(L, -2, "value");
    if (it.hasSort) { if (it.sortIsNumber) lua_pushnumber(L, it.sortNum); else lua_pushstring(L, it.sortStr.c_str()); lua_setfield(L, -2, "sortKey"); }
}
static int MemGetAsync(lua_State* L) {
    auto& st = memStoreOf(L, self(L));
    auto it = st.items.find(memKey(L, 2));
    if (it == st.items.end()) { lua_pushnil(L); return 1; }
    pushJson(L, it->second.json);
    if (st.kind == 's') { if (!it->second.hasSort) lua_pushnil(L); else if (it->second.sortIsNumber) lua_pushnumber(L, it->second.sortNum); else lua_pushstring(L, it->second.sortStr.c_str()); return 2; }
    return 1;
}
static int MemSetAsync(lua_State* L) {
    auto& st = memStoreOf(L, self(L));
    std::string k = memKey(L, 2);
    if (lua_isnoneornil(L, 3)) luaL_error(L, "Argument 2 missing or nil");
    Runtime::Impl::MemItem it;
    it.json = toJson(L, 3);
    it.expires = memExpiry(L, 4);
    if (st.kind == 's') readSortKey(L, 5, it);
    st.items[k] = std::move(it);
    lua_pushboolean(L, 1);
    return 1;
}
static int MemRemoveAsync(lua_State* L) {
    Instance& i = self(L);
    auto& st = memStoreOf(L, i);
    if (st.kind == 'q') {   // a queue: the items of one read, by its id
        std::string id = luaL_checkstring(L, 2);
        for (auto e = st.queue.begin(); e != st.queue.end();) { if (e->readId == id) e = st.queue.erase(e); else ++e; }
        return 0;
    }
    st.items.erase(memKey(L, 2));
    return 0;
}
// The transform sees the value (and a sorted map's sort key) and returns the new one(s); nil cancels.
static int MemUpdateAsync(lua_State* L) {
    Instance& i = self(L);
    auto& st = memStoreOf(L, i);
    std::string k = memKey(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    double expires = memExpiry(L, 4);
    bool sorted = st.kind == 's';
    lua_pushvalue(L, 3);
    auto it = st.items.find(k);
    int nargs = 1;
    if (it == st.items.end()) { lua_pushnil(L); if (sorted) { lua_pushnil(L); nargs = 2; } }
    else {
        pushJson(L, it->second.json);
        if (sorted) { nargs = 2; if (!it->second.hasSort) lua_pushnil(L); else if (it->second.sortIsNumber) lua_pushnumber(L, it->second.sortNum); else lua_pushstring(L, it->second.sortStr.c_str()); }
    }
    lua_call(L, nargs, sorted ? 2 : 1);
    int valueIdx = sorted ? -2 : -1;
    if (lua_isnil(L, valueIdx)) { lua_pushnil(L); return 1; }
    Runtime::Impl::MemItem item;
    item.json = toJson(L, valueIdx);
    item.expires = expires;
    if (sorted) readSortKey(L, -1, item);
    st.items[k] = item;
    pushJson(L, item.json);
    if (sorted) { if (!item.hasSort) lua_pushnil(L); else if (item.sortIsNumber) lua_pushnumber(L, item.sortNum); else lua_pushstring(L, item.sortStr.c_str()); return 2; }
    return 1;
}
static int MemGetSizeAsync(lua_State* L) {
    auto& st = memStoreOf(L, self(L));
    if (st.kind == 'q') {
        bool excludeInvisible = lua_toboolean(L, 2);
        double now = rtOf(L).now;
        int n = 0;
        for (auto& e : st.queue) if (!excludeInvisible || e.invisibleUntil <= now) n++;
        lua_pushinteger(L, n);
        return 1;
    }
    lua_pushinteger(L, (int)st.items.size());
    return 1;
}
// ListItemsAsync(pageSize): every item, in key order, as MemoryStoreHashMapPages of {key, value}.
static int MemListItemsAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    auto& st = memStoreOf(L, self(L));
    lua_Integer pageSize = luaL_checkinteger(L, 2);
    if (pageSize < 1 || pageSize > 200) luaL_error(L, "pageSize must be between 1 and 200");
    Runtime::Impl::Pages pg;
    pg.pageSize = (size_t)pageSize;
    for (auto& [k, v] : st.items) pg.jsonItems.push_back({k, v.json});
    Instance::Ptr pages = rt.dm.createInternal("MemoryStoreHashMapPages");
    pages->setName("MemoryStoreHashMapPages");
    pages->setInternal("IsFinished", Value::boolean(pg.jsonItems.size() <= pg.pageSize));
    rt.dataStorePages[pages->id()] = std::move(pg);
    rt.pushInstance(L, pages.get());
    return 1;
}
// GetRangeAsync(direction, count, exclusiveLowerBound, exclusiveUpperBound): a bound is a key, or
// {key, sortKey}; the rows are {key, value, sortKey} in the map's order, or its reverse.
static int MemGetRangeAsync(lua_State* L) {
    auto& st = memStoreOf(L, self(L));
    bool descending = false;
    if (!lua_isnoneornil(L, 2)) { const EnumItem* d = checkEnumItem(L, 2, findEnum("SortDirection")); descending = d && d->value == 1; }
    lua_Integer count = luaL_checkinteger(L, 3);
    if (count < 1 || count > 200) luaL_error(L, "count must be between 1 and 200");
    auto bound = [&](int idx, std::string& key, Runtime::Impl::MemItem& it) -> bool {
        if (lua_isnoneornil(L, idx)) return false;
        if (lua_type(L, idx) == LUA_TSTRING) { key = lua_tostring(L, idx); it.hasSort = false; return true; }
        luaL_checktype(L, idx, LUA_TTABLE);
        lua_getfield(L, idx, "key"); key = lua_isstring(L, -1) ? lua_tostring(L, -1) : ""; lua_pop(L, 1);
        lua_getfield(L, idx, "sortKey"); readSortKey(L, lua_gettop(L), it); lua_pop(L, 1);
        return true;
    };
    std::string loKey, hiKey; Runtime::Impl::MemItem lo, hi;
    bool hasLo = bound(4, loKey, lo), hasHi = bound(5, hiKey, hi);
    std::vector<std::pair<const std::string*, const Runtime::Impl::MemItem*>> rows;
    for (auto& [k, v] : st.items) {
        if (hasLo && !memBefore(loKey, lo, k, v)) continue;
        if (hasHi && !memBefore(k, v, hiKey, hi)) continue;
        rows.push_back({&k, &v});
    }
    std::sort(rows.begin(), rows.end(), [&](auto& a, auto& b) { return descending ? memBefore(*b.first, *b.second, *a.first, *a.second) : memBefore(*a.first, *a.second, *b.first, *b.second); });
    lua_newtable(L);
    int n = 0;
    for (auto& r : rows) { if (n >= count) break; pushMemRow(L, *r.first, *r.second); lua_rawseti(L, -2, ++n); }
    return 1;
}
// AddAsync(value, expiration, priority): higher priority reads first, then the order added.
static int QueueAddAsync(lua_State* L) {
    auto& st = memStoreOf(L, self(L));
    if (lua_isnoneornil(L, 2)) luaL_error(L, "Argument 1 missing or nil");
    Runtime::Impl::MemQueueItem it;
    it.json = toJson(L, 2);
    it.expires = memExpiry(L, 3);
    it.priority = luaL_optnumber(L, 4, 0);
    it.seq = st.nextSeq++;
    st.queue.push_back(std::move(it));
    return 0;
}
// The visible items a read of `count` would take, best first; empty when allOrNothing wants more than there are.
static std::vector<Runtime::Impl::MemQueueItem*> queuePick(Runtime::Impl& rt, Runtime::Impl::MemStore& st, int count, bool allOrNothing) {
    std::vector<Runtime::Impl::MemQueueItem*> vis;
    for (auto& e : st.queue) if (e.invisibleUntil <= rt.now) vis.push_back(&e);
    std::stable_sort(vis.begin(), vis.end(), [](auto* a, auto* b) { return a->priority != b->priority ? a->priority > b->priority : a->seq < b->seq; });
    if (vis.empty() || (allOrNothing && (int)vis.size() < count)) return {};
    if ((int)vis.size() > count) vis.resize(count);
    return vis;
}
// Pushes the read's items and id, marking them invisible until RemoveAsync(id) or the window runs out.
static int queueTake(lua_State* co, Runtime::Impl& rt, Runtime::Impl::MemStore& st, std::vector<Runtime::Impl::MemQueueItem*> picked) {
    std::string id = "read" + std::to_string(st.nextRead++);
    lua_createtable(co, (int)picked.size(), 0);
    int n = 0;
    for (auto* e : picked) {
        e->invisibleUntil = rt.now + st.invisibility;
        e->readId = id;
        pushJson(co, e->json);
        lua_rawseti(co, -2, ++n);
    }
    lua_pushstring(co, id.c_str());
    return 2;
}
// ReadAsync(count, allOrNothing, waitTimeout): waits, up to the timeout (-1: without limit), for the items.
static int QueueReadAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = self(L);
    auto& st = memStoreOf(L, i);
    lua_Integer count = luaL_checkinteger(L, 2);
    if (count < 1 || count > 100) luaL_error(L, "count must be between 1 and 100");
    bool allOrNothing = lua_toboolean(L, 3);
    double wait = luaL_optnumber(L, 4, -1);
    auto picked = queuePick(rt, st, (int)count, allOrNothing);
    if (!picked.empty()) return queueTake(L, rt, st, std::move(picked));
    if (wait == 0 || !lua_isyieldable(L)) { lua_newtable(L); lua_pushnil(L); return 2; }
    Task* t = rt.taskFor(L);
    t->state = Task::Parked;
    rt.queueWaiters.push_back({t, rt.memoryStoreKeys[i.id()], (int)count, allOrNothing, wait < 0 ? HUGE_VAL : rt.now + wait});
    return lua_yield(L, 0);
}
} // namespace m
void Runtime::Impl::checkQueueWaiters() {
    for (size_t k = 0; k < queueWaiters.size();) {
        QueueWait& w = queueWaiters[k];
        Task* t = w.task;
        if (t->state != Task::Parked) { queueWaiters.erase(queueWaiters.begin() + k); continue; }
        auto it = memoryStores.find(w.store);
        int n = 0;
        if (it != memoryStores.end()) {
            auto picked = m::queuePick(*this, it->second, w.count, w.allOrNothing);
            if (!picked.empty()) n = m::queueTake(t->co, *this, it->second, std::move(picked));
        }
        if (n == 0 && now < w.deadline && it != memoryStores.end()) { k++; continue; }
        if (n == 0) { lua_newtable(t->co); lua_pushnil(t->co); n = 2; }
        queueWaiters.erase(queueWaiters.begin() + k);
        t->state = Task::Ready;
        resume(t, n);
    }
}
namespace m {

// ---- MessagingService ------------------------------------------------------------------------
// One server here: a message reaches this server's subscribers on the next deferred pass, as
// {Data, Sent}. Server only, as on Roblox.
static void messagingGuard(lua_State* L, int topicIdx) {
    if (!rtOf(L).dm.isServer()) luaL_error(L, "MessagingService can only be used on the server");
    size_t n; luaL_checklstring(L, topicIdx, &n);
    if (n == 0 || n > 80) luaL_error(L, "Topic name can't be empty or longer than 80 characters");
}
static int PublishAsync(lua_State* L) {
    Instance& svc = self(L);
    messagingGuard(L, 2);
    std::string topic = std::string("Topic:") + lua_tostring(L, 2);
    std::string json = lua_isnoneornil(L, 3) ? "null" : toJson(L, 3);
    if (json.size() > 1024) luaL_error(L, "MessagingService:PublishAsync() failed: message is over 1 KB");
    double sent = unixNow();
    fireLater(L, svc, topic.c_str(), [&](lua_State* co) {
        lua_createtable(co, 0, 2);
        pushJson(co, json); lua_setfield(co, -2, "Data");
        lua_pushnumber(co, sent); lua_setfield(co, -2, "Sent");
    });
    return 0;
}
static int SubscribeAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& svc = self(L);
    messagingGuard(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    rt.pushSignal(L, svc, rt.signal(svc, std::string("Topic:") + lua_tostring(L, 2)));
    lua_replace(L, 1);           // the topic's signal takes the service's place, the handler the topic's
    lua_pushvalue(L, 3);
    lua_replace(L, 2);
    lua_settop(L, 2);
    return connect(L, false);
}

// ---- Stats ------------------------------------------------------------------------------------
static int GetTotalMemoryUsageMb(lua_State* L) { lua_pushnumber(L, (double)rtOf(L).stats.memoryNow / 1048576.0); return 1; }
// The sandbox allocator's figure is the Lua heap; nothing here measures the other categories.
static int GetMemoryUsageMbForTag(lua_State* L) {
    const EnumItem* tag = checkEnumItem(L, 2, findEnum("DeveloperMemoryTag"));
    lua_pushnumber(L, tag && tag->value == 4 ? (double)rtOf(L).stats.memoryNow / 1048576.0 : 0);
    return 1;
}
static int StatsItemGetValue(lua_State* L) { (void)self(L); lua_pushnumber(L, 0); return 1; }
static int StatsItemGetValueString(lua_State* L) { (void)self(L); lua_pushstring(L, "0"); return 1; }

// ---- TeleportService ---------------------------------------------------------------------------
// No teleport leaves here: TeleportInitFailed fires for each player, then the call errors, as a
// refused teleport does on Roblox.
static int TeleportAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ts = self(L);
    if (!rt.dm.isServer()) luaL_error(L, "TeleportAsync can only be called from the server");
    double placeId = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    Instance* options = lua_isnoneornil(L, 4) ? nullptr : &rt.checkInstance(L, 4);
    std::vector<Instance*> players;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 3, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        Instance* p = rt.toInstance(L, -1);
        if (!p || !p->isA("Player")) luaL_error(L, "TeleportAsync: players must be a table of Players");
        players.push_back(p);
        lua_pop(L, 1);
    }
    if (Signal* s = rt.findSignal(ts, "TeleportInitFailed")) if (s->hasConns()) {
        const EnumDef* e = findEnum("TeleportResult");
        for (Instance* p : players)
            rt.fire(*s, [&](lua_State* co) {
                rt.pushInstance(co, p);
                pushEnumItem(co, e, e->find("Failure"));
                lua_pushstring(co, "Teleports are not available here");
                lua_pushnumber(co, placeId);
                rt.pushInstance(co, options);
                return 5;
            });
    }
    luaL_error(L, "TeleportAsync failed: teleports are not available here");
}
static int GetArrivingTeleportGui(lua_State* L) { (void)self(L); lua_pushnil(L); return 1; }        // nobody arrives by teleport
static int GetLocalPlayerTeleportData(lua_State* L) { (void)self(L); lua_pushnil(L); return 1; }
static int SetTeleportGui(lua_State* L) { (void)self(L); rtOf(L).teleportGui = rtOf(L).checkInstance(L, 2).id(); return 0; }
// The settings are kept for the session, per side.
static int GetTeleportSetting(lua_State* L) {
    (void)self(L);
    auto& s = rtOf(L).teleportSettings;
    auto it = s.find(luaL_checkstring(L, 2));
    if (it == s.end()) lua_pushnil(L); else pushJson(L, it->second);
    return 1;
}
static int SetTeleportSetting(lua_State* L) {
    (void)self(L);
    std::string key = luaL_checkstring(L, 2);
    if (lua_isnoneornil(L, 3)) rtOf(L).teleportSettings.erase(key);
    else rtOf(L).teleportSettings[key] = toJson(L, 3);
    return 0;
}
static int GetTeleportData(lua_State* L) { pushJson(L, self(L).get("TeleportData").s); return 1; }
static int SetTeleportData(lua_State* L) { self(L).set("TeleportData", Value::string(lua_isnoneornil(L, 2) ? "null" : toJson(L, 2))); return 0; }

// ---- TweenService:SmoothDamp ----------------------------------------------------------------------
// A critically damped spring per component, as Unity's: (current, target, velocity, smoothTime,
// maxSpeed, dt) -> (value, velocity), for numbers, Vector2s and Vector3s.
static int SmoothDamp(lua_State* L) {
    (void)self(L);
    Runtime::Impl& rt = rtOf(L);
    double smoothTime = std::max(1e-4, luaL_checknumber(L, 5));
    double maxSpeed = luaL_optnumber(L, 6, HUGE_VAL);
    double dt = luaL_optnumber(L, 7, rt.lastDt);
    if (dt <= 0) dt = rt.lastDt;
    double omega = 2 / smoothTime, x = omega * dt, decay = 1 / (1 + x + 0.48 * x * x + 0.235 * x * x * x);
    auto damp = [&](double cur, double tgt, double& vel) -> double {
        double change = cur - tgt, maxChange = maxSpeed * smoothTime;
        change = std::max(-maxChange, std::min(maxChange, change));
        double target = cur - change;
        double temp = (vel + omega * change) * dt;
        vel = (vel - omega * temp) * decay;
        double out = target + (change + temp) * decay;
        if ((tgt - cur > 0) == (out > tgt)) { out = tgt; vel = (out - tgt) / dt; }
        return out;
    };
    if (lua_type(L, 2) == LUA_TNUMBER) {
        double v = luaL_checknumber(L, 4);
        double out = damp(lua_tonumber(L, 2), luaL_checknumber(L, 3), v);
        lua_pushnumber(L, out); lua_pushnumber(L, v);
        return 2;
    }
    if (lua_isvector(L, 2)) {
        Vec3 c = checkVec3(L, 2), t = checkVec3(L, 3), v = checkVec3(L, 4);
        double vx = v.x, vy = v.y, vz = v.z;
        Vec3 out{(float)damp(c.x, t.x, vx), (float)damp(c.y, t.y, vy), (float)damp(c.z, t.z, vz)};
        pushVec3(L, out); pushVec3(L, Vec3{(float)vx, (float)vy, (float)vz});
        return 2;
    }
    if (isVector2(L, 2)) {
        Vec2 c = checkVector2(L, 2), t = checkVector2(L, 3), v = checkVector2(L, 4);
        double vx = v.x, vy = v.y;
        Vec2 out{(float)damp(c.x, t.x, vx), (float)damp(c.y, t.y, vy)};
        pushVector2(L, out); pushVector2(L, Vec2{(float)vx, (float)vy});
        return 2;
    }
    luaL_typeerror(L, 2, "number, Vector2 or Vector3");
    return 0;
}

// ---- VRService ----------------------------------------------------------------------------------
// No headset here: the user frames are the identity and none is enabled. The touchpad modes are kept.
static int GetUserCFrame(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserCFrame")); pushCFrame(L, CFrameV::identity()); return 1; }
static int GetUserCFrameEnabled(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("UserCFrame")); lua_pushboolean(L, 0); return 1; }
static int RecenterUserHeadCFrame(lua_State* L) { (void)self(L); return 0; }
static int GetTouchpadMode(lua_State* L) {
    (void)self(L);
    const EnumItem* pad = checkEnumItem(L, 2, findEnum("VRTouchpad"));
    const EnumDef* e = findEnum("VRTouchpadMode");
    pushEnumItem(L, e, e->find(rtOf(L).vrTouchpadMode[pad->value == 1 ? 1 : 0]));
    return 1;
}
static int SetTouchpadMode(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& vr = self(L);
    const EnumItem* pad = checkEnumItem(L, 2, findEnum("VRTouchpad"));
    const EnumItem* mode = checkEnumItem(L, 3, findEnum("VRTouchpadMode"));
    std::string& slot = rt.vrTouchpadMode[pad->value == 1 ? 1 : 0];
    if (slot == mode->name) return 0;
    slot = mode->name;
    if (Signal* s = rt.findSignal(vr, "TouchpadModeChanged")) if (s->hasConns())
        rt.fire(*s, [&](lua_State* co) { pushEnumItem(co, findEnum("VRTouchpad"), pad); pushEnumItem(co, findEnum("VRTouchpadMode"), mode); return 2; });
    return 0;
}
// RequestNavigation(cframe, inputUserCFrame): fires NavigationRequested with both; nothing here walks the avatar there.
static int RequestNavigation(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& vr = self(L);
    CFrameV cf = checkCFrame(L, 2);
    const EnumItem* which = checkEnumItem(L, 3, findEnum("UserCFrame"));
    if (Signal* s = rt.findSignal(vr, "NavigationRequested")) if (s->hasConns())
        rt.fire(*s, [&](lua_State* co) { pushCFrame(co, cf); pushEnumItem(co, findEnum("UserCFrame"), which); return 2; });
    return 0;
}
static int IsVoiceEnabledForUserIdAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); lua_pushboolean(L, 0); return 1; }   // no voice here

// ---- chat ----------------------------------------------------------------------------------------
// Everyone here may chat with everyone (CanUsersChatAsync), so the ids come back as given.
static int CanUsersDirectChatAsync(lua_State* L) {
    (void)self(L);
    luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    lua_newtable(L);
    int n = 0;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 3, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}
static int DeriveNewMessageProperties(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    (void)self(L);
    Instance::Ptr p = rt.dm.create("ChatWindowMessageProperties");
    p->setName("ChatWindowMessageProperties");
    rt.pushInstance(L, p.get());
    return 1;
}

// ---- LocalizationTable and Translator ---------------------------------------------------------------
// Contents is Roblox's JSON: [{key, source, context, example, values: {locale: text}}]. The entries
// are handed to scripts with Roblox's field names (Key, Source, Context, Example, Values).
static void pushContents(lua_State* L, Instance& table) { pushJson(L, table.get("Contents").s); if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); } }
static void storeContents(lua_State* L, Instance& table, int idx) { table.set("Contents", Value::string(toJson(L, idx))); }
static std::string fieldString(lua_State* L, int idx, const char* name) {
    lua_getfield(L, idx, name);
    std::string s = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    return s;
}
// The index in the contents array of the entry with this key, source and context; 0 for none. A
// key given matches by key alone, as Roblox's methods do.
static int findEntry(lua_State* L, int contents, const std::string& key, const std::string& source, const std::string& context) {
    contents = lua_absindex(L, contents);
    for (int i = 1;; i++) {
        lua_rawgeti(L, contents, i);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return 0; }
        bool hit = !key.empty() ? fieldString(L, -1, "key") == key
                                : fieldString(L, -1, "source") == source && fieldString(L, -1, "context") == context;
        lua_pop(L, 1);
        if (hit) return i;
    }
}
static int GetEntries(lua_State* L) {
    Instance& t = self(L);
    pushContents(L, t);
    int contents = lua_gettop(L);
    lua_newtable(L);
    for (int i = 1;; i++) {
        lua_rawgeti(L, contents, i);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); break; }
        lua_createtable(L, 0, 5);
        for (auto [from, to] : {std::pair{"key", "Key"}, std::pair{"source", "Source"}, std::pair{"context", "Context"}, std::pair{"example", "Example"}}) {
            lua_pushstring(L, fieldString(L, -2, from).c_str()); lua_setfield(L, -2, to);
        }
        lua_getfield(L, -2, "values");
        if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); }
        lua_setfield(L, -2, "Values");
        lua_rawseti(L, -3, i);
        lua_pop(L, 1);
    }
    lua_remove(L, contents);
    return 1;
}
static int SetEntries(lua_State* L) {
    Instance& t = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_newtable(L);
    int out = lua_gettop(L);
    for (int i = 1;; i++) {
        lua_rawgeti(L, 2, i);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); break; }
        lua_createtable(L, 0, 5);
        for (auto [from, to] : {std::pair{"Key", "key"}, std::pair{"Source", "source"}, std::pair{"Context", "context"}, std::pair{"Example", "example"}}) {
            lua_pushstring(L, fieldString(L, -2, from).c_str()); lua_setfield(L, -2, to);
        }
        lua_getfield(L, -2, "Values");
        if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); }
        lua_setfield(L, -2, "values");
        lua_rawseti(L, out, i);
        lua_pop(L, 1);
    }
    storeContents(L, t, out);
    return 0;
}
// The entry the three name, made if absent, left on the stack top over the contents array.
static void entryFor(lua_State* L, Instance& t, const std::string& key, const std::string& source, const std::string& context, bool make) {
    pushContents(L, t);
    int idx = findEntry(L, -1, key, source, context);
    if (idx) { lua_rawgeti(L, -1, idx); return; }
    if (!make) luaL_error(L, "LocalizationTable: no entry with key \"%s\", source \"%s\" and context \"%s\"", key.c_str(), source.c_str(), context.c_str());
    lua_createtable(L, 0, 5);
    lua_pushstring(L, key.c_str()); lua_setfield(L, -2, "key");
    lua_pushstring(L, source.c_str()); lua_setfield(L, -2, "source");
    lua_pushstring(L, context.c_str()); lua_setfield(L, -2, "context");
    lua_pushstring(L, ""); lua_setfield(L, -2, "example");
    lua_newtable(L); lua_setfield(L, -2, "values");
    lua_pushvalue(L, -1);
    lua_rawseti(L, -3, lua_objlen(L, -3) + 1);
}
static int SetEntryValue(lua_State* L) {   // (key, source, context, localeId, text)
    Instance& t = self(L);
    std::string key = luaL_optstring(L, 2, ""), source = luaL_optstring(L, 3, ""), context = luaL_optstring(L, 4, "");
    std::string locale = luaL_checkstring(L, 5), text = luaL_checkstring(L, 6);
    entryFor(L, t, key, source, context, true);
    lua_getfield(L, -1, "values");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setfield(L, -3, "values"); }
    lua_pushstring(L, text.c_str()); lua_setfield(L, -2, locale.c_str());
    lua_pop(L, 2);
    storeContents(L, t, -1);
    return 0;
}
static int setEntryField(lua_State* L, const char* field) {   // (key, source, context, newValue)
    Instance& t = self(L);
    std::string key = luaL_optstring(L, 2, ""), source = luaL_optstring(L, 3, ""), context = luaL_optstring(L, 4, "");
    std::string value = luaL_checkstring(L, 5);
    entryFor(L, t, key, source, context, false);
    lua_pushstring(L, value.c_str()); lua_setfield(L, -2, field);
    lua_pop(L, 1);
    storeContents(L, t, -1);
    return 0;
}
static int SetEntryContext(lua_State* L) { return setEntryField(L, "context"); }
static int SetEntryExample(lua_State* L) { return setEntryField(L, "example"); }
static int SetEntryKey(lua_State* L) { return setEntryField(L, "key"); }
static int SetEntrySource(lua_State* L) { return setEntryField(L, "source"); }
static int RemoveEntry(lua_State* L) {   // (key, source, context)
    Instance& t = self(L);
    std::string key = luaL_optstring(L, 2, ""), source = luaL_optstring(L, 3, ""), context = luaL_optstring(L, 4, "");
    pushContents(L, t);
    int idx = findEntry(L, -1, key, source, context);
    if (!idx) return 0;
    int n = (int)lua_objlen(L, -1);
    for (int i = idx; i < n; i++) { lua_rawgeti(L, -1, i + 1); lua_rawseti(L, -2, i); }
    lua_pushnil(L); lua_rawseti(L, -2, n);
    storeContents(L, t, -1);
    return 0;
}
static int RemoveEntryValue(lua_State* L) {   // (key, source, context, localeId)
    Instance& t = self(L);
    std::string key = luaL_optstring(L, 2, ""), source = luaL_optstring(L, 3, ""), context = luaL_optstring(L, 4, "");
    std::string locale = luaL_checkstring(L, 5);
    entryFor(L, t, key, source, context, false);
    lua_getfield(L, -1, "values");
    if (lua_istable(L, -1)) { lua_pushnil(L); lua_setfield(L, -2, locale.c_str()); }
    lua_pop(L, 2);
    storeContents(L, t, -1);
    return 0;
}
static int RemoveTargetLocale(lua_State* L) {   // every entry loses that locale's text
    Instance& t = self(L);
    std::string locale = luaL_checkstring(L, 2);
    pushContents(L, t);
    for (int i = 1;; i++) {
        lua_rawgeti(L, -1, i);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); break; }
        lua_getfield(L, -1, "values");
        if (lua_istable(L, -1)) { lua_pushnil(L); lua_setfield(L, -2, locale.c_str()); }
        lua_pop(L, 2);
    }
    storeContents(L, t, -1);
    return 0;
}
static Instance::Ptr makeTranslator(Runtime::Impl& rt, const std::string& locale, Instance* table) {
    Instance::Ptr tr = rt.dm.createInternal("Translator");
    tr->setName("Translator");
    tr->setInternal("LocaleId", Value::string(locale));
    tr->setInternal("Table", Value::instance(table ? table->id() : 0));
    return tr;
}
static int GetTranslator(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    rt.pushInstance(L, makeTranslator(rt, luaL_checkstring(L, 2), &t).get());
    return 1;
}
static int GetTranslatorForLocaleAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    rt.pushInstance(L, makeTranslator(rt, luaL_checkstring(L, 2), nullptr).get());
    return 1;
}
static int GetTranslatorForPlayerAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Instance& p = rt.checkInstance(L, 2);
    if (!p.isA("Player")) luaL_typeerror(L, 2, "Player");
    rt.pushInstance(L, makeTranslator(rt, p.get("LocaleId").s, nullptr).get());
    return 1;
}
static int GetTableEntries(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Instance& t = rt.checkInstance(L, 2);
    if (!t.isA("LocalizationTable")) luaL_typeerror(L, 2, "LocalizationTable");
    rt.pushInstance(L, &t);
    lua_replace(L, 1);
    return GetEntries(L);
}
// The tables a Translator reads: its own, or every LocalizationTable under LocalizationService.
static std::vector<Instance*> translatorTables(Runtime::Impl& rt, Instance& tr) {
    std::vector<Instance*> out;
    if (Instance* own = rt.dm.findRef(tr.get("Table").ref)) { out.push_back(own); return out; }
    for (Instance* d : rt.dm.getService("LocalizationService")->getDescendants()) if (d->isA("LocalizationTable")) out.push_back(d);
    return out;
}
// An entry's text for the locale, falling back from "en-us" to "en"; "" for none. On the stack: the entry table at idx.
static bool entryText(lua_State* L, int idx, const std::string& locale, std::string& out) {
    idx = lua_absindex(L, idx);
    lua_getfield(L, idx, "values");
    bool ok = false;
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, locale.c_str());
        if (lua_isstring(L, -1)) { out = lua_tostring(L, -1); ok = true; }
        lua_pop(L, 1);
        size_t dash = locale.find('-');
        if (!ok && dash != std::string::npos) {
            lua_getfield(L, -1, locale.substr(0, dash).c_str());
            if (lua_isstring(L, -1)) { out = lua_tostring(L, -1); ok = true; }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return ok;
}
// Translate(context, text): the entry whose source is the text, preferring one whose context is
// the object's full name over one with none; "" when no table has it.
static int Translate(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& tr = self(L);
    Instance* ctx = lua_isnoneornil(L, 2) ? nullptr : rt.toInstance(L, 2);
    std::string text = luaL_checkstring(L, 3);
    std::string locale = tr.get("LocaleId").s, want = ctx ? ctx->fullName() : "";
    std::string best; bool found = false, exact = false;
    for (Instance* t : translatorTables(rt, tr)) {
        pushContents(L, *t);
        for (int i = 1; !exact; i++) {
            lua_rawgeti(L, -1, i);
            if (!lua_istable(L, -1)) { lua_pop(L, 1); break; }
            std::string context = fieldString(L, -1, "context");
            if (fieldString(L, -1, "source") == text && (context.empty() || context == want)) {
                std::string out;
                if (entryText(L, -1, locale, out)) { best = out; found = true; exact = !context.empty(); }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        if (exact) break;
    }
    lua_pushstring(L, found ? best.c_str() : "");
    return 1;
}
// FormatByKey(key, args): the entry's text for the locale (its source when the locale has none),
// with {name} and {n} filled from a dictionary or an array; a missing key errors, as on Roblox.
static int FormatByKey(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& tr = self(L);
    std::string key = luaL_checkstring(L, 2);
    std::string locale = tr.get("LocaleId").s;
    std::string text; bool found = false;
    for (Instance* t : translatorTables(rt, tr)) {
        pushContents(L, *t);
        int idx = findEntry(L, -1, key, "", "");
        if (idx) {
            lua_rawgeti(L, -1, idx);
            if (!entryText(L, -1, locale, text)) text = fieldString(L, -1, "source");
            lua_pop(L, 1);
            found = true;
        }
        lua_pop(L, 1);
        if (found) break;
    }
    if (!found) luaL_error(L, "Translator:FormatByKey(): key \"%s\" not found", key.c_str());
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] != '{' || lua_isnoneornil(L, 3)) { out += text[i]; continue; }
        size_t close = text.find('}', i);
        if (close == std::string::npos) { out += text.substr(i); break; }
        std::string name = text.substr(i + 1, close - i - 1);
        size_t colon = name.find(':');
        if (colon != std::string::npos) name = name.substr(0, colon);   // a format spec is dropped: the value is stringified as tostring
        if (lua_istable(L, 3)) {
            char* end = nullptr;
            long n = std::strtol(name.c_str(), &end, 10);
            if (end && *end == 0 && !name.empty()) lua_rawgeti(L, 3, (int)n); else lua_getfield(L, 3, name.c_str());
            if (!lua_isnil(L, -1)) { size_t len; const char* s = luaL_tolstring(L, -1, &len); out.append(s, len); lua_pop(L, 1); }
            else out += text.substr(i, close - i + 1);
            lua_pop(L, 1);
        } else out += text.substr(i, close - i + 1);
        i = close;
    }
    lua_pushstring(L, out.c_str());
    return 1;
}

// ---- LogService ------------------------------------------------------------------------------------
static int GetLogHistory(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    const EnumDef* e = findEnum("MessageType");
    lua_createtable(L, (int)rt.logHistory.size(), 0);
    int n = 0;
    for (auto& line : rt.logHistory) {
        lua_createtable(L, 0, 3);
        lua_pushstring(L, line.message.c_str()); lua_setfield(L, -2, "message");
        pushEnumItem(L, e, e->findValue(line.type)); lua_setfield(L, -2, "messageType");
        lua_pushnumber(L, line.timestamp); lua_setfield(L, -2, "timestamp");
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

// ---- MarketplaceService --------------------------------------------------------------------------------
// No purchase can be made here: a prompt is declined on the next deferred pass, on the side that asked.
static int promptDeclined(lua_State* L, const char* event, bool byUserId) {
    Runtime::Impl& rt = rtOf(L);
    Instance& mps = self(L);
    Instance& player = rt.checkInstance(L, 2);
    if (!player.isA("Player")) luaL_typeerror(L, 2, "Player");
    double id = luaL_checknumber(L, 3);
    double userId = player.get("UserId").n;
    Instance::Ptr keep = player.shared_from_this();
    fireLater(L, mps, event, [&, keep](lua_State* co) {
        if (byUserId) lua_pushnumber(co, userId); else rt.pushInstance(co, keep.get());
        lua_pushnumber(co, id);
        lua_pushboolean(co, 0);
    });
    return 0;
}
static int PromptPurchase(lua_State* L) { return promptDeclined(L, "PromptPurchaseFinished", false); }
static int PromptGamePassPurchase(lua_State* L) { return promptDeclined(L, "PromptGamePassPurchaseFinished", false); }
static int PromptProductPurchase(lua_State* L) { return promptDeclined(L, "PromptProductPurchaseFinished", true); }   // Roblox hands the user id here, not the Player
static int PromptBundlePurchase(lua_State* L) { return promptDeclined(L, "PromptBundlePurchaseFinished", false); }
static int PromptSubscriptionPurchase(lua_State* L) { return promptDeclined(L, "PromptSubscriptionPurchaseFinished", false); }
// Nobody owns anything from the catalog here, and nobody subscribes.
static int PlayerOwnsAssetAsync(lua_State* L) { (void)self(L); rtOf(L).checkInstance(L, 2); luaL_checknumber(L, 3); lua_pushboolean(L, 0); return 1; }
static int PlayerOwnsBundleAsync(lua_State* L) { (void)self(L); rtOf(L).checkInstance(L, 2); luaL_checknumber(L, 3); lua_pushboolean(L, 0); return 1; }
static int UserOwnsGamePassAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); luaL_checknumber(L, 3); lua_pushboolean(L, 0); return 1; }
static int GetUserSubscriptionStatusAsync(lua_State* L) {
    (void)self(L); rtOf(L).checkInstance(L, 2); luaL_checkstring(L, 3);
    lua_createtable(L, 0, 2);
    lua_pushboolean(L, 0); lua_setfield(L, -2, "IsSubscribed");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "IsRenewing");
    return 1;
}
static int GetProductInfoAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); luaL_error(L, "MarketplaceService:GetProductInfoAsync() failed: no catalog here"); }
static int GetDeveloperProductsAsync(lua_State* L) { (void)self(L); pushEmptyPages(L, "StandardPages"); return 1; }

// ---- the social and policy backends: no account here has friends, groups, parties or restrictions ----
static int CanSendGameInviteAsync(lua_State* L) { (void)self(L); rtOf(L).checkInstance(L, 2); lua_pushboolean(L, 0); return 1; }
static int CanSendCallInviteAsync(lua_State* L) { (void)self(L); rtOf(L).checkInstance(L, 2); lua_pushboolean(L, 0); return 1; }
static int PromptGameInvite(lua_State* L) {   // no invite UI here: the prompt closes, nobody invited
    Runtime::Impl& rt = rtOf(L);
    Instance& social = self(L);
    Instance& player = rt.checkInstance(L, 2);
    Instance::Ptr keep = player.shared_from_this();
    fireLater(L, social, "GameInvitePromptClosed", [&, keep](lua_State* co) { rt.pushInstance(co, keep.get()); lua_newtable(co); });
    return 0;
}
static int GetPlayersByPartyId(lua_State* L) { (void)self(L); luaL_checkstring(L, 2); lua_newtable(L); return 1; }
static int GetGroupsAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); lua_newtable(L); return 1; }
static int GetAlliesAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); pushEmptyPages(L, "StandardPages"); return 1; }
static int GetEnemiesAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); pushEmptyPages(L, "StandardPages"); return 1; }
static int GetGroupInfoAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); luaL_error(L, "GroupService:GetGroupInfoAsync() failed: no group backend here"); }
static int GetRolesInGroupAsync(lua_State* L) { (void)self(L); luaL_checknumber(L, 2); luaL_error(L, "GroupService:GetRolesInGroupAsync() failed: no group backend here"); }
// The policy of an account under no restriction.
static int GetPolicyInfoForPlayerAsync(lua_State* L) {
    (void)self(L); rtOf(L).checkInstance(L, 2);
    lua_createtable(L, 0, 8);
    lua_newtable(L);
    int n = 0;
    for (const char* site : {"Discord", "Facebook", "Twitch", "YouTube", "X", "GitHub", "Reddit", "Guilded"}) { lua_pushstring(L, site); lua_rawseti(L, -2, ++n); }
    lua_setfield(L, -2, "AllowedExternalLinkReferences");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "ArePaidRandomItemsRestricted");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "IsContentSharingAllowed");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "IsEligibleToPurchaseCommerceProduct");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "IsEligibleToPurchaseSubscription");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "IsPaidItemTradingAllowed");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "IsSubjectToChinaPolicies");
    return 1;
}
static int CanViewBrandProjectAsync(lua_State* L) { (void)self(L); lua_pushboolean(L, 0); return 1; }
// UserService: the users known here are the players in the game, in the order asked; unknown ids are left out.
static int GetUserInfosByUserIdsAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    Instance* players = rt.dm.getService("Players");
    lua_newtable(L);
    int n = 0;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        double id = lua_tonumber(L, -1);
        lua_pop(L, 1);
        for (auto& c : players->children()) if (c->isA("Player") && c->get("UserId").n == id) {
            lua_createtable(L, 0, 4);
            lua_pushnumber(L, id); lua_setfield(L, -2, "Id");
            lua_pushstring(L, c->name().c_str()); lua_setfield(L, -2, "Username");
            lua_pushstring(L, c->get("DisplayName").s.c_str()); lua_setfield(L, -2, "DisplayName");
            lua_pushboolean(L, c->get("HasVerifiedBadge").b); lua_setfield(L, -2, "HasVerifiedBadge");
            lua_rawseti(L, -2, ++n);
            break;
        }
    }
    return 1;
}
static int CanPromptOptInAsync(lua_State* L) { (void)self(L); lua_pushboolean(L, 0); return 1; }
static int PromptOptIn(lua_State* L) { fireLater(L, self(L), "OptInPromptClosed", [](lua_State*) {}); return 0; }   // no prompt opens here: closed at once
static int IsResimulating(lua_State* L) { (void)self(L); lua_pushboolean(L, 0); return 1; }   // nothing here resimulates

// ---- ContentProvider --------------------------------------------------------------------------------------
static int GetAssetFetchStatus(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    const EnumDef* e = findEnum("AssetFetchStatus");
    auto it = rt.fetchStatus.find(luaL_checkstring(L, 2));
    pushEnumItem(L, e, it == rt.fetchStatus.end() ? e->find("None") : e->findValue(it->second));
    return 1;
}
static int GetAssetFetchStatusChangedSignal(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& cp = self(L);
    rt.pushSignal(L, cp, rt.signal(cp, std::string("Fetch:") + luaL_checkstring(L, 2)));
    return 1;
}

// ---- CollectionService:GetAllTags: every tag on an instance in the tree, each once ----
static int GetAllTags(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    std::set<std::string> tags;
    for (Instance* d : rt.dm.root()->getDescendants()) for (auto& t : d->tags()) tags.insert(t);
    lua_createtable(L, (int)tags.size(), 0);
    int n = 0;
    for (auto& t : tags) { lua_pushstring(L, t.c_str()); lua_rawseti(L, -2, ++n); }
    return 1;
}

// ---- TestService: the checks count into ErrorCount and WarnCount and go to the Output ----
static void testLine(lua_State* L, const char* text, int type) {
    Runtime::Impl& rt = rtOf(L);
    auto ctx = rt.ctxOf(L);
    std::string who = ctx ? ctx->name : "";
    if (type == 3) { if (rt.cb.error) rt.cb.error(who, text); }
    else if (type == 2) { if (rt.cb.warn) rt.cb.warn(who, text); }
    else if (rt.cb.print) rt.cb.print(who, text);
    rt.logLine(text, type);
}
static void testCount(Instance& ts, const char* prop) { ts.setInternal(prop, Value::number(ts.get(prop).n + 1)); }
static int TestCheck(lua_State* L) {   // (condition, description): a failed check is an error line
    Instance& ts = self(L);
    if (lua_toboolean(L, 2)) return 0;
    testCount(ts, "ErrorCount");
    testLine(L, luaL_optstring(L, 3, "Check failed"), 3);
    return 0;
}
static int TestRequire(lua_State* L) { return TestCheck(L); }
static int TestWarn(lua_State* L) {   // (condition, description): a failed one is a warning line
    Instance& ts = self(L);
    if (lua_toboolean(L, 2)) return 0;
    testCount(ts, "WarnCount");
    testLine(L, luaL_optstring(L, 3, "Warning"), 2);
    return 0;
}
static int TestError(lua_State* L) { testCount(self(L), "ErrorCount"); testLine(L, luaL_optstring(L, 2, "Error"), 3); return 0; }
static int TestFail(lua_State* L) { testCount(self(L), "ErrorCount"); testLine(L, luaL_optstring(L, 2, "Fail"), 3); return 0; }
static int TestMessage(lua_State* L) { (void)self(L); testLine(L, luaL_optstring(L, 2, ""), 0); return 0; }
static int TestCheckpoint(lua_State* L) { (void)self(L); testLine(L, luaL_optstring(L, 2, "Checkpoint"), 0); return 0; }
static int TestDone(lua_State* L) { (void)self(L); testLine(L, "Testing Done", 0); return 0; }
static int isFeatureEnabled(lua_State* L) { (void)self(L); luaL_checkstring(L, 2); lua_pushboolean(L, 0); return 1; }   // no feature flags here

// ---- AvatarEditorService:GetAccessoryType: the Enum.AccessoryType an Enum.AvatarAssetType wears as ----
static int GetAccessoryType(lua_State* L) {
    (void)self(L);
    const EnumItem* kind = checkEnumItem(L, 2, findEnum("AvatarAssetType"));
    const char* out = "Unknown";
    std::string n = kind->name;
    if (n == "Hat") out = "Hat";
    else if (n.size() > 9 && n.compare(n.size() - 9, 9, "Accessory") == 0) {
        std::string stem = n.substr(0, n.size() - 9);
        static const char* kWorn[] = {"Hair", "Face", "Neck", "Shoulder", "Front", "Back", "Waist", "TShirt", "Shirt", "Pants", "Jacket", "Sweater",
                                      "Shorts", "LeftShoe", "RightShoe", "DressSkirt", "Eyebrow", "Eyelash"};
        for (const char* w : kWorn) if (stem == w) out = w;
    }
    const EnumDef* e = findEnum("AccessoryType");
    pushEnumItem(L, e, e->find(out));
    return 1;
}
// AnalyticsService's loggers take their arguments and drop them: nothing here collects analytics.
static int AnalyticsDrop(lua_State* L) { (void)self(L); return 0; }

// ---- the data family --------------------------------------------------------------------
// The Actor the instance sits under (itself included), or nil.
static int GetActor(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance* p = &self(L);
    while (p && !p->isA("Actor")) p = p->parent();
    rt.pushInstance(L, p);
    return 1;
}
// No StyleSheet applies here: the styled value is the property's own, and its signal the property's.
static int GetStyled(lua_State* L) { luaL_checkstring(L, 2); lua_settop(L, 2); return inst_index(L); }
static int GetStyledPropertyChangedSignal(lua_State* L) { return GetPropertyChangedSignal(L); }
static const PropDef& declaredProp(lua_State* L, Instance& i, const char* method) {
    const char* name = luaL_checkstring(L, 2);
    const PropDef* d = i.cls().findProp(name);
    if (!d) luaL_error(L, "%s is not a valid property name.", name);
    if (d->flags & NoScriptRead) luaL_error(L, "The current thread cannot read '%s' (lacking capability RobloxScript)", name);
    (void)method;
    return *d;
}
static int IsPropertyModified(lua_State* L) {   // a Name's default is the class name, as Instance.new leaves it
    Instance& i = self(L);
    const PropDef& d = declaredProp(L, i, "IsPropertyModified");
    lua_pushboolean(L, d.name == "Name" ? i.name() != i.className() : !(i.get(d.name) == d.def));
    return 1;
}
static int ResetPropertyToDefault(lua_State* L) {
    Instance& i = self(L);
    const PropDef& d = declaredProp(L, i, "ResetPropertyToDefault");
    if (d.flags & ReadOnly) luaL_error(L, "Unable to assign property %s. Property is read only", d.name.c_str());
    if (d.flags & NoScriptWrite) luaL_error(L, "Unable to assign property %s. Script write access is restricted", d.name.c_str());
    if ((d.flags & PluginWrite) && !rtOf(L).hasPluginCapability(L)) luaL_error(L, "The current thread cannot set '%s' (lacking capability Plugin)", d.name.c_str());
    std::string err;
    if (!i.set(d.name, d.name == "Name" ? Value::string(i.className()) : d.def, &err)) luaL_error(L, "%s", err.c_str());
    return 0;
}
// An Actor's messages are per-topic signals on it; a send lands on the next deferred pass, as a
// message crosses to another VM on Roblox. Parallel here is the same thread.
static int ActorBindToMessage(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& a = self(L);
    std::string topic = std::string("Msg:") + luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    rt.pushSignal(L, a, rt.signal(a, topic));
    lua_replace(L, 1);
    lua_pushvalue(L, 3);
    lua_replace(L, 2);
    lua_settop(L, 2);
    return connect(L, false);
}
static int ActorSendMessage(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& a = self(L);
    std::string topic = std::string("Msg:") + luaL_checkstring(L, 2);
    Signal* s = rt.findSignal(a, topic);
    if (!s || !s->hasConns()) return 0;
    const int top = lua_gettop(L);
    fireLater(L, a, topic.c_str(), [&](lua_State* co) { for (int k = 3; k <= top; k++) { lua_pushvalue(L, k); lua_xmove(L, co, 1); } });
    return 0;
}
// DataModel: the jobs table is its header row alone (the runtime's steps are not job-timed here);
// the ids are a plugin's to set, into the read-only PlaceId / GameId.
static void pluginOnly(lua_State* L, const char* method) {
    if (!rtOf(L).hasPluginCapability(L)) luaL_error(L, "The current thread cannot call '%s' (lacking capability Plugin)", method);
}
static int GetJobsInfo(lua_State* L) {
    (void)self(L); pluginOnly(L, "GetJobsInfo");
    lua_createtable(L, 1, 0);
    lua_createtable(L, 0, 5);
    for (const char* k : {"name", "averagestepsPerSecond", "averagestepTime", "averageError", "isRunning"}) { lua_pushstring(L, k); lua_setfield(L, -2, k); }
    lua_rawseti(L, -2, 1);
    return 1;
}
static int SetPlaceId(lua_State* L) { Instance& g = self(L); pluginOnly(L, "SetPlaceId"); g.setInternal("PlaceId", Value::number(luaL_checknumber(L, 2))); return 0; }
static int SetUniverseId(lua_State* L) { Instance& g = self(L); pluginOnly(L, "SetUniverseId"); g.setInternal("GameId", Value::number(luaL_checknumber(L, 2))); return 0; }
// Dialog: nobody is in a conversation here.
static int DialogGetCurrentPlayers(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
// Plugin: a menu is an object whose actions are its children; nothing here shows one. The ribbon
// tool is kept for GetSelectedRibbonTool; the Studio here has no ribbon. Joints are not created
// on drag here: None. The prompts decline (-1, false), the FBX importers refuse, the wiki page,
// the save and the drag do nothing.
static int PluginCreateMenu(lua_State* L) {   // (id, title?, icon?)
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Instance::Ptr menu = pluginObject(rt, "PluginMenu", luaL_checkstring(L, 2));
    menu->setInternal("Title", Value::string(luaL_optstring(L, 3, "")));
    menu->setInternal("Icon", Value::string(luaL_optstring(L, 4, "")));
    rt.pushInstance(L, menu.get());
    return 1;
}
static int PluginGetJoinMode(lua_State* L) { (void)self(L); const EnumDef* e = findEnum("JointCreationMode"); pushEnumItem(L, e, e->find("None")); return 1; }
static int PluginGetSelectedRibbonTool(lua_State* L) { Runtime::Impl& rt = rtOf(L); (void)self(L); const EnumDef* e = findEnum("RibbonTool"); pushEnumItem(L, e, e->find(rt.ribbonTool)); return 1; }
static int PluginSelectRibbonTool(lua_State* L) {   // (tool, position)
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    const EnumItem* it = checkEnumItem(L, 2, findEnum("RibbonTool"));
    if (it) rt.ribbonTool = it->name;
    return 0;
}
static int PushMinusOne(lua_State* L) { (void)self(L); lua_pushnumber(L, -1); return 1; }
static int PushNil(lua_State* L) { (void)self(L); lua_pushnil(L); return 1; }
static int PushEmptyTable(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
static int NoFbx(lua_State* L) { (void)self(L); luaL_error(L, "FBX import is not available here"); return 0; }
static int MenuAddAction(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& menu = self(L);
    Instance& a = rt.checkInstance(L, 2);
    if (!a.isA("PluginAction") && !a.isA("PluginMenu")) luaL_error(L, "invalid argument #2 to 'AddAction' (PluginAction expected, got %s)", a.className().c_str());
    std::string err;
    if (!a.setParent(&menu, &err)) luaL_error(L, "%s", err.c_str());
    return 0;
}
static int MenuAddNewAction(lua_State* L) {   // (actionId, text, icon?) -> the PluginAction, under the menu
    Runtime::Impl& rt = rtOf(L); Instance& menu = self(L);
    Instance::Ptr a = pluginObject(rt, "PluginAction", luaL_checkstring(L, 2));
    a->setInternal("ActionId", Value::string(lua_tostring(L, 2)));
    a->setInternal("Text", Value::string(luaL_optstring(L, 3, "")));
    a->setParent(&menu);
    rt.pushInstance(L, a.get());
    return 1;
}
static int MenuAddSeparator(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& menu = self(L);
    Instance::Ptr a = pluginObject(rt, "PluginAction", "Separator");
    a->setParent(&menu);
    rt.pushInstance(L, a.get());
    return 1;
}
static int MenuClear(lua_State* L) {
    Instance& menu = self(L);
    for (Instance::Ptr c : std::vector<Instance::Ptr>(menu.children().begin(), menu.children().end())) c->setParent(nullptr);
    return 0;
}
// PluginGui: the close callback is kept on the gui; the dock drawn here has no close button to
// invoke it. The pointer's position is the Mouse's, less the gui's own corner.
static int GuiBindToClose(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& g = self(L);
    InstBinding& b = rt.binding(g);
    auto it = b.callbacks.find("BindToClose");
    if (it != b.callbacks.end()) { lua_unref(L, it->second.ref); b.callbacks.erase(it); }
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        b.callbacks["BindToClose"] = Callback{lua_ref(L, -1), rt.ctxOf(L)};
        lua_pop(L, 1);
    }
    return 0;
}
static int GuiGetRelativeMousePosition(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); Instance& g = self(L);
    Instance* mouse = rt.mouse ? rt.mouse.get() : rt.pluginMouse.empty() ? nullptr : rt.pluginMouse.begin()->second;
    Vec2 at = g.get("AbsolutePosition").v2();
    Vec2 m = mouse ? Vec2{(float)mouse->get("X").n, (float)mouse->get("Y").n} : Vec2{0, 0};
    pushVector2(L, Vec2{m.x - at.x, m.y - at.y});
    return 1;
}
// UserGameSettings: the camera is not inverted (1), the window is not full screen, Studio mode is
// the host's option, an onboarding is completed for the session; the two visibility setters
// change nothing (no settings menu here). UserSettings:Reset puts the settings back to their
// defaults; no user feature exists here, so IsUserFeatureEnabled errors as an unknown one does.
static int PushOne(lua_State* L) { (void)self(L); lua_pushnumber(L, 1); return 1; }
static int GetOnboardingCompleted(lua_State* L) { Runtime::Impl& rt = rtOf(L); (void)self(L); lua_pushboolean(L, rt.onboardingsDone.count(luaL_checkstring(L, 2)) > 0); return 1; }
static int SetOnboardingCompleted(lua_State* L) { Runtime::Impl& rt = rtOf(L); (void)self(L); rt.onboardingsDone.insert(luaL_checkstring(L, 2)); return 0; }
static int InStudioMode(lua_State* L) { Runtime::Impl& rt = rtOf(L); (void)self(L); lua_pushboolean(L, rt.opts.isStudio); return 1; }
static int IsUserFeatureEnabled(lua_State* L) { (void)self(L); luaL_error(L, "'%s' is not a valid user feature", luaL_checkstring(L, 2)); return 0; }
static int SettingsReset(lua_State* L) {
    Instance& us = self(L);
    for (auto& c : us.children())
        for (const PropDef& d : c->cls().props) if (!(d.flags & ReadOnly)) c->setInternal(d.name, d.def);
    return 0;
}
// StudioService: no account (0), no class icons (an empty image), no file prompt (declined).
static int PushZero(lua_State* L) { (void)self(L); lua_pushnumber(L, 0); return 1; }
static int GetClassIcon(lua_State* L) {
    (void)self(L); luaL_checkstring(L, 2);
    lua_createtable(L, 0, 3);
    lua_pushstring(L, ""); lua_setfield(L, -2, "Image");
    pushVector2(L, Vec2{0, 0}); lua_setfield(L, -2, "ImageRectOffset");
    pushVector2(L, Vec2{0, 0}); lua_setfield(L, -2, "ImageRectSize");
    return 1;
}
// ---- the rest family ---------------------------------------------------------------------
// The animation curves. Keys are kept in the hidden KeysBlob, space-separated fields, one key a
// line: a FloatCurve's "t v interp left right" (a tangent "n" when unset), a RotationCurve's
// "t interp px py pz ox oy oz" (the orientation in degrees), a MarkerCurve's "t\x1fname". Keys
// stay sorted by time, one a time; indices are one-based as Roblox's are.
static std::vector<std::string> blobLines(const std::string& blob) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at < blob.size()) {
        size_t end = blob.find('\n', at);
        if (end == std::string::npos) end = blob.size();
        if (end > at) out.push_back(blob.substr(at, end - at));
        at = end + 1;
    }
    return out;
}
static float tangentOf(const char* s) { return *s == 'n' ? NAN : (float)std::atof(s); }
static const char* fmtTangent(float t, char* b, size_t n) { if (std::isnan(t)) return "n"; std::snprintf(b, n, "%.9g", t); return b; }
static std::vector<FloatKeyV> floatKeysOf(const Instance& c) {
    std::vector<FloatKeyV> keys;
    for (const std::string& line : blobLines(c.get("KeysBlob").s)) {
        char lt[32] = "n", rt_[32] = "n";
        FloatKeyV k;
        if (std::sscanf(line.c_str(), "%f %f %d %31s %31s", &k.time, &k.value, &k.interp, lt, rt_) < 3) continue;
        k.left = tangentOf(lt); k.right = tangentOf(rt_);
        keys.push_back(k);
    }
    return keys;
}
static void setFloatKeys(Instance& c, const std::vector<FloatKeyV>& keys) {
    std::string blob;
    char b[64], l[32], r[32];
    for (const FloatKeyV& k : keys) {
        std::snprintf(b, sizeof b, "%.9g %.9g %d ", k.time, k.value, k.interp);
        blob += b; blob += fmtTangent(k.left, l, sizeof l); blob += ' '; blob += fmtTangent(k.right, r, sizeof r); blob += '\n';
    }
    c.setInternal("KeysBlob", Value::string(blob));
    c.setInternal("Length", Value::number((double)keys.size()));
}
static std::vector<RotKeyV> rotKeysOf(const Instance& c) {
    std::vector<RotKeyV> keys;
    for (const std::string& line : blobLines(c.get("KeysBlob").s)) {
        RotKeyV k; Vec3 p, o;
        if (std::sscanf(line.c_str(), "%f %d %f %f %f %f %f %f", &k.time, &k.interp, &p.x, &p.y, &p.z, &o.x, &o.y, &o.z) < 8) continue;
        k.value = cframeFromPosOrient(p, o);
        keys.push_back(k);
    }
    return keys;
}
static void setRotKeys(Instance& c, const std::vector<RotKeyV>& keys) {
    std::string blob;
    char b[160];
    for (const RotKeyV& k : keys) {
        Vec3 p, o; cframeToPosOrient(k.value, p, o);
        std::snprintf(b, sizeof b, "%.9g %d %.9g %.9g %.9g %.9g %.9g %.9g\n", k.time, k.interp, p.x, p.y, p.z, o.x, o.y, o.z);
        blob += b;
    }
    c.setInternal("KeysBlob", Value::string(blob));
    c.setInternal("Length", Value::number((double)keys.size()));
}
struct MarkerV { float time; std::string name; };
static std::vector<MarkerV> markersOf(const Instance& c) {
    std::vector<MarkerV> out;
    for (const std::string& line : blobLines(c.get("KeysBlob").s)) {
        size_t sep = line.find('\x1f');
        if (sep == std::string::npos) continue;
        out.push_back({(float)std::atof(line.substr(0, sep).c_str()), line.substr(sep + 1)});
    }
    return out;
}
static void setMarkers(Instance& c, const std::vector<MarkerV>& ms) {
    std::string blob;
    char b[32];
    for (const MarkerV& m : ms) { std::snprintf(b, sizeof b, "%.9g", m.time); blob += b; blob += '\x1f'; blob += m.name; blob += '\n'; }
    c.setInternal("KeysBlob", Value::string(blob));
    c.setInternal("Length", Value::number((double)ms.size()));
}
// Insert at time t, replacing a key there; answers whether one was added and where (one-based).
template <class K> static std::pair<bool, int> insertSorted(std::vector<K>& keys, const K& k, float t) {
    size_t at = 0;
    while (at < keys.size() && keys[at].time < t) at++;
    if (at < keys.size() && keys[at].time == t) { keys[at] = k; return {false, (int)at + 1}; }
    keys.insert(keys.begin() + at, k);
    return {true, (int)at + 1};
}
template <class K> static int removeRange(std::vector<K>& keys, int start, int count) {
    if (start < 1 || start > (int)keys.size() || count < 1) return 0;
    int n = std::min(count, (int)keys.size() - start + 1);
    keys.erase(keys.begin() + start - 1, keys.begin() + start - 1 + n);
    return n;
}
// The two one-based indices bracketing t, as Roblox's GetKeyIndicesAtTime describes them.
template <class K> static void pushIndicesAtTime(lua_State* L, const std::vector<K>& keys, float t) {
    int n = (int)keys.size(), lo = 0, hi = 0;
    for (int i = 0; i < n; i++) if (keys[i].time <= t) lo = i + 1;
    for (int i = n - 1; i >= 0; i--) if (keys[i].time >= t) hi = i + 1;
    if (!lo) lo = n ? 1 : 0;
    if (!hi) hi = n;
    lua_createtable(L, 2, 0);
    lua_pushinteger(L, lo); lua_rawseti(L, -2, 1);
    lua_pushinteger(L, hi); lua_rawseti(L, -2, 2);
}
template <class K> static void sortKeys(std::vector<K>& keys) {
    std::stable_sort(keys.begin(), keys.end(), [](const K& a, const K& b) { return a.time < b.time; });
    std::vector<K> out;
    for (const K& k : keys) { if (!out.empty() && out.back().time == k.time) out.back() = k; else out.push_back(k); }
    keys.swap(out);
}
static bool sampleFloat(const std::vector<FloatKeyV>& keys, float t, float& out) {
    if (keys.empty()) return false;
    if (t <= keys.front().time) { out = keys.front().value; return true; }
    if (t >= keys.back().time) { out = keys.back().value; return true; }
    size_t i = 0;
    while (i + 1 < keys.size() && keys[i + 1].time <= t) i++;
    const FloatKeyV& a = keys[i];
    const FloatKeyV& b = keys[i + 1];
    float span = b.time - a.time, u = span > 0 ? (t - a.time) / span : 0;
    if (a.interp == 0) out = a.value;
    else if (a.interp == 1) out = a.value + (b.value - a.value) * u;
    else {   // cubic Hermite on the segment's tangents, an unset one 0
        float m0 = (std::isnan(a.right) ? 0 : a.right) * span, m1 = (std::isnan(b.left) ? 0 : b.left) * span;
        float u2 = u * u, u3 = u2 * u;
        out = (2 * u3 - 3 * u2 + 1) * a.value + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * b.value + (u3 - u2) * m1;
    }
    return true;
}
static int FloatCurveGetKeyAtIndex(lua_State* L) {
    std::vector<FloatKeyV> keys = floatKeysOf(self(L));
    int idx = (int)luaL_checkinteger(L, 2);
    if (idx < 1 || idx > (int)keys.size()) luaL_error(L, "GetKeyAtIndex: index %d is out of range (1..%d)", idx, (int)keys.size());
    pushFloatCurveKey(L, keys[idx - 1]);
    return 1;
}
static int FloatCurveGetKeyIndicesAtTime(lua_State* L) { pushIndicesAtTime(L, floatKeysOf(self(L)), (float)luaL_checknumber(L, 2)); return 1; }
static int FloatCurveGetKeys(lua_State* L) {
    std::vector<FloatKeyV> keys = floatKeysOf(self(L));
    lua_createtable(L, (int)keys.size(), 0);
    for (size_t i = 0; i < keys.size(); i++) { pushFloatCurveKey(L, keys[i]); lua_rawseti(L, -2, (int)i + 1); }
    return 1;
}
static int FloatCurveGetValueAtTime(lua_State* L) {
    float v;
    if (sampleFloat(floatKeysOf(self(L)), (float)luaL_checknumber(L, 2), v)) lua_pushnumber(L, v); else lua_pushnil(L);
    return 1;
}
static void pushInserted(lua_State* L, std::pair<bool, int> r) {
    lua_createtable(L, 2, 0);
    lua_pushboolean(L, r.first); lua_rawseti(L, -2, 1);
    lua_pushinteger(L, r.second); lua_rawseti(L, -2, 2);
}
static int FloatCurveInsertKey(lua_State* L) {
    Instance& c = self(L);
    FloatKeyV k = checkFloatCurveKey(L, 2);
    std::vector<FloatKeyV> keys = floatKeysOf(c);
    std::pair<bool, int> r = insertSorted(keys, k, k.time);
    setFloatKeys(c, keys);
    pushInserted(L, r);
    return 1;
}
static int FloatCurveRemoveKeyAtIndex(lua_State* L) {
    Instance& c = self(L);
    std::vector<FloatKeyV> keys = floatKeysOf(c);
    int n = removeRange(keys, (int)luaL_checkinteger(L, 2), (int)luaL_optinteger(L, 3, 1));
    setFloatKeys(c, keys);
    lua_pushinteger(L, n);
    return 1;
}
static int FloatCurveSetKeys(lua_State* L) {
    Instance& c = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<FloatKeyV> keys;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        keys.push_back(checkFloatCurveKey(L, -1));
        lua_pop(L, 1);
    }
    sortKeys(keys);
    setFloatKeys(c, keys);
    lua_pushinteger(L, (int)keys.size());
    return 1;
}
static int RotationCurveGetKeyAtIndex(lua_State* L) {
    std::vector<RotKeyV> keys = rotKeysOf(self(L));
    int idx = (int)luaL_checkinteger(L, 2);
    if (idx < 1 || idx > (int)keys.size()) luaL_error(L, "GetKeyAtIndex: index %d is out of range (1..%d)", idx, (int)keys.size());
    pushRotationCurveKey(L, keys[idx - 1]);
    return 1;
}
static int RotationCurveGetKeyIndicesAtTime(lua_State* L) { pushIndicesAtTime(L, rotKeysOf(self(L)), (float)luaL_checknumber(L, 2)); return 1; }
static int RotationCurveGetKeys(lua_State* L) {
    std::vector<RotKeyV> keys = rotKeysOf(self(L));
    lua_createtable(L, (int)keys.size(), 0);
    for (size_t i = 0; i < keys.size(); i++) { pushRotationCurveKey(L, keys[i]); lua_rawseti(L, -2, (int)i + 1); }
    return 1;
}
// Constant holds the key; Linear and Cubic both blend the two frames (CFrameV::lerp): no
// tangents act on a rotation here.
static int RotationCurveGetValueAtTime(lua_State* L) {
    std::vector<RotKeyV> keys = rotKeysOf(self(L));
    float t = (float)luaL_checknumber(L, 2);
    if (keys.empty()) { lua_pushnil(L); return 1; }
    if (t <= keys.front().time) { pushCFrame(L, keys.front().value); return 1; }
    if (t >= keys.back().time) { pushCFrame(L, keys.back().value); return 1; }
    size_t i = 0;
    while (i + 1 < keys.size() && keys[i + 1].time <= t) i++;
    const RotKeyV& a = keys[i];
    const RotKeyV& b = keys[i + 1];
    float span = b.time - a.time, u = span > 0 ? (t - a.time) / span : 0;
    pushCFrame(L, a.interp == 0 ? a.value : a.value.lerp(b.value, u));
    return 1;
}
static int RotationCurveInsertKey(lua_State* L) {
    Instance& c = self(L);
    RotKeyV k = checkRotationCurveKey(L, 2);
    std::vector<RotKeyV> keys = rotKeysOf(c);
    std::pair<bool, int> r = insertSorted(keys, k, k.time);
    setRotKeys(c, keys);
    pushInserted(L, r);
    return 1;
}
static int RotationCurveRemoveKeyAtIndex(lua_State* L) {
    Instance& c = self(L);
    std::vector<RotKeyV> keys = rotKeysOf(c);
    int n = removeRange(keys, (int)luaL_checkinteger(L, 2), (int)luaL_optinteger(L, 3, 1));
    setRotKeys(c, keys);
    lua_pushinteger(L, n);
    return 1;
}
static int RotationCurveSetKeys(lua_State* L) {
    Instance& c = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<RotKeyV> keys;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        keys.push_back(checkRotationCurveKey(L, -1));
        lua_pop(L, 1);
    }
    sortKeys(keys);
    setRotKeys(c, keys);
    lua_pushinteger(L, (int)keys.size());
    return 1;
}
static void pushMarker(lua_State* L, const MarkerV& m) {
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, m.time); lua_setfield(L, -2, "Time");
    lua_pushstring(L, m.name.c_str()); lua_setfield(L, -2, "Value");
}
static int MarkerCurveGetMarkerAtIndex(lua_State* L) {
    std::vector<MarkerV> ms = markersOf(self(L));
    int idx = (int)luaL_checkinteger(L, 2);
    if (idx < 1 || idx > (int)ms.size()) luaL_error(L, "GetMarkerAtIndex: index %d is out of range (1..%d)", idx, (int)ms.size());
    pushMarker(L, ms[idx - 1]);
    return 1;
}
static int MarkerCurveGetMarkers(lua_State* L) {
    std::vector<MarkerV> ms = markersOf(self(L));
    lua_createtable(L, (int)ms.size(), 0);
    for (size_t i = 0; i < ms.size(); i++) { pushMarker(L, ms[i]); lua_rawseti(L, -2, (int)i + 1); }
    return 1;
}
static int MarkerCurveInsertMarkerAtTime(lua_State* L) {
    Instance& c = self(L);
    MarkerV m{(float)luaL_checknumber(L, 2), luaL_checkstring(L, 3)};
    std::vector<MarkerV> ms = markersOf(c);
    std::pair<bool, int> r = insertSorted(ms, m, m.time);
    setMarkers(c, ms);
    pushInserted(L, r);
    return 1;
}
static int MarkerCurveRemoveMarkerAtIndex(lua_State* L) {
    Instance& c = self(L);
    std::vector<MarkerV> ms = markersOf(c);
    int n = removeRange(ms, (int)luaL_checkinteger(L, 2), (int)luaL_optinteger(L, 3, 1));
    setMarkers(c, ms);
    lua_pushinteger(L, n);
    return 1;
}
// An EulerRotationCurve's or Vector3Curve's channel: the child FloatCurve of that name, made empty
// when missing, as Roblox's X() / Y() / Z() do.
static Instance* curveChannel(Runtime::Impl& rt, Instance& c, const char* axis) {
    for (auto& ch : c.children()) if (ch->isA("FloatCurve") && ch->name() == axis) return ch.get();
    Instance::Ptr made = rt.dm.create("FloatCurve", &c);
    made->setName(axis);
    return made.get();
}
static int CurveX(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, curveChannel(rt, self(L), "X")); return 1; }
static int CurveY(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, curveChannel(rt, self(L), "Y")); return 1; }
static int CurveZ(lua_State* L) { Runtime::Impl& rt = rtOf(L); rt.pushInstance(L, curveChannel(rt, self(L), "Z")); return 1; }
static bool sampleChannel(Instance& c, const char* axis, float t, float& out) {
    for (auto& ch : c.children()) if (ch->isA("FloatCurve") && ch->name() == axis) return sampleFloat(floatKeysOf(*ch), t, out);
    return false;
}
// The three channels at t, each nil where its curve is empty (Roblox's Array of three).
static int CurveGetValueAtTime3(lua_State* L) {
    Instance& c = self(L);
    float t = (float)luaL_checknumber(L, 2);
    lua_createtable(L, 3, 0);
    int k = 1;
    for (const char* axis : {"X", "Y", "Z"}) {
        float v;
        if (sampleChannel(c, axis, t, v)) lua_pushnumber(L, v); else lua_pushnil(L);
        lua_rawseti(L, -2, k++);
    }
    return 1;
}
// The angles are radians, composed as CFrame.fromEulerAngles(x, y, z, RotationOrder) does: the
// order's first axis outermost. An empty channel counts 0.
static int EulerGetRotationAtTime(lua_State* L) {
    Instance& c = self(L);
    float t = (float)luaL_checknumber(L, 2), a[3] = {0, 0, 0};
    const char* axes[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; i++) { float v; if (sampleChannel(c, axes[i], t, v)) a[i] = v; }
    const std::string order = c.get("RotationOrder").s;
    CFrameV out;
    for (char ch : order) {
        int i = ch == 'X' ? 0 : ch == 'Y' ? 1 : 2;
        Vec3 axis{i == 0 ? 1.f : 0.f, i == 1 ? 1.f : 0.f, i == 2 ? 1.f : 0.f};
        out = out * CFrameV::axisAngle(axis, a[i]);
    }
    pushCFrame(L, out);
    return 1;
}

// IKControl: the chain from EndEffector up to ChainRoot, a Motor6D's Part1 -> Part0 at each step
// (a Bone's parent when the effector is a Bone). Node 0 is the ChainRoot end.
static Instance* jointInto(Runtime::Impl& rt, Instance& part) {
    for (int64_t id : rt.motors) {
        Instance* j = rt.dm.find(id);
        if (j && !j->destroyed() && j->get("Part1").ref == part.id()) return j;
    }
    return nullptr;
}
static std::vector<Instance*> ikChain(Runtime::Impl& rt, Instance& ik) {
    std::vector<Instance*> nodes;
    Instance* end = rt.dm.findRef(ik.get("EndEffector").ref);
    Instance* root = rt.dm.findRef(ik.get("ChainRoot").ref);
    if (!end) return nodes;
    Instance* cur = end;
    for (int guard = 0; cur && guard < 64; guard++) {
        nodes.push_back(cur);
        if (cur == root) break;
        if (cur->isA("Bone")) { cur = cur->parent(); if (cur && !cur->isA("Bone") && !cur->isA("BasePart")) cur = nullptr; continue; }
        Instance* j = jointInto(rt, *cur);
        cur = j ? rt.dm.findRef(j->get("Part0").ref) : nullptr;
    }
    std::reverse(nodes.begin(), nodes.end());
    return nodes;
}
static CFrameV nodeWorld(Instance& n) {
    if (n.isA("Bone")) return boneTransformedWorld(n);
    if (n.isA("Attachment")) return attachmentWorld(n);
    return partCFrame(n);
}
static int IKGetChainCount(lua_State* L) { Runtime::Impl& rt = rtOf(L); lua_pushinteger(L, (int)ikChain(rt, self(L)).size()); return 1; }
static int IKGetChainLength(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    std::vector<Instance*> nodes = ikChain(rt, self(L));
    double len = 0;
    for (size_t i = 1; i < nodes.size(); i++) {
        Vec3 a = nodeWorld(*nodes[i - 1]).p, b = nodeWorld(*nodes[i]).p;
        len += std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    }
    lua_pushnumber(L, len);
    return 1;
}
static Instance& ikNode(lua_State* L, std::vector<Instance*>& nodes, const char* method) {
    int idx = (int)luaL_checkinteger(L, 2);
    if (idx < 0 || idx >= (int)nodes.size()) luaL_error(L, "%s: node index %d is out of range (0..%d)", method, idx, (int)nodes.size() - 1);
    return *nodes[idx];
}
static int IKGetNodeWorldCFrame(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    std::vector<Instance*> nodes = ikChain(rt, self(L));
    pushCFrame(L, nodeWorld(ikNode(L, nodes, "GetNodeWorldCFrame")));
    return 1;
}
static int IKGetNodeLocalCFrame(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    std::vector<Instance*> nodes = ikChain(rt, self(L));
    int idx = (int)luaL_checkinteger(L, 2);
    Instance& n = ikNode(L, nodes, "GetNodeLocalCFrame");
    CFrameV w = nodeWorld(n);
    pushCFrame(L, idx > 0 ? nodeWorld(*nodes[idx - 1]).inverse() * w : w);
    return 1;
}
// Target's frame (a part's, an attachment's or a bone's) times Offset; no smoothing here.
static int IKGetRawFinalTarget(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& ik = self(L);
    Instance* target = rt.dm.findRef(ik.get("Target").ref);
    CFrameV base = target ? nodeWorld(*target) : CFrameV::identity();
    pushCFrame(L, base * getCFrameProp(ik, "OffsetPosition", "OffsetOrientation"));
    return 1;
}

// The Input Action System. An action's state is computed from its InputBinding children: the
// keys they hold down (bindingKeysDown) and what Fire handed them.
static Vec3 scaleVec(Vec3 v, const Value& s) { return {v.x * s.v.x, v.y * s.v.y, v.z * s.v.z}; }
static void setActionState(Runtime::Impl& rt, Instance& action, const Value& v) {
    const std::string type = action.get("Type").s;
    const char* prop = type == "Bool" ? "BoolState" : type == "Direction1D" ? "Direction1DState" : type == "Direction2D" ? "Direction2DState"
                     : type == "Direction3D" ? "Direction3DState" : "ViewportPositionState";
    Value old = action.get(prop);
    if (old == v) return;
    action.setInternal(prop, v);
    if (Signal* s = rt.findSignal(action, "StateChanged")) if (s->hasConns()) rt.fireValues(action, "StateChanged", {v});
    if (type == "Bool") {
        if (Signal* s = rt.findSignal(action, v.b ? "Pressed" : "Released")) if (s->hasConns()) rt.fireValues(action, v.b ? "Pressed" : "Released", {});
    }
}
static bool keyHeld(Runtime::Impl& rt, Instance& b, const char* prop) {
    int code = (int)b.get(prop).n;
    if (!code) return false;
    auto it = rt.bindingKeysDown.find(b.id());
    return it != rt.bindingKeysDown.end() && it->second.count(code);
}
static void refreshAction(Runtime::Impl& rt, Instance& action) {
    const std::string type = action.get("Type").s;
    bool any = false;
    float d1 = 0; Vec3 d{0, 0, 0};
    for (auto& ch : action.children()) {
        if (!ch->isA("InputBinding")) continue;
        Instance& b = *ch;
        float scale = (float)b.get("Scale").n;
        if (keyHeld(rt, b, "KeyCode")) { any = true; d1 += scale; }
        Vec3 c{(keyHeld(rt, b, "Right") ? 1.f : 0.f) - (keyHeld(rt, b, "Left") ? 1.f : 0.f), (keyHeld(rt, b, "Up") ? 1.f : 0.f) - (keyHeld(rt, b, "Down") ? 1.f : 0.f),
               (keyHeld(rt, b, "Backward") ? 1.f : 0.f) - (keyHeld(rt, b, "Forward") ? 1.f : 0.f)};
        if (c.x != 0 || c.y != 0 || c.z != 0) any = true;
        c = {c.x * scale, c.y * scale, c.z * scale};
        c = type == "Direction2D" ? scaleVec(c, b.get("Vector2Scale")) : scaleVec(c, b.get("Vector3Scale"));
        if (b.get("ClampMagnitudeToOne").b) {
            float len = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
            if (len > 1) c = {c.x / len, c.y / len, c.z / len};
        }
        d = {d.x + c.x, d.y + c.y, d.z + c.z};
    }
    if (type == "Bool") setActionState(rt, action, Value::boolean(any));
    else if (type == "Direction1D") setActionState(rt, action, Value::number(d1));
    else if (type == "Direction2D") setActionState(rt, action, Value::vector2(d.x, d.y));
    else if (type == "Direction3D") setActionState(rt, action, Value::vector3(d));
}
static bool actionLive(Instance& action) {
    if (!action.get("Enabled").b) return false;
    Instance* ctx = action.parent();
    return !(ctx && ctx->isA("InputContext") && !ctx->get("Enabled").b);
}
static void inputActionsKeyImpl(Runtime::Impl& rt, int keyCode, bool down) {
    if (!keyCode || rt.inputBindings.empty()) return;
    struct Hit { Instance* binding; Instance* action; double priority; bool sink; };
    std::vector<Hit> hits;
    for (int64_t id : rt.inputBindings) {
        Instance* b = rt.dm.find(id);
        if (!b || b->destroyed()) continue;
        Instance* action = b->parent();
        if (!action || !action->isA("InputAction") || !actionLive(*action)) continue;
        bool match = false;
        for (const char* prop : {"KeyCode", "Up", "Down", "Left", "Right", "Forward", "Backward"}) if ((int)b->get(prop).n == keyCode) match = true;
        if (!match) continue;
        Instance* ctx = action->parent();
        bool inContext = ctx && ctx->isA("InputContext");
        hits.push_back({b, action, inContext ? ctx->get("Priority").n : 0, inContext && ctx->get("Sink").b});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.priority > b.priority; });
    double sunkBelow = -std::numeric_limits<double>::infinity();
    bool sunk = false;
    for (const Hit& h : hits) {
        if (sunk && h.priority < sunkBelow) break;
        if (down) rt.bindingKeysDown[h.binding->id()].insert(keyCode); else rt.bindingKeysDown[h.binding->id()].erase(keyCode);
        refreshAction(rt, *h.action);
        if (h.sink) { sunk = true; sunkBelow = h.priority; }
    }
}
static int InputActionGetState(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& a = self(L);
    const std::string type = a.get("Type").s;
    const char* prop = type == "Bool" ? "BoolState" : type == "Direction1D" ? "Direction1DState" : type == "Direction2D" ? "Direction2DState"
                     : type == "Direction3D" ? "Direction3DState" : "ViewportPositionState";
    rt.pushValue(L, a.get(prop));
    return 1;
}
// Fire(state) hands the action the state as given, converted to its Type.
static int InputBindingFire(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& b = self(L);
    Instance* action = b.parent();
    if (!action || !action->isA("InputAction") || !actionLive(*action)) return 0;
    const std::string type = action->get("Type").s;
    Value v;
    if (type == "Bool") v = Value::boolean(lua_toboolean(L, 2));
    else if (type == "Direction1D") v = Value::number(luaL_checknumber(L, 2));
    else if (type == "Direction2D" || type == "ViewportPosition") v = Value::vector2(checkVector2(L, 2).x, checkVector2(L, 2).y);
    else v = Value::vector3(checkVec3(L, 2));
    setActionState(rt, *action, v);
    return 0;
}

// ReflectionService reads this registry. A ReflectedClass carries Name, Superclass, Subclasses and
// Serialized; a member its Name and Owner, a property its Serialized and Type, a method its
// Parameters (none recorded here) and CanYield (an ...Async name). Roblox's Display and Permits
// are left out: no SecurityCapabilities here. A Type's shape is not verified here: {Name}.
static void pushReflectionType(lua_State* L, const std::string& name) {
    lua_createtable(L, 0, 1);
    lua_pushstring(L, name.c_str()); lua_setfield(L, -2, "Name");
}
static void pushReflectedClass(lua_State* L, const ClassDef& c) {
    lua_createtable(L, 0, 4);
    lua_pushstring(L, c.name.c_str()); lua_setfield(L, -2, "Name");
    if (c.base) { lua_pushstring(L, c.base->name.c_str()); lua_setfield(L, -2, "Superclass"); }
    lua_pushboolean(L, !c.service); lua_setfield(L, -2, "Serialized");
    lua_newtable(L);
    int n = 0;
    for (const ClassDef* sub : allClasses()) if (sub->base == &c) { lua_pushstring(L, sub->name.c_str()); lua_rawseti(L, -2, ++n); }
    lua_setfield(L, -2, "Subclasses");
}
static bool reflectionIsA(lua_State* L, int filterIdx, const ClassDef& c) {
    if (lua_isnoneornil(L, filterIdx)) return true;
    luaL_checktype(L, filterIdx, LUA_TTABLE);
    lua_getfield(L, filterIdx, "IsA");
    bool ok = lua_isnil(L, -1) || c.isA(lua_tostring(L, -1));
    lua_pop(L, 1);
    return ok;
}
static bool reflectionInherited(lua_State* L, int filterIdx) {
    if (lua_isnoneornil(L, filterIdx)) return true;
    lua_getfield(L, filterIdx, "ExcludeInherited");
    bool ex = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return !ex;
}
static int ReflectionGetClass(lua_State* L) {
    (void)self(L);
    const ClassDef* c = findClass(luaL_checkstring(L, 2));
    if (!c || !reflectionIsA(L, 3, *c)) { lua_pushnil(L); return 1; }
    pushReflectedClass(L, *c);
    return 1;
}
static int ReflectionGetClasses(lua_State* L) {
    (void)self(L);
    lua_newtable(L);
    int n = 0;
    for (const ClassDef* c : allClasses()) if (reflectionIsA(L, 2, *c)) { pushReflectedClass(L, *c); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int ReflectionGetPropertiesOfClass(lua_State* L) {
    (void)self(L);
    const ClassDef* c = findClass(luaL_checkstring(L, 2));
    if (!c) { lua_pushnil(L); return 1; }
    bool inherited = reflectionInherited(L, 3);
    lua_newtable(L);
    int n = 0;
    for (const ClassDef* k = c; k; k = inherited ? k->base : nullptr)
        for (const PropDef& p : k->props) {
            if (p.flags & Hidden) continue;
            lua_createtable(L, 0, 4);
            lua_pushstring(L, p.name.c_str()); lua_setfield(L, -2, "Name");
            lua_pushstring(L, k->name.c_str()); lua_setfield(L, -2, "Owner");
            lua_pushboolean(L, !(p.flags & (NoReplicate | ReadOnly)) && p.aliasOf.empty()); lua_setfield(L, -2, "Serialized");
            pushReflectionType(L, p.type == Value::Enum && p.enumType ? std::string("Enum.") + p.enumType->name : Value::typeName(p.type)); lua_setfield(L, -2, "Type");
            lua_rawseti(L, -2, ++n);
        }
    return 1;
}
static int ReflectionGetEventsOfClass(lua_State* L) {
    (void)self(L);
    const ClassDef* c = findClass(luaL_checkstring(L, 2));
    if (!c) { lua_pushnil(L); return 1; }
    bool inherited = reflectionInherited(L, 3);
    lua_newtable(L);
    int n = 0;
    for (const ClassDef* k = c; k; k = inherited ? k->base : nullptr)
        for (const std::string& e : k->events) {
            lua_createtable(L, 0, 3);
            lua_pushstring(L, e.c_str()); lua_setfield(L, -2, "Name");
            lua_pushstring(L, k->name.c_str()); lua_setfield(L, -2, "Owner");
            lua_newtable(L); lua_setfield(L, -2, "Parameters");
            lua_rawseti(L, -2, ++n);
        }
    return 1;
}

// EncodingService: base64 over a buffer (a string is taken too), a buffer back, as Roblox's
// signatures say. The hashes and zstd are not answered: none of them is here.
static std::string bufferOrString(lua_State* L, int idx) {
    if (lua_isbuffer(L, idx)) { size_t n; const char* p = (const char*)lua_tobuffer(L, idx, &n); return std::string(p, n); }
    size_t n; const char* p = luaL_checklstring(L, idx, &n);
    return std::string(p, n);
}
static void pushBuffer(lua_State* L, const std::string& bytes) {
    void* b = lua_newbuffer(L, bytes.size());
    if (!bytes.empty()) std::memcpy(b, bytes.data(), bytes.size());
}
static int EncodingBase64Encode(lua_State* L) {
    (void)self(L);
    std::string in = bufferOrString(L, 2);
    pushBuffer(L, base64Encode(std::vector<uint8_t>(in.begin(), in.end())));
    return 1;
}
static int EncodingBase64Decode(lua_State* L) {
    (void)self(L);
    std::vector<uint8_t> out;
    if (!base64Decode(bufferOrString(L, 2), out)) luaL_error(L, "Base64Decode: the input is not base64");
    pushBuffer(L, std::string(out.begin(), out.end()));
    return 1;
}

// TextService:FilterStringAsync gives a TextFilterResult holding the text as it came: no filter
// here, so every reading is the original.
static int TextFilterStringAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    const char* text = luaL_checkstring(L, 2);
    Instance::Ptr r = rt.dm.createInternal("TextFilterResult");
    r->setName("TextFilterResult");
    r->setInternal("FilteredText", Value::string(text));
    rt.pushInstance(L, r.get());
    return 1;
}
static int TextFilterResultText(lua_State* L) { lua_pushstring(L, self(L).get("FilteredText").s.c_str()); return 1; }
static int TextFilterGetTranslations(lua_State* L) { (void)self(L); lua_newtable(L); return 1; }
static int TextFilterGetTranslationForLocale(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& t = self(L);
    luaL_checkstring(L, 2);
    rt.pushInstance(L, rt.dm.findRef(t.get("SourceText").ref));
    return 1;
}

// The rigs. GetJointNames lists the joints this description names; the R15 / R6 lists are Roblox's.
static const char* const kR15Joints[] = {"Root", "Waist", "Spine", "Chest", "Neck", "HeadBase", "LeftClavicle", "LeftShoulder", "LeftElbow", "LeftWrist",
                                         "RightClavicle", "RightShoulder", "RightElbow", "RightWrist", "LeftHip", "LeftKnee", "LeftAnkle", "LeftToeBase",
                                         "RightHip", "RightKnee", "RightAnkle", "RightToeBase"};
static const char* const kR6Joints[] = {"Root", "Neck", "LeftShoulder", "RightShoulder", "LeftHip", "RightHip"};
static int pushNames(lua_State* L, const char* const* names, size_t n) {
    lua_createtable(L, (int)n, 0);
    for (size_t i = 0; i < n; i++) { lua_pushstring(L, names[i]); lua_rawseti(L, -2, (int)i + 1); }
    return 1;
}
static int RigGetR15JointNames(lua_State* L) { (void)self(L); return pushNames(L, kR15Joints, sizeof kR15Joints / sizeof *kR15Joints); }
static int RigGetR6JointNames(lua_State* L) { (void)self(L); return pushNames(L, kR6Joints, sizeof kR6Joints / sizeof *kR6Joints); }
static int RigGetJointNames(lua_State* L) {
    Instance& rig = self(L);
    lua_newtable(L);
    int n = 0;
    for (const char* j : kR15Joints) if (rig.get(j).ref) { lua_pushstring(L, j); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int RigGetJointFromName(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& rig = self(L);
    const char* name = luaL_checkstring(L, 2);
    bool known = false;
    for (const char* j : kR15Joints) if (!std::strcmp(j, name)) known = true;
    rt.pushInstance(L, known ? rt.dm.findRef(rig.get(name).ref) : nullptr);
    return 1;
}
// A DigitsRigDescription's finger tips and controls, five Vector3s each in a hidden blob.
static std::vector<Vec3> fingerVecs(const Instance& d, const char* prop) {
    std::vector<Vec3> out(5, Vec3{0, 0, 0});
    std::vector<std::string> lines = blobLines(d.get(prop).s);
    for (size_t i = 0; i < lines.size() && i < 5; i++) std::sscanf(lines[i].c_str(), "%f %f %f", &out[i].x, &out[i].y, &out[i].z);
    return out;
}
static void setFingerVecs(Instance& d, const char* prop, const std::vector<Vec3>& v) {
    std::string blob;
    char b[96];
    for (const Vec3& p : v) { std::snprintf(b, sizeof b, "%.9g %.9g %.9g\n", p.x, p.y, p.z); blob += b; }
    d.setInternal(prop, Value::string(blob));
}
static int fingerIndexArg(lua_State* L) {
    int i = (int)luaL_checkinteger(L, 2);
    if (i < 0 || i > 4) luaL_error(L, "fingerIndex must be 0..4");
    return i;
}
static int DigitsGetFingerTip(lua_State* L) { Instance& d = self(L); pushVec3(L, fingerVecs(d, "FingerTips")[fingerIndexArg(L)]); return 1; }
static int DigitsGetFingerControl(lua_State* L) { Instance& d = self(L); pushVec3(L, fingerVecs(d, "FingerControls")[fingerIndexArg(L)]); return 1; }
static int DigitsSetFingerTip(lua_State* L) { Instance& d = self(L); int i = fingerIndexArg(L); std::vector<Vec3> v = fingerVecs(d, "FingerTips"); v[i] = checkVec3(L, 3); setFingerVecs(d, "FingerTips", v); return 0; }
static int DigitsSetFingerControl(lua_State* L) { Instance& d = self(L); int i = fingerIndexArg(L); std::vector<Vec3> v = fingerVecs(d, "FingerControls"); v[i] = checkVec3(L, 3); setFingerVecs(d, "FingerControls", v); return 0; }

// A description's applied instance: the Instance property, as nothing here wears one.
static int DescriptionGetAppliedInstance(lua_State* L) { Runtime::Impl& rt = rtOf(L); Instance& d = self(L); rt.pushInstance(L, rt.dm.findRef(d.get("Instance").ref)); return 1; }
// A HapticEffect's waveform: FloatCurveKeys, kept as a FloatCurve's are. Play / Stop rumble nothing.
static int HapticSetWaveformKeys(lua_State* L) { return FloatCurveSetKeys(L); }
// FluidForceSensor:EvaluateAsync(linearVelocity, angularVelocity, cframe): the three readings, zero here.
static int FluidEvaluateAsync(lua_State* L) {
    Instance& s = self(L);
    checkVec3(L, 2); checkVec3(L, 3); checkCFrame(L, 4);
    pushVec3(L, s.get("CenterOfPressure").v); pushVec3(L, s.get("Force").v); pushVec3(L, s.get("Torque").v);
    return 3;
}

// Styling. A StyleBase's rules and a StyleSheet's derives are its children; a rule's properties
// are held in the runtime (styleProps), its transitions as JSON in the hidden blobs.
static int StyleGetStyleRules(lua_State* L) { return childrenOfClass(L, "StyleRule"); }
static int StyleGetDerives(lua_State* L) { return childrenOfClass(L, "StyleDerive"); }
static void styleRulesChanged(Runtime::Impl& rt, Instance& base) { if (Signal* s = rt.findSignal(base, "StyleRulesChanged")) if (s->hasConns()) rt.fireValues(base, "StyleRulesChanged", {}); }
static int StyleInsertStyleRule(lua_State* L) {   // (rule, priority?)
    Runtime::Impl& rt = rtOf(L);
    Instance& base = self(L);
    Instance& rule = rt.checkInstance(L, 2);
    if (!rule.isA("StyleRule")) luaL_error(L, "InsertStyleRule: a StyleRule is expected");
    if (!lua_isnoneornil(L, 3)) rule.set("Priority", Value::number(luaL_checknumber(L, 3)));
    std::string err;
    if (!rule.setParent(&base, &err)) luaL_error(L, "%s", err.c_str());
    styleRulesChanged(rt, base);
    return 0;
}
static int styleSetChildren(lua_State* L, const char* cls, const char* method) {
    Runtime::Impl& rt = rtOf(L);
    Instance& base = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<Instance::Ptr> keep;
    for (auto& ch : base.children()) if (ch->isA(cls)) keep.push_back(ch);
    for (auto& ch : keep) ch->setParent(nullptr);
    for (int i = 1;; i++) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        Instance& r = rt.checkInstance(L, -1);
        if (!r.isA(cls)) luaL_error(L, "%s: a %s is expected", method, cls);
        r.setParent(&base);
        lua_pop(L, 1);
    }
    if (!std::strcmp(cls, "StyleRule")) styleRulesChanged(rt, base);
    return 0;
}
static int StyleSetStyleRules(lua_State* L) { return styleSetChildren(L, "StyleRule", "SetStyleRules"); }
static int StyleSetDerives(lua_State* L) { return styleSetChildren(L, "StyleDerive", "SetDerives"); }
static int StyleRuleSetProperty(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& rule = self(L);
    const char* name = luaL_checkstring(L, 2);
    Value v; std::string err;
    if (lua_isnil(L, 3)) { rt.styleProps[rule.id()].erase(name); return 0; }
    if (!rt.toValue(L, 3, Value::Nil, nullptr, v, err)) luaL_error(L, "invalid argument #3 to 'SetProperty' (%s is not a supported style value)", err.c_str());
    rt.styleProps[rule.id()][name] = v;
    return 0;
}
static int StyleRuleSetProperties(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& rule = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    auto& props = rt.styleProps[rule.id()];
    lua_pushnil(L);
    while (lua_next(L, 2)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            Value v; std::string err;
            if (!rt.toValue(L, -1, Value::Nil, nullptr, v, err)) luaL_error(L, "SetProperties: %s is not a supported style value", err.c_str());
            props[lua_tostring(L, -2)] = v;
        }
        lua_pop(L, 1);
    }
    return 0;
}
static int StyleRuleGetProperty(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& rule = self(L);
    const char* name = luaL_checkstring(L, 2);
    auto it = rt.styleProps.find(rule.id());
    if (it == rt.styleProps.end() || !it->second.count(name)) { lua_pushnil(L); return 1; }
    rt.pushValue(L, it->second.at(name));
    return 1;
}
static int StyleRuleGetProperties(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& rule = self(L);
    lua_newtable(L);
    auto it = rt.styleProps.find(rule.id());
    if (it != rt.styleProps.end()) for (auto& [k, v] : it->second) { rt.pushValue(L, v); lua_setfield(L, -2, k.c_str()); }
    return 1;
}
static int jsonBlobGet(lua_State* L, const char* prop) { pushJson(L, self(L).get(prop).s); return 1; }
static int StyleRuleGetPropertyTransitions(lua_State* L) { return jsonBlobGet(L, "TransitionsBlob"); }
static int StyleRuleGetDefaultPropertyTransition(lua_State* L) {
    const std::string blob = self(L).get("DefaultTransitionBlob").s;
    if (blob.empty()) lua_pushnil(L); else pushJson(L, blob);
    return 1;
}
static int StyleRuleSetDefaultPropertyTransition(lua_State* L) {
    Instance& rule = self(L);
    rule.setInternal("DefaultTransitionBlob", Value::string(lua_isnoneornil(L, 2) ? "" : toJson(L, 2)));
    return 0;
}
static int StyleRuleSetPropertyTransitions(lua_State* L) {
    Instance& rule = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    rule.setInternal("TransitionsBlob", Value::string(toJson(L, 2)));
    return 0;
}
static int StyleRuleSetPropertyTransition(lua_State* L) {   // (property, transitionParams)
    Instance& rule = self(L);
    const char* name = luaL_checkstring(L, 2);
    pushJson(L, rule.get("TransitionsBlob").s);
    int t = lua_gettop(L);
    if (!lua_istable(L, t)) { lua_pop(L, 1); lua_newtable(L); t = lua_gettop(L); }
    lua_pushvalue(L, 3); lua_setfield(L, t, name);
    rule.setInternal("TransitionsBlob", Value::string(toJson(L, t)));
    return 0;
}
static int StyleQueryGetConditions(lua_State* L) { return jsonBlobGet(L, "ConditionsBlob"); }
static int StyleQueryGetCondition(lua_State* L) {
    Instance& q = self(L);
    const char* name = luaL_checkstring(L, 2);
    pushJson(L, q.get("ConditionsBlob").s);
    if (lua_istable(L, -1)) lua_getfield(L, -1, name); else lua_pushnil(L);
    return 1;
}
static int StyleQuerySetConditions(lua_State* L) {
    Instance& q = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    q.setInternal("ConditionsBlob", Value::string(toJson(L, 2)));
    return 0;
}
static int StyleQuerySetCondition(lua_State* L) {
    Instance& q = self(L);
    const char* name = luaL_checkstring(L, 2);
    pushJson(L, q.get("ConditionsBlob").s);
    int t = lua_gettop(L);
    if (!lua_istable(L, t)) { lua_pop(L, 1); lua_newtable(L); t = lua_gettop(L); }
    lua_pushvalue(L, 3); lua_setfield(L, t, name);
    q.setInternal("ConditionsBlob", Value::string(toJson(L, t)));
    return 0;
}

// Video: no decoder here. LoadAsync answers Failure and fires PlayFailed with it; Play / Pause /
// Unload leave IsPlaying and IsLoaded false. The pins are a player's Output and a display's Input.
static int VideoLoadAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& v = self(L);
    const EnumDef* e = findEnum("AssetFetchStatus");
    if (Signal* s = rt.findSignal(v, "PlayFailed")) if (s->hasConns()) rt.fireValues(v, "PlayFailed", {Value::enumItem("AssetFetchStatus.Failure", 1)});
    pushEnumItem(L, e, e->find("Failure"));
    return 1;
}
static int VideoPins(lua_State* L, bool output) {
    Instance& v = self(L);
    lua_newtable(L);
    const char* pin = v.isA("VideoPlayer") ? (output ? "Output" : nullptr) : (output ? nullptr : "Input");
    if (pin) { lua_pushstring(L, pin); lua_rawseti(L, -2, 1); }
    return 1;
}
static int VideoGetInputPins(lua_State* L) { return VideoPins(L, false); }
static int VideoGetOutputPins(lua_State* L) { return VideoPins(L, true); }

// Captures decline: no screenshot is taken, so CaptureScreenshot never calls back, the gallery
// reads empty, the share prompt is denied and the permission refused.
static int CapturePromptSave(lua_State* L) {   // (captures, resultCallback)
    (void)self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    if (lua_isfunction(L, 3)) { lua_pushvalue(L, 3); lua_newtable(L); lua_call(L, 1, 0); }
    return 0;
}
static int CapturePromptShare(lua_State* L) {   // (content, launchData, onAccepted, onDenied)
    (void)self(L);
    if (lua_isfunction(L, 5)) { lua_pushvalue(L, 5); lua_call(L, 0, 0); }
    return 0;
}
static int PushEmptyString(lua_State* L) { (void)self(L); lua_pushstring(L, ""); return 1; }

// ConfigService: a snapshot of the testing values (SetTestingValue / ClearTestingValue), kept as
// JSON. GetValue reads one; nothing here refreshes, so Refresh does nothing and UpdateAvailable
// never fires. GetValueChangedSignal is a signal that never fires.
static int ConfigSetTestingValue(lua_State* L) {
    Instance& s = self(L);
    const char* key = luaL_checkstring(L, 2);
    pushJson(L, s.get("TestingValues").s);
    int t = lua_gettop(L);
    if (!lua_istable(L, t)) { lua_pop(L, 1); lua_newtable(L); t = lua_gettop(L); }
    lua_pushvalue(L, 3); lua_setfield(L, t, key);
    s.setInternal("TestingValues", Value::string(toJson(L, t)));
    return 0;
}
static int ConfigClearTestingValue(lua_State* L) {
    Instance& s = self(L);
    const char* key = luaL_checkstring(L, 2);
    pushJson(L, s.get("TestingValues").s);
    int t = lua_gettop(L);
    if (!lua_istable(L, t)) return 0;
    lua_pushnil(L); lua_setfield(L, t, key);
    s.setInternal("TestingValues", Value::string(toJson(L, t)));
    return 0;
}
static int ConfigGetConfigAsync(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& s = self(L);
    Instance::Ptr snap = rt.dm.createInternal("ConfigSnapshot");
    snap->setInternal("ValuesBlob", Value::string(s.get("TestingValues").s));
    rt.pushInstance(L, snap.get());
    return 1;
}
static int ConfigSnapshotGetValue(lua_State* L) {
    Instance& snap = self(L);
    const char* key = luaL_checkstring(L, 2);
    pushJson(L, snap.get("ValuesBlob").s);
    if (lua_istable(L, -1)) lua_getfield(L, -1, key); else lua_pushnil(L);
    return 1;
}
static int ConfigSnapshotGetValueChangedSignal(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& snap = self(L);
    rt.pushSignal(L, snap, rt.signal(snap, std::string("Config:") + luaL_checkstring(L, 2)));
    return 1;
}

// The legacy Controller keeps its bound buttons (comma-joined Enum.Button names); no such input
// arrives here, so GetButton is false.
static int ControllerBindButton(lua_State* L) {
    Instance& c = self(L);
    const EnumItem* it = checkEnumItem(L, 2, findEnum("Button"));
    luaL_checkstring(L, 3);
    std::string bound = c.get("BoundButtons").s;
    if ((bound + ",").find(std::string(it->name) + ",") == std::string::npos) c.setInternal("BoundButtons", Value::string(bound.empty() ? it->name : bound + "," + it->name));
    return 0;
}
static int ControllerUnbindButton(lua_State* L) {
    Instance& c = self(L);
    const EnumItem* it = checkEnumItem(L, 2, findEnum("Button"));
    std::string out, bound = c.get("BoundButtons").s;
    size_t at = 0;
    while (at <= bound.size()) {
        size_t end = bound.find(',', at);
        if (end == std::string::npos) end = bound.size();
        std::string one = bound.substr(at, end - at);
        if (!one.empty() && one != it->name) out += (out.empty() ? "" : ",") + one;
        at = end + 1;
    }
    c.setInternal("BoundButtons", Value::string(out));
    return 0;
}
static int ControllerGetButton(lua_State* L) { (void)self(L); checkEnumItem(L, 2, findEnum("Button")); lua_pushboolean(L, 0); return 1; }

// AnimationNodeDefinition's input pins, ordered, comma-joined in InputPins.
static std::vector<std::string> splitComma(const std::string& s) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at <= s.size()) {
        size_t end = s.find(',', at);
        if (end == std::string::npos) end = s.size();
        if (end > at) out.push_back(s.substr(at, end - at));
        at = end + 1;
    }
    return out;
}
static void setPins(Runtime::Impl& rt, Instance& n, const std::vector<std::string>& pins) {
    std::string joined;
    for (const std::string& p : pins) joined += (joined.empty() ? "" : ",") + p;
    if (n.get("InputPins").s == joined) return;
    n.setInternal("InputPins", Value::string(joined));
    if (Signal* s = rt.findSignal(n, "InputPinsChanged")) if (s->hasConns()) rt.fireValues(n, "InputPinsChanged", {});
}
static int NodeAddInputPin(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& n = self(L);
    std::string pin = luaL_checkstring(L, 2);
    std::vector<std::string> pins = splitComma(n.get("InputPins").s);
    for (const std::string& p : pins) if (p == pin) return 0;
    pins.push_back(pin);
    setPins(rt, n, pins);
    return 0;
}
static int NodeRemoveInputPin(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& n = self(L);
    std::string pin = luaL_checkstring(L, 2);
    std::vector<std::string> pins = splitComma(n.get("InputPins").s), kept;
    for (const std::string& p : pins) if (p != pin) kept.push_back(p);
    setPins(rt, n, kept);
    return 0;
}
static int NodeGetOrderedInputPinNames(lua_State* L) {
    std::vector<std::string> pins = splitComma(self(L).get("InputPins").s);
    lua_createtable(L, (int)pins.size(), 0);
    for (size_t i = 0; i < pins.size(); i++) { lua_pushstring(L, pins[i].c_str()); lua_rawseti(L, -2, (int)i + 1); }
    return 1;
}
static int NodeSetOrderedInputPinNames(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& n = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<std::string> pins;
    for (int i = 1;; i++) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        pins.push_back(luaL_checkstring(L, -1));
        lua_pop(L, 1);
    }
    setPins(rt, n, pins);
    return 0;
}

// MatchmakingService keeps this server's attributes as JSON; nothing here matches players on them.
// Each answers (true) or (false, message) as Roblox's do.
static int MatchGetServerAttribute(lua_State* L) {
    Instance& s = self(L);
    const char* name = luaL_checkstring(L, 2);
    pushJson(L, s.get("ServerAttributes").s);
    if (!lua_istable(L, -1)) { lua_pushnil(L); return 1; }
    lua_getfield(L, -1, name);
    return 1;
}
static int MatchSetServerAttribute(lua_State* L) {
    Instance& s = self(L);
    const char* name = luaL_checkstring(L, 2);
    pushJson(L, s.get("ServerAttributes").s);
    int t = lua_gettop(L);
    if (!lua_istable(L, t)) { lua_pop(L, 1); lua_newtable(L); t = lua_gettop(L); }
    lua_pushvalue(L, 3); lua_setfield(L, t, name);
    s.setInternal("ServerAttributes", Value::string(toJson(L, t)));
    lua_pushboolean(L, 1);
    return 1;
}
static int MatchInitializeServerAttributes(lua_State* L) {
    Instance& s = self(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    s.setInternal("ServerAttributes", Value::string(toJson(L, 2)));
    lua_pushboolean(L, 1);
    return 1;
}
static int GamepadCursor(lua_State* L, bool on) { self(L).setInternal("GamepadCursorEnabled", Value::boolean(on)); return 0; }
static int EnableGamepadCursor(lua_State* L) { rtOf(L).checkInstance(L, 2); return GamepadCursor(L, true); }
static int DisableGamepadCursor(lua_State* L) { return GamepadCursor(L, false); }
static int GeneratedSetPrimaryPart(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& f = self(L);
    Instance& part = rt.checkInstance(L, 2);
    if (!part.isA("BasePart")) luaL_error(L, "SetPrimaryPart: a BasePart is expected");
    f.setInternal("GeneratedPrimaryPart", Value::instance(part.id()));
    return 0;
}
// PlayerViewService:GetDeviceCameraCFrame: the current camera's frame, as no device camera is here.
static int GetDeviceCameraCFrame(lua_State* L) {
    Runtime::Impl& rt = rtOf(L); (void)self(L);
    Instance* cam = rt.dm.findRef(rt.dm.workspace()->get("CurrentCamera").ref);
    pushCFrame(L, cam ? partCFrame(*cam) : CFrameV::identity());
    return 1;
}
static int GetMaxQualityLevel(lua_State* L) { (void)self(L); lua_pushinteger(L, 21); return 1; }
} // namespace m
void inputActionsKey(Runtime::Impl& rt, int keyCode, bool down) { m::inputActionsKeyImpl(rt, keyCode, down); }
// Where the pointer's ray lands, for Humanoid.TargetPoint on a tool click (Runtime::input).
bool mouseHit(Runtime::Impl& rt, Vec3& out) {
    if (!rt.mouse) return false;
    RayV ray; RayHit hit;
    if (!m::mouseRay(rt, *rt.mouse, ray, hit)) return false;
    out = hit.position;
    return true;
}
// The KeyframeSequence an Animation names, for a track's markers (Runtime::fireEvent).
Instance* animationSequenceOf(Runtime::Impl& rt, Instance* anim) { return m::animationSequence(rt, anim); }

struct MethodDef { const char* cls; lua_CFunction fn; };
static const std::unordered_map<std::string, std::vector<MethodDef>>& methods();
std::vector<std::string> methodNamesFor(const ClassDef& cls) {
    std::vector<std::string> out;
    for (auto& [name, defs] : methods())
        for (auto& d : defs) if (cls.isA(d.cls)) { out.push_back(name); break; }
    std::sort(out.begin(), out.end());
    return out;
}
static std::unordered_map<std::string, std::vector<MethodDef>> baseMethods();
// ReflectionService:GetMethodsOfClass, here for methods(): the owner is the chain's nearest class that answers the name.
static int ReflectionGetMethodsOfClass(lua_State* L) {
    (void)self(L);
    const ClassDef* c = findClass(luaL_checkstring(L, 2));
    if (!c) { lua_pushnil(L); return 1; }
    bool inherited = m::reflectionInherited(L, 3);
    lua_newtable(L);
    int n = 0;
    for (const std::string& name : methodNamesFor(*c)) {
        const ClassDef* owner = nullptr;
        auto it = methods().find(name);
        for (const ClassDef* k = c; k && !owner && it != methods().end(); k = k->base)
            for (auto& d : it->second) if (k->name == d.cls) { owner = k; break; }
        if (!owner) owner = c;
        if (!inherited && owner != c) continue;
        lua_createtable(L, 0, 4);
        lua_pushstring(L, name.c_str()); lua_setfield(L, -2, "Name");
        lua_pushstring(L, owner->name.c_str()); lua_setfield(L, -2, "Owner");
        lua_newtable(L); lua_setfield(L, -2, "Parameters");
        lua_pushboolean(L, name.size() > 5 && name.compare(name.size() - 5, 5, "Async") == 0); lua_setfield(L, -2, "CanYield");
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

// The memory stores' four, beside the data stores' entries of the same names.
struct SharedMethod { const char* name; const char* cls; lua_CFunction fn; };
static const SharedMethod kServicesShared[] = {
    {"GetAsync", "MemoryStoreHashMap", m::MemGetAsync}, {"GetAsync", "MemoryStoreSortedMap", m::MemGetAsync},
    {"SetAsync", "MemoryStoreHashMap", m::MemSetAsync}, {"SetAsync", "MemoryStoreSortedMap", m::MemSetAsync},
    {"UpdateAsync", "MemoryStoreHashMap", m::MemUpdateAsync}, {"UpdateAsync", "MemoryStoreSortedMap", m::MemUpdateAsync},
    {"RemoveAsync", "MemoryStoreHashMap", m::MemRemoveAsync}, {"RemoveAsync", "MemoryStoreSortedMap", m::MemRemoveAsync}, {"RemoveAsync", "MemoryStoreQueue", m::MemRemoveAsync},
};
// The rest family's methods whose names other classes answer too.
static const SharedMethod kRestShared[] = {
    {"Play", "HapticEffect", m::Noop}, {"Stop", "HapticEffect", m::Noop},                       // no controller here rumbles
    {"Play", "VideoPlayer", m::Noop}, {"Pause", "VideoPlayer", m::Noop}, {"Unload", "VideoPlayer", m::Noop}, {"LoadAsync", "VideoPlayer", m::VideoLoadAsync},
    {"GetConnectedWires", "VideoPlayer", m::AudioGetConnectedWires}, {"GetConnectedWires", "VideoDisplay", m::AudioGetConnectedWires},
    {"GetInputPins", "VideoPlayer", m::VideoGetInputPins}, {"GetInputPins", "VideoDisplay", m::VideoGetInputPins},
    {"GetOutputPins", "VideoPlayer", m::VideoGetOutputPins}, {"GetOutputPins", "VideoDisplay", m::VideoGetOutputPins},
    {"GetState", "InputAction", m::InputActionGetState}, {"Fire", "InputBinding", m::InputBindingFire},
    {"X", "EulerRotationCurve", m::CurveX}, {"Y", "EulerRotationCurve", m::CurveY}, {"Z", "EulerRotationCurve", m::CurveZ},
    {"X", "Vector3Curve", m::CurveX}, {"Y", "Vector3Curve", m::CurveY}, {"Z", "Vector3Curve", m::CurveZ},
    {"GetValueAtTime", "Vector3Curve", m::CurveGetValueAtTime3},
    {"GetValue", "ConfigSnapshot", m::ConfigSnapshotGetValue}, {"GetButton", "Controller", m::ControllerGetButton},
    {"GetMarkers", "MarkerCurve", m::MarkerCurveGetMarkers}, {"FilterStringAsync", "TextService", m::TextFilterStringAsync},
    {"Disconnect", "MemStorageConnection", m::Noop},   // nothing here to disconnect
};
static const std::unordered_map<std::string, std::vector<MethodDef>>& methods() {
    static const std::unordered_map<std::string, std::vector<MethodDef>> t = [] {
        auto all = baseMethods();
        for (auto& [name, fn] : editableMeshMethods()) all[name].push_back({"EditableMesh", fn});
        for (const SharedMethod& m : kServicesShared) all[m.name].push_back({m.cls, m.fn});
        for (const SharedMethod& m : kRestShared) all[m.name].push_back({m.cls, m.fn});
        return all;
    }();
    return t;
}
static std::unordered_map<std::string, std::vector<MethodDef>> baseMethods() {
    std::unordered_map<std::string, std::vector<MethodDef>> t = {
        {"GetService", {{"DataModel", m::GetService}, {"UserSettings", m::GetService}}},
        {"FindService", {{"DataModel", m::FindService}}},
        {"IsLoaded", {{"DataModel", m::IsLoaded}}},
        {"BindToClose", {{"DataModel", m::BindToClose}, {"PluginGui", m::GuiBindToClose}}},
        // the data family
        {"GetActor", {{"Instance", m::GetActor}}},
        {"GetStyled", {{"Instance", m::GetStyled}}},
        {"GetStyledPropertyChangedSignal", {{"Instance", m::GetStyledPropertyChangedSignal}}},
        {"IsPropertyModified", {{"Instance", m::IsPropertyModified}}},
        {"ResetPropertyToDefault", {{"Instance", m::ResetPropertyToDefault}}},
        {"BindToMessage", {{"Actor", m::ActorBindToMessage}}},
        {"BindToMessageParallel", {{"Actor", m::ActorBindToMessage}}},
        {"SendMessage", {{"Actor", m::ActorSendMessage}}},
        {"GetJobsInfo", {{"DataModel", m::GetJobsInfo}}},
        {"SetPlaceId", {{"DataModel", m::SetPlaceId}}},
        {"SetUniverseId", {{"DataModel", m::SetUniverseId}}},
        {"IsRecordingInProgress", {{"ChangeHistoryService", m::HistoryIsRecording}}},
        {"ResetWaypoints", {{"ChangeHistoryService", m::Noop}}},   // no undo stack here to reset
        {"GetCurrentPlayers", {{"Dialog", m::DialogGetCurrentPlayers}}},
        {"CreateDockWidgetPluginGuiAsync", {{"Plugin", m::PluginCreateDockWidget}}},
        {"CreatePluginMenu", {{"Plugin", m::PluginCreateMenu}}},
        {"GetJoinMode", {{"Plugin", m::PluginGetJoinMode}}},
        {"GetSelectedRibbonTool", {{"Plugin", m::PluginGetSelectedRibbonTool}}},
        {"SelectRibbonTool", {{"Plugin", m::PluginSelectRibbonTool}}},
        {"OpenWikiPage", {{"Plugin", m::Noop}}},
        {"PromptForExistingAssetId", {{"Plugin", m::PushMinusOne}}},
        {"PromptForExistingAssetIdAsync", {{"Plugin", m::PushMinusOne}}},
        {"PromptSaveSelectionAsync", {{"Plugin", m::PushFalse}}},
        {"SaveSelectedToRoblox", {{"Plugin", m::Noop}}},
        {"StartDrag", {{"Plugin", m::Noop}}},
        {"ImportFbxAnimationAsync", {{"Plugin", m::NoFbx}}},
        {"ImportFbxRigAsync", {{"Plugin", m::NoFbx}}},
        {"AddAction", {{"PluginMenu", m::MenuAddAction}}},
        {"AddMenu", {{"PluginMenu", m::MenuAddAction}}},
        {"AddNewAction", {{"PluginMenu", m::MenuAddNewAction}}},
        {"AddSeparator", {{"PluginMenu", m::MenuAddSeparator}}},
        {"ShowAsync", {{"PluginMenu", m::PushNil}}},
        {"GetRelativeMousePosition", {{"PluginGui", m::GuiGetRelativeMousePosition}}},
        {"GetCameraYInvertValue", {{"UserGameSettings", m::PushOne}}},
        {"GetOnboardingCompleted", {{"UserGameSettings", m::GetOnboardingCompleted}}},
        {"SetOnboardingCompleted", {{"UserGameSettings", m::SetOnboardingCompleted}}},
        {"InFullScreen", {{"UserGameSettings", m::PushFalse}}},
        {"InStudioMode", {{"UserGameSettings", m::InStudioMode}}},
        {"SetCameraYInvertVisible", {{"UserGameSettings", m::Noop}}},
        {"SetGamepadCameraSensitivityVisible", {{"UserGameSettings", m::Noop}}},
        {"IsUserFeatureEnabled", {{"UserSettings", m::IsUserFeatureEnabled}}},
        {"Reset", {{"UserSettings", m::SettingsReset}}},
        {"GetClassIcon", {{"StudioService", m::GetClassIcon}}},
        {"GetUserId", {{"StudioService", m::PushZero}}},
        {"PromptImportFileAsync", {{"StudioService", m::PushNil}}},
        {"PromptImportFilesAsync", {{"StudioService", m::PushEmptyTable}}},
        {"FindFirstChild", {{"Instance", m::FindFirstChild}}},
        {"FindFirstChildOfClass", {{"Instance", m::FindFirstChildOfClass}}},
        {"FindFirstChildWhichIsA", {{"Instance", m::FindFirstChildWhichIsA}}},
        {"FindFirstAncestor", {{"Instance", m::FindFirstAncestor}}},
        {"FindFirstAncestorOfClass", {{"Instance", m::FindFirstAncestorOfClass}}},
        {"FindFirstAncestorWhichIsA", {{"Instance", m::FindFirstAncestorWhichIsA}}},
        {"FindFirstDescendant", {{"Instance", m::FindFirstDescendant}}},
        {"WaitForChild", {{"Instance", m::WaitForChild}}},
        {"GetChildren", {{"Instance", m::GetChildren}}},
        {"GetDescendants", {{"Instance", m::GetDescendants}}},
        {"IsA", {{"Object", m::IsA}}},
        {"IsDescendantOf", {{"Instance", m::IsDescendantOf}}},
        {"IsAncestorOf", {{"Instance", m::IsAncestorOf}}},
        {"Destroy", {{"Instance", m::Destroy}, {"EditableImage", m::ImageDestroy}}},
        {"Remove", {{"Selection", m::SelectionRemove}, {"Instance", m::Remove}}},
        {"Clone", {{"Instance", m::Clone}}},
        {"ClearAllChildren", {{"Instance", m::ClearAllChildren}}},
        {"GetFullName", {{"Instance", m::GetFullName}}},
        {"GetAttribute", {{"Instance", m::GetAttribute}}},
        {"SetAttribute", {{"Instance", m::SetAttribute}}},
        {"GetAttributes", {{"Instance", m::GetAttributes}}},
        {"GetAttributeChangedSignal", {{"Instance", m::GetAttributeChangedSignal}}},
        {"GetPropertyChangedSignal", {{"Object", m::GetPropertyChangedSignal}}},
        {"GetDebugId", {{"Instance", m::GetDebugId}}},
        {"AddTag", {{"CollectionService", m::CsAddTag}, {"Instance", m::AddTag}}},
        {"RemoveTag", {{"CollectionService", m::CsRemoveTag}, {"Instance", m::RemoveTag}}},
        {"HasTag", {{"CollectionService", m::CsHasTag}, {"Instance", m::HasTag}}},
        {"GetTags", {{"CollectionService", m::CsGetTags}, {"Instance", m::GetTags}}},
        {"GetTagged", {{"CollectionService", m::GetTagged}}},
        {"GetInstanceAddedSignal", {{"CollectionService", m::GetInstanceAddedSignal}}},
        {"GetInstanceRemovedSignal", {{"CollectionService", m::GetInstanceRemovedSignal}}},
        {"GetPlayers", {{"Players", m::GetPlayers}, {"Team", m::TeamGetPlayers}}},
        {"GetTeams", {{"Teams", m::GetTeams}}},
        {"GetPlayerByUserId", {{"Players", m::GetPlayerByUserId}}},
        {"GetPlayerFromCharacter", {{"Players", m::GetPlayerFromCharacter}}},
        {"Kick", {{"Player", m::Kick}}},
        {"LoadCharacter", {{"Player", m::LoadCharacter}}},
        {"LoadCharacterAsync", {{"Player", m::LoadCharacter}}},
        {"DistanceFromCharacter", {{"Player", m::DistanceFromCharacter}}},
        {"IsServer", {{"RunService", m::IsServer}}},
        {"IsClient", {{"RunService", m::IsClient}}},
        {"IsStudio", {{"RunService", m::IsStudio}}},
        {"IsRunning", {{"RunService", m::IsRunning}}},
        {"IsRunMode", {{"RunService", m::IsRunning}}},
        {"IsEdit", {{"RunService", m::IsEdit}}},
        {"BindToRenderStep", {{"RunService", m::BindToRenderStep}}},
        {"UnbindFromRenderStep", {{"RunService", m::UnbindFromRenderStep}}},
        {"Fire", {{"BindableEvent", m::Fire}}},
        {"Invoke", {{"BindableFunction", m::Invoke}}},
        {"FireServer", {{"RemoteEvent", m::FireServer}}},
        {"FireClient", {{"RemoteEvent", m::FireClient}}},
        {"FireAllClients", {{"RemoteEvent", m::FireAllClients}}},
        {"InvokeServer", {{"RemoteFunction", m::InvokeServer}}},
        {"InvokeClient", {{"RemoteFunction", m::InvokeClient}}},
        {"TakeDamage", {{"Humanoid", m::TakeDamage}}},
        {"EquipTool", {{"Humanoid", m::EquipTool}}},
        {"UnequipTools", {{"Humanoid", m::UnequipTools}}},
        {"Activate", {{"Tool", m::ToolActivate}, {"Plugin", m::PluginActivate}}},
        {"Deactivate", {{"Tool", m::ToolDeactivate}, {"Plugin", m::PluginDeactivate}}},
        {"InputHoldBegin", {{"ProximityPrompt", m::PromptInputHoldBegin}}},
        {"InputHoldEnd", {{"ProximityPrompt", m::PromptInputHoldEnd}}},
        {"GetState", {{"Humanoid", m::GetState}}},
        {"ChangeState", {{"Humanoid", m::ChangeState}}},
        {"AddAccessory", {{"Humanoid", m::AddAccessory}}},
        {"LoadAnimation", {{"Animator", m::LoadAnimation}, {"Humanoid", m::LoadAnimation}}},
        {"GetPlayingAnimationTracks", {{"Animator", m::GetPlayingAnimationTracks}, {"Humanoid", m::GetPlayingAnimationTracks}}},
        {"AdjustSpeed", {{"AnimationTrack", m::AdjustSpeed}}},
        {"AdjustWeight", {{"AnimationTrack", m::AdjustWeight}}},
        {"GetTimeOfKeyframe", {{"AnimationTrack", m::GetTimeOfKeyframe}}},
        {"GetAccessories", {{"Humanoid", m::GetAccessories}, {"HumanoidDescription", m::DescGetAccessories}}},
        {"SetAccessories", {{"HumanoidDescription", m::DescSetAccessories}}},
        {"GetEmotes", {{"HumanoidDescription", m::GetEmotes}}},
        {"SetEmotes", {{"HumanoidDescription", m::SetEmotes}}},
        {"AddEmote", {{"HumanoidDescription", m::AddEmote}}},
        {"RemoveEmote", {{"HumanoidDescription", m::RemoveEmote}}},
        {"GetEquippedEmotes", {{"HumanoidDescription", m::GetEquippedEmotes}}},
        {"SetEquippedEmotes", {{"HumanoidDescription", m::SetEquippedEmotes}}},
        {"GetLimb", {{"Humanoid", m::GetLimb}}},
        {"GetBodyPartR15", {{"Humanoid", m::GetBodyPartR15}}},
        {"ReplaceBodyPartR15", {{"Humanoid", m::ReplaceBodyPartR15}}},
        {"GetMoveVelocity", {{"Humanoid", m::GetMoveVelocity}}},
        {"GetStateEnabled", {{"Humanoid", m::GetStateEnabled}}},
        {"SetStateEnabled", {{"Humanoid", m::SetStateEnabled}}},
        {"PlayEmoteAsync", {{"Humanoid", m::PlayEmoteAsync}}},
        {"BuildRigFromAttachments", {{"Humanoid", m::BuildRigFromAttachments}}},
        {"ApplyDescriptionAsync", {{"Humanoid", m::ApplyDescriptionAsync}}},
        {"ApplyDescriptionResetAsync", {{"Humanoid", m::ApplyDescriptionAsync}}},   // the same: nothing worn survives an apply here
        {"GetAppliedDescription", {{"Humanoid", m::GetAppliedDescription}}},
        {"GetMarkerReachedSignal", {{"AnimationTrack", m::GetMarkerReachedSignal}}},
        {"SetParameter", {{"AnimationTrack", m::SetParameter}}},
        {"GetParameter", {{"AnimationTrack", m::GetParameter}}},
        {"GetParameterDefaults", {{"AnimationTrack", m::GetParameterDefaults}}},
        {"ApplyJointVelocities", {{"Animator", m::ApplyJointVelocities}}},
        {"GetKeyframes", {{"KeyframeSequence", m::GetKeyframes}}},
        {"AddKeyframe", {{"KeyframeSequence", m::AddKeyframe}}},
        {"RemoveKeyframe", {{"KeyframeSequence", m::RemoveKeyframe}}},
        {"GetPoses", {{"Keyframe", m::GetPoses}}},
        {"AddPose", {{"Keyframe", m::AddPose}}},
        {"RemovePose", {{"Keyframe", m::RemovePose}}},
        {"GetMarkers", {{"Keyframe", m::GetMarkers}}},
        {"AddMarker", {{"Keyframe", m::AddMarker}}},
        {"RemoveMarker", {{"Keyframe", m::RemoveMarker}}},
        {"GetSubPoses", {{"Pose", m::GetSubPoses}}},
        {"AddSubPose", {{"Pose", m::AddSubPose}}},
        {"RemoveSubPose", {{"Pose", m::RemoveSubPose}}},
        {"LoadCharacterWithHumanoidDescriptionAsync", {{"Player", m::LoadCharacterWithHumanoidDescriptionAsync}}},
        {"ClearCharacterAppearance", {{"Player", m::ClearCharacterAppearance}}},
        {"ClearCachedAvatarAppearance", {{"Player", m::ClearCachedAvatarAppearance}}},
        {"HasAppearanceLoaded", {{"Player", m::HasAppearanceLoaded}}},
        {"GetFriendsOnlineAsync", {{"Player", m::GetFriendsOnlineAsync}}},
        {"GetFriendsWhoPlayedAsync", {{"Player", m::GetFriendsWhoPlayedAsync}}},
        {"IsFriendsWithAsync", {{"Player", m::IsFriendsWithAsync}}},
        {"IsInGroupAsync", {{"Player", m::IsInGroupAsync}}},
        {"IsVerified", {{"Player", m::IsVerified}}},
        {"GetJoinData", {{"Player", m::GetJoinData}}},
        {"GetNetworkPing", {{"Player", m::GetNetworkPing}}},
        {"SetAccountAge", {{"Player", m::SetAccountAge}}},
        {"AddReplicationFocus", {{"Player", m::AddReplicationFocus}}},
        {"RemoveReplicationFocus", {{"Player", m::RemoveReplicationFocus}}},
        {"RequestStreamAroundAsync", {{"Player", m::RequestStreamAroundAsync}}},
        {"GetNameFromUserIdAsync", {{"Players", m::GetNameFromUserIdAsync}}},
        {"GetUserIdFromNameAsync", {{"Players", m::GetUserIdFromNameAsync}}},
        {"SetChatStyle", {{"Players", m::SetChatStyle}}},
        {"RegisterComputerCameraMovementMode", {{"PlayerScripts", m::RegisterMovementMode}}},
        {"RegisterComputerMovementMode", {{"PlayerScripts", m::RegisterMovementMode}}},
        {"RegisterTouchCameraMovementMode", {{"PlayerScripts", m::RegisterMovementMode}}},
        {"RegisterTouchMovementMode", {{"PlayerScripts", m::RegisterMovementMode}}},
        {"ClearComputerCameraMovementModes", {{"PlayerScripts", m::ClearMovementModes}}},
        {"ClearComputerMovementModes", {{"PlayerScripts", m::ClearMovementModes}}},
        {"ClearTouchCameraMovementModes", {{"PlayerScripts", m::ClearMovementModes}}},
        {"ClearTouchMovementModes", {{"PlayerScripts", m::ClearMovementModes}}},
        {"RemoveAccessories", {{"Humanoid", m::RemoveAccessories}}},
        {"MoveTo", {{"Humanoid", m::MoveTo}, {"Model", m::MoveTo}}},
        {"Move", {{"Humanoid", m::Move}}},
        {"GetPivot", {{"Model", m::GetPivot}, {"BasePart", m::GetPivot}}},
        {"PivotTo", {{"Model", m::PivotTo}, {"BasePart", m::PivotTo}}},
        {"ScaleTo", {{"Model", m::ScaleTo}}},
        {"GetScale", {{"Model", m::GetScale}}},
        {"GetPrimaryPartCFrame", {{"Model", m::GetPivot}}},
        {"SetPrimaryPartCFrame", {{"Model", m::PivotTo}}},
        {"GetBoundingBox", {{"Model", m::GetBoundingBox}}},
        {"GetMass", {{"BasePart", m::GetMass}}},
        {"UnionAsync", {{"BasePart", m::UnionAsync}, {"GeometryService", m::GeometryUnionAsync}}},
        {"SubtractAsync", {{"BasePart", m::SubtractAsync}, {"GeometryService", m::GeometrySubtractAsync}}},
        {"IntersectAsync", {{"BasePart", m::IntersectAsync}, {"GeometryService", m::GeometryIntersectAsync}}},
        {"SubstituteGeometry", {{"PartOperation", m::SubstituteGeometry}}},
        {"ApplyMesh", {{"MeshPart", m::ApplyMesh}}},
        {"CalculateConstraintsToPreserve", {{"GeometryService", m::CalculateConstraintsToPreserve}}},
        {"GetConnectedParts", {{"BasePart", m::GetConnectedParts}}},
        {"GetRootPart", {{"BasePart", m::GetRootPart}}},
        {"ApplyImpulse", {{"BasePart", m::ApplyImpulse}}},
        {"ApplyImpulseAtPosition", {{"BasePart", m::ApplyImpulseAtPosition}}},
        {"ApplyAngularImpulse", {{"BasePart", m::ApplyAngularImpulse}}},
        {"FillBlock", {{"Terrain", m::FillBlock}}},
        {"FillBall", {{"Terrain", m::FillBall}}},
        {"Clear", {{"Terrain", m::ClearTerrain}, {"ParticleEmitter", m::ClearParticles}, {"WireframeHandleAdornment", m::ClearWireframe}, {"Trail", m::ClearTrail}, {"AudioRecorder", m::RecorderClear}, {"PluginMenu", m::MenuClear}}},
        {"SetTextureOffset", {{"Beam", m::SetTextureOffset}}},
        {"GetMinutesAfterMidnight", {{"Lighting", m::GetMinutesAfterMidnight}}},
        {"SetMinutesAfterMidnight", {{"Lighting", m::SetMinutesAfterMidnight}}},
        {"GetSunDirection", {{"Lighting", m::GetSunDirection}}},
        {"GetMoonDirection", {{"Lighting", m::GetMoonDirection}}},
        {"GetBaseMaterialOverride", {{"MaterialService", m::GetBaseMaterialOverride}}},
        {"SetBaseMaterialOverride", {{"MaterialService", m::SetBaseMaterialOverride}}},
        {"GetMaterialVariant", {{"MaterialService", m::GetMaterialVariant}}},   // one entry per name: a second is dropped by the map
        {"BreakJoints", {{"BasePart", m::Noop}, {"Model", m::Noop}}},
        {"MakeJoints", {{"BasePart", m::Noop}, {"Model", m::Noop}}},
        {"SetNetworkOwner", {{"BasePart", m::Noop}}},
        // the parts family
        {"GetConstraints", {{"Attachment", m::GetConstraints}}},
        {"GetJoints", {{"BasePart", m::GetJoints}}},
        {"GetNoCollisionConstraints", {{"BasePart", m::GetNoCollisionConstraints}}},
        {"CanCollideWith", {{"BasePart", m::CanCollideWith}}},
        {"CanSetNetworkOwnership", {{"BasePart", m::CanSetNetworkOwnership}}},
        {"GetNetworkOwner", {{"BasePart", m::GetNetworkOwner}}},
        {"GetNetworkOwnershipAuto", {{"BasePart", m::GetNetworkOwnershipAuto}}},
        {"SetNetworkOwnershipAuto", {{"BasePart", m::SetNetworkOwnershipAuto}}},
        {"GetClosestPointOnSurface", {{"BasePart", m::GetClosestPointOnSurface}}},
        {"GetVelocityAtPosition", {{"BasePart", m::GetVelocityAtPosition}}},
        {"IsGrounded", {{"BasePart", m::IsGrounded}}},
        {"AngularAccelerationToTorque", {{"BasePart", m::AngularAccelerationToTorque}}},
        {"TorqueToAngularAcceleration", {{"BasePart", m::TorqueToAngularAcceleration}}},
        {"Resize", {{"BasePart", m::Resize}}},
        {"GetPartsObscuringTarget", {{"Camera", m::GetPartsObscuringTarget}}},
        {"GetExtentsSize", {{"Model", m::GetExtentsSize}}},
        {"TranslateBy", {{"Model", m::TranslateBy}}},
        {"AddPersistentPlayer", {{"Model", m::AddPersistentPlayer}}},
        {"RemovePersistentPlayer", {{"Model", m::RemovePersistentPlayer}}},
        {"GetPersistentPlayers", {{"Model", m::GetPersistentPlayers}}},
        {"SetDesiredAngle", {{"Motor", m::SetDesiredAngle}, {"Motor6D", m::SetDesiredAngle}}},
        {"ArePartsTouchingOthers", {{"WorldRoot", m::ArePartsTouchingOthers}}},
        {"BulkMoveTo", {{"WorldRoot", m::BulkMoveTo}}},
        {"GetNumAwakeParts", {{"Workspace", m::GetNumAwakeParts}}},
        {"GetPhysicsThrottling", {{"Workspace", m::GetPhysicsThrottling}}},
        {"GetRealPhysicsFPS", {{"Workspace", m::GetRealPhysicsFPS}}},
        {"PGSIsEnabled", {{"Workspace", m::PGSIsEnabled}}},
        {"JoinToOutsiders", {{"Workspace", m::Noop}}},       // no surface joints are made here (as MakeJoints)
        {"UnjoinFromOutsiders", {{"Workspace", m::Noop}}},
        {"CellCenterToWorld", {{"Terrain", m::CellCenterToWorld}}},
        {"CellCornerToWorld", {{"Terrain", m::CellCornerToWorld}}},
        {"WorldToCell", {{"Terrain", m::WorldToCell}}},
        {"WorldToCellPreferEmpty", {{"Terrain", m::WorldToCell}}},
        {"WorldToCellPreferSolid", {{"Terrain", m::WorldToCell}}},
        {"CountCells", {{"Terrain", m::CountCells}}},
        {"FillRegion", {{"Terrain", m::FillRegion}}},
        {"FillCylinder", {{"Terrain", m::FillCylinder}}},
        {"FillWedge", {{"Terrain", m::FillWedge}}},
        {"ReplaceMaterial", {{"Terrain", m::ReplaceMaterial}}},
        {"ReadVoxels", {{"Terrain", m::ReadVoxels}}},
        {"WriteVoxels", {{"Terrain", m::WriteVoxels}}},
        {"ReadVoxelChannels", {{"Terrain", m::ReadVoxelChannels}}},
        {"WriteVoxelChannels", {{"Terrain", m::WriteVoxelChannels}}},
        {"GetMaterialColor", {{"Terrain", m::GetMaterialColor}}},
        {"SetMaterialColor", {{"Terrain", m::SetMaterialColor}}},
        {"Create", {{"TweenService", m::TweenCreate}}},
        {"CreatePath", {{"PathfindingService", m::PathCreate}}},
        {"FindPathAsync", {{"PathfindingService", m::PathFind}}},
        {"ComputeAsync", {{"Path", m::PathComputeAsync}}},
        {"GetWaypoints", {{"Path", m::PathGetWaypoints}}},
        {"CheckOccupancyAsync", {{"Path", m::PathCheckOccupancy}}},
        {"GetValue", {{"TweenService", m::TweenGetValue}, {"StatsItem", m::StatsItemGetValue}}},
        {"Play", {{"Tween", m::TweenPlay}, {"Sound", m::SoundPlay}, {"AnimationTrack", m::TrackPlay}, {"AudioPlayer", m::AudioPlayerPlay}, {"VideoFrame", m::VideoPlay}, {"AudioTextToSpeech", m::TtsPlay}}},
        {"Pause", {{"Tween", m::TweenPause}, {"Sound", m::SoundPause}, {"VideoFrame", m::VideoPause}, {"AudioTextToSpeech", m::TtsPause}}},
        {"Stop", {{"Sound", m::SoundStop}, {"AnimationTrack", m::TrackStop}, {"AudioPlayer", m::AudioPlayerStop}, {"AudioRecorder", m::RecorderStop}}},
        {"GetMixerTime", {{"SoundService", m::GetMixerTime}}},
        {"SetListener", {{"SoundService", m::SetListener}}},
        {"GetListener", {{"SoundService", m::GetListener}}},
        {"GetInputPins", {{"AudioPlayer", m::AudioGetInputPins}, {"AudioFilter", m::AudioGetInputPins}, {"AudioReverb", m::AudioGetInputPins},
                          {"AudioEmitter", m::AudioGetInputPins}, {"AudioListener", m::AudioGetInputPins}, {"AudioDeviceOutput", m::AudioGetInputPins},
                          {"AudioFader", m::AudioGetInputPins}, {"AudioEcho", m::AudioGetInputPins}, {"AudioCompressor", m::AudioGetInputPins},
                          {"AudioLimiter", m::AudioGetInputPins}, {"AudioDistortion", m::AudioGetInputPins}, {"AudioEqualizer", m::AudioGetInputPins},
                          {"AudioChorus", m::AudioGetInputPins}, {"AudioFlanger", m::AudioGetInputPins}, {"AudioGate", m::AudioGetInputPins},
                          {"AudioPitchShifter", m::AudioGetInputPins}, {"AudioTremolo", m::AudioGetInputPins}, {"AudioAnalyzer", m::AudioGetInputPins},
                          {"AudioChannelMixer", m::AudioGetInputPins}, {"AudioChannelSplitter", m::AudioGetInputPins}, {"AudioDeviceInput", m::AudioGetInputPins},
                          {"AudioRecorder", m::AudioGetInputPins}, {"AudioSpeechToText", m::AudioGetInputPins}, {"AudioTextToSpeech", m::AudioGetInputPins}}},
        {"GetOutputPins", {{"AudioPlayer", m::AudioGetOutputPins}, {"AudioFilter", m::AudioGetOutputPins}, {"AudioReverb", m::AudioGetOutputPins},
                           {"AudioEmitter", m::AudioGetOutputPins}, {"AudioListener", m::AudioGetOutputPins}, {"AudioDeviceOutput", m::AudioGetOutputPins},
                           {"AudioFader", m::AudioGetOutputPins}, {"AudioEcho", m::AudioGetOutputPins}, {"AudioCompressor", m::AudioGetOutputPins},
                           {"AudioLimiter", m::AudioGetOutputPins}, {"AudioDistortion", m::AudioGetOutputPins}, {"AudioEqualizer", m::AudioGetOutputPins},
                           {"AudioChorus", m::AudioGetOutputPins}, {"AudioFlanger", m::AudioGetOutputPins}, {"AudioGate", m::AudioGetOutputPins},
                           {"AudioPitchShifter", m::AudioGetOutputPins}, {"AudioTremolo", m::AudioGetOutputPins}, {"AudioAnalyzer", m::AudioGetOutputPins},
                           {"AudioChannelMixer", m::AudioGetOutputPins}, {"AudioChannelSplitter", m::AudioGetOutputPins}, {"AudioDeviceInput", m::AudioGetOutputPins},
                           {"AudioRecorder", m::AudioGetOutputPins}, {"AudioSpeechToText", m::AudioGetOutputPins}, {"AudioTextToSpeech", m::AudioGetOutputPins}}},
        {"GetConnectedWires", {{"AudioPlayer", m::AudioGetConnectedWires}, {"AudioFilter", m::AudioGetConnectedWires}, {"AudioReverb", m::AudioGetConnectedWires},
                               {"AudioEmitter", m::AudioGetConnectedWires}, {"AudioListener", m::AudioGetConnectedWires}, {"AudioDeviceOutput", m::AudioGetConnectedWires},
                               {"AudioFader", m::AudioGetConnectedWires}, {"AudioEcho", m::AudioGetConnectedWires}, {"AudioCompressor", m::AudioGetConnectedWires},
                               {"AudioLimiter", m::AudioGetConnectedWires}, {"AudioDistortion", m::AudioGetConnectedWires}, {"AudioEqualizer", m::AudioGetConnectedWires},
                               {"AudioChorus", m::AudioGetConnectedWires}, {"AudioFlanger", m::AudioGetConnectedWires}, {"AudioGate", m::AudioGetConnectedWires},
                               {"AudioPitchShifter", m::AudioGetConnectedWires}, {"AudioTremolo", m::AudioGetConnectedWires}, {"AudioAnalyzer", m::AudioGetConnectedWires},
                               {"AudioChannelMixer", m::AudioGetConnectedWires}, {"AudioChannelSplitter", m::AudioGetConnectedWires}, {"AudioDeviceInput", m::AudioGetConnectedWires},
                               {"AudioRecorder", m::AudioGetConnectedWires}, {"AudioSpeechToText", m::AudioGetConnectedWires}, {"AudioTextToSpeech", m::AudioGetConnectedWires}}},
        {"GetWaveformAsync", {{"AudioPlayer", m::AudioGetWaveformAsync}, {"AudioTextToSpeech", m::AudioGetWaveformAsync}}},
        {"GetSpectrum", {{"AudioAnalyzer", m::AudioGetSpectrum}}},
        {"LoadAsync", {{"AudioTextToSpeech", m::TtsLoadAsync}}},
        {"Unload", {{"AudioTextToSpeech", m::TtsUnload}}},
        {"GetUserIdAccessList", {{"AudioDeviceInput", m::AudioGetUserIdAccessList}}},
        {"SetUserIdAccessList", {{"AudioDeviceInput", m::AudioSetUserIdAccessList}}},
        {"CanRecordAsync", {{"AudioRecorder", m::RecorderCanRecordAsync}}},
        {"RecordAsync", {{"AudioRecorder", m::RecorderRecordAsync}}},
        {"GetTemporaryContent", {{"AudioRecorder", m::RecorderGetTemporaryContent}}},
        {"GetUnrecordableInstancesAsync", {{"AudioRecorder", m::RecorderGetUnrecordableInstancesAsync}}},
        {"GetGainAt", {{"AudioFilter", m::AudioFilterGetGainAt}}},
        {"SetDistanceAttenuation", {{"AudioEmitter", m::AudioSetDistanceAttenuation}, {"AudioListener", m::AudioSetDistanceAttenuation}}},
        {"GetDistanceAttenuation", {{"AudioEmitter", m::AudioGetDistanceAttenuation}, {"AudioListener", m::AudioGetDistanceAttenuation}}},
        {"SetAngleAttenuation", {{"AudioEmitter", m::AudioSetAngleAttenuation}, {"AudioListener", m::AudioSetAngleAttenuation}}},
        {"GetAngleAttenuation", {{"AudioEmitter", m::AudioGetAngleAttenuation}, {"AudioListener", m::AudioGetAngleAttenuation}}},
        {"GetAudibilityFor", {{"AudioEmitter", m::AudioGetAudibilityFor}, {"AudioListener", m::AudioGetAudibilityFor}}},
        {"GetInteractingListeners", {{"AudioEmitter", m::AudioGetInteractingListeners}}},
        {"GetInteractingEmitters", {{"AudioListener", m::AudioGetInteractingEmitters}}},
        {"Emit", {{"ParticleEmitter", m::EmitParticles}}},
        {"Sit", {{"Seat", m::SeatSit}, {"VehicleSeat", m::SeatSit}}},
        {"Resume", {{"Sound", m::SoundResume}}},
        {"PlayLocalSound", {{"SoundService", m::PlayLocalSound}}},
        {"Cancel", {{"Tween", m::TweenCancel}, {"AudioPlayer", m::AudioPlayerCancel}}},
        {"AddItem", {{"Debris", m::AddItem}}},
        {"JSONEncode", {{"HttpService", m::JSONEncode}}},
        {"JSONDecode", {{"HttpService", m::JSONDecode}}},
        {"GenerateGUID", {{"HttpService", m::GenerateGUID}}},
        {"GetAsync", {{"HttpService", m::HttpGetAsync}, {"GlobalDataStore", m::DsGetAsync}}},
        {"GetDataStore", {{"DataStoreService", m::GetDataStore}}},
        {"GetOrderedDataStore", {{"DataStoreService", m::GetOrderedDataStore}}},
        {"GetSortedAsync", {{"OrderedDataStore", m::GetSortedAsync}}},
        {"GetCurrentPage", {{"Pages", m::GetCurrentPage}}},
        {"AdvanceToNextPageAsync", {{"Pages", m::AdvanceToNextPageAsync}}},
        {"ListKeysAsync", {{"GlobalDataStore", m::DsListKeysAsync}}},
        {"ListVersionsAsync", {{"GlobalDataStore", m::DsListVersionsAsync}}},
        {"GetVersionAsync", {{"GlobalDataStore", m::DsGetVersionAsync}}},
        {"RemoveVersionAsync", {{"GlobalDataStore", m::DsRemoveVersionAsync}}},
        {"ListDataStoresAsync", {{"DataStoreService", m::ListDataStoresAsync}}},
        {"GetRequestBudgetForRequestType", {{"DataStoreService", m::GetRequestBudgetForRequestType}}},
        {"SetAsync", {{"GlobalDataStore", m::DsSetAsync}}},
        {"UpdateAsync", {{"GlobalDataStore", m::DsUpdateAsync}}},
        {"IncrementAsync", {{"GlobalDataStore", m::DsIncrementAsync}}},
        {"RemoveAsync", {{"GlobalDataStore", m::DsRemoveAsync}}},
        {"PostAsync", {{"HttpService", m::HttpPostAsync}}},
        {"RequestAsync", {{"HttpService", m::HttpRequestAsync}}},
        {"IsKeyDown", {{"UserInputService", m::IsKeyDown}}},
        {"IsMouseButtonPressed", {{"UserInputService", m::IsMouseButtonPressed}}},
        {"GetMouseLocation", {{"UserInputService", m::GetMouseLocation}}},
        {"GetFocusedTextBox", {{"UserInputService", m::GetFocusedTextBox}}},
        {"CaptureFocus", {{"TextBox", m::CaptureFocus}}},
        {"ReleaseFocus", {{"TextBox", m::ReleaseFocus}}},
        {"IsFocused", {{"TextBox", m::IsFocused}}},
        {"Raycast", {{"Workspace", m::Raycast}}},
        {"GetServerTimeNow", {{"Workspace", m::GetServerTimeNow}}},
        {"Blockcast", {{"Workspace", m::Blockcast}}},
        {"Spherecast", {{"Workspace", m::Spherecast}}},
        {"Shapecast", {{"Workspace", m::Shapecast}}},
        {"GetPartBoundsInBox", {{"Workspace", m::GetPartBoundsInBox}}},
        {"GetPartBoundsInRadius", {{"Workspace", m::GetPartBoundsInRadius}}},
        {"GetPartsInPart", {{"Workspace", m::GetPartsInPart}}},
        {"GetTouchingParts", {{"BasePart", m::GetTouchingParts}}},
        {"FindPartOnRay", {{"Workspace", m::FindPartOnRay}}},
        {"FindPartOnRayWithIgnoreList", {{"Workspace", m::FindPartOnRay}}},
        {"ViewportPointToRay", {{"Camera", m::ViewportPointToRay}}},
        {"ScreenPointToRay", {{"Camera", m::ViewportPointToRay}}},
        {"WorldToViewportPoint", {{"Camera", m::WorldToViewportPoint}}},
        {"WorldToScreenPoint", {{"Camera", m::WorldToViewportPoint}}},
        {"GetRenderCFrame", {{"Camera", m::GetRenderCFrame}}},
        {"GetMouse", {{"Player", m::GetMouse}, {"Plugin", m::PluginGetMouse}}},
        {"CreateToolbar", {{"Plugin", m::PluginCreateToolbar}}},
        {"CreateEditableImage", {{"AssetService", m::CreateEditableImage}}},
        {"CreateMeshPartAsync", {{"AssetService", m::CreateMeshPartAsync}, {"InsertService", m::CreateMeshPartAsync}}},
        {"CreateEditableImageAsync", {{"AssetService", m::CreateEditableImageAsync}}},
        {"CreateEditableMesh", {{"AssetService", m::CreateEditableMesh}}},
        {"CreateEditableMeshAsync", {{"AssetService", m::CreateEditableMeshAsync}}},
        {"CreateDataModelContentAsync", {{"AssetService", m::CreateDataModelContentAsync}}},
        {"PreloadAsync", {{"ContentProvider", m::PreloadAsync}}},
        {"WritePixelsBuffer", {{"EditableImage", m::WritePixelsBuffer}}},
        {"ReadPixelsBuffer", {{"EditableImage", m::ReadPixelsBuffer}}},
        {"DrawRectangle", {{"EditableImage", m::DrawRectangle}}},
        {"DrawLine", {{"EditableImage", m::DrawLine}}},
        {"DrawCircle", {{"EditableImage", m::DrawCircle}}},
        {"DrawImage", {{"EditableImage", m::DrawImage}}},
        {"DrawImageTransformed", {{"EditableImage", m::DrawImageTransformed}}},
        {"DrawImageProjected", {{"EditableImage", m::DrawImageProjected}}},
        {"SampleImageProjected", {{"EditableImage", m::SampleImageProjected}}},
        {"CreateButton", {{"PluginToolbar", m::ToolbarCreateButton}}},
        {"SetActive", {{"PluginToolbarButton", m::ButtonSetActive}}},
        {"IsActivated", {{"Plugin", m::PluginIsActivated}}},
        {"IsActivatedWithExclusiveMouse", {{"Plugin", m::PluginIsActivatedExclusive}}},
        {"GetSetting", {{"Plugin", m::PluginGetSetting}}},
        {"SetSetting", {{"Plugin", m::PluginSetSetting}}},
        {"OpenScript", {{"Plugin", m::PluginOpenScript}}},
        {"CreateDockWidgetPluginGui", {{"Plugin", m::PluginCreateDockWidget}}},
        {"CreatePluginAction", {{"Plugin", m::PluginCreateAction}}},
        {"Get", {{"Selection", m::SelectionGet}}},
        {"Set", {{"Selection", m::SelectionSet}}},
        {"Add", {{"Selection", m::SelectionAdd}}},
        {"SetWaypoint", {{"ChangeHistoryService", m::HistorySetWaypoint}}},
        {"TryBeginRecording", {{"ChangeHistoryService", m::HistoryTryBegin}}},
        {"FinishRecording", {{"ChangeHistoryService", m::HistoryFinish}}},
        {"SetEnabled", {{"ChangeHistoryService", m::Noop}}},
        {"GetCanUndo", {{"ChangeHistoryService", m::PushFalse}}},
        {"GetCanRedo", {{"ChangeHistoryService", m::PushFalse}}},
        {"Undo", {{"ChangeHistoryService", m::Noop}}},
        {"Redo", {{"ChangeHistoryService", m::Noop}}},
        {"GetMouseDelta", {{"UserInputService", m::GetMouseDelta}}},
        {"GetKeysPressed", {{"UserInputService", m::GetKeysPressed}}},
        {"GetLastInputType", {{"UserInputService", m::GetLastInputType}}},
        {"BindAction", {{"ContextActionService", m::BindAction}}},
        {"BindActionAtPriority", {{"ContextActionService", m::BindActionAtPriority}}},
        {"UnbindAction", {{"ContextActionService", m::UnbindAction}}},
        {"UnbindAllActions", {{"ContextActionService", m::UnbindAllActions}}},
        {"GetAllBoundActionInfo", {{"ContextActionService", m::GetAllBoundActionInfo}}},
        {"SetCore", {{"StarterGui", m::SetCore}}},
        {"GetCore", {{"StarterGui", m::GetCore}}},
        {"SendAsync", {{"TextChannel", m::SendAsync}}},
        {"DisplaySystemMessage", {{"TextChannel", m::DisplaySystemMessage}}},
        {"AddUserAsync", {{"TextChannel", m::AddUserAsync}}},
        {"SetDirectChatRequester", {{"TextChannel", m::SetDirectChatRequester}}},
        {"DisplayBubble", {{"TextChatService", m::DisplayBubble}}},
        {"CanUserChatAsync", {{"TextChatService", m::CanUsersChat}}},
        {"CanUsersChatAsync", {{"TextChatService", m::CanUsersChat}}},
        {"Chat", {{"Chat", m::ChatChat}}},
        {"FilterStringAsync", {{"Chat", m::FilterString}}},
        {"FilterStringForBroadcast", {{"Chat", m::FilterString}}},
        {"SetCoreGuiEnabled", {{"StarterGui", m::SetCoreGuiEnabled}}},
        {"GetCoreGuiEnabled", {{"StarterGui", m::GetCoreGuiEnabled}}},
        // the gui family
        {"GetGuiObjectsAtPosition", {{"BasePlayerGui", m::GetGuiObjectsAtPosition}}},
        {"GetGuiInset", {{"GuiService", m::GetGuiInset}}},
        {"Select", {{"GuiService", m::GuiSelect}}},
        {"GetInspectMenuEnabled", {{"GuiService", m::GetInspectMenuEnabled}}},
        {"SetInspectMenuEnabled", {{"GuiService", m::SetInspectMenuEnabled}}},
        {"CloseInspectMenu", {{"GuiService", m::CloseInspectMenu}}},
        {"GetEmotesMenuOpen", {{"GuiService", m::GetEmotesMenuOpen}}},
        {"SetEmotesMenuOpen", {{"GuiService", m::SetEmotesMenuOpen}}},
        {"GetGameplayPausedNotificationEnabled", {{"GuiService", m::GetGameplayPausedNotificationEnabled}}},
        {"SetGameplayPausedNotificationEnabled", {{"GuiService", m::SetGameplayPausedNotificationEnabled}}},
        {"GetConnectedGamepads", {{"UserInputService", m::GetConnectedGamepads}}},
        {"GetNavigationGamepads", {{"UserInputService", m::GetNavigationGamepads}}},
        {"GetSupportedGamepadKeyCodes", {{"UserInputService", m::GetSupportedGamepadKeyCodes}}},
        {"GetGamepadState", {{"UserInputService", m::GetGamepadState}}},
        {"GetGamepadConnected", {{"UserInputService", m::GetGamepadConnected}}},
        {"IsNavigationGamepad", {{"UserInputService", m::IsNavigationGamepad}}},
        {"SetNavigationGamepad", {{"UserInputService", m::SetNavigationGamepad}}},
        {"GamepadSupports", {{"UserInputService", m::GamepadSupports}}},
        {"IsGamepadButtonDown", {{"UserInputService", m::IsGamepadButtonDown}}},
        {"GetDeviceAcceleration", {{"UserInputService", m::GetDeviceAcceleration}}},
        {"GetDeviceGravity", {{"UserInputService", m::GetDeviceGravity}}},
        {"GetDeviceRotation", {{"UserInputService", m::GetDeviceRotation}}},
        {"GetMouseButtonsPressed", {{"UserInputService", m::GetMouseButtonsPressed}}},
        {"GetStringForKeyCode", {{"UserInputService", m::GetStringForKeyCode}}},
        {"IsModifierKeyDown", {{"InputObject", m::IsModifierKeyDown}}},
        {"SetTitle", {{"ContextActionService", m::SetTitle}}},
        {"SetDescription", {{"ContextActionService", m::SetDescription}}},
        {"SetImage", {{"ContextActionService", m::SetImage}}},
        {"SetPosition", {{"ContextActionService", m::SetPosition}}},
        {"GetButton", {{"ContextActionService", m::GetButton}}},
        {"BindActivate", {{"ContextActionService", m::BindActivate}}},
        {"UnbindActivate", {{"ContextActionService", m::UnbindActivate}}},
        {"GetBoundActionInfo", {{"ContextActionService", m::GetBoundActionInfo}}},
        {"GetCurrentLocalToolIcon", {{"ContextActionService", m::GetCurrentLocalToolIcon}}},
        {"GetScrollVelocity", {{"ScrollingFrame", m::GetScrollVelocity}}},
        {"ResetScrollVelocity", {{"ScrollingFrame", m::ResetScrollVelocity}}},
        {"JumpTo", {{"UIPageLayout", m::JumpTo}}},
        {"JumpToIndex", {{"UIPageLayout", m::JumpToIndex}}},
        {"Next", {{"UIPageLayout", m::PageNext}}},
        {"Previous", {{"UIPageLayout", m::PagePrevious}}},
        {"AddLine", {{"WireframeHandleAdornment", m::AddLine}}},
        {"AddLines", {{"WireframeHandleAdornment", m::AddLines}}},
        {"AddPath", {{"WireframeHandleAdornment", m::AddPath}}},
        // the services family
        {"AwardBadgeAsync", {{"BadgeService", m::AwardBadgeAsync}}},
        {"UserHasBadgeAsync", {{"BadgeService", m::UserHasBadgeAsync}}},
        {"CheckUserBadgesAsync", {{"BadgeService", m::CheckUserBadgesAsync}}},
        {"GetBadgeInfoAsync", {{"BadgeService", m::GetBadgeInfoAsync}}},
        {"UrlEncode", {{"HttpService", m::UrlEncode}}},
        {"GetVersionAtTimeAsync", {{"GlobalDataStore", m::DsGetVersionAtTimeAsync}}},
        {"GetGlobalDataStore", {{"DataStoreService", m::GetGlobalDataStore}}},
        {"GetUserIds", {{"DataStoreKeyInfo", m::KeyInfoGetUserIds}}},
        {"GetMetadata", {{"DataStoreKeyInfo", m::KeyInfoGetMetadata}, {"DataStoreSetOptions", m::OptionsGetMetadata}, {"DataStoreIncrementOptions", m::OptionsGetMetadata}}},
        {"SetMetadata", {{"DataStoreSetOptions", m::OptionsSetMetadata}, {"DataStoreIncrementOptions", m::OptionsSetMetadata}}},
        {"SetExperimentalFeatures", {{"DataStoreOptions", m::SetExperimentalFeatures}}},
        {"GetHashMap", {{"MemoryStoreService", m::GetHashMap}}},
        {"GetSortedMap", {{"MemoryStoreService", m::GetSortedMap}}},
        {"GetQueue", {{"MemoryStoreService", m::GetQueue}}},
        {"GetSizeAsync", {{"MemoryStoreSortedMap", m::MemGetSizeAsync}, {"MemoryStoreQueue", m::MemGetSizeAsync}}},
        {"ListItemsAsync", {{"MemoryStoreHashMap", m::MemListItemsAsync}}},
        {"GetRangeAsync", {{"MemoryStoreSortedMap", m::MemGetRangeAsync}}},
        {"AddAsync", {{"MemoryStoreQueue", m::QueueAddAsync}}},
        {"ReadAsync", {{"MemoryStoreQueue", m::QueueReadAsync}}},
        {"PublishAsync", {{"MessagingService", m::PublishAsync}}},
        {"SubscribeAsync", {{"MessagingService", m::SubscribeAsync}}},
        {"GetTotalMemoryUsageMb", {{"Stats", m::GetTotalMemoryUsageMb}}},
        {"GetMemoryUsageMbForTag", {{"Stats", m::GetMemoryUsageMbForTag}}},
        {"GetValueString", {{"StatsItem", m::StatsItemGetValueString}}},
        {"TeleportAsync", {{"TeleportService", m::TeleportAsync}}},
        {"GetArrivingTeleportGui", {{"TeleportService", m::GetArrivingTeleportGui}}},
        {"GetLocalPlayerTeleportData", {{"TeleportService", m::GetLocalPlayerTeleportData}}},
        {"SetTeleportGui", {{"TeleportService", m::SetTeleportGui}}},
        {"GetTeleportSetting", {{"TeleportService", m::GetTeleportSetting}}},
        {"SetTeleportSetting", {{"TeleportService", m::SetTeleportSetting}}},
        {"GetTeleportData", {{"TeleportOptions", m::GetTeleportData}}},
        {"SetTeleportData", {{"TeleportOptions", m::SetTeleportData}}},
        {"SmoothDamp", {{"TweenService", m::SmoothDamp}}},
        {"GetUserCFrame", {{"VRService", m::GetUserCFrame}}},
        {"GetUserCFrameEnabled", {{"VRService", m::GetUserCFrameEnabled}}},
        {"RecenterUserHeadCFrame", {{"VRService", m::RecenterUserHeadCFrame}}},
        {"GetTouchpadMode", {{"VRService", m::GetTouchpadMode}}},
        {"SetTouchpadMode", {{"VRService", m::SetTouchpadMode}}},
        {"RequestNavigation", {{"VRService", m::RequestNavigation}}},
        {"IsVoiceEnabledForUserIdAsync", {{"VoiceChatService", m::IsVoiceEnabledForUserIdAsync}}},
        {"CanUsersDirectChatAsync", {{"TextChatService", m::CanUsersDirectChatAsync}}},
        {"DeriveNewMessageProperties", {{"ChatWindowConfiguration", m::DeriveNewMessageProperties}}},
        {"GetEntries", {{"LocalizationTable", m::GetEntries}}},
        {"SetEntries", {{"LocalizationTable", m::SetEntries}}},
        {"SetEntryValue", {{"LocalizationTable", m::SetEntryValue}}},
        {"SetEntryContext", {{"LocalizationTable", m::SetEntryContext}}},
        {"SetEntryExample", {{"LocalizationTable", m::SetEntryExample}}},
        {"SetEntryKey", {{"LocalizationTable", m::SetEntryKey}}},
        {"SetEntrySource", {{"LocalizationTable", m::SetEntrySource}}},
        {"RemoveEntry", {{"LocalizationTable", m::RemoveEntry}}},
        {"RemoveEntryValue", {{"LocalizationTable", m::RemoveEntryValue}}},
        {"RemoveTargetLocale", {{"LocalizationTable", m::RemoveTargetLocale}}},
        {"GetTranslator", {{"LocalizationTable", m::GetTranslator}}},
        {"GetTranslatorForLocaleAsync", {{"LocalizationService", m::GetTranslatorForLocaleAsync}}},
        {"GetTranslatorForPlayerAsync", {{"LocalizationService", m::GetTranslatorForPlayerAsync}}},
        {"GetTableEntries", {{"LocalizationService", m::GetTableEntries}}},
        {"Translate", {{"Translator", m::Translate}}},
        {"FormatByKey", {{"Translator", m::FormatByKey}}},
        {"GetLogHistory", {{"LogService", m::GetLogHistory}}},
        {"PromptPurchase", {{"MarketplaceService", m::PromptPurchase}}},
        {"PromptGamePassPurchase", {{"MarketplaceService", m::PromptGamePassPurchase}}},
        {"PromptProductPurchase", {{"MarketplaceService", m::PromptProductPurchase}}},
        {"PromptBundlePurchase", {{"MarketplaceService", m::PromptBundlePurchase}}},
        {"PromptSubscriptionPurchase", {{"MarketplaceService", m::PromptSubscriptionPurchase}}},
        {"PlayerOwnsAssetAsync", {{"MarketplaceService", m::PlayerOwnsAssetAsync}}},
        {"PlayerOwnsBundleAsync", {{"MarketplaceService", m::PlayerOwnsBundleAsync}}},
        {"UserOwnsGamePassAsync", {{"MarketplaceService", m::UserOwnsGamePassAsync}}},
        {"GetUserSubscriptionStatusAsync", {{"MarketplaceService", m::GetUserSubscriptionStatusAsync}}},
        {"GetProductInfoAsync", {{"MarketplaceService", m::GetProductInfoAsync}}},
        {"GetDeveloperProductsAsync", {{"MarketplaceService", m::GetDeveloperProductsAsync}}},
        {"CanSendGameInviteAsync", {{"SocialService", m::CanSendGameInviteAsync}}},
        {"CanSendCallInviteAsync", {{"SocialService", m::CanSendCallInviteAsync}}},
        {"PromptGameInvite", {{"SocialService", m::PromptGameInvite}}},
        {"GetPlayersByPartyId", {{"SocialService", m::GetPlayersByPartyId}}},
        {"HideSelfView", {{"SocialService", m::Noop}}},   // no self view is shown here
        {"ShowSelfView", {{"SocialService", m::Noop}}},
        {"GetGroupsAsync", {{"GroupService", m::GetGroupsAsync}}},
        {"GetAlliesAsync", {{"GroupService", m::GetAlliesAsync}}},
        {"GetEnemiesAsync", {{"GroupService", m::GetEnemiesAsync}}},
        {"GetGroupInfoAsync", {{"GroupService", m::GetGroupInfoAsync}}},
        {"GetRolesInGroupAsync", {{"GroupService", m::GetRolesInGroupAsync}}},
        {"GetPolicyInfoForPlayerAsync", {{"PolicyService", m::GetPolicyInfoForPlayerAsync}}},
        {"CanViewBrandProjectAsync", {{"PolicyService", m::CanViewBrandProjectAsync}}},
        {"GetUserInfosByUserIdsAsync", {{"UserService", m::GetUserInfosByUserIdsAsync}}},
        {"CanPromptOptInAsync", {{"ExperienceNotificationService", m::CanPromptOptInAsync}}},
        {"PromptOptIn", {{"ExperienceNotificationService", m::PromptOptIn}}},
        {"IsResimulating", {{"RunService", m::IsResimulating}}},
        {"GetAssetFetchStatus", {{"ContentProvider", m::GetAssetFetchStatus}}},
        {"GetAssetFetchStatusChangedSignal", {{"ContentProvider", m::GetAssetFetchStatusChangedSignal}}},
        {"GetAllTags", {{"CollectionService", m::GetAllTags}}},
        {"Check", {{"TestService", m::TestCheck}}},
        {"Checkpoint", {{"TestService", m::TestCheckpoint}}},
        {"Done", {{"TestService", m::TestDone}}},
        {"Error", {{"TestService", m::TestError}}},
        {"Fail", {{"TestService", m::TestFail}}},
        {"Message", {{"TestService", m::TestMessage}}},
        {"Require", {{"TestService", m::TestRequire}}},
        {"Warn", {{"TestService", m::TestWarn}}},
        {"isFeatureEnabled", {{"TestService", m::isFeatureEnabled}}},
        {"GetAccessoryType", {{"AvatarEditorService", m::GetAccessoryType}}},
        {"LogCustomEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogEconomyEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogFunnelStepEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogJourneyEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogOnboardingFunnelStepEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogProgressionCompleteEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogProgressionEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogProgressionFailEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"LogProgressionStartEvent", {{"AnalyticsService", m::AnalyticsDrop}}},
        {"RemoveDefaultLoadingScreen", {{"ReplicatedFirst", m::Noop}}},   // no loading screen is drawn here
        // the rest family
        {"GetKeyAtIndex", {{"FloatCurve", m::FloatCurveGetKeyAtIndex}, {"RotationCurve", m::RotationCurveGetKeyAtIndex}}},
        {"GetKeyIndicesAtTime", {{"FloatCurve", m::FloatCurveGetKeyIndicesAtTime}, {"RotationCurve", m::RotationCurveGetKeyIndicesAtTime}}},
        {"GetKeys", {{"FloatCurve", m::FloatCurveGetKeys}, {"RotationCurve", m::RotationCurveGetKeys}}},
        {"GetValueAtTime", {{"FloatCurve", m::FloatCurveGetValueAtTime}, {"RotationCurve", m::RotationCurveGetValueAtTime}}},
        {"InsertKey", {{"FloatCurve", m::FloatCurveInsertKey}, {"RotationCurve", m::RotationCurveInsertKey}}},
        {"RemoveKeyAtIndex", {{"FloatCurve", m::FloatCurveRemoveKeyAtIndex}, {"RotationCurve", m::RotationCurveRemoveKeyAtIndex}}},
        {"SetKeys", {{"FloatCurve", m::FloatCurveSetKeys}, {"RotationCurve", m::RotationCurveSetKeys}}},
        {"GetMarkerAtIndex", {{"MarkerCurve", m::MarkerCurveGetMarkerAtIndex}}},
        {"InsertMarkerAtTime", {{"MarkerCurve", m::MarkerCurveInsertMarkerAtTime}}},
        {"RemoveMarkerAtIndex", {{"MarkerCurve", m::MarkerCurveRemoveMarkerAtIndex}}},
        {"GetAnglesAtTime", {{"EulerRotationCurve", m::CurveGetValueAtTime3}}},
        {"GetRotationAtTime", {{"EulerRotationCurve", m::EulerGetRotationAtTime}}},
        {"GetChainCount", {{"IKControl", m::IKGetChainCount}}},
        {"GetChainLength", {{"IKControl", m::IKGetChainLength}}},
        {"GetNodeLocalCFrame", {{"IKControl", m::IKGetNodeLocalCFrame}}},
        {"GetNodeWorldCFrame", {{"IKControl", m::IKGetNodeWorldCFrame}}},
        {"GetRawFinalTarget", {{"IKControl", m::IKGetRawFinalTarget}}},
        {"GetSmoothedFinalTarget", {{"IKControl", m::IKGetRawFinalTarget}}},   // no smoothing here
        {"GetClass", {{"ReflectionService", m::ReflectionGetClass}}},
        {"GetClasses", {{"ReflectionService", m::ReflectionGetClasses}}},
        {"GetPropertiesOfClass", {{"ReflectionService", m::ReflectionGetPropertiesOfClass}}},
        {"GetMethodsOfClass", {{"ReflectionService", ReflectionGetMethodsOfClass}}},
        {"GetEventsOfClass", {{"ReflectionService", m::ReflectionGetEventsOfClass}}},
        {"Base64Encode", {{"EncodingService", m::EncodingBase64Encode}}},
        {"Base64Decode", {{"EncodingService", m::EncodingBase64Decode}}},
        {"GetNonChatStringForBroadcastAsync", {{"TextFilterResult", m::TextFilterResultText}}},
        {"GetNonChatStringForUserAsync", {{"TextFilterResult", m::TextFilterResultText}}},
        {"GetChatForUserAsync", {{"TextFilterResult", m::TextFilterResultText}}},
        {"GetTranslations", {{"TextFilterTranslatedResult", m::TextFilterGetTranslations}}},
        {"GetTranslationForLocale", {{"TextFilterTranslatedResult", m::TextFilterGetTranslationForLocale}}},
        {"GetJointNames", {{"HumanoidRigDescription", m::RigGetJointNames}}},
        {"GetR15JointNames", {{"HumanoidRigDescription", m::RigGetR15JointNames}}},
        {"GetR6JointNames", {{"HumanoidRigDescription", m::RigGetR6JointNames}}},
        {"GetJointFromName", {{"HumanoidRigDescription", m::RigGetJointFromName}}},
        {"GetFingerTip", {{"DigitsRigDescription", m::DigitsGetFingerTip}}},
        {"SetFingerTip", {{"DigitsRigDescription", m::DigitsSetFingerTip}}},
        {"GetFingerControl", {{"DigitsRigDescription", m::DigitsGetFingerControl}}},
        {"SetFingerControl", {{"DigitsRigDescription", m::DigitsSetFingerControl}}},
        {"GetAppliedInstance", {{"AccessoryDescription", m::DescriptionGetAppliedInstance}, {"MakeupDescription", m::DescriptionGetAppliedInstance}}},
        {"SetWaveformKeys", {{"HapticEffect", m::HapticSetWaveformKeys}}},
        {"EvaluateAsync", {{"FluidForceSensor", m::FluidEvaluateAsync}}},
        {"GetStyleRules", {{"StyleBase", m::StyleGetStyleRules}}},
        {"InsertStyleRule", {{"StyleBase", m::StyleInsertStyleRule}}},
        {"SetStyleRules", {{"StyleBase", m::StyleSetStyleRules}}},
        {"GetDerives", {{"StyleSheet", m::StyleGetDerives}}},
        {"SetDerives", {{"StyleSheet", m::StyleSetDerives}}},
        {"GetProperty", {{"StyleRule", m::StyleRuleGetProperty}}},
        {"SetProperty", {{"StyleRule", m::StyleRuleSetProperty}}},
        {"GetProperties", {{"StyleRule", m::StyleRuleGetProperties}}},
        {"SetProperties", {{"StyleRule", m::StyleRuleSetProperties}}},
        {"GetPropertyTransitions", {{"StyleRule", m::StyleRuleGetPropertyTransitions}}},
        {"SetPropertyTransition", {{"StyleRule", m::StyleRuleSetPropertyTransition}}},
        {"SetPropertyTransitions", {{"StyleRule", m::StyleRuleSetPropertyTransitions}}},
        {"GetDefaultPropertyTransition", {{"StyleRule", m::StyleRuleGetDefaultPropertyTransition}}},
        {"SetDefaultPropertyTransition", {{"StyleRule", m::StyleRuleSetDefaultPropertyTransition}}},
        {"GetCondition", {{"StyleQuery", m::StyleQueryGetCondition}}},
        {"GetConditions", {{"StyleQuery", m::StyleQueryGetConditions}}},
        {"SetCondition", {{"StyleQuery", m::StyleQuerySetCondition}}},
        {"SetConditions", {{"StyleQuery", m::StyleQuerySetConditions}}},
        // the captures and the prompts decline
        {"CaptureScreenshot", {{"CaptureService", m::Noop}}},
        {"TakeScreenshotCaptureAsync", {{"CaptureService", m::Noop}}},
        {"StopVideoCapture", {{"CaptureService", m::Noop}}},
        {"PromptSaveCapturesToGallery", {{"CaptureService", m::CapturePromptSave}}},
        {"PromptShareCapture", {{"CaptureService", m::CapturePromptShare}}},
        {"PromptCaptureGalleryPermissionAsync", {{"CaptureService", m::PushFalse}}},
        {"ReadCapturesFromGalleryAsync", {{"CaptureService", m::PushEmptyTable}}},
        {"CanCaptureScreenshot", {{"StudioCaptureService", m::PushFalse}}},
        {"RequestScreenshotPermissionAsync", {{"StudioCaptureService", m::PushFalse}}},
        {"UserEligibleForRealWorldCommerceAsync", {{"CommerceService", m::PushFalse}}},
        {"SetTestingValue", {{"ConfigService", m::ConfigSetTestingValue}}},
        {"ClearTestingValue", {{"ConfigService", m::ConfigClearTestingValue}}},
        {"GetConfigAsync", {{"ConfigService", m::ConfigGetConfigAsync}}},
        {"GetConfigForPlayerAsync", {{"ConfigService", m::ConfigGetConfigAsync}}},
        {"GetValueChangedSignal", {{"ConfigSnapshot", m::ConfigSnapshotGetValueChangedSignal}}},
        {"Refresh", {{"ConfigSnapshot", m::Noop}}},
        {"BindButton", {{"Controller", m::ControllerBindButton}}},
        {"UnbindButton", {{"Controller", m::ControllerUnbindButton}}},
        {"AddInputPin", {{"AnimationNodeDefinition", m::NodeAddInputPin}}},
        {"RemoveInputPin", {{"AnimationNodeDefinition", m::NodeRemoveInputPin}}},
        {"GetOrderedInputPinNames", {{"AnimationNodeDefinition", m::NodeGetOrderedInputPinNames}}},
        {"SetOrderedInputPinNames", {{"AnimationNodeDefinition", m::NodeSetOrderedInputPinNames}}},
        {"GetServerAttribute", {{"MatchmakingService", m::MatchGetServerAttribute}}},
        {"SetServerAttribute", {{"MatchmakingService", m::MatchSetServerAttribute}}},
        {"InitializeServerAttributesForStudio", {{"MatchmakingService", m::MatchInitializeServerAttributes}}},
        {"EnableGamepadCursor", {{"GamepadService", m::EnableGamepadCursor}}},
        {"DisableGamepadCursor", {{"GamepadService", m::DisableGamepadCursor}}},
        {"SetPrimaryPart", {{"GeneratedFolder", m::GeneratedSetPrimaryPart}}},
        {"GetDeviceCameraCFrame", {{"PlayerViewService", m::GetDeviceCameraCFrame}}},
        {"GetMaxQualityLevel", {{"RenderSettings", m::GetMaxQualityLevel}}},
        {"ForceGeneration", {{"ProceduralModel", m::PushFalse}}},          // no generator here
        {"WaitForGenerationAsync", {{"ProceduralModel", m::PushFalse}}},
        {"GetBinaryContents", {{"File", m::PushEmptyString}}},              // no file is ever handed out here
        {"GetTemporaryId", {{"File", m::PushEmptyString}}},
        {"GetLogPath", {{"CustomLog", m::PushEmptyString}}},                // nothing here writes a log
        {"Open", {{"CustomLog", m::Noop}}},
        {"Close", {{"CustomLog", m::Noop}}},
        {"WriteAppend", {{"CustomLog", m::Noop}}},
        {"GetPlayer", {{"NetworkReplicator", m::PushNil}}},                 // no replicator is ever made here
        {"SetOutgoingKBPSLimit", {{"NetworkPeer", m::Noop}}},
    };
    return t;
}

static lua_CFunction findMethod(const Instance& i, const char* name) {
    auto& t = methods();
    auto it = t.find(name);
    if (it == t.end()) return nullptr;
    for (auto& d : it->second) if (i.isA(d.cls)) return d.fn;
    return nullptr;
}

// ---- instance metatable --------------------------------------------------------------
// An Object that is not an Instance has no Name, no Parent and no children to index.
static int object_index(lua_State* L, Runtime::Impl& rt, Instance& o, const char* key) {
    if (!std::strcmp(key, "ClassName") || !std::strcmp(key, "className")) { lua_pushstring(L, o.className().c_str()); return 1; }
    if (const PropDef* d = o.cls().findProp(key)) { rt.pushValue(L, o.get(key), d->enumType); return 1; }
    if (lua_CFunction fn = findMethod(o, key)) { lua_pushcfunction(L, fn, key); return 1; }
    if (o.cls().hasEvent(key)) { rt.pushSignal(L, o, rt.signal(o, key)); return 1; }
    std::string cap = m::legacyName(key);
    if (cap != key) if (lua_CFunction fn = findMethod(o, cap.c_str())) { lua_pushcfunction(L, fn, key); return 1; }
    luaL_error(L, "%s is not a valid member of %s", key, o.className().c_str());
    return 0;
}

static int inst_index(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = rt.checkObject(L, 1);
    if (lua_type(L, 2) != LUA_TSTRING) luaL_error(L, "invalid argument #2 (string expected, got %s)", luaL_typename(L, 2));
    const char* key = lua_tostring(L, 2);
    if (!i.isA("Instance")) return object_index(L, rt, i, key);

    // Properties first (they shadow children, like Roblox).
    if (!std::strcmp(key, "Parent")) { rt.pushInstance(L, i.parent()); return 1; }
    if (!std::strcmp(key, "ClassName")) { lua_pushstring(L, i.className().c_str()); return 1; }
    if (!std::strcmp(key, "Name")) { lua_pushstring(L, i.name().c_str()); return 1; }
    bool partLike = i.isA("BasePart") || i.isA("Camera");
    { std::string cfPos, cfOri; if (m::cframeProp(i, key, cfPos, cfOri)) { pushCFrame(L, m::getCFrameProp(i, cfPos, cfOri)); return 1; } }
    if (i.isA("Attachment")) {
        if (!std::strcmp(key, "WorldCFrame")) { pushCFrame(L, m::attachmentWorld(i)); return 1; }
        if (!std::strcmp(key, "WorldPosition")) { pushVec3(L, m::attachmentWorld(i).p); return 1; }
        if (!std::strcmp(key, "WorldOrientation")) { Vec3 p, o; cframeToPosOrient(m::attachmentWorld(i), p, o); pushVec3(L, o); return 1; }
        if (!std::strcmp(key, "Axis")) { pushVec3(L, m::partCFrame(i).col(0)); return 1; }
        if (!std::strcmp(key, "SecondaryAxis")) { pushVec3(L, m::partCFrame(i).col(1)); return 1; }
        if (!std::strcmp(key, "WorldAxis")) { pushVec3(L, m::attachmentWorld(i).col(0)); return 1; }
        if (!std::strcmp(key, "WorldSecondaryAxis")) { pushVec3(L, m::attachmentWorld(i).col(1)); return 1; }
    }
    if (!std::strcmp(key, "Grip") && i.isA("Tool")) { pushCFrame(L, m::toolGrip(i)); return 1; }
    if (partLike && !std::strcmp(key, "Rotation")) { pushVec3(L, i.get("Orientation").v); return 1; }
    if (partLike && !std::strcmp(key, "BrickColor")) { pushBrickColor(L, brickNearest(i.get("Color").c)); return 1; }
    if (partLike && !std::strcmp(key, "Velocity")) { pushVec3(L, i.get("AssemblyLinearVelocity").v); return 1; }
    if (partLike && !std::strcmp(key, "RotVelocity")) { pushVec3(L, i.get("AssemblyAngularVelocity").v); return 1; }
    if (i.isA("BasePart")) {
        if (!std::strcmp(key, "Mass")) { lua_pushnumber(L, m::massOf(i)); return 1; }
        if (!std::strcmp(key, "AssemblyMass")) { lua_pushnumber(L, m::assemblyMass(m::assemblyOf(rt, i))); return 1; }
        if (!std::strcmp(key, "AssemblyRootPart")) { rt.pushInstance(L, m::assemblyRoot(m::assemblyOf(rt, i))); return 1; }
        if (!std::strcmp(key, "CenterOfMass")) { pushVec3(L, Vec3{0, 0, 0}); return 1; }   // in the part's own frame: a box's middle
        if (!std::strcmp(key, "AssemblyCenterOfMass")) { pushVec3(L, m::assemblyCentre(m::assemblyOf(rt, i), i)); return 1; }
    }
    if (partLike && !std::strcmp(key, "ExtentsSize")) { pushVec3(L, i.get("Size").v); return 1; }
    if (i.isA("BasePart")) {
        if (!std::strcmp(key, "ExtentsCFrame")) { pushCFrame(L, m::partCFrame(i)); return 1; }   // a box's extents sit on its own frame
        // the material's figures, or CustomPhysicalProperties' when the part has them
        if (!std::strcmp(key, "CurrentPhysicalProperties")) {
            MaterialPhysics mp = physicsOf(i.get("Material").s, i.get("CustomPhysicalProperties"));
            pushPhysicalProperties(L, Value::physProps((float)mp.density, (float)mp.friction, (float)mp.elasticity));
            return 1;
        }
    }
    if (i.isA("Bone")) {
        if (!std::strcmp(key, "TransformedCFrame")) { pushCFrame(L, m::boneTransformed(i)); return 1; }
        if (!std::strcmp(key, "TransformedWorldCFrame")) { pushCFrame(L, m::boneTransformedWorld(i)); return 1; }
    }
    if (i.isA("Model") && !std::strcmp(key, "WorldPivot")) { pushCFrame(L, m::pivotOf(rt, i)); return 1; }
    if (i.isA("Workspace") && !std::strcmp(key, "Terrain")) { rt.pushInstance(L, i.findFirstChildOfClass("Terrain")); return 1; }
    if (i.isA("CylindricalConstraint") && !std::strcmp(key, "WorldRotationAxis")) {   // Attachment0's WorldAxis, or the world's X with none
        Instance* a = rt.dm.findRef(i.get("Attachment0").ref);
        pushVec3(L, a && a->isA("Attachment") ? m::attachmentWorld(*a).col(0) : Vec3{1, 0, 0});
        return 1;
    }
    if (i.isA("BaseWrap") && (!std::strcmp(key, "CageOriginWorld") || !std::strcmp(key, "ImportOriginWorld") || (i.isA("WrapLayer") && !std::strcmp(key, "ReferenceOriginWorld")))) {
        std::string base(key, std::strlen(key) - 5);   // the local CFrame, in the parent part's frame
        Instance* p = i.parent();
        CFrameV local = m::getCFrameProp(i, base + "Position", base + "Orientation");
        pushCFrame(L, p && p->isA("BasePart") ? m::partCFrame(*p) * local : local);
        return 1;
    }
    if (i.isA("Camera") && (!std::strcmp(key, "DiagonalFieldOfView") || !std::strcmp(key, "MaxAxisFieldOfView"))) {
        // FieldOfView is the vertical angle; the viewport's aspect gives the diagonal and the wider axis
        Vec2 vs = i.get("ViewportSize").v2();
        double aspect = vs.y > 0 ? vs.x / vs.y : 4.0 / 3.0;
        double halfV = std::tan(i.get("FieldOfView").n * 3.14159265358979323846 / 360.0);
        double t = key[0] == 'D' ? halfV * std::sqrt(1 + aspect * aspect) : halfV * std::max(1.0, aspect);
        lua_pushnumber(L, std::atan(t) * 360.0 / 3.14159265358979323846);
        return 1;
    }
    if (i.isA("Humanoid") && !std::strcmp(key, "RootPart")) { rt.pushInstance(L, i.parent() ? i.parent()->findFirstChild("HumanoidRootPart") : nullptr); return 1; }
    // the data family
    if (i.isA("RayValue") && std::string_view(key) == "Value") { pushRay(L, RayV{i.get("ValueOrigin").v, i.get("ValueDirection").v}); return 1; }
    if (i.isA("DataModel") && (std::string_view(key) == "Workspace" || std::string_view(key) == "RunService")) { rt.pushInstance(L, rt.dm.getService(key)); return 1; }
    if (!std::strcmp(key, "IsInSandbox")) {   // the instance or an ancestor is Sandboxed
        bool in = false;
        for (Instance* p = &i; p && !in; p = p->parent()) in = p->get("Sandboxed").b;
        lua_pushboolean(L, in);
        return 1;
    }
    // the services family
    if (i.isA("Tween") && (std::string_view(key) == "Instance" || std::string_view(key) == "TweenInfo")) {   // the running tween's object and info
        Tween* tw = m::findTween(rt, i);
        if (key[0] == 'I') rt.pushInstance(L, tw ? tw->obj.get() : nullptr); else pushTweenInfo(L, tw ? tw->info : TweenInfoV{});
        return 1;
    }
    if (i.isA("HttpService") && std::string_view(key) == "HttpEnabled") { lua_pushboolean(L, rt.opts.httpEnabled); return 1; }   // the host's option
    if (i.isA("Stats") && (std::string_view(key) == "InstanceCount" || std::string_view(key) == "PrimitivesCount")) {   // counted from the tree
        if (key[0] == 'I') { lua_pushnumber(L, (double)rt.dm.instanceCount()); return 1; }
        int parts = 0;
        for (Instance* d : rt.dm.workspace()->getDescendants()) if (d->isA("BasePart")) parts++;
        lua_pushnumber(L, parts);
        return 1;
    }
    // DebugSettings reads the tree: the instances, the Players' children, one job (this thread), no
    // version string of Roblox's, and the DataModel's id.
    if (i.isA("DebugSettings")) {
        std::string_view k = key;
        if (k == "InstanceCount") { lua_pushnumber(L, (double)rt.dm.instanceCount()); return 1; }
        if (k == "PlayerCount") { int n = 0; for (auto& c : rt.dm.getService("Players")->children()) if (c->isA("Player")) n++; lua_pushnumber(L, n); return 1; }
        if (k == "JobCount") { lua_pushnumber(L, 1); return 1; }
        if (k == "DataModel") { lua_pushnumber(L, (double)rt.dm.root()->id()); return 1; }
    }
    if (i.isA("TaskScheduler") && std::string_view(key) == "ThreadPoolSize") { lua_pushnumber(L, 1); return 1; }   // one VM thread here
    // The first InputBinding child: no device here to prefer one over another.
    if (i.isA("InputAction") && std::string_view(key) == "PreferredBinding") {
        for (auto& ch : i.children()) if (ch->isA("InputBinding")) { rt.pushInstance(L, ch.get()); return 1; }
        lua_pushnil(L);
        return 1;
    }
    if (i.isA("BillboardGui") && !std::strcmp(key, "CurrentDistance")) { lua_pushnumber(L, m::billboardDistance(rt, i)); return 1; }
    // None while IgnoreGuiInset opts out of the topbar gap, else what was written
    if (i.isA("ScreenGui") && !std::strcmp(key, "ScreenInsets")) {
        const EnumDef* e = findEnum("ScreenInsets");
        pushEnumItem(L, e, i.get("IgnoreGuiInset").b ? e->find("None") : e->find(i.get("ScreenInsets").s));
        return 1;
    }
    if (!std::strcmp(key, "LocalizedText") && i.cls().findProp("LocalizedText")) { lua_pushstring(L, i.get("Text").s.c_str()); return 1; }   // nothing here localises
    if (i.isA("Humanoid") && !std::strcmp(key, "FloorMaterial")) {
        // Cast down from the root past the legs (HipHeight, or an R6's 2-stud legs) with half a
        // stud to spare; the character's own parts are not the floor.
        const EnumDef* e = findEnum("Material");
        const EnumItem* item = e->find("Air");
        const std::string& state = i.get("StateName").s;
        Instance* ch = i.parent();
        Instance* root = ch ? ch->findFirstChild("HumanoidRootPart") : nullptr;
        if (state == "Swimming") item = e->find("Water");
        else if (state != "Freefall" && state != "Jumping" && root && root->isA("BasePart")) {
            float hip = (float)i.get("HipHeight").n;
            float reach = root->get("Size").v.y * 0.5f + (hip > 0 ? hip : 2.f) + 0.5f;
            RaycastFilter f; f.ids.push_back(ch->id());
            RayHit h;
            if (raycastTree(rt.dm, root->get("Position").v, Vec3{0, -reach, 0}, &f, h) && h.part)
                if (const EnumItem* mat = e->find(h.part->get("Material").s)) item = mat;
        }
        pushEnumItem(L, e, item);
        return 1;
    }
    if (i.isA("Mouse")) {
        if (!std::strcmp(key, "ViewSizeX") || !std::strcmp(key, "ViewSizeY")) {
            Instance* cam = rt.dm.find(rt.dm.workspace()->get("CurrentCamera").ref);
            Vec2 vs = cam ? cam->get("ViewportSize").v2() : Vec2{0, 0};
            lua_pushnumber(L, key[8] == 'X' ? vs.x : vs.y);
            return 1;
        }
        if (!std::strcmp(key, "Hit") || !std::strcmp(key, "Target") || !std::strcmp(key, "UnitRay") || !std::strcmp(key, "Origin")) {
            RayV ray{{0, 0, 0}, {0, 0, -1}}; RayHit hit;
            bool has = m::mouseRay(rt, i, ray, hit);
            if (!std::strcmp(key, "Target")) rt.pushInstance(L, has ? hit.part : nullptr);
            else if (!std::strcmp(key, "UnitRay")) pushRay(L, ray);
            else if (!std::strcmp(key, "Origin")) pushCFrame(L, CFrameV::fromPos(ray.origin));
            else pushCFrame(L, CFrameV::fromPos(has ? hit.position : add(ray.origin, mul(ray.direction, 1000))));
            return 1;
        }
        if (!std::strcmp(key, "TargetSurface")) {   // the face of Target the ray hit: the one whose normal the hit's is most along
            RayV ray{{0, 0, 0}, {0, 0, -1}}; RayHit hit;
            const EnumDef* e = findEnum("NormalId");
            const char* face = "Top";
            if (m::mouseRay(rt, i, ray, hit) && hit.part) {
                CFrameV cf = m::partCFrame(*hit.part);
                struct Face { const char* name; int axis; float sign; };
                float best = -2;
                for (Face f : {Face{"Right", 0, 1}, Face{"Top", 1, 1}, Face{"Back", 2, 1}, Face{"Left", 0, -1}, Face{"Bottom", 1, -1}, Face{"Front", 2, -1}}) {
                    Vec3 a = cf.col(f.axis);
                    float d = f.sign * (hit.normal.x * a.x + hit.normal.y * a.y + hit.normal.z * a.z);
                    if (d > best) { best = d; face = f.name; }
                }
            }
            pushEnumItem(L, e, e->find(face));
            return 1;
        }
    }
    if (const PropDef* d = i.cls().findProp(key)) {
        // RobloxScriptSecurity / RobloxEngineSecurity: the engine's own, not a plugin's and not
        // the command bar's.
        if (d->flags & NoScriptRead) luaL_error(L, "The current thread cannot read '%s' (lacking capability RobloxScript)", key);
        if (d->flags & PluginRead) {
            if (!rt.hasPluginCapability(L))
                luaL_error(L, "The current thread cannot read '%s' (lacking capability Plugin)", key);
        }
        if (d->flags & Brick) pushBrickColor(L, brickNearest(i.get(key).c)); else rt.pushValue(L, i.get(key), d->enumType);
        return 1;
    }
    if (lua_CFunction fn = findMethod(i, key)) { lua_pushcfunction(L, fn, key); return 1; }
    if (i.cls().hasEvent(key) || (i.isA("Humanoid") && !std::strcmp(key, "MoveToFinished"))) { rt.pushSignal(L, i, rt.signal(i, key)); return 1; }
    if (i.cls().hasCallback(key)) {
        InstBinding& b = rt.binding(i);
        auto it = b.callbacks.find(key);
        if (it == b.callbacks.end()) lua_pushnil(L); else lua_getref(L, it->second.ref);
        return 1;
    }
    if (Instance* c = i.findFirstChild(key)) { rt.pushInstance(L, c); return 1; }
    {
        std::string cap = m::legacyName(key);
        std::string cp, co;
        if (cap != key && (i.cls().findProp(cap) || m::cframeProp(i, cap.c_str(), cp, co) || findMethod(i, cap.c_str()) || i.cls().hasEvent(cap))) {
            lua_pushstring(L, cap.c_str());
            lua_replace(L, 2);
            return inst_index(L);
        }
    }
    luaL_error(L, "%s is not a valid member of %s \"%s\"", key, i.className().c_str(), i.fullName().c_str());
    return 0;
}

static int inst_newindex(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = rt.checkObject(L, 1);
    if (lua_type(L, 2) != LUA_TSTRING) luaL_error(L, "invalid argument #2 (string expected, got %s)", luaL_typename(L, 2));
    const char* key = lua_tostring(L, 2);
    std::string err;
    if (!i.isA("Instance")) {
        const PropDef* d = i.cls().findProp(key);
        if (!d) luaL_error(L, "%s is not a valid member of %s", key, i.className().c_str());
        if (d->flags & ReadOnly) luaL_error(L, "Unable to assign property %s. Property is read only", key);
        Value v;
        if (!rt.toValue(L, 3, d->type, d->enumType, v, err))
            luaL_error(L, "invalid argument #3 to '%s' (%s expected, got %s)", key, Value::typeName(d->type), err.c_str());
        if (!i.set(key, v, &err)) luaL_error(L, "%s", err.c_str());
        return 0;
    }
    if (!std::strcmp(key, "Parent")) {
        Instance* p = nullptr;
        if (!lua_isnil(L, 3)) { p = rt.toInstance(L, 3); if (!p) luaL_error(L, "invalid argument #3 to 'Parent' (Instance expected, got %s)", typeOfName(L, 3)); }
        if (p && rt.inPluginTree(p) && !rt.hasPluginCapability(L))
            luaL_error(L, "The current thread cannot parent to PluginDebugService (lacking capability Plugin)");
        Instance::Ptr keep = i.shared_from_this();
        if (!i.setParent(p, &err)) luaL_error(L, "%s", err.c_str());
        return 0;
    }
    bool partLike = i.isA("BasePart") || i.isA("Camera");
    {
        std::string cfPos, cfOri;
        if (m::cframeProp(i, key, cfPos, cfOri)) {
            if (!isCFrame(L, 3)) luaL_error(L, "invalid argument #3 to '%s' (CFrame expected, got %s)", key, typeOfName(L, 3));
            if (const PropDef* d = i.cls().findProp(cfPos)) {   // the pair's flags are the CFrame's
                if (d->flags & ReadOnly) luaL_error(L, "Unable to assign property %s. Property is read only", key);
                if ((d->flags & PluginWrite) && !rt.hasPluginCapability(L)) luaL_error(L, "The current thread cannot set '%s' (lacking capability Plugin)", key);
            }
            m::setCFrameProp(i, cfPos, cfOri, checkCFrame(L, 3));
            return 0;
        }
    }
    if (i.isA("Attachment") && (!std::strcmp(key, "WorldCFrame") || !std::strcmp(key, "WorldPosition"))) {
        // set in the world, kept relative to the part
        Instance* p = i.parent();
        CFrameV partCf = p && p->isA("BasePart") ? m::partCFrame(*p) : CFrameV::identity();
        if (!std::strcmp(key, "WorldCFrame")) {
            if (!isCFrame(L, 3)) luaL_error(L, "invalid argument #3 to 'WorldCFrame' (CFrame expected, got %s)", typeOfName(L, 3));
            m::setPartCFrame(i, partCf.inverse() * checkCFrame(L, 3));
        } else {
            CFrameV own = m::partCFrame(i);
            own.p = partCf.inverse() * checkVec3(L, 3);
            m::setPartCFrame(i, own);
        }
        return 0;
    }
    if (!std::strcmp(key, "Grip") && i.isA("Tool")) {
        if (!isCFrame(L, 3)) luaL_error(L, "invalid argument #3 to 'Grip' (CFrame expected, got %s)", typeOfName(L, 3));
        m::setToolGrip(i, checkCFrame(L, 3));
        return 0;
    }
    if (i.isA("Model") && !std::strcmp(key, "WorldPivot")) {
        if (!isCFrame(L, 3)) luaL_error(L, "invalid argument #3 to 'WorldPivot' (CFrame expected, got %s)", typeOfName(L, 3));
        m::setWorldPivot(rt, i, checkCFrame(L, 3));
        return 0;
    }
    if (i.isA("ScreenGui") && !std::strcmp(key, "ScreenInsets")) {   // None is IgnoreGuiInset; anything else keeps the topbar gap
        Value v;
        if (!rt.toValue(L, 3, Value::Enum, findEnum("ScreenInsets"), v, err)) luaL_error(L, "invalid argument #3 to 'ScreenInsets' (EnumItem expected, got %s)", err.c_str());
        i.set("ScreenInsets", v);
        i.set("IgnoreGuiInset", Value::boolean(v.s == "None"));
        return 0;
    }
    // the data family
    if (i.isA("RayValue") && std::string_view(key) == "Value") {   // the pair, Changed once on the last half to change
        if (!isRay(L, 3)) luaL_error(L, "invalid argument #3 to 'Value' (Ray expected, got %s)", typeOfName(L, 3));
        RayV r = checkRay(L, 3);
        g_cframeValueLastHalf = i.get("ValueDirection") != Value::vector3(r.direction) ? "ValueDirection" : "ValueOrigin";
        i.set("ValueOrigin", Value::vector3(r.origin)); i.set("ValueDirection", Value::vector3(r.direction));
        g_cframeValueLastHalf = nullptr;
        return 0;
    }
    if ((i.isA("DataModel") && (std::string_view(key) == "Workspace" || std::string_view(key) == "RunService")) || !std::strcmp(key, "IsInSandbox"))
        luaL_error(L, "Unable to assign property %s. Property is read only", key);
    // the services family
    if (i.isA("HttpService") && std::string_view(key) == "HttpEnabled") {   // LocalUserSecurity: the command bar or a plugin turns it on
        if (!rt.hasPluginCapability(L)) luaL_error(L, "The current thread cannot set 'HttpEnabled' (lacking capability Plugin)");
        rt.opts.httpEnabled = lua_toboolean(L, 3);
        return 0;
    }
    if ((i.isA("Tween") && (std::string_view(key) == "Instance" || std::string_view(key) == "TweenInfo")) ||
        (i.isA("Stats") && (std::string_view(key) == "InstanceCount" || std::string_view(key) == "PrimitivesCount")))
        luaL_error(L, "Unable to assign property %s. Property is read only", key);
    if (i.isA("GuiService") && !std::strcmp(key, "SelectedObject")) {
        Instance* to = nullptr;
        if (!lua_isnil(L, 3)) {
            to = rt.toInstance(L, 3);
            if (!to || !to->isA("GuiObject")) luaL_error(L, "invalid argument #3 to 'SelectedObject' (GuiObject expected, got %s)", typeOfName(L, 3));
        }
        m::moveSelection(rt, i, to);
        return 0;
    }
    if (i.isA("Camera") && (!std::strcmp(key, "DiagonalFieldOfView") || !std::strcmp(key, "MaxAxisFieldOfView"))) {
        // the vertical FieldOfView that gives this diagonal / wider-axis angle on the viewport's aspect
        Vec2 vs = i.get("ViewportSize").v2();
        double aspect = vs.y > 0 ? vs.x / vs.y : 4.0 / 3.0;
        double t = std::tan(luaL_checknumber(L, 3) * 3.14159265358979323846 / 360.0);
        double halfV = key[0] == 'D' ? t / std::sqrt(1 + aspect * aspect) : t / std::max(1.0, aspect);
        i.set("FieldOfView", Value::number(std::atan(halfV) * 360.0 / 3.14159265358979323846));
        return 0;
    }
    for (const char* ro : {"ExtentsCFrame", "CurrentPhysicalProperties", "TransformedCFrame", "TransformedWorldCFrame", "Terrain", "WorldRotationAxis",
                           "CageOriginWorld", "ImportOriginWorld", "ReferenceOriginWorld"})
        if (!std::strcmp(key, ro) && (i.isA("BasePart") || i.isA("Bone") || i.isA("Workspace") || i.isA("CylindricalConstraint") || i.isA("BaseWrap")))
            luaL_error(L, "Unable to assign property %s. Property is read only", key);
    if (partLike && !std::strcmp(key, "Rotation")) key = "Orientation";
    if (partLike && !std::strcmp(key, "Velocity")) key = "AssemblyLinearVelocity";
    if (partLike && !std::strcmp(key, "RotVelocity")) key = "AssemblyAngularVelocity";
    bool brick = false;
    if (i.isA("BasePart") && !std::strcmp(key, "BrickColor")) { key = "Color"; brick = true; }
    if (i.cls().hasCallback(key)) {
        InstBinding& b = rt.binding(i);
        auto it = b.callbacks.find(key);
        if (it != b.callbacks.end()) { lua_unref(L, it->second.ref); b.callbacks.erase(it); }
        if (!lua_isnil(L, 3)) {
            luaL_checktype(L, 3, LUA_TFUNCTION);
            lua_pushvalue(L, 3);
            b.callbacks[key] = Callback{lua_ref(L, -1), rt.ctxOf(L)};
            lua_pop(L, 1);
        }
        return 0;
    }
    const PropDef* d = i.cls().findProp(key);
    if (!d) {
        if (!std::strcmp(key, "ClassName")) luaL_error(L, "Unable to assign property ClassName. Property is read only");
        std::string cap = m::legacyName(key);
        std::string cp, co;
        if (cap != key && (i.cls().findProp(cap) || m::cframeProp(i, cap.c_str(), cp, co))) {
            lua_pushstring(L, cap.c_str());
            lua_replace(L, 2);
            return inst_newindex(L);
        }
        luaL_error(L, "%s is not a valid member of %s \"%s\"", key, i.className().c_str(), i.fullName().c_str());
    }
    if (d->flags & ReadOnly) luaL_error(L, "Unable to assign property %s. Property is read only", key);
    if (d->flags & NoScriptWrite) luaL_error(L, "Unable to assign property %s. Script write access is restricted", key);
    if ((d->flags & ServerWrite) && !rt.dm.isServer()) luaL_error(L, "Unable to assign property %s. It can only be set on the server", key);
    // PluginSecurity: a plugin or a chunk the host runs (the command bar) may set it, a game's
    // own scripts may not.
    if (d->flags & PluginWrite) {
        if (!rt.hasPluginCapability(L))
            luaL_error(L, "The current thread cannot set '%s' (lacking capability Plugin)", key);
    }
    if (!std::strcmp(key, "RenderFidelity") && i.isA("PartOperation")) {
        Value rv;
        std::string rerr;
        if (rt.toValue(L, 3, Value::Enum, findEnum("RenderFidelity"), rv, rerr) && rv.s == "Performance")
            luaL_error(L, "A PartOperation's RenderFidelity cannot be Performance");
    }
    Value v;
    if (brick || (d->flags & Brick)) {
        // a BrickColor, its name, or a Color3 kept as it is: a read snaps to the palette
        Col3 c;
        if (isBrickColor(L, 3)) c = brickColor(checkBrickColor(L, 3));
        else if (lua_type(L, 3) == LUA_TSTRING) c = brickColor(brickNumber(lua_tostring(L, 3)));
        else if (isColor3(L, 3)) c = checkColor3(L, 3);
        else luaL_error(L, "invalid argument #3 to '%s' (BrickColor expected, got %s)", brick ? "BrickColor" : key, typeOfName(L, 3));
        v = Value::color3(c.r, c.g, c.b);
    } else if (!rt.toValue(L, 3, d->type, d->enumType, v, err))
        luaL_error(L, "invalid argument #3 to '%s' (%s expected, got %s)", key, Value::typeName(d->type), err.c_str());
    if (!i.set(key, v, &err)) luaL_error(L, "%s", err.c_str());
    return 0;
}

static int inst_namecall(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    Instance& i = rt.checkObject(L, 1);
    const char* name = lua_namecallatom(L, nullptr);
    if (!name) luaL_error(L, "invalid method call");
    if (lua_CFunction fn = findMethod(i, name)) return fn(L);
    { std::string cap = m::legacyName(name); if (cap != name) if (lua_CFunction fn = findMethod(i, cap.c_str())) return fn(L); }   // obj:findFirstChild()
    if (!i.isA("Instance")) luaL_error(L, "%s is not a valid member of %s", name, i.className().c_str());
    // A child, property or event called as a method: Roblox's own wording for it.
    if (i.cls().findProp(name) || i.cls().hasEvent(name) || i.findFirstChild(name))
        luaL_error(L, "attempt to call a %s value (member '%s' of %s)", i.cls().hasEvent(name) ? "RBXScriptSignal" : "non-function", name, i.className().c_str());
    luaL_error(L, "%s is not a valid member of %s \"%s\"", name, i.className().c_str(), i.fullName().c_str());
    return 0;
}

static int inst_tostring(lua_State* L) {
    Instance& i = rtOf(L).checkObject(L, 1);
    lua_pushstring(L, (i.isA("Instance") ? i.name() : i.className()).c_str());   // an Object has no Name
    return 1;
}

// ---- signal / connection --------------------------------------------------------------
static SignalUD& checkSignal(lua_State* L, int idx) {
    if (lua_userdatatag(L, idx) != TAG_SIGNAL) luaL_typeerror(L, idx, "RBXScriptSignal");
    return *static_cast<SignalUD*>(lua_touserdata(L, idx));
}
static void pushConn(lua_State* L, SignalUD& s, int id) {
    void* p = lua_newuserdatataggedwithmetatable(L, sizeof(ConnUD), TAG_CONNECTION);
    new (p) ConnUD{s.owner, s.sig, id};
}
static int connect(lua_State* L, bool once) {
    Runtime::Impl& rt = rtOf(L);
    SignalUD& s = checkSignal(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (s.owner->destroyed()) { pushConn(L, s, 0); return 1; }   // Roblox: connecting to a destroyed instance's signal is a dead connection
    lua_pushvalue(L, 2);
    Conn c; c.id = s.sig->nextId++; c.fnRef = lua_ref(L, -1); c.once = once; c.connected = true; c.ctx = rt.ctxOf(L);
    lua_pop(L, 1);
    s.sig->conns.push_back(c);
    pushConn(L, s, c.id);
    return 1;
}
static int sig_namecall(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    SignalUD& s = checkSignal(L, 1);
    const char* name = lua_namecallatom(L, nullptr);
    if (!name) luaL_error(L, "invalid method call");
    if (!std::strcmp(name, "Connect") || !std::strcmp(name, "connect") || !std::strcmp(name, "ConnectParallel")) return connect(L, false);
    if (!std::strcmp(name, "Once")) return connect(L, true);
    if (!std::strcmp(name, "Wait") || !std::strcmp(name, "wait")) {
        if (!lua_isyieldable(L)) luaL_error(L, "cannot yield from this context");
        Task* t = rt.taskFor(L);
        t->state = Task::WaitSignal;
        s.sig->waiters.push_back(t);
        return lua_yield(L, 0);
    }
    luaL_error(L, "%s is not a valid member of RBXScriptSignal", name);
    return 0;
}
static int sig_index(lua_State* L) {
    checkSignal(L, 1);
    const char* key = luaL_checkstring(L, 2);
    // signal.Connect(signal, fn) style
    if (!std::strcmp(key, "Connect") || !std::strcmp(key, "connect")) { lua_pushcfunction(L, [](lua_State* L) -> int { return connect(L, false); }, "Connect"); return 1; }
    if (!std::strcmp(key, "Once")) { lua_pushcfunction(L, [](lua_State* L) -> int { return connect(L, true); }, "Once"); return 1; }
    luaL_error(L, "%s is not a valid member of RBXScriptSignal", key);
    return 0;
}
static int sig_tostring(lua_State* L) { SignalUD& s = checkSignal(L, 1); lua_pushfstring(L, "Signal %s", s.sig->name.c_str()); return 1; }

static ConnUD& checkConn(lua_State* L, int idx) {
    if (lua_userdatatag(L, idx) != TAG_CONNECTION) luaL_typeerror(L, idx, "RBXScriptConnection");
    return *static_cast<ConnUD*>(lua_touserdata(L, idx));
}
static bool connConnected(ConnUD& c) {
    for (auto& k : c.sig->conns) if (k.id == c.id) return k.connected;
    return false;
}
static void disconnect(lua_State* L, ConnUD& c) {
    for (auto& k : c.sig->conns) if (k.id == c.id && k.connected) { k.connected = false; lua_unref(L, k.fnRef); }
    // fire() snapshots its connections, so erasing during a fire is safe.
    c.sig->conns.erase(std::remove_if(c.sig->conns.begin(), c.sig->conns.end(), [&](const Conn& k) { return k.id == c.id; }), c.sig->conns.end());
}
static int conn_namecall(lua_State* L) {
    ConnUD& c = checkConn(L, 1);
    const char* name = lua_namecallatom(L, nullptr);
    if (name && (!std::strcmp(name, "Disconnect") || !std::strcmp(name, "disconnect"))) { disconnect(L, c); return 0; }
    luaL_error(L, "%s is not a valid member of RBXScriptConnection", name ? name : "?");
    return 0;
}
static int conn_index(lua_State* L) {
    ConnUD& c = checkConn(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!std::strcmp(key, "Connected") || !std::strcmp(key, "connected")) { lua_pushboolean(L, connConnected(c)); return 1; }
    if (!std::strcmp(key, "Disconnect") || !std::strcmp(key, "disconnect")) { lua_pushcfunction(L, [](lua_State* L) -> int { disconnect(L, checkConn(L, 1)); return 0; }, "Disconnect"); return 1; }
    luaL_error(L, "%s is not a valid member of RBXScriptConnection", key);
    return 0;
}
static int conn_tostring(lua_State* L) { lua_pushstring(L, "Connection"); return 1; }

// ---- globals ---------------------------------------------------------------------------
static int instance_new(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    const char* cn = luaL_checkstring(L, 1);
    Instance* parent = nullptr;
    if (!lua_isnoneornil(L, 2)) { parent = rt.toInstance(L, 2); if (!parent) luaL_error(L, "invalid argument #2 to 'new' (Instance expected, got %s)", typeOfName(L, 2)); }
    std::string err;
    Instance::Ptr i = rt.dm.create(cn, nullptr, &err);
    if (!i) luaL_error(L, "%s", err.c_str());
    rt.pushInstance(L, i.get());
    if (parent && rt.inPluginTree(parent) && !rt.hasPluginCapability(L))
        luaL_error(L, "The current thread cannot parent to PluginDebugService (lacking capability Plugin)");
    if (parent && !i->setParent(parent, &err)) luaL_error(L, "%s", err.c_str());
    return 1;
}

static void setMetaTagged(lua_State* L, int tag, const char* type, lua_CFunction index, lua_CFunction newindex, lua_CFunction namecall, lua_CFunction tostring) {
    lua_newtable(L);
    lua_pushcfunction(L, index, "__index"); lua_setfield(L, -2, "__index");
    if (newindex) { lua_pushcfunction(L, newindex, "__newindex"); lua_setfield(L, -2, "__newindex"); }
    lua_pushcfunction(L, namecall, "__namecall"); lua_setfield(L, -2, "__namecall");
    lua_pushcfunction(L, tostring, "__tostring"); lua_setfield(L, -2, "__tostring");
    lua_pushstring(L, type); lua_setfield(L, -2, "__type");
    lua_pushstring(L, "The metatable is locked"); lua_setfield(L, -2, "__metatable");
    lua_setreadonly(L, -1, true);
    lua_setuserdatametatable(L, tag);
}

void openInstanceApi(Runtime::Impl& rt) {
    lua_State* L = rt.L;
    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "v"); lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, kInstances);

    setMetaTagged(L, TAG_INSTANCE, "Instance", inst_index, inst_newindex, inst_namecall, inst_tostring);
    lua_setuserdatadtor(L, TAG_INSTANCE, [](lua_State*, void* p) { static_cast<InstanceUD*>(p)->~InstanceUD(); });
    setMetaTagged(L, TAG_OBJECT, "Object", inst_index, inst_newindex, inst_namecall, inst_tostring);
    lua_setuserdatadtor(L, TAG_OBJECT, [](lua_State*, void* p) { static_cast<InstanceUD*>(p)->~InstanceUD(); });
    setMetaTagged(L, TAG_SIGNAL, "RBXScriptSignal", sig_index, nullptr, sig_namecall, sig_tostring);
    lua_setuserdatadtor(L, TAG_SIGNAL, [](lua_State*, void* p) { static_cast<SignalUD*>(p)->~SignalUD(); });
    setMetaTagged(L, TAG_CONNECTION, "RBXScriptConnection", conn_index, nullptr, conn_namecall, conn_tostring);
    lua_setuserdatadtor(L, TAG_CONNECTION, [](lua_State*, void* p) { static_cast<ConnUD*>(p)->~ConnUD(); });

    lua_newtable(L);
    lua_pushcfunction(L, instance_new, "Instance.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Instance");
    lua_pushcfunction(L, m::UserSettingsFn, "UserSettings"); lua_setglobal(L, "UserSettings");

    setMetaTagged(L, TAG_RAYCASTPARAMS, "RaycastParams", m::rp_index, m::rp_newindex, m::rp_namecall, m::rp_tostring);
    lua_setuserdatadtor(L, TAG_RAYCASTPARAMS, [](lua_State*, void* p) { static_cast<m::RaycastParamsUD*>(p)->~RaycastParamsUD(); });
    lua_newtable(L);
    lua_pushcfunction(L, m::rp_new, "RaycastParams.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "RaycastParams");
    setMetaTagged(L, TAG_OVERLAPPARAMS, "OverlapParams", m::rp_index, m::rp_newindex, m::rp_namecall, m::rp_tostring);
    lua_setuserdatadtor(L, TAG_OVERLAPPARAMS, [](lua_State*, void* p) { static_cast<m::RaycastParamsUD*>(p)->~RaycastParamsUD(); });
    lua_newtable(L);
    lua_pushcfunction(L, m::op_new, "OverlapParams.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "OverlapParams");

    rt.pushInstance(L, rt.dm.root()); lua_setglobal(L, "game");
    rt.pushInstance(L, rt.dm.root()); lua_setglobal(L, "Game");
    rt.pushInstance(L, rt.dm.workspace()); lua_setglobal(L, "workspace");
    rt.pushInstance(L, rt.dm.workspace()); lua_setglobal(L, "Workspace");
}

} // namespace pulseblockz::rbx
