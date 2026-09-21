// Prepares the Familiar's head and wing meshes into models/, for the chain. Trimmed for gas:
// no vertex normals -- the engine derives them, and a flat-shaded PS1 asset has no smoothing
// to lose -- coordinates rounded, unused vertices dropped.
//
//   node scripts/prep-pets.js <extracted-dir>
const fs = require("fs");
const path = require("path");
const { Canvas, encode, decode, blit } = require("./png");

const OUT = path.join(__dirname, "models");

/// Parses a Wavefront OBJ far enough to drop whole named objects and re-emit it.
function readObj(text) {
  const v = [], vt = [], faces = [];
  let object = "";
  for (const line of text.split(/\r?\n/)) {
    const bits = line.trim().split(/\s+/);
    switch (bits[0]) {
      case "o": object = bits.slice(1).join(" "); break;
      case "v": v.push([+bits[1], +bits[2], +bits[3]]); break;
      case "vt": vt.push([+bits[1], +bits[2]]); break;
      case "f": {
        // f v/vt/vn, with vt and vn each optional
        const corners = bits.slice(1).map((c) => {
          const [a, b] = c.split("/");
          return { v: +a, vt: b ? +b : 0 };
        });
        for (let i = 1; i + 1 < corners.length; i++)
          faces.push({ object, corners: [corners[0], corners[i], corners[i + 1]] });
        break;
      }
    }
  }
  return { v, vt, faces };
}

/// Re-emits an OBJ with only the wanted faces, compacted.
function writeObj(mesh, keep, opts = {}) {
  const dp = opts.dp === undefined ? 3 : opts.dp;
  const round = (n) => {
    const s = n.toFixed(dp).replace(/\.?0+$/, "");
    return s === "" || s === "-" ? "0" : s;
  };
  const faces = mesh.faces.filter((f) => keep(f, mesh));
  const vMap = new Map(), vtMap = new Map();
  const vOut = [], vtOut = [];
  const take = (map, out, src, i) => {
    if (i <= 0) return 0;
    if (!map.has(i)) { out.push(src[i - 1]); map.set(i, out.length); }
    return map.get(i);
  };
  const lines = [];
  const fLines = [];
  for (const f of faces) {
    const parts = f.corners.map((c) => {
      const vi = take(vMap, vOut, mesh.v, c.v);
      const ti = take(vtMap, vtOut, mesh.vt, c.vt);
      return ti ? `${vi}/${ti}` : `${vi}`;
    });
    // Not reversed: this OBJ is already clockwise-from-outside, the engine's front face.
    // scripts/check-winding.js checks it -- a published mesh's signed volume is negative.
    fLines.push("f " + parts.join(" "));
  }
  for (const p of vOut) lines.push(`v ${round(p[0])} ${round(p[1])} ${round(p[2])}`);
  for (const t of vtOut) lines.push(`vt ${round(t[0])} ${round(t[1])}`);
  return {
    text: lines.concat(fLines).join("\n") + "\n",
    vertices: vOut.length, faces: faces.length,
  };
}

/// A fairy wing: a lens in the XY plane, pointed at both ends, root at the origin and tip at
/// +X, about four times as long as it is wide. The exponents make the profile asymmetric --
/// widest near t = 0.46 rather than at the midpoint, and fuller through the ends than a sine.
function wingObj(segments = 22) {
  const top = [], bottom = [];
  for (let i = 0; i <= segments; i++) {
    const t = i / segments;                       // 0 at the root, 1 at the tip
    const width = Math.pow(Math.sin(Math.PI * Math.pow(t, 0.88)), 0.85) * 0.13;
    top.push([t, width, 0]);
    bottom.push([t, -width, 0]);
  }
  // Two layers a hair apart: a flat mesh reports a zero extent on that axis, and the engine
  // fits a mesh to its Size by scaling each axis. One sheet also disappears seen edge on.
  const T = 0.012;
  const v = [], f = [];
  const push = (p) => { v.push(p); return v.length; };
  const layers = [T, -T].map((z) => ({
    top: top.map((p) => push([p[0], p[1], z])),
    bottom: bottom.map((p) => push([p[0], p[1], z])),
  }));
  for (const [n, layer] of layers.entries()) {
    for (let i = 0; i < segments; i++) {
      // The back layer winds the other way so both faces point outwards
      const a = layer.top[i], b = layer.top[i + 1], c = layer.bottom[i + 1], d = layer.bottom[i];
      if (n === 0) { f.push([a, b, c]); f.push([a, c, d]); }
      else { f.push([a, c, b]); f.push([a, d, c]); }
    }
  }
  // The rim, joining the two layers into a solid
  for (let i = 0; i < segments; i++) {
    const [fr, bk] = layers;
    f.push([fr.top[i], bk.top[i], bk.top[i + 1]]);
    f.push([fr.top[i], bk.top[i + 1], fr.top[i + 1]]);
    f.push([bk.bottom[i], fr.bottom[i], fr.bottom[i + 1]]);
    f.push([bk.bottom[i], fr.bottom[i + 1], bk.bottom[i + 1]]);
  }
  const round = (n) => n.toFixed(4).replace(/\.?0+$/, "") || "0";
  return v.map((p) => `v ${round(p[0])} ${round(p[1])} ${round(p[2])}`)
    .concat(f.map((t) => `f ${t[0]} ${t[1]} ${t[2]}`)).join("\n") + "\n";
}

function shrinkTexture(file, size) {
  const src = decode(fs.readFileSync(file));
  const c = new Canvas(size, size);
  blit(c, src, 0, 0, size, size);
  return encode(c.px, c.w, c.h);
}

const dir = process.argv[2];
if (!dir) {
  console.error("usage: node scripts/prep-pets.js <dir with pup/ and head/ extracted>");
  process.exit(1);
}
fs.mkdirSync(OUT, { recursive: true });

// ---- Steven: the head, minus its neck -------------------------------------------------
const headObj = readObj(fs.readFileSync(path.join(dir, "head", "friend_ps1_head_roblox.obj"), "utf8"));
const objects = [...new Set(headObj.faces.map((f) => f.object))];
console.log("head objects:", objects.join(", "));
// The neck comes off by height, not by object name: the front and side panels run the whole
// way down and carry its faces. -1.55 is below the jaw, above the flare into the shoulders.
const NECK_Y = process.env.NECK_Y ? Number(process.env.NECK_Y) : -1.55;
const aboveNeck = (f, mesh) => f.corners.every((c) => mesh.v[c.v - 1][1] >= NECK_Y);

// Two meshes: a MeshPart has one TextureID and one Color, and only half this model has UVs.
// Head_Back_Hair takes a flat hair colour; Feature_0/1/2 are dropped -- no UVs, so they
// render as white shapes on the chin and mouth.
const TEXTURED = new Set(["Head_Front_Textured", "Head_Sides_Textured"]);
const face = writeObj(headObj, (f, mesh) => TEXTURED.has(f.object) && aboveNeck(f, mesh));
fs.writeFileSync(path.join(OUT, "steven-face.obj"), face.text);
console.log(`steven-face.obj   ${face.vertices} verts, ${face.faces} tris, ${face.text.length.toLocaleString()} bytes`);

const skull = writeObj(headObj, (f, mesh) => f.object === "Head_Back_Hair" && aboveNeck(f, mesh));
fs.writeFileSync(path.join(OUT, "steven-skull.obj"), skull.text);
console.log(`steven-skull.obj  ${skull.vertices} verts, ${skull.faces} tris, ${skull.text.length.toLocaleString()} bytes`);

// A MeshPart is fitted to its own Size about its own centre, so both halves would land
// concentric at the origin; the catalogue offsets them from these measurements.
function measure(text) {
  const m = readObj(text);
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const p of m.v) for (let i = 0; i < 3; i++) {
    if (p[i] < lo[i]) lo[i] = p[i];
    if (p[i] > hi[i]) hi[i] = p[i];
  }
  return {
    size: [hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]],
    mid: [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2],
  };
}
const measured = { face: measure(face.text), skull: measure(skull.text) };
fs.writeFileSync(path.join(OUT, "steven.json"), JSON.stringify(measured, null, 1));
console.log("steven.json       face", measured.face.size.map((n) => n.toFixed(2)).join(" x "),
  " skull", measured.skull.size.map((n) => n.toFixed(2)).join(" x "));

const skin = shrinkTexture(path.join(dir, "head", "FaceTexture_PS1.png"), 256);
fs.writeFileSync(path.join(OUT, "steven-face.png"), skin);
console.log(`steven-face.png   256x256, ${skin.length.toLocaleString()} bytes`);

// ---- the wings, lifted out of the back-mounted fairy head ------------------------------
//
// Nothing in that OBJ is grouped and every triangle is its own island, so the wings are
// picked out by the baked per-vertex RGB, which leaves the head's browns and skin behind.
function liftWings(text) {
  const NL = String.fromCharCode(10);
  const v = [], tris = [];
  for (const line of text.split(NL)) {
    const b = line.trim().split(/\s+/);
    if (b[0] === "v") v.push(b.slice(1).map(Number));      // x y z r g b
    else if (b[0] === "f") {
      const c = b.slice(1).map((x) => +x.split("/")[0] - 1);
      for (let i = 1; i + 1 < c.length; i++) tris.push([c[0], c[i], c[i + 1]]);
    }
  }
  const pale = (i) => {
    const [r, g, bl] = [v[i][3], v[i][4], v[i][5]];
    return bl > 0.55 && bl >= r && r > 0.4;                // lavender through white
  };
  const bright = (i) => v[i][3] > 0.9 && v[i][4] > 0.9;    // the lit rim
  // Colour alone catches the eye whites as well. Per the README both pairs of wings root at
  // Y = +0.700 on the back of the head, so behind 0.6 the pale geometry is wing.
  const BACK_Y = 0.6;
  const behind = (t) => t.every((i) => v[i][1] >= BACK_Y);
  // Left and right are separate parts so each wing can flap; rim apart from membrane so each
  // takes its own colour.
  const groups = { bodyL: [], bodyR: [], rimL: [], rimR: [] };
  for (const t of tris) {
    if (!t.every(pale) || !behind(t)) continue;
    const side = t.reduce((a, i) => a + v[i][0], 0) / 3 < 0 ? "L" : "R";
    groups[(t.every(bright) ? "rim" : "body") + side].push(t);
  }
  const emit = (ts) => {
    const map = new Map(), out = [], fl = [];
    for (const t of ts) {
      const idx = t.map((i) => {
        if (!map.has(i)) {
          const p = v[i];
          out.push([p[0], p[2], -p[1]]);                   // Z-up, -Y front -> Y-up, +Z front
          map.set(i, out.length);
        }
        return map.get(i);
      });
      fl.push("f " + idx[2] + " " + idx[1] + " " + idx[0]);   // an OBJ is counter-clockwise, the engine's front face clockwise
    }
    // Three places: a thousandth of a unit over a three-unit span, and 15% fewer bytes
    const r = (n) => n.toFixed(3).replace(/\.?0+$/, "") || "0";
    const lines = out.map((p) => "v " + r(p[0]) + " " + r(p[1]) + " " + r(p[2])).concat(fl);
    return { text: lines.join(NL) + NL, tris: ts.length, verts: out.length };
  };
  return Object.fromEntries(Object.entries(groups).map(([k, ts]) => [k, emit(ts)]));
}

const fairyFile = path.join(dir, "fairy", "PS1_FairyHead_BackMountedWings_Roblox.obj");
if (fs.existsSync(fairyFile)) {
  const w = liftWings(fs.readFileSync(fairyFile, "utf8"));
  for (const [name, part] of Object.entries(w)) {
    fs.writeFileSync(path.join(OUT, `wing-${name}.obj`), part.text);
    measured[`wing_${name}`] = measure(part.text);
    console.log(`wing-${name}.obj`.padEnd(18) + `${part.verts} verts, ${part.tris} tris, ${part.text.length.toLocaleString()} bytes`);
  }
  fs.writeFileSync(path.join(OUT, "steven.json"), JSON.stringify(measured, null, 1));
} else {
  const wing = wingObj();
  fs.writeFileSync(path.join(OUT, "wing.obj"), wing);
  console.log(`wing.obj          ${wing.length.toLocaleString()} bytes (generated; no fairy model found)`);
}

// The pup has its own script: scripts/prep-pup.js
