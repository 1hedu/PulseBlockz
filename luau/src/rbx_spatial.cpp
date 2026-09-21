// Spatial queries against the tree's BaseParts: workspace:GetPartBoundsInBox /
// GetPartBoundsInRadius / GetPartsInPart, Blockcast / Spherecast / Shapecast and
// BasePart:GetTouchingParts. A swept shape that starts inside a part misses it, as a ray does.
#include "rbx_internal.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace pulseblockz::rbx {

namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float len(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 unit(Vec3 a) { float l = len(a); return l > 0 ? mul(a, 1 / l) : Vec3{0, 0, 0}; }
float comp(Vec3 v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }
float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// An oriented box: centre, unit axes, half sizes. Blocks are these; balls are spheres.
struct Box { Vec3 c; Vec3 u[3]; Vec3 h; };
Box boxOf(const ShapeV& s) { Box b; b.c = s.cf.p; for (int i = 0; i < 3; i++) b.u[i] = s.cf.col(i); b.h = s.half; return b; }
float radiusAlong(const Box& b, Vec3 L) { return std::fabs(dot(b.u[0], L)) * b.h.x + std::fabs(dot(b.u[1], L)) * b.h.y + std::fabs(dot(b.u[2], L)) * b.h.z; }
// The box's furthest point along w -- always one of its vertices.
Vec3 support(const Box& b, Vec3 w) {
    Vec3 p = b.c;
    for (int i = 0; i < 3; i++) p = add(p, mul(b.u[i], dot(b.u[i], w) >= 0 ? comp(b.h, i) : -comp(b.h, i)));
    return p;
}
Vec3 closestOnBox(const Box& b, Vec3 p) {
    Vec3 d = sub(p, b.c), q = b.c;
    for (int i = 0; i < 3; i++) q = add(q, mul(b.u[i], clampf(dot(d, b.u[i]), -comp(b.h, i), comp(b.h, i))));
    return q;
}
Vec3 corner(const Box& b, int k) {
    Vec3 p = b.c;
    for (int i = 0; i < 3; i++) p = add(p, mul(b.u[i], (k >> i) & 1 ? comp(b.h, i) : -comp(b.h, i)));
    return p;
}
// SAT axes of two boxes: each one's three, plus edge cross products. `pair[k]` names the edges that made axis k.
int sepAxes(const Box& a, const Box& b, Vec3 out[15], int pair[15][2]) {
    int n = 0;
    for (int i = 0; i < 3; i++) { out[n] = a.u[i]; pair[n][0] = i; pair[n][1] = -1; n++; }
    for (int i = 0; i < 3; i++) { out[n] = b.u[i]; pair[n][0] = -1; pair[n][1] = i; n++; }
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            Vec3 x = cross(a.u[i], b.u[j]);
            if (len(x) < 1e-6f) continue;
            out[n] = unit(x); pair[n][0] = i; pair[n][1] = j; n++;
        }
    return n;
}
bool boxesOverlap(const Box& a, const Box& b) {
    Vec3 axes[15]; int pair[15][2];
    int n = sepAxes(a, b, axes, pair);
    for (int k = 0; k < n; k++)
        if (std::fabs(dot(sub(b.c, a.c), axes[k])) > radiusAlong(a, axes[k]) + radiusAlong(b, axes[k])) return false;
    return true;
}
bool sphereBoxOverlap(Vec3 c, float r, const Box& b) { return len(sub(c, closestOnBox(b, c))) <= r; }

// Ray o + d t, t in [0, 1], against a sphere's surface; a start inside misses.
bool raySphere(Vec3 c, float r, Vec3 o, Vec3 d, float& t, Vec3& n) {
    Vec3 m = sub(o, c);
    float a = dot(d, d), b = dot(m, d), cc = dot(m, m) - r * r;
    if (cc < 0 || a <= 0) return false;
    float disc = b * b - a * cc;
    if (disc < 0) return false;
    float tt = (-b - std::sqrt(disc)) / a;
    if (tt < 0 || tt > 1) return false;
    t = tt; n = unit(sub(add(o, mul(d, tt)), c));
    return true;
}
// Ray against a box, slabs along its axes; a start inside misses. `axis`/`sign` name the face.
bool rayBox(const Box& b, Vec3 o, Vec3 d, float& t, int& axis, float& sign) {
    Vec3 rel = sub(o, b.c);
    float tmin = 0, tmax = 1e30f;
    axis = -1; sign = 0;
    for (int i = 0; i < 3; i++) {
        float oi = dot(rel, b.u[i]), di = dot(d, b.u[i]), h = comp(b.h, i);
        if (std::fabs(di) < 1e-9f) { if (oi < -h || oi > h) return false; continue; }
        float t1 = (-h - oi) / di, t2 = (h - oi) / di, s = -1;
        if (t1 > t2) { std::swap(t1, t2); s = 1; }
        if (t1 > tmin) { tmin = t1; axis = i; sign = s; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    if (axis < 0 || tmin > 1) return false;
    t = tmin;
    return true;
}
// Ray against a capsule's round side only -- the end caps are the corner spheres' job. `q` lands on the segment.
bool rayCapsuleSide(Vec3 e0, Vec3 e1, float r, Vec3 o, Vec3 d, float& t, Vec3& n, Vec3& q) {
    Vec3 axis = sub(e1, e0);
    float L = len(axis);
    if (L <= 0) return false;
    axis = mul(axis, 1 / L);
    Vec3 m = sub(o, e0);
    float md = dot(m, axis), nd = dot(d, axis);
    Vec3 mp = sub(m, mul(axis, md)), np = sub(d, mul(axis, nd));
    float A = dot(np, np), B = 2 * dot(mp, np), C = dot(mp, mp) - r * r;
    if (A < 1e-12f || C < 0) return false;
    float disc = B * B - 4 * A * C;
    if (disc < 0) return false;
    float tt = (-B - std::sqrt(disc)) / (2 * A);
    if (tt < 0 || tt > 1) return false;
    float along = md + nd * tt;
    if (along < 0 || along > L) return false;
    q = add(e0, mul(axis, along));
    n = unit(sub(add(o, mul(d, tt)), q));
    t = tt;
    return true;
}
// A sphere sweep is a ray against the box grown by r: faces first, then the edge capsules and corner spheres the grown box overshoots.
bool sweepSphereBox(Vec3 c, float r, Vec3 d, const Box& b, float& t, Vec3& pos, Vec3& n) {
    if (sphereBoxOverlap(c, r, b)) return false;
    Box grown = b; grown.h = {b.h.x + r, b.h.y + r, b.h.z + r};
    float tf; int axis; float sign;
    if (rayBox(grown, c, d, tf, axis, sign)) {
        Vec3 rel = sub(add(c, mul(d, tf)), b.c);
        bool onFace = true;
        for (int i = 0; i < 3; i++) if (i != axis && std::fabs(dot(rel, b.u[i])) > comp(b.h, i)) onFace = false;
        if (onFace) { t = tf; n = mul(b.u[axis], sign); pos = sub(add(c, mul(d, tf)), mul(n, r)); return true; }
    }
    bool found = false; float best = 2;
    for (int k = 0; k < 8; k++) {
        float tt; Vec3 nn; Vec3 v = corner(b, k);
        if (raySphere(v, r, c, d, tt, nn) && tt < best) { best = tt; n = nn; pos = v; found = true; }
        for (int i = 0; i < 3; i++) {
            if (k & (1 << i)) continue;
            Vec3 q;
            if (rayCapsuleSide(v, corner(b, k | (1 << i)), r, c, d, tt, nn, q) && tt < best) { best = tt; n = nn; pos = q; found = true; }
        }
    }
    if (found) t = best;
    return found;
}
// The point on segment p2-q2 nearest segment p1-q1.
Vec3 closestOnSegment2(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2) {
    Vec3 d1 = sub(q1, p1), d2 = sub(q2, p2), r = sub(p1, p2);
    float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r), s, t;
    const float eps = 1e-9f;
    if (a <= eps && e <= eps) return p2;
    if (a <= eps) { s = 0; t = clampf(f / e, 0, 1); }
    else {
        float cc = dot(d1, r);
        if (e <= eps) { t = 0; s = clampf(-cc / a, 0, 1); }
        else {
            float bb = dot(d1, d2), denom = a * e - bb * bb;
            s = denom != 0 ? clampf((bb * f - cc * e) / denom, 0, 1) : 0;
            t = (bb * s + f) / e;
            if (t < 0) { t = 0; s = clampf(-cc / a, 0, 1); }
            else if (t > 1) { t = 1; s = clampf((bb - cc) / a, 0, 1); }
        }
    }
    (void)s;
    return add(p2, mul(d2, t));
}
// Swept SAT: the last axis to stop separating the boxes is the one they meet across, at its time.
bool sweepBoxBox(const Box& a, Vec3 d, const Box& b, float& t, Vec3& pos, Vec3& n) {
    Vec3 axes[15]; int pair[15][2];
    int na = sepAxes(a, b, axes, pair);
    float tEnter = -1e30f, tExit = 1e30f; int hit = -1; float hitSign = 0;
    for (int k = 0; k < na; k++) {
        Vec3 L = axes[k];
        float pa = dot(a.c, L), pb = dot(b.c, L), ra = radiusAlong(a, L), rb = radiusAlong(b, L), v = dot(d, L);
        float lowA = pa - ra, highA = pa + ra, lowB = pb - rb, highB = pb + rb;
        if (std::fabs(v) < 1e-9f) { if (lowA > highB || highA < lowB) return false; continue; }
        float enter = v > 0 ? (lowB - highA) / v : (highB - lowA) / v;
        float exit = v > 0 ? (highB - lowA) / v : (lowB - highA) / v;
        if (enter > tEnter) { tEnter = enter; hit = k; hitSign = v > 0 ? -1 : 1; }
        if (exit < tExit) tExit = exit;
        if (tEnter > tExit) return false;
    }
    if (hit < 0 || tEnter <= 0 || tEnter > 1) return false;   // never apart, already overlapping, or out of reach
    t = tEnter; n = mul(axes[hit], hitSign);
    Box am = a; am.c = add(a.c, mul(d, t));
    Vec3 back = mul(n, -1);
    if (pair[hit][1] < 0) pos = support(b, n);              // a face of the moving box meets b's corner
    else if (pair[hit][0] < 0) pos = support(am, back);     // the moving box's corner meets b's face
    else {                                                  // edge against edge
        int i = pair[hit][0], j = pair[hit][1];
        Vec3 vA = support(am, back), vB = support(b, n);
        Vec3 vA2 = sub(vA, mul(am.u[i], 2 * (dot(sub(vA, am.c), am.u[i]) >= 0 ? comp(am.h, i) : -comp(am.h, i))));
        Vec3 vB2 = sub(vB, mul(b.u[j], 2 * (dot(sub(vB, b.c), b.u[j]) >= 0 ? comp(b.h, j) : -comp(b.h, j))));
        pos = closestOnSegment2(vA, vA2, vB, vB2);
    }
    return true;
}
} // namespace

ShapeV partShape(Instance& part) {
    ShapeV s;
    s.cf = cframeFromPosOrient(part.get("Position").v, part.get("Orientation").v);
    Vec3 size = part.get("Size").v;
    s.half = mul(size, 0.5f);
    s.sphere = part.get("Shape").s == "Ball";
    s.radius = std::min({size.x, size.y, size.z}) / 2;
    return s;
}
// The part's world-aligned bounding box (what GetPartBoundsIn* looks at).
ShapeV partBounds(Instance& part) {
    ShapeV s = partShape(part);
    ShapeV b; b.cf = CFrameV::fromPos(s.cf.p);
    if (s.sphere) b.half = {s.radius, s.radius, s.radius};
    else {
        Box bx = boxOf(s);
        b.half = {radiusAlong(bx, {1, 0, 0}), radiusAlong(bx, {0, 1, 0}), radiusAlong(bx, {0, 0, 1})};
    }
    return b;
}

bool sweepShapeRaw(const ShapeV& a, Vec3 d, const ShapeV& b, float& t, Vec3& pos, Vec3& n);
bool shapesOverlap(const ShapeV& a, const ShapeV& b) {
    if (a.sphere && b.sphere) return len(sub(a.cf.p, b.cf.p)) <= a.radius + b.radius;
    if (a.sphere) return sphereBoxOverlap(a.cf.p, a.radius, boxOf(b));
    if (b.sphere) return sphereBoxOverlap(b.cf.p, b.radius, boxOf(a));
    return boxesOverlap(boxOf(a), boxOf(b));
}

Vec3 noNegZero(Vec3 v) { return {v.x + 0.f, v.y + 0.f, v.z + 0.f}; }
bool sweepShape(const ShapeV& a, Vec3 d, const ShapeV& b, float& t, Vec3& pos, Vec3& n) {
    if (!sweepShapeRaw(a, d, b, t, pos, n)) return false;
    pos = noNegZero(pos); n = noNegZero(n);
    return true;
}
bool sweepShapeRaw(const ShapeV& a, Vec3 d, const ShapeV& b, float& t, Vec3& pos, Vec3& n) {
    if (a.sphere && b.sphere) {
        if (!raySphere(b.cf.p, a.radius + b.radius, a.cf.p, d, t, n)) return false;
        pos = add(b.cf.p, mul(n, b.radius));
        return true;
    }
    if (a.sphere) return sweepSphereBox(a.cf.p, a.radius, d, boxOf(b), t, pos, n);
    if (b.sphere) {   // the ball moving the other way against the box, seen from the box
        Vec3 posA, nA;
        if (!sweepSphereBox(b.cf.p, b.radius, mul(d, -1), boxOf(a), t, posA, nA)) return false;
        pos = add(posA, mul(d, t)); n = mul(nA, -1);
        return true;
    }
    return sweepBoxBox(boxOf(a), d, boxOf(b), t, pos, n);
}

// The parts a query sees: CanQuery, RespectCanCollide, and the RaycastParams / OverlapParams
// Include / Exclude list, which applies to whole subtrees. `fn` returns false to stop.
void queryParts(DataModel& dm, const RaycastFilter* filter, const std::function<bool(Instance&)>& fn) {
    auto listed = [&](Instance* i) { return filter && std::find(filter->ids.begin(), filter->ids.end(), i->id()) != filter->ids.end(); };
    bool stop = false;
    std::function<void(Instance*, bool)> walk = [&](Instance* i, bool included) {
        if (stop || i->destroyed()) return;
        bool inList = listed(i);
        if (filter && !filter->include && inList) return;               // Exclude: this subtree is invisible
        if (filter && filter->include && inList) included = true;       // Include: only listed subtrees count
        // Terrain isA BasePart but has no Size or Position of its own: taking the class
        // literally puts an invisible 4 x 1.2 x 2 brick at the origin in front of every query.
        if (i->isA("BasePart") && !i->isA("Terrain") && (!filter || !filter->include || included))
            if (i->get("CanQuery").b && (!filter || !filter->respectCanCollide || i->get("CanCollide").b))
                if (!fn(*i)) { stop = true; return; }
        for (auto& c : i->children()) walk(c.get(), included);
    };
    walk(dm.workspace(), false);
}

std::vector<Instance*> overlapTree(DataModel& dm, const ShapeV& s, const RaycastFilter* filter, Instance* skip, bool bounds) {
    std::vector<Instance*> out;
    int max = filter ? filter->maxParts : 20;
    queryParts(dm, filter, [&](Instance& p) {
        if (&p != skip && shapesOverlap(s, bounds ? partBounds(p) : partShape(p))) out.push_back(&p);
        return max <= 0 || (int)out.size() < max;
    });
    return out;
}

bool shapecastTree(DataModel& dm, const ShapeV& s, Vec3 d, const RaycastFilter* filter, Instance* skip, RayHit& out) {
    bool found = false; float best = 2;
    queryParts(dm, filter, [&](Instance& p) {
        float t; Vec3 pos, n;
        if (&p != skip && sweepShape(s, d, partShape(p), t, pos, n) && t < best) {
            best = t; found = true;
            out.part = &p; out.position = pos; out.normal = n; out.distance = t * len(d);
        }
        return true;
    });
    return found;
}

} // namespace pulseblockz::rbx
