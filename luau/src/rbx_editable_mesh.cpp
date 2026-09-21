// EditableMesh, its data and its methods. See rbx_editable_mesh.h.
#include "rbx_editable_mesh.h"
#include "rbx_internal.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace pulseblockz::rbx {

// ---- the data ----------------------------------------------------------------------------
static Vec3 vsub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 vadd(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 vmul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float vdot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 vcross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static float vlen(Vec3 a) { return std::sqrt(vdot(a, a)); }
static Vec3 vunit(Vec3 a) { float l = vlen(a); return l > 1e-12f ? vmul(a, 1 / l) : Vec3{0, 0, 0}; }

Vec3 EditableMeshData::faceNormal(const F& f) const {
    return vcross(vsub(verts[f.v[1]].p, verts[f.v[0]].p), vsub(verts[f.v[2]].p, verts[f.v[0]].p));   // area-weighted: not unit
}
Vec3 EditableMeshData::normalOf(uint32_t slot) const {
    if (slot >= normals.size()) return {0, 0, 0};
    if (!normals[slot].autoCalc) return normals[slot].n;
    Vec3 sum{0, 0, 0};
    for (const F& f : faces) {
        if (!f.alive) continue;
        for (int k = 0; k < 3; k++) if (f.n[k] == slot) { sum = vadd(sum, faceNormal(f)); break; }
    }
    return vunit(sum);
}
void EditableMeshData::bounds(Vec3& lo, Vec3& hi) const {
    const float inf = std::numeric_limits<float>::infinity();
    lo = {inf, inf, inf}; hi = {-inf, -inf, -inf};
    for (const V& v : verts) {
        if (!v.alive) continue;
        lo = {std::min(lo.x, v.p.x), std::min(lo.y, v.p.y), std::min(lo.z, v.p.z)};
        hi = {std::max(hi.x, v.p.x), std::max(hi.y, v.p.y), std::max(hi.z, v.p.z)};
    }
}
void EditableMeshData::corners(std::vector<float>& pos, std::vector<float>& nrm, std::vector<float>& uv, std::vector<float>& rgba) const {
    std::vector<Vec3> nCache(normals.size());
    std::vector<char> have(normals.size(), 0);
    for (const F& f : faces) {
        if (!f.alive) continue;
        for (int k = 0; k < 3; k++) {
            const Vec3& p = verts[f.v[k]].p;
            pos.insert(pos.end(), {p.x, p.y, p.z});
            uint32_t ns = f.n[k];
            if (!have[ns]) { nCache[ns] = normalOf(ns); have[ns] = 1; }
            nrm.insert(nrm.end(), {nCache[ns].x, nCache[ns].y, nCache[ns].z});
            uv.insert(uv.end(), {uvs[f.u[k]].u, uvs[f.u[k]].v});
            const C& c = colors[f.c[k]];
            rgba.insert(rgba.end(), {c.c.r, c.c.g, c.c.b, c.a});
        }
    }
}

namespace {
struct Out {
    std::string s;
    void u32(uint32_t v) { s.append((const char*)&v, 4); }
    void f32(float v) { s.append((const char*)&v, 4); }
};
struct In {
    const std::string& s; size_t i = 0; bool ok = true;
    uint32_t u32() { uint32_t v = 0; if (i + 4 > s.size()) { ok = false; return 0; } std::memcpy(&v, s.data() + i, 4); i += 4; return v; }
    float f32() { float v = 0; if (i + 4 > s.size()) { ok = false; return 0; } std::memcpy(&v, s.data() + i, 4); i += 4; return v; }
};
}
// "PBEM", then a count and raw values per list -- vertices, normals (with autoCalc), uvs, colours
// -- then each face's 3 corners as 4 indices. Dead slots go and the rest renumber: ids do not survive.
std::string EditableMeshData::serialize() const {
    Out o;
    o.s = "PBEM";
    auto remap = [](const auto& list) { std::vector<uint32_t> m(list.size(), 0); uint32_t n = 0; for (size_t k = 0; k < list.size(); k++) if (list[k].alive) m[k] = n++; return std::make_pair(m, n); };
    auto [vm, vn] = remap(verts); auto [nm, nn] = remap(normals); auto [um, un] = remap(uvs); auto [cm, cn] = remap(colors); auto [fm, fn] = remap(faces);
    (void)fm;
    o.u32(vn); for (const V& v : verts) if (v.alive) { o.f32(v.p.x); o.f32(v.p.y); o.f32(v.p.z); }
    o.u32(nn); for (size_t k = 0; k < normals.size(); k++) if (normals[k].alive) { Vec3 n = normalOf((uint32_t)k); o.f32(n.x); o.f32(n.y); o.f32(n.z); o.u32(normals[k].autoCalc); }
    o.u32(un); for (const U& u : uvs) if (u.alive) { o.f32(u.u); o.f32(u.v); }
    o.u32(cn); for (const C& c : colors) if (c.alive) { o.f32(c.c.r); o.f32(c.c.g); o.f32(c.c.b); o.f32(c.a); }
    o.u32(fn);
    for (const F& f : faces) if (f.alive) for (int k = 0; k < 3; k++) { o.u32(vm[f.v[k]]); o.u32(nm[f.n[k]]); o.u32(um[f.u[k]]); o.u32(cm[f.c[k]]); }
    return o.s;
}
bool EditableMeshData::deserialize(const std::string& bytes) {
    if (bytes.size() < 4 || bytes.compare(0, 4, "PBEM")) return false;
    In in{bytes};
    in.i = 4;
    *this = EditableMeshData();
    uint32_t vn = in.u32(); if (vn > 1000000) return false;
    for (uint32_t k = 0; k < vn && in.ok; k++) { V v; v.p.x = in.f32(); v.p.y = in.f32(); v.p.z = in.f32(); verts.push_back(v); }
    uint32_t nn = in.u32(); if (nn > 4000000) return false;
    for (uint32_t k = 0; k < nn && in.ok; k++) { N n; n.n.x = in.f32(); n.n.y = in.f32(); n.n.z = in.f32(); n.autoCalc = in.u32() != 0; normals.push_back(n); }
    uint32_t un = in.u32(); if (un > 4000000) return false;
    for (uint32_t k = 0; k < un && in.ok; k++) { U u; u.u = in.f32(); u.v = in.f32(); uvs.push_back(u); }
    uint32_t cn = in.u32(); if (cn > 4000000) return false;
    for (uint32_t k = 0; k < cn && in.ok; k++) { C c; c.c.r = in.f32(); c.c.g = in.f32(); c.c.b = in.f32(); c.a = in.f32(); colors.push_back(c); }
    uint32_t fn = in.u32(); if (fn > 1000000) return false;
    for (uint32_t k = 0; k < fn && in.ok; k++) {
        F f;
        for (int j = 0; j < 3; j++) { f.v[j] = in.u32(); f.n[j] = in.u32(); f.u[j] = in.u32(); f.c[j] = in.u32(); }
        for (int j = 0; j < 3; j++) if (f.v[j] >= vn || f.n[j] >= nn || f.u[j] >= un || f.c[j] >= cn) return false;
        faces.push_back(f);
    }
    return in.ok;
}

// ---- the Lua side ---------------------------------------------------------------------------
namespace em {

using M = EditableMeshData;
static constexpr size_t kMaxVerts = 60000, kMaxFaces = 20000, kMaxBones = 256;

static Instance& selfObj(lua_State* L) { return rtOf(L).checkObject(L, 1); }
M& meshOf(lua_State* L, Instance& o, const char* method) {
    Runtime::Impl& rt = rtOf(L);
    auto it = rt.editableMeshes.find(o.id());
    if (it == rt.editableMeshes.end() || it->second.destroyed) luaL_error(L, "%s: the EditableMesh has been destroyed", method);
    if (it->second.placeholder) luaL_error(L, "%s: this EditableMesh is a placeholder for one on the other side, and its contents cannot be read or written", method);
    return it->second;
}
static M& me(lua_State* L, const char* method) { return meshOf(L, selfObj(L), method); }
static int64_t idArg(lua_State* L, int idx) {
    double d = luaL_checknumber(L, idx);
    if (!std::isfinite(d) || d < 0) luaL_error(L, "invalid id");
    return (int64_t)d;
}
static void pushId(lua_State* L, M::Kind k, uint32_t slot) { lua_pushnumber(L, (double)M::id(k, slot)); }
static const char* kindName(int k) {
    switch (k) { case M::Vertex: return "vertex"; case M::Normal: return "normal"; case M::UV: return "UV"; case M::Color: return "color"; case M::Face: return "face"; case M::Bone: return "bone"; }
    return "unknown";
}
// The slot of a live element of kind k; throws when it is not one. liveSlot returns false.
static uint32_t slot(lua_State* L, M& m, int64_t id, M::Kind k) {
    uint32_t s = M::slotOf(id);
    bool ok = M::kindOf(id) == k;
    if (ok) switch (k) {
        case M::Vertex: ok = s < m.verts.size() && m.verts[s].alive; break;
        case M::Normal: ok = s < m.normals.size() && m.normals[s].alive; break;
        case M::UV: ok = s < m.uvs.size() && m.uvs[s].alive; break;
        case M::Color: ok = s < m.colors.size() && m.colors[s].alive; break;
        case M::Face: ok = s < m.faces.size() && m.faces[s].alive; break;
        case M::Bone: ok = s < m.bones.size() && m.bones[s].alive; break;
    }
    if (!ok) luaL_error(L, "invalid %s id", kindName(k));
    return s;
}
static bool liveSlot(M& m, int64_t id, M::Kind k, uint32_t& s) {
    s = M::slotOf(id);
    if (M::kindOf(id) != k) return false;
    switch (k) {
        case M::Vertex: return s < m.verts.size() && m.verts[s].alive;
        case M::Normal: return s < m.normals.size() && m.normals[s].alive;
        case M::UV: return s < m.uvs.size() && m.uvs[s].alive;
        case M::Color: return s < m.colors.size() && m.colors[s].alive;
        case M::Face: return s < m.faces.size() && m.faces[s].alive;
        case M::Bone: return s < m.bones.size() && m.bones[s].alive;
    }
    return false;
}
static void growable(lua_State* L, M& m, const char* method) {
    // Values may still be set on a fixed-size mesh; only adding and removing is barred (Roblox).
    if (m.fixedSize) luaL_error(L, "%s: a fixed-size EditableMesh cannot have elements added or removed", method);
}
static std::vector<float> cframeData(const CFrameV& c) { std::vector<float> d{c.p.x, c.p.y, c.p.z}; d.insert(d.end(), c.m, c.m + 9); return d; }
static CFrameV cframeOf(const std::vector<float>& d) { CFrameV c; if (d.size() == 12) { c.p = {d[0], d[1], d[2]}; std::copy(d.begin() + 3, d.end(), c.m); } return c; }
static std::vector<int64_t> idList(lua_State* L, int idx) {
    luaL_checktype(L, idx, LUA_TTABLE);
    std::vector<int64_t> out;
    int n = (int)lua_objlen(L, idx);
    for (int k = 1; k <= n; k++) { lua_rawgeti(L, idx, k); out.push_back(idArg(L, -1)); lua_pop(L, 1); }
    return out;
}
static void pushIds(lua_State* L, M::Kind k, const std::vector<uint32_t>& slots) {
    lua_createtable(L, (int)slots.size(), 0);
    for (size_t j = 0; j < slots.size(); j++) { pushId(L, k, slots[j]); lua_rawseti(L, -2, (int)j + 1); }
}

// ---- adding -------------------------------------------------------------------------------
static uint32_t addVertex(lua_State* L, M& m, Vec3 p) {
    if (m.liveVerts() >= kMaxVerts) luaL_error(L, "AddVertex: an EditableMesh holds at most 60,000 vertices");
    m.verts.push_back({p});
    m.dirty = true;
    return (uint32_t)m.verts.size() - 1;
}
static uint32_t addNormal(M& m, const Vec3* n) { M::N x; if (n) { x.n = *n; x.autoCalc = false; } m.normals.push_back(x); return (uint32_t)m.normals.size() - 1; }
static uint32_t addUV(M& m, float u, float v) { M::U x; x.u = u; x.v = v; m.uvs.push_back(x); return (uint32_t)m.uvs.size() - 1; }
static uint32_t addColor(M& m, Col3 c, float a) { M::C x; x.c = c; x.a = a; m.colors.push_back(x); return (uint32_t)m.colors.size() - 1; }
// A corner takes the ids another face already gave this vertex, else fresh ones with default values (Roblox's rule).
static uint32_t addTriangle(lua_State* L, M& m, uint32_t v[3]) {
    if (m.liveFaces() >= kMaxFaces) luaL_error(L, "AddTriangle: an EditableMesh holds at most 20,000 triangles");
    M::F f;
    for (int k = 0; k < 3; k++) {
        f.v[k] = v[k];
        bool found = false;
        for (const M::F& o : m.faces) {
            if (!o.alive) continue;
            for (int j = 0; j < 3; j++) if (o.v[j] == v[k]) { f.n[k] = o.n[j]; f.u[k] = o.u[j]; f.c[k] = o.c[j]; found = true; break; }
            if (found) break;
        }
        if (!found) {
            // the same vertex twice in one new triangle shares the one set it just made
            for (int j = 0; j < k && !found; j++) if (v[j] == v[k]) { f.n[k] = f.n[j]; f.u[k] = f.u[j]; f.c[k] = f.c[j]; found = true; }
        }
        if (!found) { f.n[k] = addNormal(m, nullptr); f.u[k] = addUV(m, 0, 0); f.c[k] = addColor(m, {1, 1, 1}, 1); }
    }
    m.faces.push_back(f);
    m.dirty = true;
    return (uint32_t)m.faces.size() - 1;
}

static int AddVertex(lua_State* L) { M& m = me(L, "AddVertex"); growable(L, m, "AddVertex"); pushId(L, M::Vertex, addVertex(L, m, checkVec3(L, 2))); return 1; }
static int AddNormal(lua_State* L) {
    M& m = me(L, "AddNormal"); growable(L, m, "AddNormal");
    if (lua_isnoneornil(L, 2)) pushId(L, M::Normal, addNormal(m, nullptr));
    else { Vec3 n = checkVec3(L, 2); pushId(L, M::Normal, addNormal(m, &n)); }
    return 1;
}
static int AddUV(lua_State* L) { M& m = me(L, "AddUV"); growable(L, m, "AddUV"); Vec2 uv = checkVector2(L, 2); pushId(L, M::UV, addUV(m, uv.x, uv.y)); return 1; }
static int AddColor(lua_State* L) { M& m = me(L, "AddColor"); growable(L, m, "AddColor"); pushId(L, M::Color, addColor(m, checkColor3(L, 2), (float)luaL_checknumber(L, 3))); return 1; }
static int AddTriangle(lua_State* L) {
    M& m = me(L, "AddTriangle"); growable(L, m, "AddTriangle");
    uint32_t v[3];
    for (int k = 0; k < 3; k++) v[k] = slot(L, m, idArg(L, 2 + k), M::Vertex);
    pushId(L, M::Face, addTriangle(L, m, v));
    return 1;
}
static int AddBone(lua_State* L) {
    M& m = me(L, "AddBone"); growable(L, m, "AddBone");
    luaL_checktype(L, 2, LUA_TTABLE);
    M::B b;
    lua_getfield(L, 2, "Name"); b.name = luaL_checkstring(L, -1); lua_pop(L, 1);
    if (b.name.size() > 100) luaL_error(L, "AddBone: a bone name is at most 100 characters");
    for (const M::B& o : m.bones) if (o.alive && o.name == b.name) luaL_error(L, "AddBone: a bone named %s is already in the mesh", b.name.c_str());
    size_t live = 0; for (const M::B& o : m.bones) live += o.alive;
    if (live >= kMaxBones) luaL_error(L, "AddBone: the mesh already has the most bones it can");
    lua_getfield(L, 2, "ParentId"); if (!lua_isnil(L, -1)) { int64_t p = idArg(L, -1); if (p != 0) { slot(L, m, p, M::Bone); b.parent = p; } } lua_pop(L, 1);
    lua_getfield(L, 2, "CFrame"); b.cf = cframeData(lua_isnil(L, -1) ? CFrameV::identity() : checkCFrame(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 2, "Virtual"); b.virt = lua_toboolean(L, -1) != 0; lua_pop(L, 1);
    m.bones.push_back(b);
    pushId(L, M::Bone, (uint32_t)m.bones.size() - 1);
    return 1;
}

// ---- reading ------------------------------------------------------------------------------
static int GetPosition(lua_State* L) { M& m = me(L, "GetPosition"); pushVec3(L, m.verts[slot(L, m, idArg(L, 2), M::Vertex)].p); return 1; }
static int GetNormal(lua_State* L) { M& m = me(L, "GetNormal"); uint32_t s; if (!liveSlot(m, idArg(L, 2), M::Normal, s)) { lua_pushnil(L); return 1; } pushVec3(L, m.normalOf(s)); return 1; }
static int GetUV(lua_State* L) { M& m = me(L, "GetUV"); uint32_t s; if (!liveSlot(m, idArg(L, 2), M::UV, s)) { lua_pushnil(L); return 1; } pushVector2(L, {m.uvs[s].u, m.uvs[s].v}); return 1; }
static int GetColor(lua_State* L) { M& m = me(L, "GetColor"); uint32_t s; if (!liveSlot(m, idArg(L, 2), M::Color, s)) { lua_pushnil(L); return 1; } pushColor3(L, m.colors[s].c); return 1; }
static int GetColorAlpha(lua_State* L) { M& m = me(L, "GetColorAlpha"); uint32_t s; if (!liveSlot(m, idArg(L, 2), M::Color, s)) { lua_pushnil(L); return 1; } lua_pushnumber(L, m.colors[s].a); return 1; }
template <class T> static int listAlive(lua_State* L, const std::vector<T>& list, M::Kind k) {
    std::vector<uint32_t> s; for (size_t j = 0; j < list.size(); j++) if (list[j].alive) s.push_back((uint32_t)j);
    pushIds(L, k, s);
    return 1;
}
static int GetVertices(lua_State* L) { return listAlive(L, me(L, "GetVertices").verts, M::Vertex); }
static int GetNormals(lua_State* L) { return listAlive(L, me(L, "GetNormals").normals, M::Normal); }
static int GetUVs(lua_State* L) { return listAlive(L, me(L, "GetUVs").uvs, M::UV); }
static int GetColors(lua_State* L) { return listAlive(L, me(L, "GetColors").colors, M::Color); }
static int GetFaces(lua_State* L) { return listAlive(L, me(L, "GetFaces").faces, M::Face); }
static int GetBones(lua_State* L) { return listAlive(L, me(L, "GetBones").bones, M::Bone); }
static int faceCorners(lua_State* L, const char* method, int which) {
    M& m = me(L, method);
    const M::F& f = m.faces[slot(L, m, idArg(L, 2), M::Face)];
    static const M::Kind kinds[4] = {M::Vertex, M::Normal, M::UV, M::Color};
    const uint32_t* arr = which == 0 ? f.v : which == 1 ? f.n : which == 2 ? f.u : f.c;
    pushIds(L, kinds[which], {arr[0], arr[1], arr[2]});
    return 1;
}
static int GetFaceVertices(lua_State* L) { return faceCorners(L, "GetFaceVertices", 0); }
static int GetFaceNormals(lua_State* L) { return faceCorners(L, "GetFaceNormals", 1); }
static int GetFaceUVs(lua_State* L) { return faceCorners(L, "GetFaceUVs", 2); }
static int GetFaceColors(lua_State* L) { return faceCorners(L, "GetFaceColors", 3); }
// The faces using an attribute id, or -- vertexWanted -- the vertices at those corners.
static int withAttribute(lua_State* L, const char* method, M::Kind k, bool vertexWanted) {
    M& m = me(L, method);
    uint32_t s = slot(L, m, idArg(L, 2), k);
    std::vector<uint32_t> out;
    for (size_t j = 0; j < m.faces.size(); j++) {
        const M::F& f = m.faces[j];
        if (!f.alive) continue;
        for (int c = 0; c < 3; c++) {
            uint32_t a = k == M::Vertex ? f.v[c] : k == M::Normal ? f.n[c] : k == M::UV ? f.u[c] : f.c[c];
            if (a != s) continue;
            uint32_t v = vertexWanted ? f.v[c] : (uint32_t)j;
            if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
        }
    }
    pushIds(L, vertexWanted ? M::Vertex : M::Face, out);
    return 1;
}
static int GetFacesWithNormal(lua_State* L) { return withAttribute(L, "GetFacesWithNormal", M::Normal, false); }
static int GetFacesWithUV(lua_State* L) { return withAttribute(L, "GetFacesWithUV", M::UV, false); }
static int GetFacesWithColor(lua_State* L) { return withAttribute(L, "GetFacesWithColor", M::Color, false); }
static int GetVerticesWithNormal(lua_State* L) { return withAttribute(L, "GetVerticesWithNormal", M::Normal, true); }
static int GetVerticesWithUV(lua_State* L) { return withAttribute(L, "GetVerticesWithUV", M::UV, true); }
static int GetVerticesWithColor(lua_State* L) { return withAttribute(L, "GetVerticesWithColor", M::Color, true); }
static int GetVertexFaces(lua_State* L) { return withAttribute(L, "GetVertexFaces", M::Vertex, false); }
// Deprecated on Roblox, still answered here: one entry point for any kind of id.
static int GetFacesWithAttribute(lua_State* L) {
    M& m = me(L, "GetFacesWithAttribute");
    int k = M::kindOf(idArg(L, 2));
    if (k < M::Vertex || k > M::Color) luaL_error(L, "GetFacesWithAttribute: expected a vertex, normal, UV or color id");
    (void)m;
    return withAttribute(L, "GetFacesWithAttribute", (M::Kind)k, false);
}
static int GetVerticesWithAttribute(lua_State* L) {
    M& m = me(L, "GetVerticesWithAttribute");
    int64_t id = idArg(L, 2);
    if (M::kindOf(id) == M::Face) { const M::F& f = m.faces[slot(L, m, id, M::Face)]; pushIds(L, M::Vertex, {f.v[0], f.v[1], f.v[2]}); return 1; }
    int k = M::kindOf(id);
    if (k < M::Normal || k > M::Color) luaL_error(L, "GetVerticesWithAttribute: expected a face, normal, UV or color id");
    return withAttribute(L, "GetVerticesWithAttribute", (M::Kind)k, true);
}
static int vertexAttributes(lua_State* L, const char* method, M::Kind k) {
    M& m = me(L, method);
    uint32_t v = slot(L, m, idArg(L, 2), M::Vertex);
    std::vector<uint32_t> out;
    for (const M::F& f : m.faces) {
        if (!f.alive) continue;
        for (int c = 0; c < 3; c++) if (f.v[c] == v) {
            uint32_t a = k == M::Normal ? f.n[c] : k == M::UV ? f.u[c] : f.c[c];
            if (std::find(out.begin(), out.end(), a) == out.end()) out.push_back(a);
        }
    }
    pushIds(L, k, out);
    return 1;
}
static int GetVertexNormals(lua_State* L) { return vertexAttributes(L, "GetVertexNormals", M::Normal); }
static int GetVertexUVs(lua_State* L) { return vertexAttributes(L, "GetVertexUVs", M::UV); }
static int GetVertexColors(lua_State* L) { return vertexAttributes(L, "GetVertexColors", M::Color); }
static int corner(lua_State* L, M& m, uint32_t& face) {
    uint32_t v = slot(L, m, idArg(L, 2), M::Vertex);
    face = slot(L, m, idArg(L, 3), M::Face);
    for (int c = 0; c < 3; c++) if (m.faces[face].v[c] == v) return c;
    luaL_error(L, "the vertex is not a corner of that face");
    return 0;
}
static int GetVertexFaceNormal(lua_State* L) { M& m = me(L, "GetVertexFaceNormal"); uint32_t f; int c = corner(L, m, f); pushId(L, M::Normal, m.faces[f].n[c]); return 1; }
static int GetVertexFaceUV(lua_State* L) { M& m = me(L, "GetVertexFaceUV"); uint32_t f; int c = corner(L, m, f); pushId(L, M::UV, m.faces[f].u[c]); return 1; }
static int GetVertexFaceColor(lua_State* L) { M& m = me(L, "GetVertexFaceColor"); uint32_t f; int c = corner(L, m, f); pushId(L, M::Color, m.faces[f].c[c]); return 1; }
static int SetVertexFaceNormal(lua_State* L) { M& m = me(L, "SetVertexFaceNormal"); uint32_t f; int c = corner(L, m, f); m.faces[f].n[c] = slot(L, m, idArg(L, 4), M::Normal); m.dirty = true; return 0; }
static int SetVertexFaceUV(lua_State* L) { M& m = me(L, "SetVertexFaceUV"); uint32_t f; int c = corner(L, m, f); m.faces[f].u[c] = slot(L, m, idArg(L, 4), M::UV); m.dirty = true; return 0; }
static int SetVertexFaceColor(lua_State* L) { M& m = me(L, "SetVertexFaceColor"); uint32_t f; int c = corner(L, m, f); m.faces[f].c[c] = slot(L, m, idArg(L, 4), M::Color); m.dirty = true; return 0; }
static int GetAdjacentVertices(lua_State* L) {
    M& m = me(L, "GetAdjacentVertices");
    uint32_t v = slot(L, m, idArg(L, 2), M::Vertex);
    std::vector<uint32_t> out;
    for (const M::F& f : m.faces) {
        if (!f.alive) continue;
        for (int c = 0; c < 3; c++) if (f.v[c] == v)
            for (int o = 0; o < 3; o++) if (f.v[o] != v && std::find(out.begin(), out.end(), f.v[o]) == out.end()) out.push_back(f.v[o]);
    }
    pushIds(L, M::Vertex, out);
    return 1;
}
// Adjacent means sharing an edge, i.e. two of the three vertices.
static int GetAdjacentFaces(lua_State* L) {
    M& m = me(L, "GetAdjacentFaces");
    uint32_t fs = slot(L, m, idArg(L, 2), M::Face);
    const M::F& f = m.faces[fs];
    std::vector<uint32_t> out;
    for (size_t j = 0; j < m.faces.size(); j++) {
        if (j == fs || !m.faces[j].alive) continue;
        int shared = 0;
        for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) shared += f.v[a] == m.faces[j].v[b];
        if (shared >= 2) out.push_back((uint32_t)j);
    }
    pushIds(L, M::Face, out);
    return 1;
}
static int GetCenter(lua_State* L) { M& m = me(L, "GetCenter"); Vec3 lo, hi; m.bounds(lo, hi); pushVec3(L, lo.x > hi.x ? Vec3{0, 0, 0} : vmul(vadd(lo, hi), 0.5f)); return 1; }
static int GetSize(lua_State* L) { M& m = me(L, "GetSize"); Vec3 lo, hi; m.bounds(lo, hi); pushVec3(L, lo.x > hi.x ? Vec3{0, 0, 0} : vsub(hi, lo)); return 1; }
static int IdDebugString(lua_State* L) {
    int64_t id = idArg(L, 2);
    static const char* letters[] = {"?", "v", "n", "t", "c", "f", "b"};
    int k = M::kindOf(id);
    std::string s = std::string(k >= 1 && k <= 6 ? letters[k] : "?") + std::to_string((long long)M::slotOf(id));
    lua_pushstring(L, s.c_str());
    return 1;
}

// ---- writing ------------------------------------------------------------------------------
static int SetPosition(lua_State* L) { M& m = me(L, "SetPosition"); m.verts[slot(L, m, idArg(L, 2), M::Vertex)].p = checkVec3(L, 3); m.dirty = true; return 0; }
static int SetNormal(lua_State* L) { M& m = me(L, "SetNormal"); auto& n = m.normals[slot(L, m, idArg(L, 2), M::Normal)]; n.n = checkVec3(L, 3); n.autoCalc = false; m.dirty = true; return 0; }
static int ResetNormal(lua_State* L) { M& m = me(L, "ResetNormal"); m.normals[slot(L, m, idArg(L, 2), M::Normal)].autoCalc = true; m.dirty = true; return 0; }
static int SetUV(lua_State* L) { M& m = me(L, "SetUV"); auto& u = m.uvs[slot(L, m, idArg(L, 2), M::UV)]; Vec2 v = checkVector2(L, 3); u.u = v.x; u.v = v.y; m.dirty = true; return 0; }
static int SetColor(lua_State* L) { M& m = me(L, "SetColor"); m.colors[slot(L, m, idArg(L, 2), M::Color)].c = checkColor3(L, 3); m.dirty = true; return 0; }
static int SetColorAlpha(lua_State* L) { M& m = me(L, "SetColorAlpha"); m.colors[slot(L, m, idArg(L, 2), M::Color)].a = (float)luaL_checknumber(L, 3); m.dirty = true; return 0; }
static int setFaceCorners(lua_State* L, const char* method, M::Kind k) {
    M& m = me(L, method);
    uint32_t f = slot(L, m, idArg(L, 2), M::Face);
    std::vector<int64_t> ids = idList(L, 3);
    if (ids.size() != 3) luaL_error(L, "%s: expected one id per corner of the face (3)", method);
    uint32_t s[3];
    for (int c = 0; c < 3; c++) s[c] = slot(L, m, ids[c], k);
    for (int c = 0; c < 3; c++) {
        if (k == M::Vertex) m.faces[f].v[c] = s[c];
        else if (k == M::Normal) m.faces[f].n[c] = s[c];
        else if (k == M::UV) m.faces[f].u[c] = s[c];
        else m.faces[f].c[c] = s[c];
    }
    m.dirty = true;
    return 0;
}
static int SetFaceVertices(lua_State* L) { return setFaceCorners(L, "SetFaceVertices", M::Vertex); }
static int SetFaceNormals(lua_State* L) { return setFaceCorners(L, "SetFaceNormals", M::Normal); }
static int SetFaceUVs(lua_State* L) { return setFaceCorners(L, "SetFaceUVs", M::UV); }
static int SetFaceColors(lua_State* L) { return setFaceCorners(L, "SetFaceColors", M::Color); }

// ---- removing -----------------------------------------------------------------------------
static int RemoveFace(lua_State* L) { M& m = me(L, "RemoveFace"); growable(L, m, "RemoveFace"); m.faces[slot(L, m, idArg(L, 2), M::Face)].alive = false; m.dirty = true; return 0; }
static int RemoveUnused(lua_State* L) {
    M& m = me(L, "RemoveUnused"); growable(L, m, "RemoveUnused");
    std::vector<char> v(m.verts.size()), n(m.normals.size()), u(m.uvs.size()), c(m.colors.size());
    for (const M::F& f : m.faces) if (f.alive) for (int k = 0; k < 3; k++) { v[f.v[k]] = n[f.n[k]] = u[f.u[k]] = c[f.c[k]] = 1; }
    lua_newtable(L);
    int out = 0;
    auto sweep = [&](auto& list, std::vector<char>& used, M::Kind k) {
        for (size_t j = 0; j < list.size(); j++) if (list[j].alive && !used[j]) { list[j].alive = false; pushId(L, k, (uint32_t)j); lua_rawseti(L, -2, ++out); }
    };
    sweep(m.verts, v, M::Vertex); sweep(m.normals, n, M::Normal); sweep(m.uvs, u, M::UV); sweep(m.colors, c, M::Color);
    m.dirty = true;
    return 1;
}
static int RemoveBone(lua_State* L) {
    M& m = me(L, "RemoveBone");
    int64_t id = idArg(L, 2);
    m.bones[slot(L, m, id, M::Bone)].alive = false;
    // Skinning weights naming it are dropped, as on Roblox, and it leaves every pose it was in.
    for (M::V& v : m.verts)
        for (size_t j = 0; j < v.bones.size();) {
            if (v.bones[j] == id) { v.bones.erase(v.bones.begin() + (int64_t)j); if (j < v.weights.size()) v.weights.erase(v.weights.begin() + (int64_t)j); }
            else j++;
        }
    for (M::B& b : m.bones) if (b.parent == id) b.parent = 0;
    for (auto& [k, pose] : m.facs) pose.erase(std::remove_if(pose.begin(), pose.end(), [&](auto& e) { return e.first == id; }), pose.end());
    for (auto& [k, pose] : m.correctives) pose.erase(std::remove_if(pose.begin(), pose.end(), [&](auto& e) { return e.first == id; }), pose.end());
    return 0;
}
static int Clear(lua_State* L) {
    M& m = me(L, "Clear");
    growable(L, m, "Clear");
    bool fixed = m.fixedSize;
    m = M();
    m.fixedSize = fixed;
    m.dirty = true;
    return 0;
}
static int Triangulate(lua_State* L) { (void)me(L, "Triangulate"); return 0; }   // a no-op on Roblox too: only triangles can be created
// Vertices within `tolerance` become one: faces move to the first. Returns removed id -> survivor.
static int MergeVertices(lua_State* L) {
    M& m = me(L, "MergeVertices"); growable(L, m, "MergeVertices");
    float tol = (float)luaL_checknumber(L, 2);
    std::vector<uint32_t> into(m.verts.size());
    for (size_t j = 0; j < m.verts.size(); j++) into[j] = (uint32_t)j;
    lua_newtable(L);
    for (size_t j = 0; j < m.verts.size(); j++) {
        if (!m.verts[j].alive || into[j] != j) continue;
        for (size_t k = j + 1; k < m.verts.size(); k++) {
            if (!m.verts[k].alive || into[k] != k) continue;
            if (vlen(vsub(m.verts[j].p, m.verts[k].p)) <= tol) {
                into[k] = (uint32_t)j;
                m.verts[k].alive = false;
                pushId(L, M::Vertex, (uint32_t)k); pushId(L, M::Vertex, (uint32_t)j); lua_rawset(L, -3);
            }
        }
    }
    for (M::F& f : m.faces) for (int c = 0; c < 3; c++) f.v[c] = into[f.v[c]];
    m.dirty = true;
    return 1;
}

// ---- queries ------------------------------------------------------------------------------
static bool rayTriangle(Vec3 o, Vec3 d, Vec3 a, Vec3 b, Vec3 c, float& t, float& u, float& v) {
    Vec3 e1 = vsub(b, a), e2 = vsub(c, a), p = vcross(d, e2);
    float det = vdot(e1, p);
    if (std::abs(det) < 1e-12f) return false;
    float inv = 1 / det;
    Vec3 s = vsub(o, a);
    u = vdot(s, p) * inv; if (u < 0 || u > 1) return false;
    Vec3 q = vcross(s, e1);
    v = vdot(d, q) * inv; if (v < 0 || u + v > 1) return false;
    t = vdot(e2, q) * inv;
    return t >= 0;
}
// -> faceId, point, barycentric, 3 vertex ids: the nearest face within the direction's own length, nil on a miss.
static int RaycastLocal(lua_State* L) {
    M& m = me(L, "RaycastLocal");
    Vec3 o = checkVec3(L, 2), d = checkVec3(L, 3);
    float best = std::numeric_limits<float>::infinity(), bu = 0, bv = 0; int hit = -1;
    for (size_t j = 0; j < m.faces.size(); j++) {
        const M::F& f = m.faces[j];
        if (!f.alive) continue;
        float t, u, v;
        if (rayTriangle(o, d, m.verts[f.v[0]].p, m.verts[f.v[1]].p, m.verts[f.v[2]].p, t, u, v) && t <= 1 && t < best) { best = t; bu = u; bv = v; hit = (int)j; }
    }
    if (hit < 0) { lua_pushnil(L); return 1; }
    const M::F& f = m.faces[hit];
    pushId(L, M::Face, (uint32_t)hit);
    pushVec3(L, vadd(o, vmul(d, best)));
    pushVec3(L, Vec3{1 - bu - bv, bu, bv});
    for (int c = 0; c < 3; c++) pushId(L, M::Vertex, f.v[c]);
    return 6;
}
static Vec3 closestOnTri(Vec3 p, Vec3 a, Vec3 b, Vec3 c, Vec3& bary) {
    // Ericson, Real-Time Collision Detection 5.1.5
    Vec3 ab = vsub(b, a), ac = vsub(c, a), ap = vsub(p, a);
    float d1 = vdot(ab, ap), d2 = vdot(ac, ap);
    if (d1 <= 0 && d2 <= 0) { bary = {1, 0, 0}; return a; }
    Vec3 bp = vsub(p, b); float d3 = vdot(ab, bp), d4 = vdot(ac, bp);
    if (d3 >= 0 && d4 <= d3) { bary = {0, 1, 0}; return b; }
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { float v = d1 / (d1 - d3); bary = {1 - v, v, 0}; return vadd(a, vmul(ab, v)); }
    Vec3 cp = vsub(p, c); float d5 = vdot(ab, cp), d6 = vdot(ac, cp);
    if (d6 >= 0 && d5 <= d6) { bary = {0, 0, 1}; return c; }
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { float w = d2 / (d2 - d6); bary = {1 - w, 0, w}; return vadd(a, vmul(ac, w)); }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) { float w = (d4 - d3) / ((d4 - d3) + (d5 - d6)); bary = {0, 1 - w, w}; return vadd(b, vmul(vsub(c, b), w)); }
    float denom = 1 / (va + vb + vc), v = vb * denom, w = vc * denom;
    bary = {1 - v - w, v, w};
    return vadd(a, vadd(vmul(ab, v), vmul(ac, w)));
}
static int FindClosestPointOnSurface(lua_State* L) {
    M& m = me(L, "FindClosestPointOnSurface");
    Vec3 p = checkVec3(L, 2);
    float best = std::numeric_limits<float>::infinity(); int hit = -1; Vec3 bestP, bestB;
    for (size_t j = 0; j < m.faces.size(); j++) {
        const M::F& f = m.faces[j];
        if (!f.alive) continue;
        Vec3 bary;
        Vec3 q = closestOnTri(p, m.verts[f.v[0]].p, m.verts[f.v[1]].p, m.verts[f.v[2]].p, bary);
        float dd = vlen(vsub(q, p));
        if (dd < best) { best = dd; hit = (int)j; bestP = q; bestB = bary; }
    }
    if (hit < 0) { lua_pushnil(L); return 1; }
    pushId(L, M::Face, (uint32_t)hit); pushVec3(L, bestP); pushVec3(L, bestB);
    return 3;
}
static int FindClosestVertex(lua_State* L) {
    M& m = me(L, "FindClosestVertex");
    Vec3 p = checkVec3(L, 2);
    float best = std::numeric_limits<float>::infinity(); int hit = -1;
    for (size_t j = 0; j < m.verts.size(); j++) if (m.verts[j].alive) { float d = vlen(vsub(m.verts[j].p, p)); if (d < best) { best = d; hit = (int)j; } }
    if (hit < 0) lua_pushnil(L); else pushId(L, M::Vertex, (uint32_t)hit);
    return 1;
}
static int FindVerticesWithinSphere(lua_State* L) {
    M& m = me(L, "FindVerticesWithinSphere");
    Vec3 c = checkVec3(L, 2); float r = (float)luaL_checknumber(L, 3);
    std::vector<uint32_t> out;
    for (size_t j = 0; j < m.verts.size(); j++) if (m.verts[j].alive && vlen(vsub(m.verts[j].p, c)) <= r) out.push_back((uint32_t)j);
    pushIds(L, M::Vertex, out);
    return 1;
}

// ---- bones and FACS -----------------------------------------------------------------------------
static int GetBoneByName(lua_State* L) {
    M& m = me(L, "GetBoneByName");
    std::string name = luaL_checkstring(L, 2);
    for (size_t j = 0; j < m.bones.size(); j++) if (m.bones[j].alive && m.bones[j].name == name) { pushId(L, M::Bone, (uint32_t)j); return 1; }
    luaL_error(L, "GetBoneByName: no bone named %s", name.c_str());
    return 0;
}
static int GetBoneCFrame(lua_State* L) { M& m = me(L, "GetBoneCFrame"); pushCFrame(L, cframeOf(m.bones[slot(L, m, idArg(L, 2), M::Bone)].cf)); return 1; }
static int GetBoneIsVirtual(lua_State* L) { M& m = me(L, "GetBoneIsVirtual"); lua_pushboolean(L, m.bones[slot(L, m, idArg(L, 2), M::Bone)].virt); return 1; }
static int GetBoneName(lua_State* L) { M& m = me(L, "GetBoneName"); lua_pushstring(L, m.bones[slot(L, m, idArg(L, 2), M::Bone)].name.c_str()); return 1; }
static int GetBoneParent(lua_State* L) { M& m = me(L, "GetBoneParent"); lua_pushnumber(L, (double)m.bones[slot(L, m, idArg(L, 2), M::Bone)].parent); return 1; }
static int SetBoneCFrame(lua_State* L) { M& m = me(L, "SetBoneCFrame"); m.bones[slot(L, m, idArg(L, 2), M::Bone)].cf = cframeData(checkCFrame(L, 3)); return 0; }
static int SetBoneIsVirtual(lua_State* L) { M& m = me(L, "SetBoneIsVirtual"); m.bones[slot(L, m, idArg(L, 2), M::Bone)].virt = lua_toboolean(L, 3) != 0; return 0; }
static int SetBoneName(lua_State* L) {
    M& m = me(L, "SetBoneName");
    uint32_t s = slot(L, m, idArg(L, 2), M::Bone);
    std::string name = luaL_checkstring(L, 3);
    if (name.size() > 100) luaL_error(L, "SetBoneName: a bone name is at most 100 characters");
    for (size_t j = 0; j < m.bones.size(); j++) if (j != s && m.bones[j].alive && m.bones[j].name == name) luaL_error(L, "SetBoneName: a bone named %s is already in the mesh", name.c_str());
    m.bones[s].name = name;
    return 0;
}
static int SetBoneParent(lua_State* L) {
    M& m = me(L, "SetBoneParent");
    int64_t id = idArg(L, 2);
    uint32_t s = slot(L, m, id, M::Bone);
    int64_t parent = idArg(L, 3);
    if (parent != 0) {
        slot(L, m, parent, M::Bone);
        // A parent that would close a cycle throws, as on Roblox.
        for (int64_t up = parent; up != 0; up = m.bones[M::slotOf(up)].parent) if (up == id) luaL_error(L, "SetBoneParent: that parent would make a cycle");
    }
    m.bones[s].parent = parent;
    return 0;
}
static int GetVertexBones(lua_State* L) {
    M& m = me(L, "GetVertexBones");
    const auto& b = m.verts[slot(L, m, idArg(L, 2), M::Vertex)].bones;
    lua_createtable(L, (int)b.size(), 0);
    for (size_t j = 0; j < b.size(); j++) { lua_pushnumber(L, (double)b[j]); lua_rawseti(L, -2, (int)j + 1); }
    return 1;
}
static int GetVertexBoneWeights(lua_State* L) {
    M& m = me(L, "GetVertexBoneWeights");
    const auto& w = m.verts[slot(L, m, idArg(L, 2), M::Vertex)].weights;
    lua_createtable(L, (int)w.size(), 0);
    for (size_t j = 0; j < w.size(); j++) { lua_pushnumber(L, w[j]); lua_rawseti(L, -2, (int)j + 1); }
    return 1;
}
static int SetVertexBones(lua_State* L) {
    M& m = me(L, "SetVertexBones");
    uint32_t v = slot(L, m, idArg(L, 2), M::Vertex);
    std::vector<int64_t> ids = idList(L, 3);
    if (ids.size() > 4) luaL_error(L, "SetVertexBones: a vertex can be influenced by up to 4 bones");
    for (int64_t b : ids) slot(L, m, b, M::Bone);
    m.verts[v].bones = ids;
    return 0;
}
static int SetVertexBoneWeights(lua_State* L) {
    M& m = me(L, "SetVertexBoneWeights");
    uint32_t v = slot(L, m, idArg(L, 2), M::Vertex);
    luaL_checktype(L, 3, LUA_TTABLE);
    std::vector<float> w;
    for (int k = 1; k <= (int)lua_objlen(L, 3); k++) { lua_rawgeti(L, 3, k); w.push_back((float)luaL_checknumber(L, -1)); lua_pop(L, 1); }
    if (w.size() != m.verts[v].bones.size()) luaL_error(L, "SetVertexBoneWeights: one weight per bone SetVertexBones gave the vertex");
    m.verts[v].weights = w;
    return 0;
}
static int facsUnit(lua_State* L, int idx) { return checkEnumItem(L, idx, findEnum("FacsActionUnit"))->value; }
static std::vector<int> facsUnits(lua_State* L, int idx) {
    luaL_checktype(L, idx, LUA_TTABLE);
    std::vector<int> out;
    for (int k = 1; k <= (int)lua_objlen(L, idx); k++) { lua_rawgeti(L, idx, k); out.push_back(facsUnit(L, lua_gettop(L))); lua_pop(L, 1); }
    if (out.size() < 2 || out.size() > 3) luaL_error(L, "a corrective pose is 2 or 3 FacsActionUnits");
    std::sort(out.begin(), out.end());
    return out;
}
static std::vector<std::pair<int64_t, std::vector<float>>> poseArgs(lua_State* L, M& m, int idsIdx, int cfIdx, const char* method) {
    std::vector<int64_t> ids = idList(L, idsIdx);
    luaL_checktype(L, cfIdx, LUA_TTABLE);
    if ((int)lua_objlen(L, cfIdx) != (int)ids.size()) luaL_error(L, "%s: boneIds and cframes must be the same length", method);
    std::vector<std::pair<int64_t, std::vector<float>>> out;
    for (size_t j = 0; j < ids.size(); j++) {
        uint32_t s = slot(L, m, ids[j], M::Bone);
        if (!m.bones[s].virt) luaL_error(L, "%s: every bone in a FACS pose must be virtual", method);
        lua_rawgeti(L, cfIdx, (int)j + 1);
        out.push_back({ids[j], cframeData(checkCFrame(L, -1))});
        lua_pop(L, 1);
    }
    return out;
}
static void pushPose(lua_State* L, const std::vector<std::pair<int64_t, std::vector<float>>>& pose) {
    lua_createtable(L, (int)pose.size(), 0);
    for (size_t j = 0; j < pose.size(); j++) { lua_pushnumber(L, (double)pose[j].first); lua_rawseti(L, -2, (int)j + 1); }
    lua_createtable(L, (int)pose.size(), 0);
    for (size_t j = 0; j < pose.size(); j++) { pushCFrame(L, cframeOf(pose[j].second)); lua_rawseti(L, -2, (int)j + 1); }
}
static int SetFacsPose(lua_State* L) { M& m = me(L, "SetFacsPose"); int u = facsUnit(L, 2); m.facs[u] = poseArgs(L, m, 3, 4, "SetFacsPose"); return 0; }
static int SetFacsCorrectivePose(lua_State* L) { M& m = me(L, "SetFacsCorrectivePose"); auto u = facsUnits(L, 2); m.correctives[u] = poseArgs(L, m, 3, 4, "SetFacsCorrectivePose"); return 0; }
static int SetFacsBonePose(lua_State* L) {
    M& m = me(L, "SetFacsBonePose");
    int u = facsUnit(L, 2);
    int64_t bone = idArg(L, 3);
    if (!m.bones[slot(L, m, bone, M::Bone)].virt) luaL_error(L, "SetFacsBonePose: the bone must be virtual");
    auto& pose = m.facs[u];
    std::vector<float> cf = cframeData(checkCFrame(L, 4));
    for (auto& e : pose) if (e.first == bone) { e.second = cf; return 0; }
    pose.push_back({bone, cf});
    return 0;
}
static int GetFacsPose(lua_State* L) { M& m = me(L, "GetFacsPose"); auto it = m.facs.find(facsUnit(L, 2)); pushPose(L, it == m.facs.end() ? decltype(it->second){} : it->second); return 2; }
static int GetFacsCorrectivePose(lua_State* L) { M& m = me(L, "GetFacsCorrectivePose"); auto it = m.correctives.find(facsUnits(L, 2)); pushPose(L, it == m.correctives.end() ? decltype(it->second){} : it->second); return 2; }
static int GetFacsPoses(lua_State* L) {
    M& m = me(L, "GetFacsPoses");
    const EnumDef* e = findEnum("FacsActionUnit");
    lua_newtable(L);
    int n = 0;
    for (auto& [u, pose] : m.facs) { pushEnumItem(L, e, e->findValue(u)); lua_rawseti(L, -2, ++n); }
    return 1;
}
static int GetFacsCorrectivePoses(lua_State* L) {
    M& m = me(L, "GetFacsCorrectivePoses");
    const EnumDef* e = findEnum("FacsActionUnit");
    lua_newtable(L);
    int n = 0;
    for (auto& [units, pose] : m.correctives) {
        lua_createtable(L, (int)units.size(), 0);
        for (size_t j = 0; j < units.size(); j++) { pushEnumItem(L, e, e->findValue(units[j])); lua_rawseti(L, -2, (int)j + 1); }
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

// ---- batches ----------------------------------------------------------------------------------
static M::Kind attrArg(lua_State* L, int idx) {
    int v = checkEnumItem(L, idx, findEnum("MeshAttribute"))->value;   // Vertex 0, Normal 1, Color 2, UV 3, Face 4
    static const M::Kind kinds[] = {M::Vertex, M::Normal, M::Color, M::UV, M::Face};
    return kinds[std::clamp(v, 0, 4)];
}
static M::Kind batchKind(lua_State* L, const std::vector<int64_t>& ids, const char* method) {
    if (ids.empty()) return M::Vertex;
    int k = M::kindOf(ids[0]);
    for (int64_t id : ids) if (M::kindOf(id) != k) luaL_error(L, "%s: all ids in a batch must be of one kind", method);
    return (M::Kind)k;
}
static int BatchAdd(lua_State* L) {
    M& m = me(L, "BatchAdd"); growable(L, m, "BatchAdd");
    M::Kind k = attrArg(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    int n = (int)lua_objlen(L, 3);
    if (k == M::Color) { luaL_checktype(L, 4, LUA_TTABLE); if ((int)lua_objlen(L, 4) != n) luaL_error(L, "BatchAdd: colors and alphas must be the same length"); }
    std::vector<uint32_t> out;
    for (int j = 1; j <= n; j++) {
        lua_rawgeti(L, 3, j);
        int top = lua_gettop(L);
        switch (k) {
        case M::Vertex: out.push_back(addVertex(L, m, checkVec3(L, top))); break;
        case M::Normal: { Vec3 v = checkVec3(L, top); out.push_back(addNormal(m, &v)); break; }
        case M::UV: { Vec2 uv = checkVector2(L, top); out.push_back(addUV(m, uv.x, uv.y)); break; }
        case M::Color: { lua_rawgeti(L, 4, j); out.push_back(addColor(m, checkColor3(L, top), (float)luaL_checknumber(L, -1))); lua_pop(L, 1); break; }
        case M::Face: {
            std::vector<int64_t> ids = idList(L, top);
            if (ids.size() != 3) luaL_error(L, "BatchAdd: a face is 3 vertex ids");
            uint32_t v[3];
            for (int c = 0; c < 3; c++) v[c] = slot(L, m, ids[c], M::Vertex);
            out.push_back(addTriangle(L, m, v));
            break;
        }
        default: break;
        }
        lua_pop(L, 1);
    }
    m.dirty = true;
    pushIds(L, k, out);
    return 1;
}
static int BatchGetValues(lua_State* L) {
    M& m = me(L, "BatchGetValues");
    std::vector<int64_t> ids = idList(L, 2);
    M::Kind k = batchKind(L, ids, "BatchGetValues");
    if (k == M::Face || k == M::Bone) luaL_error(L, "BatchGetValues: face and bone ids have no values");
    lua_createtable(L, (int)ids.size(), 0);
    for (size_t j = 0; j < ids.size(); j++) {
        uint32_t s = slot(L, m, ids[j], k);
        if (k == M::Vertex) pushVec3(L, m.verts[s].p);
        else if (k == M::Normal) pushVec3(L, m.normalOf(s));
        else if (k == M::UV) pushVector2(L, {m.uvs[s].u, m.uvs[s].v});
        else pushColor3(L, m.colors[s].c);
        lua_rawseti(L, -2, (int)j + 1);
    }
    if (k != M::Color) { lua_pushnil(L); return 2; }
    lua_createtable(L, (int)ids.size(), 0);
    for (size_t j = 0; j < ids.size(); j++) { lua_pushnumber(L, m.colors[M::slotOf(ids[j])].a); lua_rawseti(L, -2, (int)j + 1); }
    return 2;
}
// An invalid id throws part-way, leaving the ids before it already applied.
static int BatchSetValues(lua_State* L) {
    M& m = me(L, "BatchSetValues");
    std::vector<int64_t> ids = idList(L, 2);
    M::Kind k = batchKind(L, ids, "BatchSetValues");
    if (k == M::Face || k == M::Bone) luaL_error(L, "BatchSetValues: face and bone ids have no values");
    bool reset = lua_isnoneornil(L, 3);
    if (reset && k != M::Normal) luaL_error(L, "BatchSetValues: values may only be nil for normal ids");
    if (!reset) { luaL_checktype(L, 3, LUA_TTABLE); if ((int)lua_objlen(L, 3) != (int)ids.size()) luaL_error(L, "BatchSetValues: ids and values must be the same length"); }
    m.dirty = true;
    for (size_t j = 0; j < ids.size(); j++) {
        uint32_t s;
        if (!liveSlot(m, ids[j], k, s)) luaL_error(L, "BatchSetValues: invalid %s id", kindName(k));
        if (reset) { m.normals[s].autoCalc = true; continue; }
        lua_rawgeti(L, 3, (int)j + 1);
        int top = lua_gettop(L);
        if (k == M::Vertex) m.verts[s].p = checkVec3(L, top);
        else if (k == M::Normal) { m.normals[s].n = checkVec3(L, top); m.normals[s].autoCalc = false; }
        else if (k == M::UV) { Vec2 uv = checkVector2(L, top); m.uvs[s].u = uv.x; m.uvs[s].v = uv.y; }
        else if (lua_type(L, top) == LUA_TNUMBER) m.colors[s].a = (float)lua_tonumber(L, top);
        else m.colors[s].c = checkColor3(L, top);
        lua_pop(L, 1);
    }
    return 0;
}
static int BatchRemove(lua_State* L) {
    M& m = me(L, "BatchRemove"); growable(L, m, "BatchRemove");
    std::vector<int64_t> ids = idList(L, 2);
    std::vector<uint32_t> slots;
    for (int64_t id : ids) slots.push_back(slot(L, m, id, M::Face));   // an invalid or already-removed id throws and removes nothing
    for (uint32_t s : slots) m.faces[s].alive = false;
    m.dirty = true;
    return 0;
}
static uint32_t cornerAttr(const M::F& f, M::Kind k, int c) { return k == M::Vertex ? f.v[c] : k == M::Normal ? f.n[c] : k == M::UV ? f.u[c] : f.c[c]; }
static int BatchGetFaceAttributes(lua_State* L) {
    M& m = me(L, "BatchGetFaceAttributes");
    M::Kind k = attrArg(L, 2);
    if (k == M::Face) luaL_error(L, "BatchGetFaceAttributes: MeshAttribute.Face is not supported");
    std::vector<int64_t> ids = idList(L, 3);
    lua_createtable(L, (int)ids.size(), 0);
    for (size_t j = 0; j < ids.size(); j++) {
        const M::F& f = m.faces[slot(L, m, ids[j], M::Face)];
        pushIds(L, k, {cornerAttr(f, k, 0), cornerAttr(f, k, 1), cornerAttr(f, k, 2)});
        lua_rawseti(L, -2, (int)j + 1);
    }
    return 1;
}
static int BatchSetFaceAttributes(lua_State* L) {
    M& m = me(L, "BatchSetFaceAttributes");
    std::vector<int64_t> faces = idList(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if ((int)lua_objlen(L, 3) != (int)faces.size()) luaL_error(L, "BatchSetFaceAttributes: faceIds and attrIdArrays must be the same length");
    int kind = -1;
    m.dirty = true;
    for (size_t j = 0; j < faces.size(); j++) {
        uint32_t fs = slot(L, m, faces[j], M::Face);
        lua_rawgeti(L, 3, (int)j + 1);
        std::vector<int64_t> attrs = idList(L, lua_gettop(L));
        lua_pop(L, 1);
        if (attrs.size() != 3) luaL_error(L, "BatchSetFaceAttributes: one id per corner (3)");
        for (int64_t a : attrs) { if (kind < 0) kind = M::kindOf(a); if (M::kindOf(a) != kind) luaL_error(L, "BatchSetFaceAttributes: all ids must be of one kind"); }
        if (kind == M::Face || kind == M::Bone) luaL_error(L, "BatchSetFaceAttributes: expected vertex, normal, color or UV ids");
        for (int c = 0; c < 3; c++) {
            uint32_t s = slot(L, m, attrs[c], (M::Kind)kind);
            M::F& f = m.faces[fs];
            if (kind == M::Vertex) f.v[c] = s; else if (kind == M::Normal) f.n[c] = s; else if (kind == M::UV) f.u[c] = s; else f.c[c] = s;
        }
    }
    return 0;
}
static int BatchGetVertexAttributes(lua_State* L) {
    M& m = me(L, "BatchGetVertexAttributes");
    M::Kind k = attrArg(L, 2);
    if (k == M::Vertex) luaL_error(L, "BatchGetVertexAttributes: MeshAttribute.Vertex is not supported (use BatchGetValues)");
    std::vector<int64_t> ids = idList(L, 3);
    lua_createtable(L, (int)ids.size(), 0);
    for (size_t j = 0; j < ids.size(); j++) {
        uint32_t v = slot(L, m, ids[j], M::Vertex);
        std::vector<uint32_t> out;
        for (size_t fi = 0; fi < m.faces.size(); fi++) {
            const M::F& f = m.faces[fi];
            if (!f.alive) continue;
            for (int c = 0; c < 3; c++) if (f.v[c] == v) {
                uint32_t a = k == M::Face ? (uint32_t)fi : cornerAttr(f, k, c);
                if (std::find(out.begin(), out.end(), a) == out.end()) out.push_back(a);
            }
        }
        pushIds(L, k, out);
        lua_rawseti(L, -2, (int)j + 1);
    }
    return 1;
}
static int BatchGetVertexFaceAttributes(lua_State* L) {
    M& m = me(L, "BatchGetVertexFaceAttributes");
    M::Kind k = attrArg(L, 2);
    if (k == M::Vertex || k == M::Face) luaL_error(L, "BatchGetVertexFaceAttributes: expected MeshAttribute Normal, Color or UV");
    std::vector<int64_t> vs = idList(L, 3), fs = idList(L, 4);
    if (vs.size() != fs.size()) luaL_error(L, "BatchGetVertexFaceAttributes: vertexIds and faceIds must be the same length");
    lua_createtable(L, (int)vs.size(), 0);
    for (size_t j = 0; j < vs.size(); j++) {
        uint32_t v = slot(L, m, vs[j], M::Vertex), f = slot(L, m, fs[j], M::Face);
        int c = -1;
        for (int q = 0; q < 3; q++) if (m.faces[f].v[q] == v) c = q;
        if (c < 0) luaL_error(L, "BatchGetVertexFaceAttributes: a vertex is not a corner of its face");
        pushId(L, k, cornerAttr(m.faces[f], k, c));
        lua_rawseti(L, -2, (int)j + 1);
    }
    return 1;
}
static int BatchSetVertexFaceAttributes(lua_State* L) {
    M& m = me(L, "BatchSetVertexFaceAttributes");
    std::vector<int64_t> vs = idList(L, 2), fs = idList(L, 3), as = idList(L, 4);
    if (vs.size() != fs.size() || vs.size() != as.size()) luaL_error(L, "BatchSetVertexFaceAttributes: the three arrays must be the same length");
    M::Kind k = batchKind(L, as, "BatchSetVertexFaceAttributes");
    if (k == M::Vertex || k == M::Face || k == M::Bone) luaL_error(L, "BatchSetVertexFaceAttributes: expected normal, color or UV ids");
    m.dirty = true;
    for (size_t j = 0; j < vs.size(); j++) {
        uint32_t v = slot(L, m, vs[j], M::Vertex), f = slot(L, m, fs[j], M::Face), a = slot(L, m, as[j], k);
        for (int c = 0; c < 3; c++) if (m.faces[f].v[c] == v) {
            if (k == M::Normal) m.faces[f].n[c] = a; else if (k == M::UV) m.faces[f].u[c] = a; else m.faces[f].c[c] = a;
        }
    }
    return 0;
}

// Frees the contents at once; the entry stays, marked destroyed, so methods can still error.
static int Destroy(lua_State* L) {
    Runtime::Impl& rt = rtOf(L);
    auto it = rt.editableMeshes.find(selfObj(L).id());
    if (it == rt.editableMeshes.end() || it->second.destroyed) return 0;
    it->second = M();
    it->second.destroyed = true;
    it->second.dirty = true;
    return 0;
}

} // namespace em

std::vector<std::pair<const char*, lua_CFunction>> editableMeshMethods() {
    using namespace em;
    return {
        {"AddBone", AddBone}, {"AddColor", AddColor}, {"AddNormal", AddNormal}, {"AddTriangle", AddTriangle}, {"AddUV", AddUV}, {"AddVertex", AddVertex},
        {"BatchAdd", BatchAdd}, {"BatchGetFaceAttributes", BatchGetFaceAttributes}, {"BatchGetValues", BatchGetValues},
        {"BatchGetVertexAttributes", BatchGetVertexAttributes}, {"BatchGetVertexFaceAttributes", BatchGetVertexFaceAttributes},
        {"BatchRemove", BatchRemove}, {"BatchSetFaceAttributes", BatchSetFaceAttributes}, {"BatchSetValues", BatchSetValues},
        {"BatchSetVertexFaceAttributes", BatchSetVertexFaceAttributes}, {"Clear", Clear}, {"Destroy", Destroy},
        {"FindClosestPointOnSurface", FindClosestPointOnSurface}, {"FindClosestVertex", FindClosestVertex}, {"FindVerticesWithinSphere", FindVerticesWithinSphere},
        {"GetAdjacentFaces", GetAdjacentFaces}, {"GetAdjacentVertices", GetAdjacentVertices},
        {"GetBoneByName", GetBoneByName}, {"GetBoneCFrame", GetBoneCFrame}, {"GetBoneIsVirtual", GetBoneIsVirtual}, {"GetBoneName", GetBoneName},
        {"GetBoneParent", GetBoneParent}, {"GetBones", GetBones}, {"GetCenter", GetCenter}, {"GetColor", GetColor}, {"GetColorAlpha", GetColorAlpha},
        {"GetColors", GetColors}, {"GetFaceColors", GetFaceColors}, {"GetFaceNormals", GetFaceNormals}, {"GetFaces", GetFaces},
        {"GetFacesWithAttribute", GetFacesWithAttribute}, {"GetFacesWithColor", GetFacesWithColor}, {"GetFacesWithNormal", GetFacesWithNormal},
        {"GetFacesWithUV", GetFacesWithUV}, {"GetFaceUVs", GetFaceUVs}, {"GetFaceVertices", GetFaceVertices},
        {"GetFacsCorrectivePose", GetFacsCorrectivePose}, {"GetFacsCorrectivePoses", GetFacsCorrectivePoses}, {"GetFacsPose", GetFacsPose}, {"GetFacsPoses", GetFacsPoses},
        {"GetNormal", GetNormal}, {"GetNormals", GetNormals}, {"GetPosition", GetPosition}, {"GetSize", GetSize}, {"GetUV", GetUV}, {"GetUVs", GetUVs},
        {"GetVertexBones", GetVertexBones}, {"GetVertexBoneWeights", GetVertexBoneWeights}, {"GetVertexColors", GetVertexColors},
        {"GetVertexFaceColor", GetVertexFaceColor}, {"GetVertexFaceNormal", GetVertexFaceNormal}, {"GetVertexFaces", GetVertexFaces},
        {"GetVertexFaceUV", GetVertexFaceUV}, {"GetVertexNormals", GetVertexNormals}, {"GetVertexUVs", GetVertexUVs}, {"GetVertices", GetVertices},
        {"GetVerticesWithAttribute", GetVerticesWithAttribute}, {"GetVerticesWithColor", GetVerticesWithColor}, {"GetVerticesWithNormal", GetVerticesWithNormal},
        {"GetVerticesWithUV", GetVerticesWithUV}, {"IdDebugString", IdDebugString}, {"MergeVertices", MergeVertices}, {"RaycastLocal", RaycastLocal},
        {"RemoveBone", RemoveBone}, {"RemoveFace", RemoveFace}, {"RemoveUnused", RemoveUnused}, {"ResetNormal", ResetNormal},
        {"SetBoneCFrame", SetBoneCFrame}, {"SetBoneIsVirtual", SetBoneIsVirtual}, {"SetBoneName", SetBoneName}, {"SetBoneParent", SetBoneParent},
        {"SetColor", SetColor}, {"SetColorAlpha", SetColorAlpha}, {"SetFaceColors", SetFaceColors}, {"SetFaceNormals", SetFaceNormals},
        {"SetFaceUVs", SetFaceUVs}, {"SetFaceVertices", SetFaceVertices}, {"SetFacsBonePose", SetFacsBonePose},
        {"SetFacsCorrectivePose", SetFacsCorrectivePose}, {"SetFacsPose", SetFacsPose}, {"SetNormal", SetNormal}, {"SetPosition", SetPosition},
        {"SetUV", SetUV}, {"SetVertexBones", SetVertexBones}, {"SetVertexBoneWeights", SetVertexBoneWeights},
        {"SetVertexFaceColor", SetVertexFaceColor}, {"SetVertexFaceNormal", SetVertexFaceNormal}, {"SetVertexFaceUV", SetVertexFaceUV},
        {"Triangulate", Triangulate},
    };
}

} // namespace pulseblockz::rbx
