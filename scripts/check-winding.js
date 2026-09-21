// Which way every .obj in scripts/models faces.
//
//   node scripts/check-winding.js
//
// The engine treats a CLOCKWISE triangle as the front and culls the rest, where a Wavefront
// OBJ is counter-clockwise, so a mesh is reversed on the way out; one that was not is
// see-through from the side you look at. For a closed mesh the signed volume sum
// a . ((b-a) x (c-a)) / 6 is negative exactly when the triangles are clockwise seen from
// outside, so one number per file decides it. Open meshes are reported, not judged.
const fs = require("fs");
const path = require("path");

const DIR = path.join(__dirname, "models");
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];

function read(file) {
  const v = [], tris = [];
  for (const line of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const b = line.trim().split(/\s+/);
    if (b[0] === "v") v.push([+b[1], +b[2], +b[3]]);
    else if (b[0] === "f") {
      const c = b.slice(1).map((x) => +x.split("/")[0]);
      for (let i = 1; i + 1 < c.length; i++) tris.push([c[0], c[i], c[i + 1]]);
    }
  }
  return { v, tris };
}

/// Closed if every edge is shared by exactly two triangles. Vertices are welded by rounded
/// position, so a seam between two duplicated vertices is still one edge.
function closed(v, tris) {
  const ids = new Map();
  const id = (i) => {
    const k = v[i - 1].map((x) => x.toFixed(4)).join(",");
    if (!ids.has(k)) ids.set(k, ids.size);
    return ids.get(k);
  };
  const seen = new Map();
  for (const t of tris) {
    const [a, b, c] = t.map(id);
    for (const [x, y] of [[a, b], [b, c], [c, a]]) {
      const k = x < y ? `${x}_${y}` : `${y}_${x}`;
      seen.set(k, (seen.get(k) || 0) + 1);
    }
  }
  for (const n of seen.values()) if (n !== 2) return false;
  return true;
}

function volume(v, tris) {
  let s = 0;
  for (const t of tris) {
    const [a, b, c] = t.map((i) => v[i - 1]);
    s += dot(a, cross(sub(b, a), sub(c, a))) / 6;
  }
  return s;
}

let bad = 0, judged = 0;
for (const file of fs.readdirSync(DIR).filter((f) => f.endsWith(".obj")).sort()) {
  const { v, tris } = read(path.join(DIR, file));
  if (!tris.length) continue;
  const vol = volume(v, tris);
  if (!closed(v, tris)) {
    console.log(`${file.padEnd(22)} ${vol.toFixed(4).padStart(12)}  open, not judged`);
    continue;
  }
  judged++;
  const ok = vol < 0;
  if (!ok) bad++;
  console.log(`${file.padEnd(22)} ${vol.toFixed(4).padStart(12)}  ${ok ? "facing out" : "INSIDE OUT"}`);
}
console.log(`\n${judged} closed mesh(es), ${bad} inside out`);
process.exitCode = bad ? 1 : 0;
