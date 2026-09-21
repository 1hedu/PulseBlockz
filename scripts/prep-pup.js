// Turns the pup's GLB into the mesh and picture the Pup pet is built from.
//
//   node scripts/prep-pup.js scripts/models/pup.glb    (PLS_Pup_R12_SingleMesh.glb)
//
// Revision 12 is one mesh wearing one 256-square colour atlas, which carries the faces, eyes
// and brows. The GLB is already the way round this wants -- glTF is Y-up and the export faces
// -Z -- and in the same units and frame as the first pup, so the catalogue's scale carries
// over. Vertices are welded and rounded as in prep-tree.js, because the exporter gives every
// corner its own; faces are reversed for the engine's winding; normals the engine derives.
const fs = require("fs");
const path = require("path");
const { Canvas, decode, encode, blit } = require("./png");

const OUT = path.join(__dirname, "models");
const TEX = 256;
const DP = 3;

/// Splits a .glb into its JSON and its binary buffer.
function readGlb(bytes) {
  if (bytes.readUInt32LE(0) !== 0x46546c67) throw new Error("not a glb");
  let at = 12, json = null, bin = null;
  while (at < bytes.length) {
    const len = bytes.readUInt32LE(at), kind = bytes.readUInt32LE(at + 4);
    const body = bytes.subarray(at + 8, at + 8 + len);
    if (kind === 0x4e4f534a) json = JSON.parse(body.toString("utf8"));
    else if (kind === 0x004e4942) bin = body;
    at += 8 + len + ((4 - (len % 4)) % 4);
  }
  if (!json) throw new Error("no json chunk");
  return { json, bin };
}

const COMPONENT = {
  5120: ["readInt8", 1], 5121: ["readUInt8", 1], 5122: ["readInt16LE", 2],
  5123: ["readUInt16LE", 2], 5125: ["readUInt32LE", 4], 5126: ["readFloatLE", 4],
};
const COUNT = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 };

function readAccessor(g, bin, index) {
  const acc = g.accessors[index];
  const [reader, size] = COMPONENT[acc.componentType];
  const n = COUNT[acc.type];
  const view = g.bufferViews[acc.bufferView];
  const base = (view.byteOffset || 0) + (acc.byteOffset || 0);
  const stride = view.byteStride || n * size;
  const out = [];
  for (let i = 0; i < acc.count; i++) {
    const at = base + i * stride;
    if (n === 1) out.push(bin[reader](at));
    else {
      const v = [];
      for (let k = 0; k < n; k++) v.push(bin[reader](at + k * size));
      out.push(v);
    }
  }
  return out;
}

const src = process.argv[2];
if (!src) {
  console.error("usage: node scripts/prep-pup.js <PLS_Pup_R12_SingleMesh.glb>");
  process.exit(1);
}
const { json: g, bin } = readGlb(fs.readFileSync(src));
if (g.meshes.length !== 1 || g.nodes.some((n) => n.matrix || n.translation || n.rotation || n.scale)) {
  throw new Error("expected one mesh with no node transform -- this is not the file this was written for");
}

const P = [], T = [], F = [];
for (const prim of g.meshes[0].primitives) {
  if (prim.mode !== undefined && prim.mode !== 4) continue;
  const pos = readAccessor(g, bin, prim.attributes.POSITION);
  const uv = readAccessor(g, bin, prim.attributes.TEXCOORD_0);
  const idx = prim.indices !== undefined ? readAccessor(g, bin, prim.indices) : pos.map((_, i) => i);
  const base = P.length;
  for (let i = 0; i < pos.length; i++) { P.push(pos[i]); T.push(uv[i]); }
  for (let i = 0; i + 2 < idx.length; i += 3) F.push([base + idx[i], base + idx[i + 1], base + idx[i + 2]]);
}

const round = (n) => {
  const s = (+n).toFixed(DP).replace(/\.?0+$/, "");
  return s === "" || s === "-" || s === "-0" ? "0" : s;
};

const out = ["# PLS Pup, revision 12, from the GLB by scripts/prep-pup.js", "mtllib pup.mtl", "usemtl PupColors"];
const vKey = new Map(), tKey = new Map(), fOut = [];
const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
let dropped = 0;
const vertex = (i) => {
  const p = P[i].map(round), pk = p.join(" ");
  if (!vKey.has(pk)) {
    vKey.set(pk, vKey.size + 1);
    out.push(`v ${pk}`);
    for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], +p[k]); hi[k] = Math.max(hi[k], +p[k]); }
  }
  return vKey.get(pk);
};
const texel = (i) => {
  // glTF puts v = 0 at the top of the picture and OBJ at the bottom.
  const t = [round(T[i][0]), round(1 - T[i][1])], tk = t.join(" ");
  if (!tKey.has(tk)) { tKey.set(tk, tKey.size + 1); out.push(`vt ${tk}`); }
  return tKey.get(tk);
};
for (const f of F) {
  const c = f.slice().reverse();                   // the engine's winding
  const v = c.map(vertex);
  if (v[0] === v[1] || v[1] === v[2] || v[0] === v[2]) { dropped++; continue; }
  fOut.push("f " + c.map((i, k) => `${v[k]}/${texel(i)}`).join(" "));
}
const text = out.concat(fOut).join("\n") + "\n";
fs.writeFileSync(path.join(OUT, "pup.obj"), text);

// The atlas, already 256 square, decoded and written back as plain RGBA.
const view = g.bufferViews[g.images[g.textures[g.materials[0].pbrMetallicRoughness.baseColorTexture.index].source].bufferView];
const raw = bin.subarray(view.byteOffset || 0, (view.byteOffset || 0) + view.byteLength);
const picture = decode(raw);
const c = new Canvas(TEX, TEX);
blit(c, picture, 0, 0, TEX, TEX);
// The atlas is a four-by-four grid of flat colours, one per source material. Under the town's
// light the body (material 1, column 1 row 0) and the toes (material 2, column 2 row 0) read
// pale grey, not charcoal; one factor for both keeps the toes darker than the coat.
const DARKER = { cells: [[1, 0], [2, 0]], by: 0.55 };
const CELL = TEX / 4;
for (const [cx, cy] of DARKER.cells) {
  for (let y = cy * CELL; y < (cy + 1) * CELL; y++) {
    for (let x = cx * CELL; x < (cx + 1) * CELL; x++) {
      const i = (y * TEX + x) * 4;
      for (let k = 0; k < 3; k++) c.px[i + k] = Math.round(c.px[i + k] * DARKER.by);
    }
  }
}
const png = encode(c.px, c.w, c.h);
fs.writeFileSync(path.join(OUT, "pup-colors.png"), png);

const size = [0, 1, 2].map((i) => +(hi[i] - lo[i]).toFixed(4));
const mid = [0, 1, 2].map((i) => +((lo[i] + hi[i]) / 2).toFixed(4));
fs.writeFileSync(path.join(OUT, "pup.json"), JSON.stringify({ size, mid }, null, 1) + "\n");

console.log("pup.obj         %d verts -> %d, %d tris%s, %d bytes", P.length, vKey.size, fOut.length,
  dropped ? ` (${dropped} degenerate dropped)` : "", text.length);
console.log("pup-colors.png  %dx%d -> %d bytes", picture.w, picture.h, png.length);
console.log("pup.json        %s studs, middle %s", size.join(" x "), mid.join(", "));
