// Roblox value types for Luau: Vector3 (Luau's own vector), Vector2, UDim, Color3, BrickColor,
// Content, CFrame, Enum, Ray, TweenInfo, Random, Font, the sequences, and JSON for HttpService.
#include "rbx_internal.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <random>
#include <algorithm>

namespace pulseblockz::rbx {

static const float PI = 3.14159265358979f;

// ---- small vector math -------------------------------------------------------------
static Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static float len(Vec3 a) { return std::sqrt(dot(a, a)); }
static Vec3 unit(Vec3 a) { float l = len(a); return l > 0 ? mul(a, 1 / l) : Vec3{0, 0, 0}; }

Vec3 checkVec3(lua_State* L, int idx) { return toVec3(luaL_checkvector(L, idx)); }

// ---- Vector3 -----------------------------------------------------------------------
static int v3_new(lua_State* L) {
    lua_pushvector(L, (float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0));
    return 1;
}
static int v3_fromNormalId(lua_State* L) {
    const EnumItem* i = checkEnumItem(L, 1, findEnum("NormalId"));
    static const Vec3 dirs[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
    pushVec3(L, dirs[i->value]);
    return 1;
}
static int v3_fromAxis(lua_State* L) {
    const EnumItem* i = checkEnumItem(L, 1, findEnum("Axis"));
    static const Vec3 dirs[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    pushVec3(L, dirs[i->value]);
    return 1;
}
static int v3_index(lua_State* L) {
    Vec3 v = checkVec3(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (k[1] == 0) {
        switch (k[0]) {
        case 'X': case 'x': lua_pushnumber(L, v.x); return 1;
        case 'Y': case 'y': lua_pushnumber(L, v.y); return 1;
        case 'Z': case 'z': lua_pushnumber(L, v.z); return 1;
        }
    }
    if (!std::strcmp(k, "Magnitude") || !std::strcmp(k, "magnitude")) { lua_pushnumber(L, len(v)); return 1; }   // Roblox still reads the lowercase spellings
    if (!std::strcmp(k, "Unit") || !std::strcmp(k, "unit")) { pushVec3(L, unit(v)); return 1; }
    luaL_error(L, "%s is not a valid member of Vector3", k);
}
static int v3_namecall(lua_State* L) {
    Vec3 a = checkVec3(L, 1);
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (!m) luaL_error(L, "invalid method call");
    if (!std::strcmp(m, "Dot")) { lua_pushnumber(L, dot(a, checkVec3(L, 2))); return 1; }
    if (!std::strcmp(m, "Cross")) { pushVec3(L, cross(a, checkVec3(L, 2))); return 1; }
    if (!std::strcmp(m, "Lerp")) { Vec3 b = checkVec3(L, 2); float t = (float)luaL_checknumber(L, 3); pushVec3(L, add(a, mul(sub(b, a), t))); return 1; }
    if (!std::strcmp(m, "FuzzyEq")) { Vec3 b = checkVec3(L, 2); float e = (float)luaL_optnumber(L, 3, 1e-5); lua_pushboolean(L, len(sub(a, b)) <= e); return 1; }
    if (!std::strcmp(m, "Abs")) { pushVec3(L, {std::fabs(a.x), std::fabs(a.y), std::fabs(a.z)}); return 1; }
    if (!std::strcmp(m, "Sign")) { auto s = [](float f) { return f > 0 ? 1.f : f < 0 ? -1.f : 0.f; }; pushVec3(L, {s(a.x), s(a.y), s(a.z)}); return 1; }
    if (!std::strcmp(m, "Floor")) { pushVec3(L, {std::floor(a.x), std::floor(a.y), std::floor(a.z)}); return 1; }
    if (!std::strcmp(m, "Ceil")) { pushVec3(L, {std::ceil(a.x), std::ceil(a.y), std::ceil(a.z)}); return 1; }
    if (!std::strcmp(m, "Min") || !std::strcmp(m, "Max")) {
        bool mn = m[1] == 'i';
        for (int i = 2; i <= lua_gettop(L); i++) {
            Vec3 b = checkVec3(L, i);
            a = mn ? Vec3{std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z)}
                   : Vec3{std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z)};
        }
        pushVec3(L, a); return 1;
    }
    if (!std::strcmp(m, "Angle")) {
        Vec3 b = checkVec3(L, 2);
        float c = dot(unit(a), unit(b));
        float ang = std::acos(std::fmax(-1.f, std::fmin(1.f, c)));
        if (lua_isvector(L, 3) && dot(cross(a, b), checkVec3(L, 3)) < 0) ang = -ang;
        lua_pushnumber(L, ang); return 1;
    }
    luaL_error(L, "%s is not a valid method of Vector3", m);
}
static int v3_tostring(lua_State* L) {
    Vec3 v = checkVec3(L, 1);
    char buf[96]; std::snprintf(buf, sizeof buf, "%g, %g, %g", v.x, v.y, v.z);
    lua_pushstring(L, buf);
    return 1;
}

static void openVector3(lua_State* L) {
    // Luau holds one metatable for the whole vector type: set on any vector, every vector has it
    lua_pushvector(L, 0, 0, 0);
    lua_newtable(L);
    lua_pushcfunction(L, v3_index, "Vector3.__index"); lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, v3_namecall, "Vector3.__namecall"); lua_setfield(L, -2, "__namecall");
    lua_pushcfunction(L, v3_tostring, "Vector3.__tostring"); lua_setfield(L, -2, "__tostring");
    lua_pushstring(L, "Vector3"); lua_setfield(L, -2, "__type");
    lua_setreadonly(L, -1, true);
    lua_setmetatable(L, -2);
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, v3_new, "Vector3.new"); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, v3_fromNormalId, "Vector3.FromNormalId"); lua_setfield(L, -2, "FromNormalId");
    lua_pushcfunction(L, v3_fromAxis, "Vector3.FromAxis"); lua_setfield(L, -2, "FromAxis");
    lua_pushvector(L, 0, 0, 0); lua_setfield(L, -2, "zero");
    lua_pushvector(L, 1, 1, 1); lua_setfield(L, -2, "one");
    lua_pushvector(L, 1, 0, 0); lua_setfield(L, -2, "xAxis");
    lua_pushvector(L, 0, 1, 0); lua_setfield(L, -2, "yAxis");
    lua_pushvector(L, 0, 0, 1); lua_setfield(L, -2, "zAxis");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Vector3");
}

// ---- Vector2 -----------------------------------------------------------------------
struct Vector2UD { Vec2 v; };
bool isVector2(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_VECTOR2; }
Vec2 checkVector2(lua_State* L, int idx) {
    if (!isVector2(L, idx)) luaL_typeerror(L, idx, "Vector2");
    return static_cast<Vector2UD*>(lua_touserdata(L, idx))->v;
}
void pushVector2(lua_State* L, Vec2 v) {
    auto* ud = static_cast<Vector2UD*>(lua_newuserdatataggedwithmetatable(L, sizeof(Vector2UD), TAG_VECTOR2));
    ud->v = v;
}
static float len2(Vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
static Vec2 unit2(Vec2 a) { float l = len2(a); return l > 0 ? Vec2{a.x / l, a.y / l} : Vec2{0, 0}; }
static int v2_new(lua_State* L) { pushVector2(L, {(float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0)}); return 1; }
static int v2_index(lua_State* L) {
    Vec2 v = checkVector2(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (k[1] == 0 && (k[0] == 'X' || k[0] == 'x')) { lua_pushnumber(L, v.x); return 1; }
    if (k[1] == 0 && (k[0] == 'Y' || k[0] == 'y')) { lua_pushnumber(L, v.y); return 1; }
    if (!std::strcmp(k, "Magnitude") || !std::strcmp(k, "magnitude")) { lua_pushnumber(L, len2(v)); return 1; }
    if (!std::strcmp(k, "Unit")) { pushVector2(L, unit2(v)); return 1; }
    luaL_error(L, "%s is not a valid member of Vector2", k);
}
static int v2_namecall(lua_State* L) {
    Vec2 a = checkVector2(L, 1);
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (!m) luaL_error(L, "invalid method call");
    if (!std::strcmp(m, "Dot")) { Vec2 b = checkVector2(L, 2); lua_pushnumber(L, a.x * b.x + a.y * b.y); return 1; }
    if (!std::strcmp(m, "Cross")) { Vec2 b = checkVector2(L, 2); lua_pushnumber(L, a.x * b.y - a.y * b.x); return 1; }
    if (!std::strcmp(m, "Lerp")) { Vec2 b = checkVector2(L, 2); float t = (float)luaL_checknumber(L, 3); pushVector2(L, {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}); return 1; }
    if (!std::strcmp(m, "FuzzyEq")) { Vec2 b = checkVector2(L, 2); float e = (float)luaL_optnumber(L, 3, 1e-5); lua_pushboolean(L, len2({a.x - b.x, a.y - b.y}) <= e); return 1; }
    if (!std::strcmp(m, "Abs")) { pushVector2(L, {std::fabs(a.x), std::fabs(a.y)}); return 1; }
    if (!std::strcmp(m, "Sign")) { auto s = [](float f) { return f > 0 ? 1.f : f < 0 ? -1.f : 0.f; }; pushVector2(L, {s(a.x), s(a.y)}); return 1; }
    if (!std::strcmp(m, "Floor")) { pushVector2(L, {std::floor(a.x), std::floor(a.y)}); return 1; }
    if (!std::strcmp(m, "Ceil")) { pushVector2(L, {std::ceil(a.x), std::ceil(a.y)}); return 1; }
    if (!std::strcmp(m, "Min") || !std::strcmp(m, "Max")) {
        bool mn = m[1] == 'i';
        for (int i = 2; i <= lua_gettop(L); i++) {
            Vec2 b = checkVector2(L, i);
            a = mn ? Vec2{std::fmin(a.x, b.x), std::fmin(a.y, b.y)} : Vec2{std::fmax(a.x, b.x), std::fmax(a.y, b.y)};
        }
        pushVector2(L, a); return 1;
    }
    if (!std::strcmp(m, "Angle")) {
        Vec2 b = checkVector2(L, 2);
        lua_pushnumber(L, std::atan2(a.x * b.y - a.y * b.x, a.x * b.x + a.y * b.y)); return 1;
    }
    luaL_error(L, "%s is not a valid method of Vector2", m);
}
static int v2_add(lua_State* L) { Vec2 a = checkVector2(L, 1), b = checkVector2(L, 2); pushVector2(L, {a.x + b.x, a.y + b.y}); return 1; }
static int v2_sub(lua_State* L) { Vec2 a = checkVector2(L, 1), b = checkVector2(L, 2); pushVector2(L, {a.x - b.x, a.y - b.y}); return 1; }
static int v2_mul(lua_State* L) {
    if (lua_isnumber(L, 1)) { float s = (float)lua_tonumber(L, 1); Vec2 b = checkVector2(L, 2); pushVector2(L, {s * b.x, s * b.y}); return 1; }
    Vec2 a = checkVector2(L, 1);
    if (lua_isnumber(L, 2)) { float s = (float)lua_tonumber(L, 2); pushVector2(L, {a.x * s, a.y * s}); return 1; }
    Vec2 b = checkVector2(L, 2); pushVector2(L, {a.x * b.x, a.y * b.y}); return 1;
}
static int v2_div(lua_State* L) {
    Vec2 a = checkVector2(L, 1);
    if (lua_isnumber(L, 2)) { float s = (float)lua_tonumber(L, 2); pushVector2(L, {a.x / s, a.y / s}); return 1; }
    Vec2 b = checkVector2(L, 2); pushVector2(L, {a.x / b.x, a.y / b.y}); return 1;
}
static int v2_unm(lua_State* L) { Vec2 a = checkVector2(L, 1); pushVector2(L, {-a.x, -a.y}); return 1; }
static int v2_eq(lua_State* L) { lua_pushboolean(L, isVector2(L, 1) && isVector2(L, 2) && checkVector2(L, 1) == checkVector2(L, 2)); return 1; }
static int v2_tostring(lua_State* L) { Vec2 v = checkVector2(L, 1); char b[64]; std::snprintf(b, sizeof b, "%g, %g", v.x, v.y); lua_pushstring(L, b); return 1; }

// ---- UDim / UDim2 ------------------------------------------------------------------
struct UDimUD { UDim d; };
struct UDim2UD { UDim2 d; };
bool isUDim(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_UDIM; }
UDim checkUDim(lua_State* L, int idx) {
    if (!isUDim(L, idx)) luaL_typeerror(L, idx, "UDim");
    return static_cast<UDimUD*>(lua_touserdata(L, idx))->d;
}
void pushUDim(lua_State* L, UDim d) { static_cast<UDimUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(UDimUD), TAG_UDIM))->d = d; }
bool isUDim2(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_UDIM2; }
UDim2 checkUDim2(lua_State* L, int idx) {
    if (!isUDim2(L, idx)) luaL_typeerror(L, idx, "UDim2");
    return static_cast<UDim2UD*>(lua_touserdata(L, idx))->d;
}
void pushUDim2(lua_State* L, UDim2 d) { static_cast<UDim2UD*>(lua_newuserdatataggedwithmetatable(L, sizeof(UDim2UD), TAG_UDIM2))->d = d; }

static int ud_new(lua_State* L) { pushUDim(L, {(float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0)}); return 1; }
static int ud_index(lua_State* L) {
    UDim d = checkUDim(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Scale")) { lua_pushnumber(L, d.scale); return 1; }
    if (!std::strcmp(k, "Offset")) { lua_pushnumber(L, d.offset); return 1; }
    luaL_error(L, "%s is not a valid member of UDim", k);
}
static int ud_namecall(lua_State* L) { int atom; const char* m = lua_namecallatom(L, &atom); luaL_error(L, "%s is not a valid method of UDim", m ? m : "?"); }
static int ud_add(lua_State* L) { UDim a = checkUDim(L, 1), b = checkUDim(L, 2); pushUDim(L, {a.scale + b.scale, a.offset + b.offset}); return 1; }
static int ud_sub(lua_State* L) { UDim a = checkUDim(L, 1), b = checkUDim(L, 2); pushUDim(L, {a.scale - b.scale, a.offset - b.offset}); return 1; }
static int ud_eq(lua_State* L) { lua_pushboolean(L, isUDim(L, 1) && isUDim(L, 2) && checkUDim(L, 1) == checkUDim(L, 2)); return 1; }
static int ud_tostring(lua_State* L) { UDim d = checkUDim(L, 1); char b[64]; std::snprintf(b, sizeof b, "%g, %g", d.scale, d.offset); lua_pushstring(L, b); return 1; }

static int ud2_new(lua_State* L) {
    if (isUDim(L, 1)) { pushUDim2(L, {checkUDim(L, 1), checkUDim(L, 2)}); return 1; }
    pushUDim2(L, {{(float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0)}, {(float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 0)}});
    return 1;
}
static int ud2_fromScale(lua_State* L) { pushUDim2(L, {{(float)luaL_optnumber(L, 1, 0), 0}, {(float)luaL_optnumber(L, 2, 0), 0}}); return 1; }
static int ud2_fromOffset(lua_State* L) { pushUDim2(L, {{0, (float)luaL_optnumber(L, 1, 0)}, {0, (float)luaL_optnumber(L, 2, 0)}}); return 1; }
static int ud2_index(lua_State* L) {
    UDim2 d = checkUDim2(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "X") || !std::strcmp(k, "Width")) { pushUDim(L, d.x); return 1; }
    if (!std::strcmp(k, "Y") || !std::strcmp(k, "Height")) { pushUDim(L, d.y); return 1; }
    luaL_error(L, "%s is not a valid member of UDim2", k);
}
static int ud2_namecall(lua_State* L) {
    UDim2 a = checkUDim2(L, 1);
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && !std::strcmp(m, "Lerp")) {
        UDim2 b = checkUDim2(L, 2); float t = (float)luaL_checknumber(L, 3);
        auto l = [t](float p, float q) { return p + (q - p) * t; };
        pushUDim2(L, {{l(a.x.scale, b.x.scale), l(a.x.offset, b.x.offset)}, {l(a.y.scale, b.y.scale), l(a.y.offset, b.y.offset)}});
        return 1;
    }
    luaL_error(L, "%s is not a valid method of UDim2", m ? m : "?");
}
static int ud2_add(lua_State* L) { UDim2 a = checkUDim2(L, 1), b = checkUDim2(L, 2); pushUDim2(L, {{a.x.scale + b.x.scale, a.x.offset + b.x.offset}, {a.y.scale + b.y.scale, a.y.offset + b.y.offset}}); return 1; }
static int ud2_sub(lua_State* L) { UDim2 a = checkUDim2(L, 1), b = checkUDim2(L, 2); pushUDim2(L, {{a.x.scale - b.x.scale, a.x.offset - b.x.offset}, {a.y.scale - b.y.scale, a.y.offset - b.y.offset}}); return 1; }
static int ud2_eq(lua_State* L) { lua_pushboolean(L, isUDim2(L, 1) && isUDim2(L, 2) && checkUDim2(L, 1) == checkUDim2(L, 2)); return 1; }
static int ud2_tostring(lua_State* L) {
    UDim2 d = checkUDim2(L, 1); char b[128];
    std::snprintf(b, sizeof b, "{%g, %g}, {%g, %g}", d.x.scale, d.x.offset, d.y.scale, d.y.offset);
    lua_pushstring(L, b); return 1;
}

// ---- Color3 ------------------------------------------------------------------------
struct Color3UD { Col3 c; };
bool isColor3(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_COLOR3; }
Col3 checkColor3(lua_State* L, int idx) {
    if (!isColor3(L, idx)) luaL_typeerror(L, idx, "Color3");
    return static_cast<Color3UD*>(lua_touserdata(L, idx))->c;
}
void pushColor3(lua_State* L, Col3 c) {
    auto* ud = static_cast<Color3UD*>(lua_newuserdatataggedwithmetatable(L, sizeof(Color3UD), TAG_COLOR3));
    ud->c = c;
}
static void hsvToRgb(float h, float s, float v, Col3& out) {
    h = h - std::floor(h);
    float i = std::floor(h * 6), f = h * 6 - i;
    float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch ((int)i % 6) {
    case 0: out = {v, t, p}; break; case 1: out = {q, v, p}; break; case 2: out = {p, v, t}; break;
    case 3: out = {p, q, v}; break; case 4: out = {t, p, v}; break; default: out = {v, p, q}; break;
    }
}
static void rgbToHsv(Col3 c, float& h, float& s, float& v) {
    float mx = std::fmax(c.r, std::fmax(c.g, c.b)), mn = std::fmin(c.r, std::fmin(c.g, c.b)), d = mx - mn;
    v = mx; s = mx > 0 ? d / mx : 0; h = 0;
    if (d > 0) {
        if (mx == c.r) h = (c.g - c.b) / d + (c.g < c.b ? 6 : 0);
        else if (mx == c.g) h = (c.b - c.r) / d + 2;
        else h = (c.r - c.g) / d + 4;
        h /= 6;
    }
}
static int c3_new(lua_State* L) { pushColor3(L, {(float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0)}); return 1; }
static int c3_fromRGB(lua_State* L) { pushColor3(L, {(float)luaL_optnumber(L, 1, 0) / 255.f, (float)luaL_optnumber(L, 2, 0) / 255.f, (float)luaL_optnumber(L, 3, 0) / 255.f}); return 1; }
static int c3_fromHSV(lua_State* L) { Col3 c; hsvToRgb((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), c); pushColor3(L, c); return 1; }
static int c3_fromHex(lua_State* L) {
    const char* s = luaL_checkstring(L, 1);
    if (*s == '#') s++;
    unsigned v = 0; size_t n = std::strlen(s);
    if ((n != 6 && n != 3) || std::sscanf(s, "%x", &v) != 1) luaL_error(L, "Unable to convert hex string to Color3");
    if (n == 3) v = ((v & 0xF00) * 0x1100) | ((v & 0x0F0) * 0x110) | ((v & 0x00F) * 0x11);
    pushColor3(L, {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f});
    return 1;
}
static int c3_index(lua_State* L) {
    Col3 c = checkColor3(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "R") || !std::strcmp(k, "r")) { lua_pushnumber(L, c.r); return 1; }
    if (!std::strcmp(k, "G") || !std::strcmp(k, "g")) { lua_pushnumber(L, c.g); return 1; }
    if (!std::strcmp(k, "B") || !std::strcmp(k, "b")) { lua_pushnumber(L, c.b); return 1; }
    luaL_error(L, "%s is not a valid member of Color3", k);
}
static int c3_namecall(lua_State* L) {
    Col3 a = checkColor3(L, 1);
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && !std::strcmp(m, "Lerp")) {
        Col3 b = checkColor3(L, 2); float t = (float)luaL_checknumber(L, 3);
        pushColor3(L, {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}); return 1;
    }
    if (m && !std::strcmp(m, "ToHSV")) { float h, s, v; rgbToHsv(a, h, s, v); lua_pushnumber(L, h); lua_pushnumber(L, s); lua_pushnumber(L, v); return 3; }
    if (m && !std::strcmp(m, "ToHex")) {
        char buf[8]; std::snprintf(buf, sizeof buf, "%02x%02x%02x", (int)std::lround(a.r * 255), (int)std::lround(a.g * 255), (int)std::lround(a.b * 255));
        lua_pushstring(L, buf); return 1;
    }
    luaL_error(L, "%s is not a valid method of Color3", m ? m : "?");
}
static int c3_eq(lua_State* L) { lua_pushboolean(L, isColor3(L, 1) && isColor3(L, 2) && checkColor3(L, 1) == checkColor3(L, 2)); return 1; }
static int c3_tostring(lua_State* L) { Col3 c = checkColor3(L, 1); char b[96]; std::snprintf(b, sizeof b, "%g, %g, %g", c.r, c.g, c.b); lua_pushstring(L, b); return 1; }

// ---- BrickColor --------------------------------------------------------------------
// Roblox's numbered palette. An unknown name gives Medium stone grey, a Color3 the nearest entry.
struct BrickEntry { int number; const char* name; unsigned rgb; };
static const BrickEntry kBricks[] = {
    {1, "White", 0xF2F3F3}, {2, "Grey", 0xA1A5A2}, {3, "Light yellow", 0xF9E999}, {5, "Brick yellow", 0xD7C59A},
    {6, "Light green (Mint)", 0xC2DAB8}, {9, "Light reddish violet", 0xE8BAC8}, {11, "Pastel Blue", 0x80BBDB},
    {12, "Light orange brown", 0xCB8442}, {18, "Nougat", 0xCC8E69}, {21, "Bright red", 0xC4281C},
    {22, "Med. reddish violet", 0xC470A0}, {23, "Bright blue", 0x0D69AC}, {24, "Bright yellow", 0xF5CD30},
    {25, "Earth orange", 0x624732}, {26, "Black", 0x1B2A35}, {27, "Dark grey", 0x6D6E6C}, {28, "Dark green", 0x287F47},
    {29, "Medium green", 0xA1C48C}, {36, "Lig. Yellowich orange", 0xF3CF9B}, {37, "Bright green", 0x4B974B},
    {38, "Dark orange", 0xA05F35}, {39, "Light bluish violet", 0xC1CADE}, {40, "Transparent", 0xECECEC},
    {41, "Tr. Red", 0xCD544B}, {42, "Tr. Lg blue", 0xC1DFF0}, {43, "Tr. Blue", 0x7BB6E8}, {44, "Tr. Yellow", 0xF7F18D},
    {45, "Light blue", 0xB4D2E4}, {47, "Tr. Flu. Reddish orange", 0xD9856C}, {48, "Tr. Green", 0x84B68D},
    {49, "Tr. Flu. Green", 0xF8F184}, {50, "Phosph. White", 0xECE8DE}, {100, "Light red", 0xEEC4B6},
    {101, "Medium red", 0xDA867A}, {102, "Medium blue", 0x6E99CA}, {103, "Light grey", 0xC7C1B7},
    {104, "Bright violet", 0x6B327C}, {105, "Br. yellowish orange", 0xE29B40}, {106, "Bright orange", 0xDA8541},
    {107, "Bright bluish green", 0x008F9C}, {108, "Earth yellow", 0x685C43}, {110, "Bright bluish violet", 0x4354A3},
    {111, "Tr. Brown", 0xBFB7B1}, {112, "Medium bluish violet", 0x6874AC}, {113, "Tr. Medi. reddish violet", 0xE5ADC8},
    {115, "Med. yellowish green", 0xC7D23C}, {116, "Med. bluish green", 0x55A5AF}, {118, "Light bluish green", 0xB7D7D5},
    {119, "Br. yellowish green", 0xA4BD47}, {120, "Lig. yellowish green", 0xD9E4A7}, {121, "Med. yellowish orange", 0xE7AC58},
    {123, "Br. reddish orange", 0xD36F4C}, {124, "Bright reddish violet", 0x923978}, {125, "Light orange", 0xEAB892},
    {126, "Tr. Bright bluish violet", 0xA5A5CB}, {127, "Gold", 0xDCBC81}, {128, "Dark nougat", 0xAE7A59},
    {131, "Silver", 0x9CA3A8}, {133, "Neon orange", 0xD5733D}, {134, "Neon green", 0xD8DD56}, {135, "Sand blue", 0x74869D},
    {136, "Sand violet", 0x877C90}, {137, "Medium orange", 0xE09864}, {138, "Sand yellow", 0x958A73},
    {140, "Earth blue", 0x203A56}, {141, "Earth green", 0x27462D}, {143, "Tr. Flu. Blue", 0xCFE2F7},
    {145, "Sand blue metallic", 0x7988A1}, {146, "Sand violet metallic", 0x958EA3}, {147, "Sand yellow metallic", 0x938767},
    {148, "Dark grey metallic", 0x575857}, {149, "Black metallic", 0x161D32}, {150, "Light grey metallic", 0xABADAC},
    {151, "Sand green", 0x789082}, {153, "Sand red", 0x957977}, {154, "Dark red", 0x7B2E2F}, {157, "Tr. Flu. Yellow", 0xFFF67B},
    {158, "Tr. Flu. Red", 0xE1A4C2}, {168, "Gun metallic", 0x756C62}, {176, "Red flip/flop", 0x97695B},
    {178, "Yellow flip/flop", 0xB48455}, {179, "Silver flip/flop", 0x898788}, {180, "Curry", 0xD7A94B},
    {190, "Fire Yellow", 0xF9D62E}, {191, "Flame yellowish orange", 0xE8AB2D}, {192, "Reddish brown", 0x694028},
    {193, "Flame reddish orange", 0xCF6024}, {194, "Medium stone grey", 0xA3A2A5}, {195, "Royal blue", 0x4C61DB},
    {196, "Dark Royal blue", 0x23478B}, {198, "Bright reddish lilac", 0x8E4285}, {199, "Dark stone grey", 0x635F62},
    {200, "Lemon metalic", 0x828A00}, {208, "Light stone grey", 0xE5E4DF}, {209, "Dark Curry", 0xB08E44},
    {210, "Faded green", 0x709578}, {211, "Turquoise", 0x79B5B5}, {212, "Light Royal blue", 0x9FC3E9},
    {213, "Medium Royal blue", 0x6C81B7}, {216, "Rust", 0x8F4C2A}, {217, "Brown", 0x7C5C46}, {218, "Reddish lilac", 0x96709F},
    {219, "Lilac", 0x6B629B}, {220, "Light lilac", 0xA7A9CE}, {221, "Bright purple", 0xCD6298}, {222, "Light purple", 0xE4ADC8},
    {223, "Light pink", 0xDC9095}, {224, "Light brick yellow", 0xF0D5A0}, {225, "Warm yellowish orange", 0xEBB87F},
    {226, "Cool yellow", 0xFDEA8D}, {232, "Dove blue", 0x7DBBDD}, {268, "Medium lilac", 0x342B75},
    {302, "Smoky grey", 0x5B5D69}, {303, "Dark blue", 0x0016A5}, {304, "Parsley green", 0x2C651D}, {305, "Steel blue", 0x527CAE},
    {306, "Storm blue", 0x335882}, {308, "Dark indigo", 0x3D1585}, {309, "Sea green", 0x348E40}, {310, "Shamrock", 0x5B9A4C},
    {311, "Fossil", 0x999595}, {313, "Forest green", 0x1F801D}, {314, "Cadet blue", 0x9FADC0}, {315, "Electric blue", 0x0989CF},
    {316, "Eggplant", 0x7B007B}, {317, "Moss", 0x7C9C6B}, {318, "Artichoke", 0x8A9A79}, {320, "Ghost grey", 0xCBCBCB},
    {322, "Plum", 0x7B2F7B}, {323, "Olivine", 0x94BE81}, {324, "Laurel green", 0xA8BD99}, {325, "Quill grey", 0xDFDFDE},
    {327, "Crimson", 0x970000}, {328, "Mint", 0xB1E5A6}, {329, "Baby blue", 0x98C2DB}, {330, "Carnation pink", 0xFF98DC},
    {331, "Persimmon", 0xFF5959}, {332, "Maroon", 0x750000}, {333, "Gold", 0xEFB838}, {334, "Daisy orange", 0xF8D96D},
    {335, "Pearl", 0xE7E7EC}, {336, "Fog", 0xC7D4E4}, {337, "Salmon", 0xFF8080}, {339, "Cocoa", 0x562B2B},
    {340, "Wheat", 0xF1E7C7}, {341, "Buttermilk", 0xFEF3BB}, {342, "Mauve", 0xE0B2D0}, {344, "Tawny", 0x966766},
    {348, "Lily white", 0xEDEDED}, {352, "Burlap", 0xC7AC78}, {353, "Beige", 0xCABFA3}, {354, "Oyster", 0xBBB3A2},
    {355, "Pine Cone", 0x6C584B}, {356, "Fawn brown", 0xA0844F}, {358, "Cloudy grey", 0xABA89E}, {359, "Linen", 0xAF9483},
    {361, "Dirt brown", 0x564236}, {364, "Dark taupe", 0x5A4C42},
    {1001, "Institutional white", 0xF8F8F8}, {1002, "Mid gray", 0xCDCDCD}, {1003, "Really black", 0x111111},
    {1004, "Really red", 0xFF0000}, {1005, "Deep orange", 0xFFB000}, {1006, "Alder", 0xB480FF}, {1007, "Dusty Rose", 0xA34B4B},
    {1008, "Olive", 0xC1BE42}, {1009, "New Yeller", 0xFFFF00}, {1010, "Really blue", 0x0000FF}, {1011, "Navy blue", 0x002060},
    {1012, "Deep blue", 0x2154B9}, {1013, "Cyan", 0x04AFEC}, {1014, "CGA brown", 0xAA5500}, {1015, "Magenta", 0xAA00FF},
    {1016, "Pink", 0xFF66CC}, {1017, "Deep orange", 0xFFAF00}, {1018, "Teal", 0x12EEFF}, {1019, "Toothpaste", 0x00FFFF},
    {1020, "Lime green", 0x00FF00}, {1021, "Camo", 0x3A7D15}, {1022, "Grime", 0x7F8E64}, {1023, "Lavender", 0x8C5B9F},
    {1024, "Pastel light blue", 0xAFDDFF}, {1025, "Pastel orange", 0xFFC9C9}, {1026, "Pastel violet", 0xB1A7FF},
    {1027, "Pastel blue-green", 0x9FF3E9}, {1028, "Pastel green", 0xCCFFCC}, {1029, "Pastel yellow", 0xFFFFCC},
    {1030, "Pastel brown", 0xFFCC99}, {1031, "Royal purple", 0x6225D1}, {1032, "Hot pink", 0xFF00BF},
};
static constexpr int kBrickDefault = 194;   // Medium stone grey
static Col3 brickCol(const BrickEntry& e) { return {((e.rgb >> 16) & 255) / 255.f, ((e.rgb >> 8) & 255) / 255.f, (e.rgb & 255) / 255.f}; }
static const BrickEntry& brickByNumber(int n) {
    for (const BrickEntry& e : kBricks) if (e.number == n) return e;
    return brickByNumber(kBrickDefault);
}
const BrickEntry* brickByName(const char* name) {
    for (const BrickEntry& e : kBricks) if (!std::strcmp(e.name, name)) return &e;
    return nullptr;
}
int brickNearest(Col3 c) {
    int best = kBrickDefault; float bestD = 1e9f;
    for (const BrickEntry& e : kBricks) {
        Col3 k = brickCol(e);
        float d = (k.r - c.r) * (k.r - c.r) + (k.g - c.g) * (k.g - c.g) + (k.b - c.b) * (k.b - c.b);
        if (d < bestD) { bestD = d; best = e.number; }
    }
    return best;
}
Col3 brickColor(int number) { return brickCol(brickByNumber(number)); }
const char* brickName(int number) { return brickByNumber(number).name; }
int brickNumber(const char* name) { const BrickEntry* e = brickByName(name); return e ? e->number : kBrickDefault; }
struct BrickColorUD { int number; };
bool isBrickColor(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_BRICKCOLOR; }
int checkBrickColor(lua_State* L, int idx) {
    if (!isBrickColor(L, idx)) luaL_typeerror(L, idx, "BrickColor");
    return static_cast<BrickColorUD*>(lua_touserdata(L, idx))->number;
}
void pushBrickColor(lua_State* L, int number) {
    auto* ud = static_cast<BrickColorUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(BrickColorUD), TAG_BRICKCOLOR));
    ud->number = brickByNumber(number).number;
}
static int bc_new(lua_State* L) {
    if (lua_type(L, 1) == LUA_TSTRING) { pushBrickColor(L, brickNumber(lua_tostring(L, 1))); return 1; }
    if (isColor3(L, 1)) { pushBrickColor(L, brickNearest(checkColor3(L, 1))); return 1; }
    if (lua_gettop(L) >= 3) { pushBrickColor(L, brickNearest({(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)})); return 1; }
    if (lua_type(L, 1) == LUA_TNUMBER) { pushBrickColor(L, (int)lua_tonumber(L, 1)); return 1; }
    luaL_error(L, "BrickColor.new takes a name, a number, a Color3 or r, g, b");
}
static int bc_palette(lua_State* L) {
    int i = (int)luaL_checknumber(L, 1);
    constexpr int n = (int)(sizeof kBricks / sizeof kBricks[0]);
    if (i < 0 || i >= n) luaL_error(L, "palette index out of range");
    pushBrickColor(L, kBricks[i].number); return 1;
}
static int bc_random(lua_State* L) {
    constexpr int n = (int)(sizeof kBricks / sizeof kBricks[0]);
    pushBrickColor(L, kBricks[std::rand() % n].number); return 1;
}
static int bc_index(lua_State* L) {
    int n = checkBrickColor(L, 1);
    const char* k = luaL_checkstring(L, 2);
    Col3 c = brickColor(n);
    if (!std::strcmp(k, "Name")) { lua_pushstring(L, brickName(n)); return 1; }
    if (!std::strcmp(k, "Number")) { lua_pushnumber(L, n); return 1; }
    if (!std::strcmp(k, "Color")) { pushColor3(L, c); return 1; }
    if (!std::strcmp(k, "r")) { lua_pushnumber(L, c.r); return 1; }
    if (!std::strcmp(k, "g")) { lua_pushnumber(L, c.g); return 1; }
    if (!std::strcmp(k, "b")) { lua_pushnumber(L, c.b); return 1; }
    luaL_error(L, "%s is not a valid member of BrickColor", k);
}
static int bc_eq(lua_State* L) { lua_pushboolean(L, isBrickColor(L, 1) && isBrickColor(L, 2) && checkBrickColor(L, 1) == checkBrickColor(L, 2)); return 1; }
static int bc_tostring(lua_State* L) { lua_pushstring(L, brickName(checkBrickColor(L, 1))); return 1; }
template <int N> static int bc_named(lua_State* L) { pushBrickColor(L, N); return 1; }

static void setMeta(lua_State* L, int tag, const char* type, lua_CFunction index, lua_CFunction namecall,
                    lua_CFunction tostring, lua_CFunction eq, std::initializer_list<std::pair<const char*, lua_CFunction>> extra = {}) {
    lua_newtable(L);
    if (index) { lua_pushcfunction(L, index, "__index"); lua_setfield(L, -2, "__index"); }
    if (namecall) { lua_pushcfunction(L, namecall, "__namecall"); lua_setfield(L, -2, "__namecall"); }
    if (tostring) { lua_pushcfunction(L, tostring, "__tostring"); lua_setfield(L, -2, "__tostring"); }
    if (eq) { lua_pushcfunction(L, eq, "__eq"); lua_setfield(L, -2, "__eq"); }
    for (auto& [k, f] : extra) { lua_pushcfunction(L, f, k); lua_setfield(L, -2, k); }
    lua_pushstring(L, type); lua_setfield(L, -2, "__type");
    lua_pushstring(L, "The metatable is locked"); lua_setfield(L, -2, "__metatable");
    lua_setreadonly(L, -1, true);
    lua_setuserdatametatable(L, tag);
}

// ---- Content ---------------------------------------------------------------------------
// An asset uri, or an Object the Content keeps alive (an EditableImage a script drew into).
struct ContentUD { int source; std::string uri; Instance::Ptr object; };
struct OpaqueUD { Instance::Ptr holder; };
static int opaque_tostring(lua_State* L) { lua_pushstring(L, "Opaque"); return 1; }
static int opaque_eq(lua_State* L) {
    bool same = lua_userdatatag(L, 1) == TAG_OPAQUE && lua_userdatatag(L, 2) == TAG_OPAQUE &&
                static_cast<OpaqueUD*>(lua_touserdata(L, 1))->holder == static_cast<OpaqueUD*>(lua_touserdata(L, 2))->holder;
    lua_pushboolean(L, same);
    return 1;
}
static int opaque_index(lua_State* L) { luaL_error(L, "%s is not a valid member of Opaque", luaL_checkstring(L, 2)); }
bool pushOpaqueContent(lua_State* L, Instance::Ptr holder) {
    if (!holder) return false;
    auto* c = static_cast<ContentUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(ContentUD), TAG_CONTENT));
    new (c) ContentUD{Value::ContentOpaque, "", std::move(holder)};
    return true;
}
bool isContent(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_CONTENT; }
static ContentUD* contentUD(lua_State* L, int idx) {
    if (!isContent(L, idx)) luaL_typeerror(L, idx, "Content");
    return static_cast<ContentUD*>(lua_touserdata(L, idx));
}
Value checkContent(lua_State* L, int idx) {
    ContentUD* c = contentUD(L, idx);
    if ((c->source == Value::ContentObject || c->source == Value::ContentOpaque) && c->object) return Value::content(c->source, c->object->className(), c->object->id());
    return Value::content(c->source, c->uri, c->object ? c->object->id() : 0);
}
static void newContent(lua_State* L, int source, std::string uri, Instance::Ptr object) {
    auto* c = static_cast<ContentUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(ContentUD), TAG_CONTENT));
    new (c) ContentUD{source, std::move(uri), std::move(object)};
}
void pushContent(lua_State* L, const Value& v) {
    Instance::Ptr object;
    if ((v.n == Value::ContentObject || v.n == Value::ContentOpaque) && v.ref) if (Instance* i = rtOf(L).dm.find(v.ref)) object = i->shared_from_this();
    if ((v.n == Value::ContentObject || v.n == Value::ContentOpaque) && !object) { newContent(L, Value::ContentNone, "", nullptr); return; }
    newContent(L, (int)v.n, v.n == Value::ContentUri ? v.s : "", std::move(object));   // an object's Value carries its class in s, not a uri
}
static int content_fromUri(lua_State* L) {
    size_t n; const char* uri = luaL_checklstring(L, 1, &n);
    newContent(L, n ? Value::ContentUri : Value::ContentNone, std::string(uri, n), nullptr);
    return 1;
}
static int content_fromAssetId(lua_State* L) {
    double id = luaL_checknumber(L, 1);
    if (!std::isfinite(id)) luaL_error(L, "Content.fromAssetId: asset id must be finite");
    if (id == 0) { newContent(L, Value::ContentNone, "", nullptr); return 1; }
    lua_pushnumber(L, id);
    std::string uri = std::string("rbxassetid://") + lua_tostring(L, -1);   // tostring(assetId), as Luau spells the number
    lua_pop(L, 1);
    newContent(L, Value::ContentUri, std::move(uri), nullptr);
    return 1;
}
static int content_fromObject(lua_State* L) {
    if (lua_isnoneornil(L, 1)) luaL_error(L, "Content.fromObject: object must not be nil");
    if (lua_userdatatag(L, 1) != TAG_INSTANCE && lua_userdatatag(L, 1) != TAG_OBJECT) luaL_typeerror(L, 1, "Object");
    Instance::Ptr object = static_cast<InstanceUD*>(lua_touserdata(L, 1))->inst;
    if (!object) luaL_error(L, "Content.fromObject: object must not be nil");
    newContent(L, Value::ContentObject, "", std::move(object));
    return 1;
}
static int content_index(lua_State* L) {
    ContentUD* c = contentUD(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "SourceType")) {
        const EnumDef* e = findEnum("ContentSourceType");
        pushEnumItem(L, e, e->findValue(c->source));
        return 1;
    }
    if (!std::strcmp(k, "Uri")) { if (c->source == Value::ContentUri) lua_pushlstring(L, c->uri.data(), c->uri.size()); else lua_pushnil(L); return 1; }
    if (!std::strcmp(k, "Object")) { if (c->source == Value::ContentObject && c->object) rtOf(L).pushInstance(L, c->object.get()); else lua_pushnil(L); return 1; }
    if (!std::strcmp(k, "Opaque")) {
        if (c->source != Value::ContentOpaque || !c->object) { lua_pushnil(L); return 1; }
        auto* o = static_cast<OpaqueUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(OpaqueUD), TAG_OPAQUE));
        new (o) OpaqueUD{c->object};
        return 1;
    }
    luaL_error(L, "%s is not a valid member of Content", k);
}
static int content_newindex(lua_State* L) {
    (void)contentUD(L, 1);
    luaL_error(L, "%s cannot be assigned to", luaL_checkstring(L, 2));
}
static int content_namecall(lua_State* L) { int atom; const char* m = lua_namecallatom(L, &atom); luaL_error(L, "%s is not a valid method of Content", m ? m : "?"); }
static int content_eq(lua_State* L) {
    if (!isContent(L, 1) || !isContent(L, 2)) { lua_pushboolean(L, false); return 1; }
    ContentUD* a = contentUD(L, 1); ContentUD* b = contentUD(L, 2);
    lua_pushboolean(L, a->source == b->source && a->uri == b->uri && a->object == b->object);
    return 1;
}
static int content_tostring(lua_State* L) {
    ContentUD* c = contentUD(L, 1);
    if (c->source == Value::ContentUri) lua_pushlstring(L, c->uri.data(), c->uri.size());
    else if (c->source == Value::ContentObject && c->object) lua_pushstring(L, c->object->name().c_str());
    else lua_pushstring(L, "");
    return 1;
}
static void openOpaque(lua_State* L) {
    setMeta(L, TAG_OPAQUE, "Opaque", opaque_index, nullptr, opaque_tostring, opaque_eq, {});
    lua_setuserdatadtor(L, TAG_OPAQUE, [](lua_State*, void* p) { static_cast<OpaqueUD*>(p)->~OpaqueUD(); });
}
static void openContent(lua_State* L) {
    openOpaque(L);
    setMeta(L, TAG_CONTENT, "Content", content_index, content_namecall, content_tostring, content_eq, {{"__newindex", content_newindex}});
    lua_setuserdatadtor(L, TAG_CONTENT, [](lua_State*, void* p) { static_cast<ContentUD*>(p)->~ContentUD(); });
    lua_newtable(L);
    lua_pushcfunction(L, content_fromUri, "Content.fromUri"); lua_setfield(L, -2, "fromUri");
    lua_pushcfunction(L, content_fromAssetId, "Content.fromAssetId"); lua_setfield(L, -2, "fromAssetId");
    lua_pushcfunction(L, content_fromObject, "Content.fromObject"); lua_setfield(L, -2, "fromObject");
    newContent(L, Value::ContentNone, "", nullptr); lua_setfield(L, -2, "none");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Content");
}

static void openVector2(lua_State* L) {
    setMeta(L, TAG_VECTOR2, "Vector2", v2_index, v2_namecall, v2_tostring, v2_eq,
            {{"__add", v2_add}, {"__sub", v2_sub}, {"__mul", v2_mul}, {"__div", v2_div}, {"__unm", v2_unm}});
    lua_newtable(L);
    lua_pushcfunction(L, v2_new, "Vector2.new"); lua_setfield(L, -2, "new");
    pushVector2(L, {0, 0}); lua_setfield(L, -2, "zero");
    pushVector2(L, {1, 1}); lua_setfield(L, -2, "one");
    pushVector2(L, {1, 0}); lua_setfield(L, -2, "xAxis");
    pushVector2(L, {0, 1}); lua_setfield(L, -2, "yAxis");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Vector2");
}

static void openUDim(lua_State* L) {
    setMeta(L, TAG_UDIM, "UDim", ud_index, ud_namecall, ud_tostring, ud_eq, {{"__add", ud_add}, {"__sub", ud_sub}});
    lua_newtable(L);
    lua_pushcfunction(L, ud_new, "UDim.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "UDim");
    setMeta(L, TAG_UDIM2, "UDim2", ud2_index, ud2_namecall, ud2_tostring, ud2_eq, {{"__add", ud2_add}, {"__sub", ud2_sub}});
    lua_newtable(L);
    lua_pushcfunction(L, ud2_new, "UDim2.new"); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, ud2_fromScale, "UDim2.fromScale"); lua_setfield(L, -2, "fromScale");
    lua_pushcfunction(L, ud2_fromOffset, "UDim2.fromOffset"); lua_setfield(L, -2, "fromOffset");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "UDim2");
}

static void openColor3(lua_State* L) {
    setMeta(L, TAG_COLOR3, "Color3", c3_index, c3_namecall, c3_tostring, c3_eq);
    lua_newtable(L);
    lua_pushcfunction(L, c3_new, "Color3.new"); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, c3_fromRGB, "Color3.fromRGB"); lua_setfield(L, -2, "fromRGB");
    lua_pushcfunction(L, c3_fromHSV, "Color3.fromHSV"); lua_setfield(L, -2, "fromHSV");
    lua_pushcfunction(L, c3_fromHex, "Color3.fromHex"); lua_setfield(L, -2, "fromHex");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Color3");
    setMeta(L, TAG_BRICKCOLOR, "BrickColor", bc_index, nullptr, bc_tostring, bc_eq);
    lua_newtable(L);
    lua_pushcfunction(L, bc_new, "BrickColor.new"); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, bc_new, "BrickColor.New"); lua_setfield(L, -2, "New");
    lua_pushcfunction(L, bc_palette, "BrickColor.palette"); lua_setfield(L, -2, "palette");
    lua_pushcfunction(L, bc_random, "BrickColor.random"); lua_setfield(L, -2, "random");
    lua_pushcfunction(L, bc_random, "BrickColor.Random"); lua_setfield(L, -2, "Random");
    lua_pushcfunction(L, bc_named<1>, "BrickColor.White"); lua_setfield(L, -2, "White");
    lua_pushcfunction(L, bc_named<26>, "BrickColor.Black"); lua_setfield(L, -2, "Black");
    lua_pushcfunction(L, bc_named<21>, "BrickColor.Red"); lua_setfield(L, -2, "Red");
    lua_pushcfunction(L, bc_named<23>, "BrickColor.Blue"); lua_setfield(L, -2, "Blue");
    lua_pushcfunction(L, bc_named<28>, "BrickColor.Green"); lua_setfield(L, -2, "Green");
    lua_pushcfunction(L, bc_named<24>, "BrickColor.Yellow"); lua_setfield(L, -2, "Yellow");
    lua_pushcfunction(L, bc_named<194>, "BrickColor.Gray"); lua_setfield(L, -2, "Gray");
    lua_pushcfunction(L, bc_named<199>, "BrickColor.DarkGray"); lua_setfield(L, -2, "DarkGray");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "BrickColor");
}

// ---- CFrame ------------------------------------------------------------------------
static CFrameV fromCols(Vec3 x, Vec3 y, Vec3 z, Vec3 p) {
    CFrameV c; c.p = p;
    c.m[0] = x.x; c.m[3] = x.y; c.m[6] = x.z;
    c.m[1] = y.x; c.m[4] = y.y; c.m[7] = y.z;
    c.m[2] = z.x; c.m[5] = z.y; c.m[8] = z.z;
    return c;
}
static CFrameV rotX(float a) { float c = std::cos(a), s = std::sin(a); CFrameV r; float m[9] = {1, 0, 0, 0, c, -s, 0, s, c}; std::memcpy(r.m, m, sizeof m); return r; }
static CFrameV rotY(float a) { float c = std::cos(a), s = std::sin(a); CFrameV r; float m[9] = {c, 0, s, 0, 1, 0, -s, 0, c}; std::memcpy(r.m, m, sizeof m); return r; }
static CFrameV rotZ(float a) { float c = std::cos(a), s = std::sin(a); CFrameV r; float m[9] = {c, -s, 0, s, c, 0, 0, 0, 1}; std::memcpy(r.m, m, sizeof m); return r; }

CFrameV CFrameV::anglesXYZ(float rx, float ry, float rz) { return rotX(rx) * rotY(ry) * rotZ(rz); }
CFrameV CFrameV::anglesYXZ(float rx, float ry, float rz) { return rotY(ry) * rotX(rx) * rotZ(rz); }
CFrameV CFrameV::axisAngle(Vec3 axis, float a) {
    Vec3 u = unit(axis); float c = std::cos(a), s = std::sin(a), t = 1 - c;
    CFrameV r;
    r.m[0] = t * u.x * u.x + c;       r.m[1] = t * u.x * u.y - s * u.z; r.m[2] = t * u.x * u.z + s * u.y;
    r.m[3] = t * u.x * u.y + s * u.z; r.m[4] = t * u.y * u.y + c;       r.m[5] = t * u.y * u.z - s * u.x;
    r.m[6] = t * u.x * u.z - s * u.y; r.m[7] = t * u.y * u.z + s * u.x; r.m[8] = t * u.z * u.z + c;
    return r;
}
CFrameV CFrameV::lookAt(Vec3 pos, Vec3 target, Vec3 up) {
    Vec3 z = unit(sub(pos, target));               // -look
    if (len(z) == 0) return fromPos(pos);
    Vec3 x = cross(up, z);
    if (len(x) < 1e-6f) x = cross(Vec3{0, 0, 1}, z);   // looking straight up/down
    x = unit(x);
    Vec3 y = cross(z, x);
    return fromCols(x, y, z, pos);
}
CFrameV CFrameV::operator*(const CFrameV& o) const {
    CFrameV r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r.m[i * 3 + j] = m[i * 3] * o.m[j] + m[i * 3 + 1] * o.m[3 + j] + m[i * 3 + 2] * o.m[6 + j];
    r.p = add(rotate(o.p), p);
    return r;
}
Vec3 CFrameV::rotate(Vec3 v) const {
    return {m[0] * v.x + m[1] * v.y + m[2] * v.z, m[3] * v.x + m[4] * v.y + m[5] * v.z, m[6] * v.x + m[7] * v.y + m[8] * v.z};
}
Vec3 CFrameV::operator*(Vec3 v) const { return add(rotate(v), p); }
CFrameV CFrameV::inverse() const {
    CFrameV r;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) r.m[i * 3 + j] = m[j * 3 + i];
    Vec3 q = r.rotate(p);
    r.p = {-q.x, -q.y, -q.z};
    return r;
}
static void toQuat(const CFrameV& c, float q[4]) {
    const float* m = c.m;
    float tr = m[0] + m[4] + m[8];
    if (tr > 0) {
        float s = std::sqrt(tr + 1) * 2;
        q[3] = 0.25f * s; q[0] = (m[7] - m[5]) / s; q[1] = (m[2] - m[6]) / s; q[2] = (m[3] - m[1]) / s;
    } else if (m[0] > m[4] && m[0] > m[8]) {
        float s = std::sqrt(1 + m[0] - m[4] - m[8]) * 2;
        q[3] = (m[7] - m[5]) / s; q[0] = 0.25f * s; q[1] = (m[1] + m[3]) / s; q[2] = (m[2] + m[6]) / s;
    } else if (m[4] > m[8]) {
        float s = std::sqrt(1 + m[4] - m[0] - m[8]) * 2;
        q[3] = (m[2] - m[6]) / s; q[0] = (m[1] + m[3]) / s; q[1] = 0.25f * s; q[2] = (m[5] + m[7]) / s;
    } else {
        float s = std::sqrt(1 + m[8] - m[0] - m[4]) * 2;
        q[3] = (m[3] - m[1]) / s; q[0] = (m[2] + m[6]) / s; q[1] = (m[5] + m[7]) / s; q[2] = 0.25f * s;
    }
}
static CFrameV fromQuat(const float q[4], Vec3 p) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    CFrameV c; c.p = p;
    c.m[0] = 1 - 2 * (y * y + z * z); c.m[1] = 2 * (x * y - z * w);     c.m[2] = 2 * (x * z + y * w);
    c.m[3] = 2 * (x * y + z * w);     c.m[4] = 1 - 2 * (x * x + z * z); c.m[5] = 2 * (y * z - x * w);
    c.m[6] = 2 * (x * z - y * w);     c.m[7] = 2 * (y * z + x * w);     c.m[8] = 1 - 2 * (x * x + y * y);
    return c;
}
CFrameV CFrameV::lerp(const CFrameV& b, float t) const {
    float qa[4], qb[4], q[4];
    toQuat(*this, qa); toQuat(b, qb);
    float d = qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3];
    if (d < 0) { d = -d; for (float& f : qb) f = -f; }
    float wa, wb;
    if (d > 0.9995f) { wa = 1 - t; wb = t; }
    else { float th = std::acos(d), s = std::sin(th); wa = std::sin((1 - t) * th) / s; wb = std::sin(t * th) / s; }
    float n = 0;
    for (int i = 0; i < 4; i++) { q[i] = wa * qa[i] + wb * qb[i]; n += q[i] * q[i]; }
    n = std::sqrt(n); for (float& f : q) f /= n;
    return fromQuat(q, add(p, mul(sub(b.p, p), t)));
}
void CFrameV::toEulerXYZ(float& x, float& y, float& z) const {
    y = std::asin(std::fmax(-1.f, std::fmin(1.f, m[2])));
    x = std::atan2(-m[5], m[8]);
    z = std::atan2(-m[1], m[0]);
}
void CFrameV::toEulerYXZ(float& x, float& y, float& z) const {
    x = std::asin(std::fmax(-1.f, std::fmin(1.f, -m[5])));
    y = std::atan2(m[2], m[8]);
    z = std::atan2(m[3], m[4]);
}
bool CFrameV::operator==(const CFrameV& o) const {
    if (!(p == o.p)) return false;
    for (int i = 0; i < 9; i++) if (m[i] != o.m[i]) return false;
    return true;
}
CFrameV cframeFromPosOrient(Vec3 pos, Vec3 o) {
    CFrameV c = CFrameV::anglesYXZ(o.x * PI / 180, o.y * PI / 180, o.z * PI / 180);
    c.p = pos;
    return c;
}
void cframeToPosOrient(const CFrameV& c, Vec3& pos, Vec3& o) {
    pos = c.p;
    float x, y, z; c.toEulerYXZ(x, y, z);
    o = {x * 180 / PI + 0.0f, y * 180 / PI + 0.0f, z * 180 / PI + 0.0f};   // + 0.0: no -0 in an Orientation
}

struct CFrameUD { CFrameV c; };
bool isCFrame(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_CFRAME; }
const CFrameV& checkCFrame(lua_State* L, int idx) {
    if (!isCFrame(L, idx)) luaL_typeerror(L, idx, "CFrame");
    return static_cast<CFrameUD*>(lua_touserdata(L, idx))->c;
}
void pushCFrame(lua_State* L, const CFrameV& c) {
    auto* ud = static_cast<CFrameUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(CFrameUD), TAG_CFRAME));
    ud->c = c;
}
static int cf_new(lua_State* L) {
    int n = lua_gettop(L);
    if (n == 0) { pushCFrame(L, CFrameV::identity()); return 1; }
    if (lua_isvector(L, 1)) {
        if (n >= 2 && lua_isvector(L, 2)) pushCFrame(L, CFrameV::lookAt(checkVec3(L, 1), checkVec3(L, 2)));
        else pushCFrame(L, CFrameV::fromPos(checkVec3(L, 1)));
        return 1;
    }
    Vec3 p = {(float)luaL_checknumber(L, 1), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0)};
    if (n == 7) {
        float q[4] = {(float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6), (float)luaL_checknumber(L, 7)};
        pushCFrame(L, fromQuat(q, p)); return 1;
    }
    if (n == 12) {
        CFrameV c; c.p = p;
        for (int i = 0; i < 9; i++) c.m[i] = (float)luaL_checknumber(L, 4 + i);
        pushCFrame(L, c); return 1;
    }
    pushCFrame(L, CFrameV::fromPos(p));
    return 1;
}
static int cf_angles(lua_State* L) { pushCFrame(L, CFrameV::anglesXYZ((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3))); return 1; }
static int cf_anglesYXZ(lua_State* L) { pushCFrame(L, CFrameV::anglesYXZ((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3))); return 1; }
static int cf_axisAngle(lua_State* L) { pushCFrame(L, CFrameV::axisAngle(checkVec3(L, 1), (float)luaL_checknumber(L, 2))); return 1; }
static int cf_lookAt(lua_State* L) {
    Vec3 up = lua_isvector(L, 3) ? checkVec3(L, 3) : Vec3{0, 1, 0};
    pushCFrame(L, CFrameV::lookAt(checkVec3(L, 1), checkVec3(L, 2), up)); return 1;
}
// The three columns go in as given: Roblox does not orthonormalise them.
static int cf_fromMatrix(lua_State* L) {
    CFrameV c;
    c.p = checkVec3(L, 1);
    Vec3 x = checkVec3(L, 2), y = checkVec3(L, 3);
    Vec3 z = lua_isvector(L, 4) ? checkVec3(L, 4) : unit(cross(x, y));
    const Vec3 cols[3] = {x, y, z};
    for (int k = 0; k < 3; k++) { c.m[k] = cols[k].x; c.m[3 + k] = cols[k].y; c.m[6 + k] = cols[k].z; }
    pushCFrame(L, c); return 1;
}
static int cf_lookAlong(lua_State* L) {
    Vec3 pos = checkVec3(L, 1), dir = checkVec3(L, 2);
    Vec3 up = lua_isvector(L, 3) ? checkVec3(L, 3) : Vec3{0, 1, 0};
    pushCFrame(L, CFrameV::lookAt(pos, add(pos, dir), up)); return 1;
}
static int cf_index(lua_State* L) {
    const CFrameV& c = checkCFrame(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Position") || !std::strcmp(k, "p")) { pushVec3(L, c.p); return 1; }
    if (!std::strcmp(k, "X") || !std::strcmp(k, "x")) { lua_pushnumber(L, c.p.x); return 1; }
    if (!std::strcmp(k, "Y") || !std::strcmp(k, "y")) { lua_pushnumber(L, c.p.y); return 1; }
    if (!std::strcmp(k, "Z") || !std::strcmp(k, "z")) { lua_pushnumber(L, c.p.z); return 1; }
    if (!std::strcmp(k, "LookVector") || !std::strcmp(k, "lookVector")) { pushVec3(L, c.look()); return 1; }
    if (!std::strcmp(k, "RightVector") || !std::strcmp(k, "XVector")) { pushVec3(L, c.col(0)); return 1; }
    if (!std::strcmp(k, "UpVector") || !std::strcmp(k, "YVector")) { pushVec3(L, c.col(1)); return 1; }
    if (!std::strcmp(k, "ZVector")) { pushVec3(L, c.col(2)); return 1; }
    if (!std::strcmp(k, "Rotation")) { CFrameV r = c; r.p = {}; pushCFrame(L, r); return 1; }
    luaL_error(L, "%s is not a valid member of CFrame", k);
}
static int cf_namecall(lua_State* L) {
    const CFrameV& c = checkCFrame(L, 1);
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (!m) luaL_error(L, "invalid method call");
    if (!std::strcmp(m, "Inverse")) { pushCFrame(L, c.inverse()); return 1; }
    if (!std::strcmp(m, "Lerp")) { pushCFrame(L, c.lerp(checkCFrame(L, 2), (float)luaL_checknumber(L, 3))); return 1; }
    if (!std::strcmp(m, "ToWorldSpace")) { int n = lua_gettop(L); for (int i = 2; i <= n; i++) pushCFrame(L, c * checkCFrame(L, i)); return n - 1; }
    if (!std::strcmp(m, "ToObjectSpace")) { int n = lua_gettop(L); CFrameV inv = c.inverse(); for (int i = 2; i <= n; i++) pushCFrame(L, inv * checkCFrame(L, i)); return n - 1; }
    if (!std::strcmp(m, "PointToWorldSpace")) { int n = lua_gettop(L); for (int i = 2; i <= n; i++) pushVec3(L, c * checkVec3(L, i)); return n - 1; }
    if (!std::strcmp(m, "PointToObjectSpace")) { int n = lua_gettop(L); CFrameV inv = c.inverse(); for (int i = 2; i <= n; i++) pushVec3(L, inv * checkVec3(L, i)); return n - 1; }
    if (!std::strcmp(m, "VectorToWorldSpace")) { int n = lua_gettop(L); for (int i = 2; i <= n; i++) pushVec3(L, c.rotate(checkVec3(L, i))); return n - 1; }
    if (!std::strcmp(m, "VectorToObjectSpace")) { int n = lua_gettop(L); CFrameV inv = c.inverse(); for (int i = 2; i <= n; i++) pushVec3(L, inv.rotate(checkVec3(L, i))); return n - 1; }
    if (!std::strcmp(m, "ToEulerAnglesXYZ")) { float x, y, z; c.toEulerXYZ(x, y, z); lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); return 3; }
    if (!std::strcmp(m, "ToEulerAnglesYXZ") || !std::strcmp(m, "ToOrientation")) { float x, y, z; c.toEulerYXZ(x, y, z); lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); return 3; }
    if (!std::strcmp(m, "ToAxisAngle")) {
        float q[4]; toQuat(c, q);
        float s = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
        float ang = 2 * std::atan2(s, q[3]);
        pushVec3(L, s > 1e-6f ? Vec3{q[0] / s, q[1] / s, q[2] / s} : Vec3{1, 0, 0});
        lua_pushnumber(L, ang); return 2;
    }
    if (!std::strcmp(m, "GetComponents") || !std::strcmp(m, "components")) {
        lua_pushnumber(L, c.p.x); lua_pushnumber(L, c.p.y); lua_pushnumber(L, c.p.z);
        for (int i = 0; i < 9; i++) lua_pushnumber(L, c.m[i]);
        return 12;
    }
    if (!std::strcmp(m, "FuzzyEq")) {
        const CFrameV& o = checkCFrame(L, 2); float e = (float)luaL_optnumber(L, 3, 1e-5);
        bool ok = len(sub(c.p, o.p)) <= e;
        for (int i = 0; ok && i < 9; i++) ok = std::fabs(c.m[i] - o.m[i]) <= e;
        lua_pushboolean(L, ok); return 1;
    }
    luaL_error(L, "%s is not a valid method of CFrame", m);
}
static int cf_mul(lua_State* L) {
    const CFrameV& a = checkCFrame(L, 1);
    if (lua_isvector(L, 2)) { pushVec3(L, a * checkVec3(L, 2)); return 1; }
    pushCFrame(L, a * checkCFrame(L, 2)); return 1;
}
static int cf_add(lua_State* L) { CFrameV c = checkCFrame(L, 1); c.p = add(c.p, checkVec3(L, 2)); pushCFrame(L, c); return 1; }
static int cf_sub(lua_State* L) { CFrameV c = checkCFrame(L, 1); c.p = sub(c.p, checkVec3(L, 2)); pushCFrame(L, c); return 1; }
static int cf_eq(lua_State* L) { lua_pushboolean(L, isCFrame(L, 1) && isCFrame(L, 2) && checkCFrame(L, 1) == checkCFrame(L, 2)); return 1; }
static int cf_tostring(lua_State* L) {
    const CFrameV& c = checkCFrame(L, 1);
    char b[256];
    std::snprintf(b, sizeof b, "%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g", c.p.x, c.p.y, c.p.z,
                  c.m[0], c.m[1], c.m[2], c.m[3], c.m[4], c.m[5], c.m[6], c.m[7], c.m[8]);
    lua_pushstring(L, b); return 1;
}
static void openCFrame(lua_State* L) {
    setMeta(L, TAG_CFRAME, "CFrame", cf_index, cf_namecall, cf_tostring, cf_eq, {{"__mul", cf_mul}, {"__add", cf_add}, {"__sub", cf_sub}});
    lua_newtable(L);
    lua_pushcfunction(L, cf_new, "CFrame.new"); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, cf_angles, "CFrame.Angles"); lua_setfield(L, -2, "Angles");
    lua_pushcfunction(L, cf_angles, "CFrame.fromEulerAnglesXYZ"); lua_setfield(L, -2, "fromEulerAnglesXYZ");
    lua_pushcfunction(L, cf_anglesYXZ, "CFrame.fromEulerAnglesYXZ"); lua_setfield(L, -2, "fromEulerAnglesYXZ");
    lua_pushcfunction(L, cf_anglesYXZ, "CFrame.fromOrientation"); lua_setfield(L, -2, "fromOrientation");
    lua_pushcfunction(L, cf_axisAngle, "CFrame.fromAxisAngle"); lua_setfield(L, -2, "fromAxisAngle");
    lua_pushcfunction(L, cf_lookAt, "CFrame.lookAt"); lua_setfield(L, -2, "lookAt");
    lua_pushcfunction(L, cf_lookAlong, "CFrame.lookAlong"); lua_setfield(L, -2, "lookAlong");
    lua_pushcfunction(L, cf_fromMatrix, "CFrame.fromMatrix"); lua_setfield(L, -2, "fromMatrix");
    pushCFrame(L, CFrameV::identity()); lua_setfield(L, -2, "identity");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "CFrame");
}

// ---- Enum --------------------------------------------------------------------------
struct EnumUD { const EnumDef* e; };
struct EnumItemUD { const EnumDef* e; const EnumItem* i; };
static const char* kEnumItems = "pulseblockz.enumitems";   // cache keyed "Type" and "Type.Item", so == on either is identity

bool isEnumItem(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_ENUMITEM; }
bool readEnumItem(lua_State* L, int idx, const EnumDef*& e, const EnumItem*& i) {
    if (!isEnumItem(L, idx)) return false;
    auto* ud = static_cast<EnumItemUD*>(lua_touserdatatagged(L, idx, TAG_ENUMITEM));
    e = ud->e; i = ud->i;
    return true;
}
static void pushEnum(lua_State* L, const EnumDef* e) {
    lua_getfield(L, LUA_REGISTRYINDEX, kEnumItems);
    lua_getfield(L, -1, e->name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        auto* ud = static_cast<EnumUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(EnumUD), TAG_ENUM));
        ud->e = e;
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, e->name);
    }
    lua_remove(L, -2);
}
void pushEnumItem(lua_State* L, const EnumDef* e, const EnumItem* i) {
    std::string key = std::string(e->name) + "." + i->name;
    lua_getfield(L, LUA_REGISTRYINDEX, kEnumItems);
    lua_getfield(L, -1, key.c_str());
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        auto* ud = static_cast<EnumItemUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(EnumItemUD), TAG_ENUMITEM));
        ud->e = e; ud->i = i;
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, key.c_str());
    }
    lua_remove(L, -2);
}
const EnumItem* checkEnumItem(lua_State* L, int idx, const EnumDef* e) {
    if (!e) luaL_error(L, "unknown enum");
    if (isEnumItem(L, idx)) {
        auto* ud = static_cast<EnumItemUD*>(lua_touserdata(L, idx));
        if (ud->e != e) luaL_error(L, "Invalid value for enum %s", e->name);
        return ud->i;
    }
    const EnumItem* i = nullptr;
    if (lua_type(L, idx) == LUA_TSTRING) i = e->find(lua_tostring(L, idx));
    else if (lua_type(L, idx) == LUA_TNUMBER) i = e->findValue((int)lua_tonumber(L, idx));
    if (!i) luaL_error(L, "Invalid value for enum %s", e->name);
    return i;
}
static int enums_index(lua_State* L) {   // Enum.X
    const char* k = luaL_checkstring(L, 2);
    const EnumDef* e = findEnum(k);
    if (!e) luaL_error(L, "%s is not a valid Enum", k);
    pushEnum(L, e);
    return 1;
}
static int enums_namecall(lua_State* L) {
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && !std::strcmp(m, "GetEnums")) {
        lua_newtable(L); int n = 1;
        for (auto& e : allEnums()) { pushEnum(L, &e); lua_rawseti(L, -2, n++); }
        return 1;
    }
    luaL_error(L, "%s is not a valid method of Enums", m ? m : "?");
}
static int enum_index(lua_State* L) {    // Enum.Material.Plastic
    auto* ud = static_cast<EnumUD*>(lua_touserdata(L, 1));
    const char* k = luaL_checkstring(L, 2);
    const EnumItem* i = ud->e->find(k);
    if (!i) luaL_error(L, "%s is not a valid EnumItem of %s", k, ud->e->name);
    pushEnumItem(L, ud->e, i);
    return 1;
}
static int enum_namecall(lua_State* L) {
    auto* ud = static_cast<EnumUD*>(lua_touserdata(L, 1));
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && (!std::strcmp(m, "GetEnumItems"))) {
        lua_newtable(L); int n = 1;
        for (auto& i : ud->e->items) { pushEnumItem(L, ud->e, &i); lua_rawseti(L, -2, n++); }
        return 1;
    }
    if (m && !std::strcmp(m, "FromValue")) { const EnumItem* i = ud->e->findValue((int)luaL_checknumber(L, 2)); if (i) pushEnumItem(L, ud->e, i); else lua_pushnil(L); return 1; }
    if (m && !std::strcmp(m, "FromName")) { const EnumItem* i = ud->e->find(luaL_checkstring(L, 2)); if (i) pushEnumItem(L, ud->e, i); else lua_pushnil(L); return 1; }
    luaL_error(L, "%s is not a valid method of Enum", m ? m : "?");
}
static int enum_tostring(lua_State* L) { auto* ud = static_cast<EnumUD*>(lua_touserdata(L, 1)); lua_pushstring(L, ud->e->name); return 1; }
static int enumitem_index(lua_State* L) {
    auto* ud = static_cast<EnumItemUD*>(lua_touserdata(L, 1));
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Name")) { lua_pushstring(L, ud->i->name); return 1; }
    if (!std::strcmp(k, "Value")) { lua_pushnumber(L, ud->i->value); return 1; }
    if (!std::strcmp(k, "EnumType")) { pushEnum(L, ud->e); return 1; }
    luaL_error(L, "%s is not a valid member of EnumItem", k);
}
static int enumitem_namecall(lua_State* L) {
    auto* ud = static_cast<EnumItemUD*>(lua_touserdata(L, 1));
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && !std::strcmp(m, "IsA")) { lua_pushboolean(L, !std::strcmp(luaL_checkstring(L, 2), ud->e->name)); return 1; }
    luaL_error(L, "%s is not a valid method of EnumItem", m ? m : "?");
}
static int enumitem_tostring(lua_State* L) {
    auto* ud = static_cast<EnumItemUD*>(lua_touserdata(L, 1));
    lua_pushfstring(L, "Enum.%s.%s", ud->e->name, ud->i->name); return 1;
}
static void openEnum(lua_State* L) {
    lua_newtable(L); lua_setfield(L, LUA_REGISTRYINDEX, kEnumItems);
    setMeta(L, TAG_ENUM, "Enum", enum_index, enum_namecall, enum_tostring, nullptr);
    setMeta(L, TAG_ENUMITEM, "EnumItem", enumitem_index, enumitem_namecall, enumitem_tostring, nullptr);
    // The `Enum` global: an untagged userdata with a metatable of its own, not a tag-wide one
    lua_newuserdata(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, enums_index, "Enum.__index"); lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, enums_namecall, "Enum.__namecall"); lua_setfield(L, -2, "__namecall");
    lua_pushstring(L, "Enums"); lua_setfield(L, -2, "__type");
    lua_setreadonly(L, -1, true);
    lua_setmetatable(L, -2);
    lua_setglobal(L, "Enum");
}

// ---- Ray ---------------------------------------------------------------------------
struct RayUD { RayV r; };
bool isRay(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_RAY; }
RayV checkRay(lua_State* L, int idx) {
    if (!isRay(L, idx)) luaL_typeerror(L, idx, "Ray");
    return static_cast<RayUD*>(lua_touserdata(L, idx))->r;
}
void pushRay(lua_State* L, RayV r) { static_cast<RayUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(RayUD), TAG_RAY))->r = r; }
static int ray_new(lua_State* L) { pushRay(L, {checkVec3(L, 1), checkVec3(L, 2)}); return 1; }
static int ray_index(lua_State* L) {
    RayV r = checkRay(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Origin")) { pushVec3(L, r.origin); return 1; }
    if (!std::strcmp(k, "Direction")) { pushVec3(L, r.direction); return 1; }
    if (!std::strcmp(k, "Unit")) { pushRay(L, {r.origin, unit(r.direction)}); return 1; }
    luaL_error(L, "%s is not a valid member of Ray", k);
}
static int ray_namecall(lua_State* L) {
    RayV r = checkRay(L, 1);
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && !std::strcmp(m, "ClosestPoint")) {
        Vec3 p = checkVec3(L, 2), d = unit(r.direction);
        float t = std::max(0.f, dot(sub(p, r.origin), d));
        pushVec3(L, add(r.origin, mul(d, t)));
        return 1;
    }
    if (m && !std::strcmp(m, "Distance")) {
        Vec3 p = checkVec3(L, 2), d = unit(r.direction);
        float t = std::max(0.f, dot(sub(p, r.origin), d));
        lua_pushnumber(L, len(sub(p, add(r.origin, mul(d, t)))));
        return 1;
    }
    luaL_error(L, "%s is not a valid method of Ray", m ? m : "?");
}
static int ray_tostring(lua_State* L) {
    RayV r = checkRay(L, 1);
    char b[128];
    std::snprintf(b, sizeof b, "{%g, %g, %g}, {%g, %g, %g}", r.origin.x, r.origin.y, r.origin.z, r.direction.x, r.direction.y, r.direction.z);
    lua_pushstring(L, b);
    return 1;
}
static int ray_eq(lua_State* L) { lua_pushboolean(L, isRay(L, 1) && isRay(L, 2) && checkRay(L, 1).origin == checkRay(L, 2).origin && checkRay(L, 1).direction == checkRay(L, 2).direction); return 1; }

// ---- Region3 -----------------------------------------------------------------------
struct Region3UD { Region3V r; };
bool isRegion3(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_REGION3; }
Region3V checkRegion3(lua_State* L, int idx) {
    if (!isRegion3(L, idx)) luaL_typeerror(L, idx, "Region3");
    return static_cast<Region3UD*>(lua_touserdata(L, idx))->r;
}
void pushRegion3(lua_State* L, Region3V r) { static_cast<Region3UD*>(lua_newuserdatataggedwithmetatable(L, sizeof(Region3UD), TAG_REGION3))->r = r; }
static int region3_new(lua_State* L) {
    Vec3 a = checkVec3(L, 1), b = checkVec3(L, 2);
    pushRegion3(L, {{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}, {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}});
    return 1;
}
static int region3_index(lua_State* L) {
    Region3V r = checkRegion3(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "CFrame")) { pushCFrame(L, CFrameV::fromPos(mul(add(r.lo, r.hi), 0.5f))); return 1; }
    if (!std::strcmp(k, "Size")) { pushVec3(L, sub(r.hi, r.lo)); return 1; }
    luaL_error(L, "%s is not a valid member of Region3", k);
}
static int region3_namecall(lua_State* L) {
    Region3V r = checkRegion3(L, 1);
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (m && !std::strcmp(m, "ExpandToGrid")) {   // the corners moved outward onto multiples of the resolution
        float g = (float)luaL_checknumber(L, 2);
        if (!(g > 0)) luaL_error(L, "ExpandToGrid: the resolution must be greater than 0");
        auto down = [g](float v) { return std::floor(v / g) * g; };
        auto up = [g](float v) { return std::ceil(v / g) * g; };
        pushRegion3(L, {{down(r.lo.x), down(r.lo.y), down(r.lo.z)}, {up(r.hi.x), up(r.hi.y), up(r.hi.z)}});
        return 1;
    }
    luaL_error(L, "%s is not a valid method of Region3", m ? m : "?");
}
static int region3_tostring(lua_State* L) {
    Region3V r = checkRegion3(L, 1);
    Vec3 c = mul(add(r.lo, r.hi), 0.5f), s = sub(r.hi, r.lo);
    char b[160];
    std::snprintf(b, sizeof b, "%g, %g, %g, 1, 0, 0, 0, 1, 0, 0, 0, 1; %g, %g, %g", c.x, c.y, c.z, s.x, s.y, s.z);
    lua_pushstring(L, b);
    return 1;
}
static int region3_eq(lua_State* L) { lua_pushboolean(L, isRegion3(L, 1) && isRegion3(L, 2) && checkRegion3(L, 1).lo == checkRegion3(L, 2).lo && checkRegion3(L, 1).hi == checkRegion3(L, 2).hi); return 1; }

// ---- TweenInfo ---------------------------------------------------------------------
struct TweenInfoUD { TweenInfoV t; };
bool isTweenInfo(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_TWEENINFO; }
TweenInfoV checkTweenInfo(lua_State* L, int idx) {
    if (!isTweenInfo(L, idx)) luaL_typeerror(L, idx, "TweenInfo");
    return static_cast<TweenInfoUD*>(lua_touserdata(L, idx))->t;
}
void pushTweenInfo(lua_State* L, const TweenInfoV& t) {
    auto* ud = static_cast<TweenInfoUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(TweenInfoUD), TAG_TWEENINFO));
    ud->t = t;
}
static int ti_new(lua_State* L) {
    TweenInfoV t;
    t.time = (float)luaL_optnumber(L, 1, 1);
    if (!lua_isnoneornil(L, 2)) t.style = checkEnumItem(L, 2, findEnum("EasingStyle"))->value; else t.style = 3; // Quad
    if (!lua_isnoneornil(L, 3)) t.direction = checkEnumItem(L, 3, findEnum("EasingDirection"))->value; else t.direction = 1;
    t.repeatCount = (int)luaL_optnumber(L, 4, 0);
    t.reverses = lua_toboolean(L, 5);
    t.delay = (float)luaL_optnumber(L, 6, 0);
    auto* ud = static_cast<TweenInfoUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(TweenInfoUD), TAG_TWEENINFO));
    ud->t = t;
    return 1;
}
static int ti_index(lua_State* L) {
    TweenInfoV t = checkTweenInfo(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Time")) { lua_pushnumber(L, t.time); return 1; }
    if (!std::strcmp(k, "RepeatCount")) { lua_pushnumber(L, t.repeatCount); return 1; }
    if (!std::strcmp(k, "Reverses")) { lua_pushboolean(L, t.reverses); return 1; }
    if (!std::strcmp(k, "DelayTime")) { lua_pushnumber(L, t.delay); return 1; }
    if (!std::strcmp(k, "EasingStyle")) { const EnumDef* e = findEnum("EasingStyle"); pushEnumItem(L, e, e->findValue(t.style)); return 1; }
    if (!std::strcmp(k, "EasingDirection")) { const EnumDef* e = findEnum("EasingDirection"); pushEnumItem(L, e, e->findValue(t.direction)); return 1; }
    luaL_error(L, "%s is not a valid member of TweenInfo", k);
}

static float easeIn(int style, float t) {
    switch (style) {
    case 0: return t;                                              // Linear
    case 1: return 1 - std::cos(t * PI / 2);                       // Sine
    case 2: { float s = 1.70158f; return t * t * ((s + 1) * t - s); } // Back
    case 3: return t * t;                                          // Quad
    case 4: return t * t * t * t;                                  // Quart
    case 5: return t * t * t * t * t;                              // Quint
    case 6: {                                                      // Bounce (in = mirrored out)
        float u = 1 - t, r;
        if (u < 1 / 2.75f) r = 7.5625f * u * u;
        else if (u < 2 / 2.75f) { u -= 1.5f / 2.75f; r = 7.5625f * u * u + 0.75f; }
        else if (u < 2.5f / 2.75f) { u -= 2.25f / 2.75f; r = 7.5625f * u * u + 0.9375f; }
        else { u -= 2.625f / 2.75f; r = 7.5625f * u * u + 0.984375f; }
        return 1 - r;
    }
    case 7: return t == 0 ? 0 : t == 1 ? 1 : -std::pow(2.f, 10 * t - 10) * std::sin((t * 10 - 10.75f) * (2 * PI / 3)); // Elastic
    case 8: return t == 0 ? 0 : std::pow(2.f, 10 * t - 10);        // Exponential
    case 9: return 1 - std::sqrt(1 - t * t);                       // Circular
    case 10: return t * t * t;                                     // Cubic
    }
    return t;
}
float ease(int style, int direction, float t) {
    t = std::fmax(0.f, std::fmin(1.f, t));
    switch (direction) {
    case 0: return easeIn(style, t);
    case 1: return 1 - easeIn(style, 1 - t);
    default: return t < 0.5f ? easeIn(style, t * 2) / 2 : 1 - easeIn(style, (1 - t) * 2) / 2;
    }
}

// ---- Random ------------------------------------------------------------------------
struct RandomUD { std::mt19937_64 gen; };
static int rnd_new(lua_State* L) {
    // Read the seed before pushing: called with no seed, the new userdata itself lands at index 1
    uint64_t seed = lua_isnoneornil(L, 1) ? (uint64_t)(lua_clock() * 1e9) : (uint64_t)(int64_t)luaL_checknumber(L, 1);
    auto* ud = static_cast<RandomUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(RandomUD), TAG_RANDOM));
    new (&ud->gen) std::mt19937_64(seed);
    return 1;
}
static int rnd_namecall(lua_State* L) {
    if (lua_userdatatag(L, 1) != TAG_RANDOM) luaL_typeerror(L, 1, "Random");
    auto* ud = static_cast<RandomUD*>(lua_touserdata(L, 1));
    int atom; const char* m = lua_namecallatom(L, &atom);
    if (!m) luaL_error(L, "invalid method call");
    if (!std::strcmp(m, "NextInteger")) {
        long long a = (long long)luaL_checknumber(L, 2), b = (long long)luaL_checknumber(L, 3);
        if (b < a) luaL_error(L, "invalid argument #3 to 'NextInteger' (interval is empty)");
        std::uniform_int_distribution<long long> d(a, b);
        lua_pushnumber(L, (double)d(ud->gen)); return 1;
    }
    if (!std::strcmp(m, "NextNumber")) {
        double a = luaL_optnumber(L, 2, 0), b = luaL_optnumber(L, 3, 1);
        std::uniform_real_distribution<double> d(a, b);
        lua_pushnumber(L, d(ud->gen)); return 1;
    }
    if (!std::strcmp(m, "NextUnitVector")) {
        std::uniform_real_distribution<float> d(-1, 1);
        Vec3 v; do { v = {d(ud->gen), d(ud->gen), d(ud->gen)}; } while (len(v) > 1 || len(v) < 1e-4f);
        pushVec3(L, unit(v)); return 1;
    }
    if (!std::strcmp(m, "Shuffle")) {
        luaL_checktype(L, 2, LUA_TTABLE);
        int n = lua_objlen(L, 2);
        for (int i = n; i > 1; i--) {
            std::uniform_int_distribution<int> d(1, i);
            int j = d(ud->gen);
            lua_rawgeti(L, 2, i); lua_rawgeti(L, 2, j);
            lua_rawseti(L, 2, i); lua_rawseti(L, 2, j);
        }
        return 0;
    }
    if (!std::strcmp(m, "Clone")) {
        auto* c = static_cast<RandomUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(RandomUD), TAG_RANDOM));
        new (&c->gen) std::mt19937_64(ud->gen); return 1;
    }
    luaL_error(L, "%s is not a valid method of Random", m);
}

// ---- JSON --------------------------------------------------------------------------
static void jsonStr(std::string& out, const char* s, size_t n) {
    out += '"';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = s[i];
        switch (c) {
        case '"': out += "\\\""; break; case '\\': out += "\\\\"; break; case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break; case '\t': out += "\\t"; break; case '\b': out += "\\b"; break; case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; } else out += (char)c;
        }
    }
    out += '"';
}
static void jsonEncode(lua_State* L, int idx, std::string& out, int depth) {
    if (depth > 100) luaL_error(L, "Can't encode: table is too deeply nested");
    idx = lua_absindex(L, idx);
    switch (lua_type(L, idx)) {
    case LUA_TNIL: out += "null"; break;
    case LUA_TBOOLEAN: out += lua_toboolean(L, idx) ? "true" : "false"; break;
    case LUA_TNUMBER: {
        double d = lua_tonumber(L, idx);
        if (std::isnan(d) || std::isinf(d)) { out += "null"; break; }
        char b[32];
        if (d == std::floor(d) && std::fabs(d) < 1e15) std::snprintf(b, sizeof b, "%lld", (long long)d);
        else std::snprintf(b, sizeof b, "%.17g", d);
        out += b; break;
    }
    case LUA_TSTRING: { size_t n; const char* s = lua_tolstring(L, idx, &n); jsonStr(out, s, n); break; }
    case LUA_TTABLE: {
        // Array if the keys are exactly 1..n
        int n = lua_objlen(L, idx);
        // {} encodes as [], as Roblox's JSONEncode does: Luau cannot tell an empty list from an empty map
        lua_pushnil(L);
        bool empty = lua_next(L, idx) == 0;
        if (!empty) lua_pop(L, 2);
        bool array = n > 0 || empty;
        if (array) {
            lua_pushnil(L);
            int count = 0;
            while (lua_next(L, idx)) {
                count++;
                if (lua_type(L, -2) != LUA_TNUMBER || lua_tonumber(L, -2) != (double)(int)lua_tonumber(L, -2)) array = false;
                lua_pop(L, 1);
            }
            if (count != n) array = false;
        }
        if (array) {
            out += '[';
            for (int i = 1; i <= n; i++) { if (i > 1) out += ','; lua_rawgeti(L, idx, i); jsonEncode(L, -1, out, depth + 1); lua_pop(L, 1); }
            out += ']';
        } else {
            out += '{';
            bool first = true;
            lua_pushnil(L);
            while (lua_next(L, idx)) {
                if (lua_type(L, -2) == LUA_TSTRING) {
                    if (!first) out += ',';
                    first = false;
                    size_t kn; const char* k = lua_tolstring(L, -2, &kn);
                    jsonStr(out, k, kn); out += ':';
                    jsonEncode(L, -1, out, depth + 1);
                } else if (lua_type(L, -2) == LUA_TNUMBER) {
                    if (!first) out += ',';
                    first = false;
                    char b[32]; std::snprintf(b, sizeof b, "\"%.14g\"", lua_tonumber(L, -2));
                    out += b; out += ':';
                    jsonEncode(L, -1, out, depth + 1);
                }
                lua_pop(L, 1);
            }
            out += '}';
        }
        break;
    }
    default: out += "null"; break;   // functions, userdata: Roblox encodes as null
    }
}
int json_encode(lua_State* L) {
    std::string out;
    jsonEncode(L, 1, out, 0);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

struct JsonParser {
    lua_State* L; const char* s; size_t n; size_t i = 0;
    void ws() { while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++; }
    [[noreturn]] void fail(const char* what) { luaL_error(L, "Can't parse JSON: %s at position %d", what, (int)i); }
    void value(int depth) {
        if (depth > 100) fail("too deeply nested");
        ws();
        if (i >= n) fail("unexpected end");
        char c = s[i];
        if (c == '{') {
            i++; lua_newtable(L); ws();
            if (i < n && s[i] == '}') { i++; return; }
            for (;;) {
                ws(); if (i >= n || s[i] != '"') fail("expected string key");
                str(); ws();
                if (i >= n || s[i] != ':') fail("expected ':'");
                i++; value(depth + 1);
                lua_rawset(L, -3); ws();
                if (i < n && s[i] == ',') { i++; continue; }
                if (i < n && s[i] == '}') { i++; return; }
                fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            i++; lua_newtable(L); ws();
            if (i < n && s[i] == ']') { i++; return; }
            for (int k = 1;; k++) {
                value(depth + 1); lua_rawseti(L, -2, k); ws();
                if (i < n && s[i] == ',') { i++; continue; }
                if (i < n && s[i] == ']') { i++; return; }
                fail("expected ',' or ']'");
            }
        }
        if (c == '"') { str(); return; }
        if (!std::strncmp(s + i, "true", 4)) { i += 4; lua_pushboolean(L, 1); return; }
        if (!std::strncmp(s + i, "false", 5)) { i += 5; lua_pushboolean(L, 0); return; }
        if (!std::strncmp(s + i, "null", 4)) { i += 4; lua_pushnil(L); return; }
        char* end = nullptr;
        double d = std::strtod(s + i, &end);
        if (end == s + i) fail("unexpected character");
        i = end - s; lua_pushnumber(L, d);
    }
    void str() {
        i++; std::string out;
        while (i < n && s[i] != '"') {
            if (s[i] == '\\') {
                i++; if (i >= n) fail("bad escape");
                switch (s[i]) {
                case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break; case '/': out += '/'; break;
                case '\\': out += '\\'; break; case '"': out += '"'; break;
                case 'u': {
                    if (i + 4 >= n) fail("bad \\u escape");
                    unsigned cp = 0; std::sscanf(s + i + 1, "%4x", &cp); i += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < n && s[i + 1] == '\\' && s[i + 2] == 'u') {
                        unsigned lo = 0; std::sscanf(s + i + 3, "%4x", &lo); i += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    if (cp < 0x80) out += (char)cp;
                    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 63)); }
                    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63)); }
                    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 63)); out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63)); }
                    break;
                }
                default: fail("bad escape");
                }
                i++;
            } else out += s[i++];
        }
        if (i >= n) fail("unterminated string");
        i++;
        lua_pushlstring(L, out.data(), out.size());
    }
};
int json_decode(lua_State* L) {
    size_t n; const char* s = luaL_checklstring(L, 1, &n);
    JsonParser p{L, s, n};
    p.value(0);
    p.ws();
    if (p.i != n) p.fail("trailing characters");
    return 1;
}

// ---- Font --------------------------------------------------------------------------
// The FontFace datatype: Family is a font asset uri, Bold a view of Weight (>= SemiBold).
struct FontUD { char family[192]; int weight; bool italic; };
bool isFont(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_FONT; }
Value checkFont(lua_State* L, int idx) {
    if (!isFont(L, idx)) luaL_typeerror(L, idx, "Font");
    auto* f = static_cast<FontUD*>(lua_touserdata(L, idx));
    return Value::font(f->family, f->weight, f->italic);
}
void pushFont(lua_State* L, const Value& v) {
    auto* f = static_cast<FontUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(FontUD), TAG_FONT));
    std::snprintf(f->family, sizeof f->family, "%s", v.s.c_str());
    f->weight = (int)v.n; f->italic = v.b;
}
static const struct { const char* font; const char* family; int weight; bool italic; } kFontFaces[] = {
    {"Legacy", "LegacyArial", 400, false}, {"Arial", "Arial", 400, false}, {"ArialBold", "Arial", 700, false},
    {"SourceSans", "SourceSansPro", 400, false}, {"SourceSansBold", "SourceSansPro", 700, false}, {"SourceSansLight", "SourceSansPro", 300, false},
    {"SourceSansItalic", "SourceSansPro", 400, true}, {"SourceSansSemibold", "SourceSansPro", 600, false},
    {"Bodoni", "AccanthisADFStd", 400, false}, {"Garamond", "Guru", 400, false}, {"Cartoon", "ComicNeueAngular", 400, false},
    {"Code", "Inconsolata", 400, false}, {"Highway", "HighwayGothic", 400, false}, {"SciFi", "Zekton", 400, false},
    {"Arcade", "PressStart2P", 400, false}, {"Fantasy", "Balthazar", 400, false}, {"Antique", "RomanAntique", 400, false},
    {"Gotham", "GothamSSm", 400, false}, {"GothamMedium", "GothamSSm", 500, false}, {"GothamBold", "GothamSSm", 700, false}, {"GothamBlack", "GothamSSm", 900, false},
    {"AmaticSC", "AmaticSC", 400, false}, {"Bangers", "Bangers", 400, false}, {"Creepster", "Creepster", 400, false}, {"DenkOne", "DenkOne", 400, false},
    {"Fondamento", "Fondamento", 400, false}, {"FredokaOne", "FredokaOne", 400, false}, {"GrenzeGotisch", "GrenzeGotisch", 400, false},
    {"IndieFlower", "IndieFlower", 400, false}, {"JosefinSans", "JosefinSans", 400, false}, {"Jura", "Jura", 400, false}, {"Kalam", "Kalam", 400, false},
    {"LuckiestGuy", "LuckiestGuy", 400, false}, {"Merriweather", "Merriweather", 400, false}, {"Michroma", "Michroma", 400, false},
    {"Nunito", "Nunito", 400, false}, {"Oswald", "Oswald", 400, false}, {"PatrickHand", "PatrickHand", 400, false},
    {"PermanentMarker", "PermanentMarker", 400, false}, {"Roboto", "Roboto", 400, false}, {"RobotoCondensed", "RobotoCondensed", 400, false},
    {"RobotoMono", "RobotoMono", 400, false}, {"Sarpanch", "Sarpanch", 400, false}, {"SpecialElite", "SpecialElite", 400, false},
    {"TitilliumWeb", "TitilliumWeb", 400, false}, {"Ubuntu", "Ubuntu", 400, false}, {"BuilderSans", "BuilderSans", 400, false},
    {"BuilderSansMedium", "BuilderSans", 500, false}, {"BuilderSansBold", "BuilderSans", 700, false}, {"BuilderSansExtraBold", "BuilderSans", 800, false},
};
static std::string familyAsset(const std::string& name) { return "rbxasset://fonts/families/" + name + ".json"; }
bool fontOfEnum(const std::string& fontName, Value& face) {
    for (auto& f : kFontFaces) if (fontName == f.font) { face = Value::font(familyAsset(f.family), f.weight, f.italic); return true; }
    return false;
}
std::string fontEnumOf(const Value& face) {
    for (auto& f : kFontFaces) if (face.s == familyAsset(f.family) && (int)face.n == f.weight && face.b == f.italic) return f.font;
    return "Unknown";
}
static int fontWeightArg(lua_State* L, int idx) {
    if (lua_isnoneornil(L, idx)) return 400;
    return checkEnumItem(L, idx, findEnum("FontWeight"))->value;
}
static bool fontStyleArg(lua_State* L, int idx) {
    if (lua_isnoneornil(L, idx)) return false;
    return checkEnumItem(L, idx, findEnum("FontStyle"))->value == 1;
}
static int font_new(lua_State* L) { pushFont(L, Value::font(luaL_checkstring(L, 1), fontWeightArg(L, 2), fontStyleArg(L, 3))); return 1; }
static int font_fromName(lua_State* L) { pushFont(L, Value::font(familyAsset(luaL_checkstring(L, 1)), fontWeightArg(L, 2), fontStyleArg(L, 3))); return 1; }
static int font_fromId(lua_State* L) { pushFont(L, Value::font("rbxassetid://" + std::to_string((long long)luaL_checkinteger(L, 1)), fontWeightArg(L, 2), fontStyleArg(L, 3))); return 1; }
static int font_fromEnum(lua_State* L) {
    const EnumItem* it = checkEnumItem(L, 1, findEnum("Font"));
    Value face;
    if (!fontOfEnum(it->name, face)) luaL_error(L, "Font.fromEnum: Enum.Font.%s has no font face", it->name);
    pushFont(L, face);
    return 1;
}
static int font_index(lua_State* L) {
    Value f = checkFont(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Family")) { lua_pushstring(L, f.s.c_str()); return 1; }
    if (!std::strcmp(k, "Weight")) { const EnumDef* e = findEnum("FontWeight"); const EnumItem* it = e->findValue((int)f.n); pushEnumItem(L, e, it ? it : e->find("Regular")); return 1; }
    if (!std::strcmp(k, "Style")) { const EnumDef* e = findEnum("FontStyle"); pushEnumItem(L, e, e->find(f.b ? "Italic" : "Normal")); return 1; }
    if (!std::strcmp(k, "Bold")) { lua_pushboolean(L, f.n >= 600); return 1; }
    luaL_error(L, "%s is not a valid member of Font", k);
}
static int font_newindex(lua_State* L) {
    auto* f = static_cast<FontUD*>(lua_touserdata(L, 1));
    if (!isFont(L, 1)) luaL_typeerror(L, 1, "Font");
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Family")) { std::snprintf(f->family, sizeof f->family, "%s", luaL_checkstring(L, 3)); return 0; }
    if (!std::strcmp(k, "Weight")) { f->weight = fontWeightArg(L, 3); return 0; }
    if (!std::strcmp(k, "Style")) { f->italic = fontStyleArg(L, 3); return 0; }
    if (!std::strcmp(k, "Bold")) { luaL_checktype(L, 3, LUA_TBOOLEAN); f->weight = lua_toboolean(L, 3) ? 700 : 400; return 0; }
    luaL_error(L, "%s is not a valid member of Font", k);
}
static int font_namecall(lua_State* L) { int atom; const char* m = lua_namecallatom(L, &atom); luaL_error(L, "%s is not a valid method of Font", m ? m : "?"); }
static int font_eq(lua_State* L) { lua_pushboolean(L, isFont(L, 1) && isFont(L, 2) && checkFont(L, 1) == checkFont(L, 2)); return 1; }
static int font_tostring(lua_State* L) {
    Value f = checkFont(L, 1);
    const EnumItem* w = findEnum("FontWeight")->findValue((int)f.n);
    lua_pushstring(L, ("Font { Family = " + f.s + ", Weight = " + (w ? w->name : "Regular") + ", Style = " + (f.b ? "Italic" : "Normal") + " }").c_str());
    return 1;
}
static void openFont(lua_State* L) {
    setMeta(L, TAG_FONT, "Font", font_index, font_namecall, font_tostring, font_eq, {{"__newindex", font_newindex}});
    lua_newtable(L);
    lua_pushcfunction(L, font_new, "Font.new"); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, font_fromEnum, "Font.fromEnum"); lua_setfield(L, -2, "fromEnum");
    lua_pushcfunction(L, font_fromName, "Font.fromName"); lua_setfield(L, -2, "fromName");
    lua_pushcfunction(L, font_fromId, "Font.fromId"); lua_setfield(L, -2, "fromId");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Font");
}

// ---- NumberRange / NumberSequence / ColorSequence ---------------------------------
// Keypoints pack flat: time, value, envelope for a number sequence; time, r, g, b for a colour one.
static const int kMaxKeypoints = 20;
struct NumberRangeUD { float lo, hi; };
struct SequenceUD { int n; float kp[kMaxKeypoints * 4]; };
struct NumberKeypointUD { float time, value, envelope; };
struct ColorKeypointUD { float time; Col3 color; };
bool isNumberRange(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_NUMBERRANGE; }
bool isNumberSequence(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_NUMBERSEQUENCE; }
bool isColorSequence(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_COLORSEQUENCE; }
Value checkSequence(lua_State* L, int idx) {
    if (isNumberRange(L, idx)) { auto* r = static_cast<NumberRangeUD*>(lua_touserdata(L, idx)); return Value::numberRange(r->lo, r->hi); }
    if (isNumberSequence(L, idx) || isColorSequence(L, idx)) {
        auto* s = static_cast<SequenceUD*>(lua_touserdata(L, idx));
        int w = isNumberSequence(L, idx) ? 3 : 4;
        std::vector<float> kp(s->kp, s->kp + s->n * w);
        return w == 3 ? Value::numberSequence(std::move(kp)) : Value::colorSequence(std::move(kp));
    }
    luaL_typeerror(L, idx, "NumberSequence");
}
void pushSequence(lua_State* L, const Value& v) {
    if (v.type == Value::NumberRange) { auto* r = static_cast<NumberRangeUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(NumberRangeUD), TAG_NUMBERRANGE)); r->lo = v.u[0]; r->hi = v.u[1]; return; }
    int w = v.type == Value::ColorSequence ? 4 : 3;
    auto* s = static_cast<SequenceUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(SequenceUD), w == 4 ? TAG_COLORSEQUENCE : TAG_NUMBERSEQUENCE));
    s->n = std::min((int)(v.kp.size() / w), kMaxKeypoints);
    std::memcpy(s->kp, v.kp.data(), sizeof(float) * s->n * w);
}
static int nr_new(lua_State* L) {
    float lo = (float)luaL_checknumber(L, 1), hi = (float)luaL_optnumber(L, 2, lo);
    if (hi < lo) luaL_error(L, "NumberRange.new(): invalid range");
    pushSequence(L, Value::numberRange(lo, hi));
    return 1;
}
static int nr_index(lua_State* L) {
    Value r = checkSequence(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Min")) { lua_pushnumber(L, r.u[0]); return 1; }
    if (!std::strcmp(k, "Max")) { lua_pushnumber(L, r.u[1]); return 1; }
    luaL_error(L, "%s is not a valid member of NumberRange", k);
}
static int nr_tostring(lua_State* L) { Value r = checkSequence(L, 1); char b[64]; std::snprintf(b, sizeof b, "%g %g", r.u[0], r.u[1]); lua_pushstring(L, b); return 1; }
static int nr_eq(lua_State* L) { lua_pushboolean(L, isNumberRange(L, 1) && isNumberRange(L, 2) && checkSequence(L, 1) == checkSequence(L, 2)); return 1; }

// ---- PhysicalProperties ------------------------------------------------------------
// The weights decide whose friction and elasticity win where two parts touch.
struct PhysPropsUD { float density, friction, elasticity, frictionWeight, elasticityWeight; };
bool isPhysicalProperties(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_PHYSICALPROPERTIES; }
Value checkPhysicalProperties(lua_State* L, int idx) {
    if (!isPhysicalProperties(L, idx)) luaL_typeerror(L, idx, "PhysicalProperties");
    auto* p = static_cast<PhysPropsUD*>(lua_touserdata(L, idx));
    return Value::physProps(p->density, p->friction, p->elasticity, p->frictionWeight, p->elasticityWeight);
}
void pushPhysicalProperties(lua_State* L, const Value& v) {
    auto* p = static_cast<PhysPropsUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(PhysPropsUD), TAG_PHYSICALPROPERTIES));
    p->density = v.u[0]; p->friction = v.u[1]; p->elasticity = v.u[2]; p->frictionWeight = v.u[3]; p->elasticityWeight = (float)v.n;
}
static int pp_new(lua_State* L) {
    if (lua_type(L, 1) != LUA_TNUMBER) {
        const EnumDef* e = findEnum("Material");
        const EnumItem* item = checkEnumItem(L, 1, e);
        const MaterialPhysics& m = materialPhysics(item ? item->name : "Plastic");
        pushPhysicalProperties(L, Value::physProps(m.density, m.friction, m.elasticity));
        return 1;
    }
    float d = (float)luaL_checknumber(L, 1), f = (float)luaL_checknumber(L, 2), e = (float)luaL_checknumber(L, 3);
    float fw = (float)luaL_optnumber(L, 4, 1), ew = (float)luaL_optnumber(L, 5, 1);
    if (d < 0.01f || d > 100) luaL_error(L, "PhysicalProperties.new(): density must be between 0.01 and 100");
    pushPhysicalProperties(L, Value::physProps(d, f, e, fw, ew));
    return 1;
}
static int pp_index(lua_State* L) {
    Value v = checkPhysicalProperties(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "Density")) { lua_pushnumber(L, v.u[0]); return 1; }
    if (!std::strcmp(k, "Friction")) { lua_pushnumber(L, v.u[1]); return 1; }
    if (!std::strcmp(k, "Elasticity")) { lua_pushnumber(L, v.u[2]); return 1; }
    if (!std::strcmp(k, "FrictionWeight")) { lua_pushnumber(L, v.u[3]); return 1; }
    if (!std::strcmp(k, "ElasticityWeight")) { lua_pushnumber(L, v.n); return 1; }
    luaL_error(L, "%s is not a valid member of PhysicalProperties", k);
}
static int pp_tostring(lua_State* L) {
    Value v = checkPhysicalProperties(L, 1);
    char b[96];
    std::snprintf(b, sizeof b, "%g, %g, %g, %g, %g", v.u[0], v.u[1], v.u[2], v.u[3], v.n);
    lua_pushstring(L, b);
    return 1;
}
static int pp_eq(lua_State* L) {
    lua_pushboolean(L, isPhysicalProperties(L, 1) && isPhysicalProperties(L, 2) && checkPhysicalProperties(L, 1) == checkPhysicalProperties(L, 2));
    return 1;
}

static int nsk_new(lua_State* L) {
    float time = (float)luaL_checknumber(L, 1), value = (float)luaL_checknumber(L, 2), envelope = (float)luaL_optnumber(L, 3, 0);
    auto* k = static_cast<NumberKeypointUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(NumberKeypointUD), TAG_NUMBERSEQUENCEKEYPOINT));
    k->time = time; k->value = value; k->envelope = envelope;
    return 1;
}
static NumberKeypointUD* checkNsk(lua_State* L, int idx) {
    if (lua_userdatatag(L, idx) != TAG_NUMBERSEQUENCEKEYPOINT) luaL_typeerror(L, idx, "NumberSequenceKeypoint");
    return static_cast<NumberKeypointUD*>(lua_touserdata(L, idx));
}
static int nsk_index(lua_State* L) {
    NumberKeypointUD* k = checkNsk(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!std::strcmp(key, "Time")) { lua_pushnumber(L, k->time); return 1; }
    if (!std::strcmp(key, "Value")) { lua_pushnumber(L, k->value); return 1; }
    if (!std::strcmp(key, "Envelope")) { lua_pushnumber(L, k->envelope); return 1; }
    luaL_error(L, "%s is not a valid member of NumberSequenceKeypoint", key);
}
static int nsk_tostring(lua_State* L) { NumberKeypointUD* k = checkNsk(L, 1); char b[96]; std::snprintf(b, sizeof b, "%g %g %g", k->time, k->value, k->envelope); lua_pushstring(L, b); return 1; }
static int nsk_eq(lua_State* L) {
    bool ok = lua_userdatatag(L, 1) == TAG_NUMBERSEQUENCEKEYPOINT && lua_userdatatag(L, 2) == TAG_NUMBERSEQUENCEKEYPOINT;
    if (ok) { NumberKeypointUD* a = checkNsk(L, 1); NumberKeypointUD* b = checkNsk(L, 2); ok = a->time == b->time && a->value == b->value && a->envelope == b->envelope; }
    lua_pushboolean(L, ok); return 1;
}
static int csk_new(lua_State* L) {
    float time = (float)luaL_checknumber(L, 1); Col3 color = checkColor3(L, 2);
    auto* k = static_cast<ColorKeypointUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(ColorKeypointUD), TAG_COLORSEQUENCEKEYPOINT));
    k->time = time; k->color = color;
    return 1;
}
static ColorKeypointUD* checkCsk(lua_State* L, int idx) {
    if (lua_userdatatag(L, idx) != TAG_COLORSEQUENCEKEYPOINT) luaL_typeerror(L, idx, "ColorSequenceKeypoint");
    return static_cast<ColorKeypointUD*>(lua_touserdata(L, idx));
}
static int csk_index(lua_State* L) {
    ColorKeypointUD* k = checkCsk(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!std::strcmp(key, "Time")) { lua_pushnumber(L, k->time); return 1; }
    if (!std::strcmp(key, "Value")) { pushColor3(L, k->color); return 1; }
    luaL_error(L, "%s is not a valid member of ColorSequenceKeypoint", key);
}
static int csk_tostring(lua_State* L) { ColorKeypointUD* k = checkCsk(L, 1); char b[128]; std::snprintf(b, sizeof b, "%g %g %g %g", k->time, k->color.r, k->color.g, k->color.b); lua_pushstring(L, b); return 1; }
static int csk_eq(lua_State* L) {
    bool ok = lua_userdatatag(L, 1) == TAG_COLORSEQUENCEKEYPOINT && lua_userdatatag(L, 2) == TAG_COLORSEQUENCEKEYPOINT;
    if (ok) { ColorKeypointUD* a = checkCsk(L, 1); ColorKeypointUD* b = checkCsk(L, 2); ok = a->time == b->time && a->color == b->color; }
    lua_pushboolean(L, ok); return 1;
}

// The rules Roblox enforces on a sequence's keypoints
static void checkKeypoints(lua_State* L, const char* type, const std::vector<float>& kp, int w) {
    size_t n = kp.size() / w;
    if (n < 2) luaL_error(L, "%s.new(): requires at least 2 keypoints", type);
    if (n > (size_t)kMaxKeypoints) luaL_error(L, "%s.new(): too many keypoints (max %d)", type, kMaxKeypoints);
    if (kp[0] != 0) luaL_error(L, "%s.new(): first keypoint must be at time 0", type);
    if (kp[(n - 1) * w] != 1) luaL_error(L, "%s.new(): last keypoint must be at time 1", type);
    for (size_t k = 1; k < n; k++) if (kp[k * w] < kp[(k - 1) * w]) luaL_error(L, "%s.new(): keypoints must be in ascending time order", type);
}
static int ns_new(lua_State* L) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
        float a = (float)lua_tonumber(L, 1), b = (float)luaL_optnumber(L, 2, a);
        pushSequence(L, Value::numberSequence(a, b));
        return 1;
    }
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<float> kp;
    for (int k = 1; ; k++) {
        lua_rawgeti(L, 1, k);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        NumberKeypointUD* p = checkNsk(L, -1);
        kp.insert(kp.end(), {p->time, p->value, p->envelope});
        lua_pop(L, 1);
    }
    checkKeypoints(L, "NumberSequence", kp, 3);
    pushSequence(L, Value::numberSequence(std::move(kp)));
    return 1;
}
static int cs_new(lua_State* L) {
    if (isColor3(L, 1)) {
        Col3 a = checkColor3(L, 1), b = lua_isnoneornil(L, 2) ? a : checkColor3(L, 2);
        pushSequence(L, Value::colorSequence(a, b));
        return 1;
    }
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<float> kp;
    for (int k = 1; ; k++) {
        lua_rawgeti(L, 1, k);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
        ColorKeypointUD* p = checkCsk(L, -1);
        kp.insert(kp.end(), {p->time, p->color.r, p->color.g, p->color.b});
        lua_pop(L, 1);
    }
    checkKeypoints(L, "ColorSequence", kp, 4);
    pushSequence(L, Value::colorSequence(std::move(kp)));
    return 1;
}
static int seq_index(lua_State* L) {
    Value v = checkSequence(L, 1);
    const char* k = luaL_checkstring(L, 2);
    bool color = v.type == Value::ColorSequence;
    if (!std::strcmp(k, "Keypoints")) {
        int w = color ? 4 : 3, n = (int)(v.kp.size() / w);
        lua_createtable(L, n, 0);
        for (int i = 0; i < n; i++) {
            const float* p = &v.kp[i * w];
            if (color) { auto* c = static_cast<ColorKeypointUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(ColorKeypointUD), TAG_COLORSEQUENCEKEYPOINT)); c->time = p[0]; c->color = {p[1], p[2], p[3]}; }
            else { auto* c = static_cast<NumberKeypointUD*>(lua_newuserdatataggedwithmetatable(L, sizeof(NumberKeypointUD), TAG_NUMBERSEQUENCEKEYPOINT)); c->time = p[0]; c->value = p[1]; c->envelope = p[2]; }
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }
    luaL_error(L, "%s is not a valid member of %s", k, color ? "ColorSequence" : "NumberSequence");
}
static int seq_tostring(lua_State* L) {
    Value v = checkSequence(L, 1);
    std::string s;
    char b[32];
    for (float f : v.kp) { std::snprintf(b, sizeof b, "%g ", f); s += b; }
    lua_pushstring(L, s.c_str());
    return 1;
}
static int seq_eq(lua_State* L) {
    int a = lua_userdatatag(L, 1), b = lua_userdatatag(L, 2);
    lua_pushboolean(L, a == b && (a == TAG_NUMBERSEQUENCE || a == TAG_COLORSEQUENCE) && checkSequence(L, 1) == checkSequence(L, 2));
    return 1;
}
static void openSequences(lua_State* L) {
    auto global = [&](int tag, const char* type, lua_CFunction index, lua_CFunction tostring, lua_CFunction eq, lua_CFunction ctor, const char* ctorName) {
        setMeta(L, tag, type, index, nullptr, tostring, eq);
        lua_newtable(L);
        lua_pushcfunction(L, ctor, ctorName); lua_setfield(L, -2, "new");
        lua_setreadonly(L, -1, true);
        lua_setglobal(L, type);
    };
    global(TAG_PHYSICALPROPERTIES, "PhysicalProperties", pp_index, pp_tostring, pp_eq, pp_new, "PhysicalProperties.new");
    global(TAG_NUMBERRANGE, "NumberRange", nr_index, nr_tostring, nr_eq, nr_new, "NumberRange.new");
    global(TAG_NUMBERSEQUENCE, "NumberSequence", seq_index, seq_tostring, seq_eq, ns_new, "NumberSequence.new");
    global(TAG_NUMBERSEQUENCEKEYPOINT, "NumberSequenceKeypoint", nsk_index, nsk_tostring, nsk_eq, nsk_new, "NumberSequenceKeypoint.new");
    global(TAG_COLORSEQUENCE, "ColorSequence", seq_index, seq_tostring, seq_eq, cs_new, "ColorSequence.new");
    global(TAG_COLORSEQUENCEKEYPOINT, "ColorSequenceKeypoint", csk_index, csk_tostring, csk_eq, csk_new, "ColorSequenceKeypoint.new");
}

// ---- FloatCurveKey / RotationCurveKey ------------------------------------------------
// Every field is writable, as on Roblox; a RightTangent needs Cubic interpolation, and leaving
// Cubic clears it.
static int interpArg(lua_State* L, int idx, int def) {
    if (lua_isnoneornil(L, idx)) return def;
    return checkEnumItem(L, idx, findEnum("KeyInterpolationMode"))->value;
}
static void pushInterp(lua_State* L, int v) { const EnumDef* e = findEnum("KeyInterpolationMode"); pushEnumItem(L, e, e->findValue(v)); }
static void pushTangent(lua_State* L, float t) { if (std::isnan(t)) lua_pushnil(L); else lua_pushnumber(L, t); }
static float tangentArg(lua_State* L, int idx) { return lua_isnil(L, idx) ? NAN : (float)luaL_checknumber(L, idx); }

bool isFloatCurveKey(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_FLOATCURVEKEY; }
FloatKeyV checkFloatCurveKey(lua_State* L, int idx) {
    if (!isFloatCurveKey(L, idx)) luaL_typeerror(L, idx, "FloatCurveKey");
    return *static_cast<FloatKeyV*>(lua_touserdata(L, idx));
}
void pushFloatCurveKey(lua_State* L, const FloatKeyV& k) {
    auto* u = static_cast<FloatKeyV*>(lua_newuserdatataggedwithmetatable(L, sizeof(FloatKeyV), TAG_FLOATCURVEKEY));
    *u = k;
}
static int fck_new(lua_State* L) {
    FloatKeyV k;
    k.time = (float)luaL_checknumber(L, 1); k.value = (float)luaL_checknumber(L, 2); k.interp = interpArg(L, 3, 2);
    pushFloatCurveKey(L, k);
    return 1;
}
static int fck_index(lua_State* L) {
    FloatKeyV k = checkFloatCurveKey(L, 1);
    const std::string_view key = luaL_checkstring(L, 2);
    if (key == "Time") { lua_pushnumber(L, k.time); return 1; }
    if (key == "Value") { lua_pushnumber(L, k.value); return 1; }
    if (key == "Interpolation") { pushInterp(L, k.interp); return 1; }
    if (key == "LeftTangent") { pushTangent(L, k.left); return 1; }
    if (key == "RightTangent") { pushTangent(L, k.right); return 1; }
    luaL_error(L, "%s is not a valid member of FloatCurveKey", key.data());
}
static int fck_newindex(lua_State* L) {
    if (!isFloatCurveKey(L, 1)) luaL_typeerror(L, 1, "FloatCurveKey");
    auto* k = static_cast<FloatKeyV*>(lua_touserdata(L, 1));
    const std::string_view key = luaL_checkstring(L, 2);
    if (key == "Time") { k->time = (float)luaL_checknumber(L, 3); return 0; }
    if (key == "Value") { k->value = (float)luaL_checknumber(L, 3); return 0; }
    if (key == "Interpolation") { k->interp = interpArg(L, 3, 2); if (k->interp != 2) k->right = NAN; return 0; }
    if (key == "LeftTangent") { k->left = tangentArg(L, 3); return 0; }
    if (key == "RightTangent") {
        if (k->interp != 2) luaL_error(L, "RightTangent can only be set on a key with Cubic interpolation");
        k->right = tangentArg(L, 3); return 0;
    }
    luaL_error(L, "%s is not a valid member of FloatCurveKey", key.data());
}
static int fck_tostring(lua_State* L) { FloatKeyV k = checkFloatCurveKey(L, 1); char b[96]; std::snprintf(b, sizeof b, "%g %g", k.time, k.value); lua_pushstring(L, b); return 1; }
static int fck_eq(lua_State* L) {
    bool ok = isFloatCurveKey(L, 1) && isFloatCurveKey(L, 2);
    if (ok) { FloatKeyV a = checkFloatCurveKey(L, 1), b = checkFloatCurveKey(L, 2); ok = a.time == b.time && a.value == b.value && a.interp == b.interp; }
    lua_pushboolean(L, ok); return 1;
}
bool isRotationCurveKey(lua_State* L, int idx) { return lua_userdatatag(L, idx) == TAG_ROTATIONCURVEKEY; }
RotKeyV checkRotationCurveKey(lua_State* L, int idx) {
    if (!isRotationCurveKey(L, idx)) luaL_typeerror(L, idx, "RotationCurveKey");
    return *static_cast<RotKeyV*>(lua_touserdata(L, idx));
}
void pushRotationCurveKey(lua_State* L, const RotKeyV& k) {
    auto* u = static_cast<RotKeyV*>(lua_newuserdatataggedwithmetatable(L, sizeof(RotKeyV), TAG_ROTATIONCURVEKEY));
    *u = k;
}
static int rck_new(lua_State* L) {
    RotKeyV k;
    k.time = (float)luaL_checknumber(L, 1); k.value = checkCFrame(L, 2); k.interp = interpArg(L, 3, 2);
    pushRotationCurveKey(L, k);
    return 1;
}
static int rck_index(lua_State* L) {
    RotKeyV k = checkRotationCurveKey(L, 1);
    const std::string_view key = luaL_checkstring(L, 2);
    if (key == "Time") { lua_pushnumber(L, k.time); return 1; }
    if (key == "Value") { pushCFrame(L, k.value); return 1; }
    if (key == "Interpolation") { pushInterp(L, k.interp); return 1; }
    if (key == "LeftTangent") { pushTangent(L, k.left); return 1; }
    if (key == "RightTangent") { pushTangent(L, k.right); return 1; }
    luaL_error(L, "%s is not a valid member of RotationCurveKey", key.data());
}
static int rck_newindex(lua_State* L) {
    if (!isRotationCurveKey(L, 1)) luaL_typeerror(L, 1, "RotationCurveKey");
    auto* k = static_cast<RotKeyV*>(lua_touserdata(L, 1));
    const std::string_view key = luaL_checkstring(L, 2);
    if (key == "Time") { k->time = (float)luaL_checknumber(L, 3); return 0; }
    if (key == "Value") { k->value = checkCFrame(L, 3); return 0; }
    if (key == "Interpolation") { k->interp = interpArg(L, 3, 2); if (k->interp != 2) k->right = NAN; return 0; }
    if (key == "LeftTangent") { k->left = tangentArg(L, 3); return 0; }
    if (key == "RightTangent") {
        if (k->interp != 2) luaL_error(L, "RightTangent can only be set on a key with Cubic interpolation");
        k->right = tangentArg(L, 3); return 0;
    }
    luaL_error(L, "%s is not a valid member of RotationCurveKey", key.data());
}
static int rck_tostring(lua_State* L) { RotKeyV k = checkRotationCurveKey(L, 1); char b[96]; std::snprintf(b, sizeof b, "%g", k.time); lua_pushstring(L, b); return 1; }
static int rck_eq(lua_State* L) {
    bool ok = isRotationCurveKey(L, 1) && isRotationCurveKey(L, 2);
    if (ok) { RotKeyV a = checkRotationCurveKey(L, 1), b = checkRotationCurveKey(L, 2); ok = a.time == b.time && a.value == b.value && a.interp == b.interp; }
    lua_pushboolean(L, ok); return 1;
}
static void openCurveKeys(lua_State* L) {
    setMeta(L, TAG_FLOATCURVEKEY, "FloatCurveKey", fck_index, nullptr, fck_tostring, fck_eq, {{"__newindex", fck_newindex}});
    lua_newtable(L);
    lua_pushcfunction(L, fck_new, "FloatCurveKey.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "FloatCurveKey");
    setMeta(L, TAG_ROTATIONCURVEKEY, "RotationCurveKey", rck_index, nullptr, rck_tostring, rck_eq, {{"__newindex", rck_newindex}});
    lua_newtable(L);
    lua_pushcfunction(L, rck_new, "RotationCurveKey.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "RotationCurveKey");
}

// ---- entry -------------------------------------------------------------------------
void openTypes(lua_State* L) {
    openCurveKeys(L);
    openFont(L);
    openSequences(L);
    openVector3(L);
    openVector2(L);
    openUDim(L);
    openColor3(L);
    openCFrame(L);
    openEnum(L);
    setMeta(L, TAG_RAY, "Ray", ray_index, ray_namecall, ray_tostring, ray_eq);
    lua_newtable(L);
    lua_pushcfunction(L, ray_new, "Ray.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Ray");
    setMeta(L, TAG_REGION3, "Region3", region3_index, region3_namecall, region3_tostring, region3_eq);
    lua_newtable(L);
    lua_pushcfunction(L, region3_new, "Region3.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Region3");
    setMeta(L, TAG_TWEENINFO, "TweenInfo", ti_index, nullptr, nullptr, nullptr);
    lua_newtable(L);
    lua_pushcfunction(L, ti_new, "TweenInfo.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "TweenInfo");
    setMeta(L, TAG_RANDOM, "Random", nullptr, rnd_namecall, nullptr, nullptr);
    lua_newtable(L);
    lua_pushcfunction(L, rnd_new, "Random.new"); lua_setfield(L, -2, "new");
    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "Random");
    openContent(L);   // last: its SourceType items are Enum items
}

} // namespace pulseblockz::rbx
