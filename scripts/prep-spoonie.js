// Decimates the shaped-spoon model into the mesh BFS 9000 is published as, by quadric error
// metric edge collapse. Writes models/spoonie.obj and models/spoonie.json at four decimals.
//
//   node scripts/prep-spoonie.js [source.obj]
const fs = require("fs");
const path = require("path");

const OUT = path.join(__dirname, "models");
const SRC = process.argv[2] || path.join(
  process.env.SPOON_SRC || "",
  "shaped_spoon_roblox.obj");

// The published mesh is paid for on chain by the byte, so the budget is a triangle count.
const TARGET_TRIS = +(process.env.SPOON_TRIS || 1400);
// Length of the unstretched spoon: scale is taken off the source bounds, so a longer handle
// does not shrink the bowl to hold a total.
const LENGTH = +(process.env.SPOON_LENGTH || 5.2);

// Handle stretch: swung two-handed, so the grip takes two fists and leverage past them.
const HANDLE = +(process.env.SPOON_HANDLE || 1.42);

// Handle/bowl boundary, in the source file's units: the cross-section goes from 0.79 wide to
// 2.71 between y 0.86 and y 1.71.
const NECK_Y = +(process.env.SPOON_NECK || 0.86);

// ---- reading ---------------------------------------------------------------------------

function readObj(text) {
  const V = [], F = [];
  for (const line of text.split(/\r?\n/)) {
    const b = line.trim().split(/\s+/);
    if (b[0] === "v") V.push([+b[1], +b[2], +b[3]]);
    else if (b[0] === "f") {
      const idx = b.slice(1).map((c) => +c.split("/")[0] - 1);
      for (let i = 1; i + 1 < idx.length; i++) F.push([idx[0], idx[i], idx[i + 1]]);
    }
  }
  return { V, F };
}

// ---- quadrics --------------------------------------------------------------------------

/// A symmetric 4x4 quadric as its ten distinct entries.
const zeroQ = () => new Float64Array(10);

function addPlane(q, a, b, c, d) {
  q[0] += a * a; q[1] += a * b; q[2] += a * c; q[3] += a * d;
  q[4] += b * b; q[5] += b * c; q[6] += b * d;
  q[7] += c * c; q[8] += c * d;
  q[9] += d * d;
}

function addQ(into, q) {
  for (let i = 0; i < 10; i++) into[i] += q[i];
}

/// v'Qv: the summed squared distance from p to every plane Q was built from.
function quadricCost(q, p) {
  const [x, y, z] = p;
  return q[0] * x * x + 2 * q[1] * x * y + 2 * q[2] * x * z + 2 * q[3] * x
       + q[4] * y * y + 2 * q[5] * y * z + 2 * q[6] * y
       + q[7] * z * z + 2 * q[8] * z
       + q[9];
}

/// Where a collapsed edge's vertex lands: the minimum of the combined quadric. The 3x3 solve
/// is singular exactly where the surface is flat or a straight ridge -- a whole line or plane
/// of equally good answers -- hence the fallback.
function bestPoint(q, a, b) {
  const m = [q[0], q[1], q[2], q[1], q[4], q[5], q[2], q[5], q[7]];
  const det = m[0] * (m[4] * m[8] - m[5] * m[7])
            - m[1] * (m[3] * m[8] - m[5] * m[6])
            + m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (Math.abs(det) > 1e-12) {
    const r = [-q[3], -q[6], -q[8]];
    const inv = [
      (m[4] * m[8] - m[5] * m[7]) / det, (m[2] * m[7] - m[1] * m[8]) / det, (m[1] * m[5] - m[2] * m[4]) / det,
      (m[5] * m[6] - m[3] * m[8]) / det, (m[0] * m[8] - m[2] * m[6]) / det, (m[2] * m[3] - m[0] * m[5]) / det,
      (m[3] * m[7] - m[4] * m[6]) / det, (m[1] * m[6] - m[0] * m[7]) / det, (m[0] * m[4] - m[1] * m[3]) / det,
    ];
    const p = [
      inv[0] * r[0] + inv[1] * r[1] + inv[2] * r[2],
      inv[3] * r[0] + inv[4] * r[1] + inv[5] * r[2],
      inv[6] * r[0] + inv[7] * r[1] + inv[8] * r[2],
    ];
    if (p.every(Number.isFinite)) return p;
  }
  const mid = [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2];
  let best = a, cost = quadricCost(q, a);
  for (const c of [b, mid]) {
    const k = quadricCost(q, c);
    if (k < cost) { cost = k; best = c; }
  }
  return best;
}

// ---- a lazy binary heap ----------------------------------------------------------------

class Heap {
  constructor() { this.a = []; }
  push(x) {
    const a = this.a;
    a.push(x);
    let i = a.length - 1;
    while (i > 0) {
      const p = (i - 1) >> 1;
      if (a[p].cost <= a[i].cost) break;
      [a[p], a[i]] = [a[i], a[p]];
      i = p;
    }
  }
  pop() {
    const a = this.a;
    if (!a.length) return null;
    const top = a[0], last = a.pop();
    if (a.length) {
      a[0] = last;
      let i = 0;
      for (;;) {
        const l = 2 * i + 1, r = l + 1;
        let s = i;
        if (l < a.length && a[l].cost < a[s].cost) s = l;
        if (r < a.length && a[r].cost < a[s].cost) s = r;
        if (s === i) break;
        [a[s], a[i]] = [a[i], a[s]];
        i = s;
      }
    }
    return top;
  }
}

// ---- decimation ------------------------------------------------------------------------

function faceNormal(V, f) {
  const [a, b, c] = f.map((i) => V[i]);
  const u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
  const w = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
  return [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]];
}

function decimate(V, F, targetFaces) {
  V = V.map((p) => p.slice());
  const Q = V.map(zeroQ);
  const alive = new Uint8Array(F.length).fill(1);
  const vfaces = V.map(() => new Set());
  const dead = new Uint8Array(V.length);

  F.forEach((f, fi) => {
    const n = faceNormal(V, f);
    const len = Math.hypot(...n) || 1;
    const [a, b, c] = [n[0] / len, n[1] / len, n[2] / len];
    const d = -(a * V[f[0]][0] + b * V[f[0]][1] + c * V[f[0]][2]);
    for (const i of f) { addPlane(Q[i], a, b, c, d); vfaces[i].add(fi); }
  });

  // Lazy heap: an entry whose endpoints have moved is skipped on pop, not deleted in place.
  const ver = new Int32Array(V.length);
  const heap = new Heap();
  const consider = (u, v) => {
    if (u === v || dead[u] || dead[v]) return;
    const q = zeroQ();
    addQ(q, Q[u]); addQ(q, Q[v]);
    const p = bestPoint(q, V[u], V[v]);
    heap.push({ cost: quadricCost(q, p), u, v, p, vu: ver[u], vv: ver[v] });
  };
  const edges = new Set();
  for (const f of F)
    for (let i = 0; i < 3; i++) {
      const a = f[i], b = f[(i + 1) % 3];
      const key = a < b ? a * V.length + b : b * V.length + a;
      if (!edges.has(key)) { edges.add(key); consider(a, b); }
    }

  let faces = F.length;
  let collapses = 0, rejected = 0;
  while (faces > targetFaces) {
    const e = heap.pop();
    if (!e) break;
    if (dead[e.u] || dead[e.v] || ver[e.u] !== e.vu || ver[e.v] !== e.vv) continue;

    // Refuse a collapse that flips a surviving face: on a watertight mesh the hole spreads.
    let flips = false;
    for (const fi of vfaces[e.v]) {
      if (!alive[fi]) continue;
      const f = F[fi];
      if (f.includes(e.u)) continue;
      const before = faceNormal(V, f);
      const moved = f.map((i) => (i === e.v ? e.p : V[i]));
      const after = faceNormal({ length: 0, ...moved.reduce((o, p, i) => (o[i] = p, o), {}) }, [0, 1, 2]);
      const dot = before[0] * after[0] + before[1] * after[1] + before[2] * after[2];
      if (dot <= 0) { flips = true; break; }
    }
    if (flips) { rejected++; continue; }

    V[e.u] = e.p;
    addQ(Q[e.u], Q[e.v]);
    for (const fi of vfaces[e.v]) {
      if (!alive[fi]) continue;
      const f = F[fi];
      if (f.includes(e.u)) { alive[fi] = 0; faces--; continue; }
      for (let i = 0; i < 3; i++) if (f[i] === e.v) f[i] = e.u;
      vfaces[e.u].add(fi);
    }
    dead[e.v] = 1;
    vfaces[e.v].clear();
    ver[e.u]++;
    collapses++;

    const nbr = new Set();
    for (const fi of vfaces[e.u]) if (alive[fi]) for (const i of F[fi]) if (i !== e.u && !dead[i]) nbr.add(i);
    for (const n of nbr) consider(e.u, n);
  }

  // Renumber the survivors; the trailing filter drops triangles collapsed to an edge.
  const map = new Int32Array(V.length).fill(-1);
  const outV = [];
  F.forEach((f, fi) => {
    if (!alive[fi]) return;
    for (const i of f) if (map[i] < 0) { map[i] = outV.length; outV.push(V[i]); }
  });
  const outF = F.filter((_, fi) => alive[fi]).map((f) => f.map((i) => map[i]))
    .filter((f) => f[0] !== f[1] && f[1] !== f[2] && f[0] !== f[2]);
  return { V: outV, F: outF, collapses, rejected };
}

// ---- normals, orientation, writing -------------------------------------------------------

/// Area-weighted vertex normals; the source file's belong to vertices the collapses removed.
function normals(V, F) {
  const N = V.map(() => [0, 0, 0]);
  for (const f of F) {
    const n = faceNormal(V, f);            // length is twice the triangle's area
    for (const i of f) { N[i][0] += n[0]; N[i][1] += n[1]; N[i][2] += n[2]; }
  }
  return N.map((n) => {
    const l = Math.hypot(...n);
    return l > 1e-9 ? [n[0] / l, n[1] / l, n[2] / l] : [0, 1, 0];
  });
}

function bounds(V) {
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const p of V) for (let i = 0; i < 3; i++) { lo[i] = Math.min(lo[i], p[i]); hi[i] = Math.max(hi[i], p[i]); }
  return { lo, hi, size: [0, 1, 2].map((i) => hi[i] - lo[i]), mid: [0, 1, 2].map((i) => (lo[i] + hi[i]) / 2) };
}

const f4 = (n) => {
  const s = (+n.toFixed(4)).toString();
  return s === "-0" ? "0" : s;
};

function writeObj(V, F, N) {
  const out = ["# BFS 9000, decimated from shaped_spoon_roblox.obj by scripts/prep-spoonie.js"];
  for (const p of V) out.push(`v ${f4(p[0])} ${f4(p[1])} ${f4(p[2])}`);
  for (const n of N) out.push(`vn ${f4(n[0])} ${f4(n[1])} ${f4(n[2])}`);
  // Reversed: the engine's front face is clockwise, an OBJ's is counter-clockwise. The vn
  // lines carry the shading, so a mesh wound the wrong way still lights correctly and only
  // backface culling gives it away -- scripts/check-winding.js checks the output.
  for (const f of F) out.push(`f ${f[2] + 1}//${f[2] + 1} ${f[1] + 1}//${f[1] + 1} ${f[0] + 1}//${f[0] + 1}`);
  return out.join("\n") + "\n";
}

// ---- run ---------------------------------------------------------------------------------

const srcText = fs.readFileSync(SRC, "utf8");
const src = readObj(srcText);
const b0 = bounds(src.V);
console.log(`source        ${src.V.length} verts, ${src.F.length} tris, ` +
  `${(Buffer.byteLength(srcText) / 1024).toFixed(0)}KB`);
console.log(`  extents     ${b0.size.map((v) => v.toFixed(2)).join(" x ")}`);

const dec = decimate(src.V, src.F, TARGET_TRIS);
console.log(`decimated     ${dec.V.length} verts, ${dec.F.length} tris ` +
  `(${dec.collapses} collapses, ${dec.rejected} refused as flips)`);

// Orientation is baked in because a MeshPart cannot be rotated: it is fitted to its Size
// about its own middle. The source runs its length on +Y with the bowl's face on +Z; the game
// wants the length on Z, bowl forward at -Z, the flat sideways so the edge leads a diagonal
// cut. (x, y, z) <- (-z, x, -y) has determinant +1; the obvious (z, x, -y) is a reflection.
const scale = LENGTH / b0.size[1];

const stretched = dec.V.map(([x, y, z]) =>
  y < NECK_Y ? [x, NECK_Y + (y - NECK_Y) * HANDLE, z] : [x, y, z]);

const oriented = stretched.map(([x, y, z]) => [-z, x, -y]);
const b1 = bounds(oriented);
const V = oriented.map((p) => [0, 1, 2].map((i) => (p[i] - b1.mid[i]) * scale));
const b = bounds(V);
const N = normals(V, dec.F);

const text = writeObj(V, dec.F, N);
fs.mkdirSync(OUT, { recursive: true });
fs.writeFileSync(path.join(OUT, "spoonie.obj"), text);

// Bowl tip and butt along the shaft, so the catalogue can put the grip on the handle.
let bowlEnd = 0;
for (const p of V) if (p[2] < bowlEnd) bowlEnd = p[2];
const meta = {
  size: b.size.map((v) => +v.toFixed(4)),
  mid: b.mid.map((v) => +v.toFixed(4)),
  bowlTip: +bowlEnd.toFixed(4),
  buttEnd: +b.hi[2].toFixed(4),
  tris: dec.F.length,
};
fs.writeFileSync(path.join(OUT, "spoonie.json"), JSON.stringify(meta, null, 1));

console.log(`handle        x${HANDLE} about the neck at y ${NECK_Y}`);
console.log(`oriented      ${b.size.map((v) => v.toFixed(2)).join(" x ")}  ` +
  `(thin x, width y, length z; bowl tip z ${meta.bowlTip}, butt z ${meta.buttEnd})`);
console.log(`wrote         models/spoonie.obj  ${(Buffer.byteLength(text) / 1024).toFixed(0)}KB  ` +
  `(${(100 - Buffer.byteLength(text) / Buffer.byteLength(srcText) * 100).toFixed(0)}% smaller)`);
