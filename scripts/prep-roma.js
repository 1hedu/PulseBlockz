// Shrinks the White Roma OBJ (3dp) and its texture (256 square) for on-chain storage, and
// splits each wheel into its own mesh so it can spin: a MeshPart is recentred on its own
// bounds by the host, so a wheel written to its own file arrives centred on its axle.
const fs = require("fs");
const path = require("path");
const { Canvas, decode, encode, blit } = require("./png");

const OUT = path.join(__dirname, "models");
const TEX = 256; // down from a 1024-square source; more detail than a pet this size can show
const DP = 3;

function readObj(text) {
  const v = [], vt = [], faces = [];
  for (const line of text.split("\n")) {
    const p = line.trim().split(/\s+/);
    if (p[0] === "v") v.push([+p[1], +p[2], +p[3]]);
    else if (p[0] === "vt") vt.push([+p[1], +p[2]]);
    else if (p[0] === "f") faces.push(p.slice(1).map((c) => c.split("/").map((n) => (n ? +n : 0))));
  }
  return { v, vt, faces };
}

const round = (n) => {
  const s = n.toFixed(DP).replace(/\.?0+$/, "");
  return s === "" || s === "-" ? "0" : s;
};

/// Writes a subset of the faces as its own OBJ, keeping only the vertices it uses.
function writeObj(mesh, faces, note) {
  const vMap = new Map(), tMap = new Map(), vs = [], ts = [];
  const keep = (map, list, src, i) => {
    if (!i) return 0;
    if (!map.has(i)) { list.push(src[i - 1]); map.set(i, list.length); }
    return map.get(i);
  };
  const out = [];
  for (const f of faces) {
    out.push(f.map((c) => [keep(vMap, vs, mesh.v, c[0]), keep(tMap, ts, mesh.vt, c[1])]));
  }
  const lines = ["# " + note + ", rewritten at 3dp by scripts/prep-roma.js",
    "mtllib roma.mtl", "usemtl Roma_Colors"];
  for (const p of vs) lines.push(`v ${round(p[0])} ${round(p[1])} ${round(p[2])}`);
  for (const p of ts) lines.push(`vt ${round(p[0])} ${round(p[1])}`);
  // Wound the other way on the way out: an OBJ winds counter-clockwise, the renderer calls a
  // clockwise triangle the front.
  for (const f of out) lines.push("f " + f.slice().reverse().map((c) => `${c[0]}/${c[1] || ""}`).join(" "));
  return lines.join("\n") + "\n";
}

function measure(mesh, faces) {
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const f of faces) for (const c of f) {
    const p = mesh.v[c[0] - 1];
    for (let i = 0; i < 3; i++) { if (p[i] < lo[i]) lo[i] = p[i]; if (p[i] > hi[i]) hi[i] = p[i]; }
  }
  return {
    size: [hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]].map((n) => +n.toFixed(4)),
    mid: [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2].map((n) => +n.toFixed(4)),
  };
}

/// Splits the faces into connected surfaces. The OBJ is one object with one material and no
/// groups, so the wheels can only be found by geometry. Vertices are welded by position
/// first: every triangle carries its own copies of its corners, so unwelded there is one
/// shell per triangle.
function shells(mesh) {
  const rep = new Map(), of = new Array(mesh.v.length + 1);
  for (let i = 1; i <= mesh.v.length; i++) {
    const p = mesh.v[i - 1];
    const k = `${p[0].toFixed(4)},${p[1].toFixed(4)},${p[2].toFixed(4)}`;
    if (!rep.has(k)) rep.set(k, i);
    of[i] = rep.get(k);
  }
  const par = new Array(mesh.v.length + 1).fill(0).map((_, i) => i);
  const find = (a) => { while (par[a] !== a) { par[a] = par[par[a]]; a = par[a]; } return a; };
  const uni = (a, b) => { a = find(of[a]); b = find(of[b]); if (a !== b) par[a] = b; };
  for (const f of mesh.faces) for (let i = 1; i < f.length; i++) uni(f[0][0], f[i][0]);
  const groups = new Map();
  for (const f of mesh.faces) {
    const k = find(of[f[0][0]]);
    if (!groups.has(k)) groups.set(k, []);
    groups.get(k).push(f);
  }
  return [...groups.values()];
}

const dir = process.argv[2];
if (!dir) {
  console.error("usage: node scripts/prep-roma.js <dir with White_Roma.obj and Roma_Colors.png>");
  process.exit(1);
}
fs.mkdirSync(OUT, { recursive: true });

const srcObj = fs.readFileSync(path.join(dir, "White_Roma.obj"), "utf8");
const mesh = readObj(srcObj);
const whole = measure(mesh, mesh.faces);

// A wheel shell is a disc lying on the X axis: Y and Z spans match, X is much shorter. The
// 1.5 floor drops disc-shaped trim -- a mirror, a badge. A wheel is four concentric shells
// (tyre, rim cap, two rings), so they are gathered by corner. Front is -Z, right is +X.
const parts = shells(mesh).map((faces) => ({ faces, m: measure(mesh, faces) }));
const corners = new Map();
for (const p of parts) {
  const [x, y, z] = p.m.size;
  if (!(y > 1.5 && Math.abs(y - z) < 0.1 * y && x < 0.6 * y)) continue;
  const key = "Wheel" + (p.m.mid[2] < 0 ? "F" : "R") + (p.m.mid[0] > 0 ? "R" : "L");
  if (!corners.has(key)) corners.set(key, []);
  corners.get(key).push(...p.faces);
}
if (corners.size !== 4) throw new Error(`expected four wheels, found ${corners.size}`);

const wheels = [...corners.entries()].map(([name, faces]) => ({ name, faces, m: measure(mesh, faces) }))
  .sort((a, b) => a.name.localeCompare(b.name));
const wheelFaces = new Set(wheels.flatMap((w) => w.faces));
const body = mesh.faces.filter((f) => !wheelFaces.has(f));

const files = [{ file: "roma-body.obj", faces: body, note: "White Roma body" }]
  .concat(wheels.map((w) => ({ file: `roma-${w.name.toLowerCase()}.obj`, faces: w.faces,
    note: "White Roma " + w.name })));
let bytes = 0;
for (const f of files) {
  const text = writeObj(mesh, f.faces, f.note);
  fs.writeFileSync(path.join(OUT, f.file), text);
  bytes += text.length;
}

const src = decode(fs.readFileSync(path.join(dir, "Roma_Colors.png")));
const c = new Canvas(TEX, TEX);
blit(c, src, 0, 0, TEX, TEX);
const tex = encode(c.px, c.w, c.h);
fs.writeFileSync(path.join(OUT, "roma-colors.png"), tex);

// Measurements in the file's own coordinates, so the catalogue can place each piece relative
// to the middle of the car.
fs.writeFileSync(path.join(OUT, "roma.json"), JSON.stringify({
  whole,
  body: measure(mesh, body),
  wheels: wheels.map((w) => ({ name: w.name, size: w.m.size, mid: w.m.mid })),
}, null, 1) + "\n");

console.log("roma meshes     %d -> %d bytes in %d files (%d%%)", srcObj.length, bytes, files.length,
  Math.round((bytes / srcObj.length) * 100));
console.log("roma-colors.png %d -> %d bytes at %d square", src.w * src.h * 4, tex.length, TEX);
console.log("roma.json       whole %s", whole.size.map((n) => n.toFixed(2)).join(" x "));
for (const w of wheels) {
  console.log("  %s %s at %s", w.name.padEnd(8), w.m.size.map((n) => n.toFixed(2)).join(" x "),
    w.m.mid.map((n) => n.toFixed(2)).join(", "));
}
