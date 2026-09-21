// EditableMesh: a mesh a script builds and edits. A vertex has one position; normal, UV and
// colour are per face corner, each with an id of its own that corners share or not ("split
// vertex attributes"). Removing one id leaves the others where they were. Bones and FACS poses
// are data the API stores and hands back: corners() and serialize() both leave them out.
#pragma once
#include "rbx_instance.h"
#include <map>
#include <string>
#include <vector>

namespace pulseblockz::rbx {

struct EditableMeshData {
    // Kind in the high bits, slot below; stays an integer a Luau number holds exactly.
    enum Kind : int { Vertex = 1, Normal, UV, Color, Face, Bone };
    static int64_t id(Kind k, uint32_t slot) { return ((int64_t)k << 40) | (int64_t)slot; }
    static int kindOf(int64_t id) { return (int)(id >> 40); }
    static uint32_t slotOf(int64_t id) { return (uint32_t)(id & 0xFFFFFFFFLL); }

    struct V { Vec3 p; bool alive = true; std::vector<int64_t> bones; std::vector<float> weights; };
    struct N { Vec3 n; bool autoCalc = true; bool alive = true; };
    struct U { float u = 0, v = 0; bool alive = true; };
    struct C { Col3 c{1, 1, 1}; float a = 1; bool alive = true; };
    struct F { uint32_t v[3], n[3], u[3], c[3]; bool alive = true; };
    struct B { std::string name; int64_t parent = 0; std::vector<float> cf; bool virt = false; bool alive = true; };   // cf: 12 floats (position, rotation)

    std::vector<V> verts;
    std::vector<N> normals;
    std::vector<U> uvs;
    std::vector<C> colors;
    std::vector<F> faces;
    std::vector<B> bones;
    std::map<int, std::vector<std::pair<int64_t, std::vector<float>>>> facs;                       // by Enum.FacsActionUnit value
    std::map<std::vector<int>, std::vector<std::pair<int64_t, std::vector<float>>>> correctives;   // by the sorted 2 or 3 units
    bool fixedSize = false, destroyed = false, placeholder = false, dirty = true;

    size_t liveVerts() const { size_t n = 0; for (auto& v : verts) n += v.alive; return n; }
    size_t liveFaces() const { size_t n = 0; for (auto& f : faces) n += f.alive; return n; }
    // Its own normal, or -- when nobody set one -- the area-weighted normal of the faces using it.
    Vec3 normalOf(uint32_t slot) const;
    Vec3 faceNormal(const F& f) const;
    void bounds(Vec3& lo, Vec3& hi) const;   // lo > hi when there is nothing
    // Triangle corners, one after another: what the engine draws.
    void corners(std::vector<float>& pos, std::vector<float>& nrm, std::vector<float>& uv, std::vector<float>& rgba) const;
    // Bytes and back, topology and all: what CreateDataModelContentAsync bakes.
    std::string serialize() const;
    bool deserialize(const std::string& bytes);
};

} // namespace pulseblockz::rbx
