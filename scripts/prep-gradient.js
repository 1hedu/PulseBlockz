// The PulseBlockz gradient as a picture, and the two meshes that wear it.
//
//   node scripts/prep-gradient.js
//
// The stops come from luau/gdextension/src/pulse_gradient.json, the same file SConstruct bakes
// into the engine for the default character skin. Diagonal as it is on a body -- 45 degrees, red
// at the bottom-left, cyan at the top-right -- and each mesh is mapped straight from the front,
// u across and v up, so the grain lies at the same angle it does across a body.
//
//   models/pulse-gradient.png   256 x 256, the place asset PulseGradient
//   models/orb.obj              the cane's orb: a sphere a stud across
//   models/engram-badge.obj     the Engram's hexagon, 1.1 across the points, 1 tall, 0.18 thick
//
// Both go on a SpecialMesh as a FileMesh with the picture as TextureId: a Part's own shapes take
// no texture, and a SpecialMesh honours TextureId only as a FileMesh.
const fs = require("fs");
const path = require("path");
const { Canvas, encode } = require("./png");

const OUT = path.join(__dirname, "models");
const { stops: STOPS } = JSON.parse(fs.readFileSync(
  path.join(__dirname, "..", "luau", "gdextension", "src", "pulse_gradient.json"), "utf8"));

const r4 = (n) => n.toFixed(4).replace(/\.?0+$/, "").replace(/^-0$/, "0") || "0";

// ---- the colours --------------------------------------------------------------------------
// What the engine's Gradient does for the skin: GRADIENT_INTERPOLATE_CUBIC in
// GRADIENT_COLOR_SPACE_OKLAB, i.e. Godot's Math::cubic_interpolate between OKLab stops.
const toLinear = (c) => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
const toSrgb = (c) => (c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055);
function oklab([r, g, b]) {
  [r, g, b] = [r, g, b].map(toLinear);
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const q = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return [0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * q,
    1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * q,
    0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * q];
}
function srgb([L, a, b]) {
  const l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3;
  const m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3;
  const q = (L - 0.0894841775 * a - 1.2914855480 * b) ** 3;
  return [4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * q,
    -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * q,
    -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * q].map((c) => toSrgb(Math.max(0, Math.min(1, c))));
}
const cubic = (from, to, pre, post, w) =>
  0.5 * ((from * 2) + (-pre + to) * w + (2 * pre - 5 * from + 4 * to - post) * w * w + (-pre + 3 * from - 3 * to + post) * w * w * w);
function colourAt(t) {
  t = Math.max(0, Math.min(1, t));
  let i = 0;
  while (i < STOPS.length - 2 && t > STOPS[i + 1].t) i++;
  const s0 = STOPS[i], s1 = STOPS[i + 1];
  const pre = oklab(STOPS[Math.max(0, i - 1)].rgb), post = oklab(STOPS[Math.min(STOPS.length - 1, i + 2)].rgb);
  const a = oklab(s0.rgb), b = oklab(s1.rgb);
  const w = Math.max(0, Math.min(1, (t - s0.t) / (s1.t - s0.t)));
  return srgb([0, 1, 2].map((k) => cubic(a[k], b[k], pre[k], post[k], w))).map((c) => Math.round(c * 255));
}

// ---- the picture --------------------------------------------------------------------------
const SIZE = 256;
const pic = new Canvas(SIZE, SIZE);
for (let y = 0; y < SIZE; y++) {
  for (let x = 0; x < SIZE; x++) {
    const u = (x + 0.5) / SIZE, v = 1 - (y + 0.5) / SIZE;   // v up: the top row is v = 1
    pic.set(x, y, colourAt((u + v) / 2));                   // along the diagonal, red to cyan
  }
}
fs.writeFileSync(path.join(OUT, "pulse-gradient.png"), encode(pic.px, SIZE, SIZE));

// ---- meshes -------------------------------------------------------------------------------
// Faces wind clockwise seen from outside -- the engine's front face; check-winding.js reports a
// negative volume for each. UVs from the front: u across, v up, over the mesh's own size.
function writeObj(file, title, verts, faces, width, height) {
  const lines = [`# ${title} -- scripts/prep-gradient.js`];
  for (const p of verts) lines.push(`v ${r4(p[0])} ${r4(p[1])} ${r4(p[2])}`);
  for (const p of verts) lines.push(`vt ${r4(p[0] / width + 0.5)} ${r4(p[1] / height + 0.5)}`);
  for (const f of faces) lines.push("f " + f.map((k) => `${k}/${k}`).join(" "));
  fs.writeFileSync(path.join(OUT, file), lines.join("\n") + "\n");
}

{
  const SEG = 32, RINGS = 20;
  const verts = [], faces = [];
  for (let j = 0; j <= RINGS; j++) {
    const lat = Math.PI * j / RINGS;
    for (let i = 0; i <= SEG; i++) {
      const lon = 2 * Math.PI * i / SEG;
      verts.push([0.5 * Math.sin(lat) * Math.cos(lon), 0.5 * Math.cos(lat), 0.5 * Math.sin(lat) * Math.sin(lon)]);
    }
  }
  const at = (i, j) => j * (SEG + 1) + i + 1;
  for (let j = 0; j < RINGS; j++) {
    for (let i = 0; i < SEG; i++) {
      const a = at(i, j), b = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
      if (j > 0) faces.push([a, c, b]);
      if (j < RINGS - 1) faces.push([a, d, c]);
    }
  }
  writeObj("orb.obj", "The cane's orb: a unit sphere", verts, faces, 1, 1);
}

// Points left and right, flat top and bottom.
{
  const W = 1.1, H = 1.0, T = 0.18;
  const ring = [[W / 2, 0], [W / 4, H / 2], [-W / 4, H / 2], [-W / 2, 0], [-W / 4, -H / 2], [W / 4, -H / 2]];   // anticlockwise from +X
  const verts = [];
  for (const [x, y] of ring) verts.push([x, y, -T / 2]);   // 1..6, the front (-Z, Roblox's Front)
  for (const [x, y] of ring) verts.push([x, y, T / 2]);    // 7..12, the back
  const faces = [];
  const n = ring.length;
  for (let k = 1; k < n - 1; k++) {
    faces.push([1, k + 1, k + 2]);                // the front, seen from -Z
    faces.push([n + 1, n + k + 2, n + k + 1]);    // the back, seen from +Z
  }
  for (let k = 0; k < n; k++) {
    const a = k + 1, b = (k + 1) % n + 1, c = n + (k + 1) % n + 1, d = n + k + 1;
    faces.push([a, c, b], [a, d, c]);              // round the edge
  }
  writeObj("engram-badge.obj", "The Engram's badge: a hexagon, extruded", verts, faces, W, H);
}

console.log("pulse-gradient.png 256x256, orb.obj, engram-badge.obj");
