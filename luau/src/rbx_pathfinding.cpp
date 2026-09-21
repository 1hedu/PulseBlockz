// PathfindingService: Path:ComputeAsync. Roblox bakes a navmesh; this A*s a 2-stud grid on the
// fly with the queries workspace:Raycast and GetPartsInPart use, then resamples the corners every
// WaypointSpacing studs. A cell is a spot the agent's box can stand; PassThrough parts are air.
#include "rbx_internal.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace pulseblockz::rbx {

namespace {
const float kCell = 2;          // the grid pitch; a Roblox navmesh voxel is 4, half keeps doorways
const float kStep = 2.25f;      // what a Humanoid walks up (HipHeight-ish) without a jump
const float kJump = 7;          // and what its jump clears
const float kDrop = 12;         // the furthest it is sent falling
const int kMaxNodes = 60000;    // the search gives up past this many cells

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float len(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

Instance* modifierOf(Instance& part) {
    for (auto& c : part.children()) if (c->className() == "PathfindingModifier" && c->get("ModifierEnabled").b) return c.get();
    return nullptr;
}
bool characterPart(Instance& part) {
    for (Instance* m = part.parent(); m; m = m->parent())
        if (m->className() == "Model") return m->findFirstChildOfClass("Humanoid") != nullptr;
    return false;
}

struct Grid {
    DataModel& dm; const PathAgent& a;
    RaycastFilter filter;                                // everything solid but the pass-throughs and characters
    Grid(DataModel& d, const PathAgent& ag) : dm(d), a(ag) {
        filter.respectCanCollide = true; filter.maxParts = 1 << 20;
        if (Instance* ws = dm.workspace())
            for (Instance* i : ws->getDescendants()) if (i->isA("BasePart")) {
                Instance* mod = modifierOf(*i);
                if ((mod && mod->get("PassThrough").b) || characterPart(*i)) filter.ids.push_back(i->id());
            }
    }
    // Anything in the agent's box standing at `feet`, shrunk a hair so its own floor misses.
    bool blocked(Vec3 feet) {
        ShapeV s; s.cf = CFrameV::identity(); s.cf.p = {feet.x, feet.y + 0.2f + (a.height - 0.2f) / 2, feet.z};
        s.half = {a.radius - 0.05f, (a.height - 0.2f) / 2, a.radius - 0.05f};
        RaycastFilter f = filter; f.maxParts = 1;
        return !overlapTree(dm, s, &f, nullptr, false).empty();
    }
    // The floor under (x, z), searched from `up` studs above `fromY` down to kDrop below it.
    bool ground(float x, float z, float fromY, float up, float& y, Instance*& part) {
        RayHit h;
        if (!raycastTree(dm, {x, fromY + up, z}, {0, -(up + kDrop), 0}, &filter, h)) return false;
        y = h.position.y; part = h.part;
        return true;
    }
    // All four corners of the footprint need floor within a step of `y`: a pole is no landing.
    bool supported(float x, float z, float y) {
        float r = a.radius * 0.6f;
        for (int k = 0; k < 4; k++) {
            float cy; Instance* cp;
            if (!ground(x + (k & 1 ? r : -r), z + (k & 2 ? r : -r), y, 0.5f, cy, cp) || std::fabs(cy - y) > kStep) return false;
        }
        return true;
    }
    // A stud here: the modifier's Label cost, else the Material's, else 1. math.huge: never walk it.
    float cost(Instance* floor) {
        if (!floor || a.costs.empty()) return 1;
        if (Instance* mod = modifierOf(*floor)) { auto it = a.costs.find(mod->get("Label").s); if (it != a.costs.end()) return it->second; }
        auto it = a.costs.find(floor->get("Material").s);
        return it != a.costs.end() ? it->second : 1;
    }
};

struct Node { int ix, iz, iy; };
struct Key { int64_t v; bool operator==(const Key& o) const { return v == o.v; } };
struct KeyHash { size_t operator()(const Key& k) const { return std::hash<int64_t>()(k.v); } };
Key keyOf(int ix, int iz, int iy) { return {((int64_t)(ix + 1000000) << 40) | ((int64_t)(iz + 1000000) << 16) | (uint16_t)(iy + 30000)}; }
}   // namespace

bool pathOccupied(DataModel& dm, const PathAgent& a, Vec3 p) { return Grid(dm, a).blocked(p); }

int computePath(DataModel& dm, const PathAgent& agent, Vec3 start, Vec3 goal, std::vector<PathPoint>& out) {
    out.clear();
    Grid grid(dm, agent);
    // Each end's floor, searched from a stud above: a Humanoid's Position is its hips, not its feet.
    float sy, gy; Instance* sp = nullptr; Instance* gp = nullptr;
    if (!grid.ground(start.x, start.z, start.y, 1, sy, sp)) return 5;      // NoPath: nothing under it
    if (grid.blocked({start.x, sy, start.z})) return 3;                    // FailStartNotEmpty
    if (!grid.ground(goal.x, goal.z, goal.y, 1, gy, gp)) return 5;
    if (grid.blocked({goal.x, gy, goal.z})) return 4;                      // FailFinishNotEmpty
    auto cellOf = [](float v) { return (int)std::lround(v / kCell); };
    auto layerOf = [](float y) { return (int)std::lround(y); };
    int gx = cellOf(goal.x), gz = cellOf(goal.z);
    struct Rec { float y; Instance* floor; float g; int64_t from; bool jump; int ix, iz; };
    std::unordered_map<Key, Rec, KeyHash> seen;
    struct Open { float f; Key k; bool operator<(const Open& o) const { return f > o.f; } };
    std::priority_queue<Open> open;
    auto h = [&](float x, float y, float z) { return len(sub({x, y, z}, {goal.x, gy, goal.z})); };
    Key sk = keyOf(cellOf(start.x), cellOf(start.z), layerOf(sy));
    seen[sk] = {sy, sp, 0, -1, false, cellOf(start.x), cellOf(start.z)};
    open.push({h(start.x, sy, start.z), sk});
    Key found{-1}; int expanded = 0;
    static const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    while (!open.empty() && expanded < kMaxNodes) {
        Open cur = open.top(); open.pop();
        Rec rec = seen[cur.k];
        if (cur.f > rec.g + h(rec.ix * kCell, rec.y, rec.iz * kCell) + 1e-3f) continue;   // a stale entry
        expanded++;
        if (rec.ix == gx && rec.iz == gz && std::fabs(rec.y - gy) <= kStep) { found = cur.k; break; }
        // the 8 cells around; with AgentCanJump, the ones past them too (over a gap, onto a ledge)
        for (int reach = 1; reach <= (agent.canJump ? 2 : 1); reach++) for (auto& d : dirs) {
            int nx = rec.ix + d[0] * reach, nz = rec.iz + d[1] * reach;
            float x = nx * kCell, z = nz * kCell;
            float up = agent.canJump ? kJump : kStep;
            float ny; Instance* floor;
            if (!grid.ground(x, z, rec.y, up, ny, floor)) continue;
            float rise = ny - rec.y;
            if (rise > up || rise < -kDrop) continue;
            bool jump = rise > kStep || reach > 1;
            float c = grid.cost(floor);
            if (!(c < INFINITY)) continue;
            if (grid.blocked({x, ny, z}) || !grid.supported(x, z, ny)) continue;
            // a thin wall between the two would fit between the cell tests: sample the midpoint
            float mx = (x + rec.ix * kCell) / 2, mz = (z + rec.iz * kCell) / 2, my = std::max(ny, rec.y);
            if (grid.blocked({mx, my, mz})) continue;
            if (reach > 1) {
                // a walkable midpoint means two ordinary steps reach it: no leap
                float wy; Instance* wf;
                if (grid.ground(mx, mz, rec.y, kStep, wy, wf) && !grid.blocked({mx, wy, mz}) && std::fabs(wy - rec.y) <= kStep && ny - wy <= kStep) continue;
                if (grid.blocked({(mx + rec.ix * kCell) / 2, my, (mz + rec.iz * kCell) / 2}) || grid.blocked({(mx + x) / 2, my, (mz + z) / 2})) continue;
            }
            float step = len({x - rec.ix * kCell, rise, z - rec.iz * kCell}) * c + (jump ? 2 : 0);
            Key nk = keyOf(nx, nz, layerOf(ny));
            auto it = seen.find(nk);
            if (it != seen.end() && it->second.g <= rec.g + step) continue;
            seen[nk] = {ny, floor, rec.g + step, cur.k.v, jump, nx, nz};
            open.push({rec.g + step + h(x, ny, z), nk});
        }
    }
    if (found.v == -1) return 5;                                            // NoPath
    // Back to the start along `from`, then only the corners and the jumps' ends.
    std::vector<PathPoint> cells;
    for (Key k = found; k.v != -1;) {
        const Rec& r = seen[k];
        cells.push_back({{r.ix * kCell, r.y, r.iz * kCell}, r.jump ? 1 : 0, ""});
        k.v = r.from;
    }
    std::reverse(cells.begin(), cells.end());
    cells.front().pos = {start.x, sy, start.z};
    cells.back().pos = {goal.x, gy, goal.z};
    std::vector<PathPoint> corners;
    for (size_t i = 0; i < cells.size(); i++) {
        bool keep = i == 0 || i + 1 == cells.size() || cells[i].action == 1 || (i + 1 < cells.size() && cells[i + 1].action == 1);
        if (!keep) {
            Vec3 a = sub(cells[i].pos, cells[i - 1].pos), b = sub(cells[i + 1].pos, cells[i].pos);
            keep = std::fabs(a.x * b.z - a.z * b.x) > 1e-3f || std::fabs((a.y / std::max(len(a), 1e-3f)) - (b.y / std::max(len(b), 1e-3f))) > 0.05f;
        }
        if (keep) corners.push_back(cells[i]);
    }
    // Waypoints every WaypointSpacing studs along the corners; a jump's landing keeps its action.
    float spacing = agent.spacing > 0 ? agent.spacing : 4;
    out.push_back(corners.front());
    for (size_t i = 1; i < corners.size(); i++) {
        Vec3 from = corners[i - 1].pos, to = corners[i].pos;
        float d = len(sub(to, from));
        int n = corners[i].action == 1 || !(spacing < INFINITY) ? 1 : std::max(1, (int)std::ceil(d / spacing - 1e-3f));
        for (int k = 1; k <= n; k++) {
            float t = (float)k / n;
            out.push_back({add(from, mul(sub(to, from), t)), k == n ? corners[i].action : 0, ""});
        }
    }
    return 0;                                                               // Success
}

}   // namespace pulseblockz::rbx
