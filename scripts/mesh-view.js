// Rasterises a model's actual meshes: orthographic, flat-lit, depth-buffered, per-pixel texture
// lookup, no perspective and no shadows. Lives outside scripts/render-pet.js, which requires the
// catalogue: a catalogue thumbnail can draw from here without closing that loop into a cycle.
const fs = require("fs");
const path = require("path");
const { decode } = require("./png");

const MODELS = path.join(__dirname, "models");

function parseObj(text) {
  const v = [], vt = [], tris = [];
  for (const line of text.split(/\r?\n/)) {
    const b = line.trim().split(/\s+/);
    if (b[0] === "v") v.push([+b[1], +b[2], +b[3]]);
    else if (b[0] === "vt") vt.push([+b[1], +b[2]]);
    else if (b[0] === "f") {
      const c = b.slice(1).map((s) => {
        const [a, t] = s.split("/");
        return { v: +a - 1, t: t ? +t - 1 : -1 };
      });
      for (let i = 1; i + 1 < c.length; i++) tris.push([c[0], c[i], c[i + 1]]);
    }
  }
  return { v, vt, tris };
}

function bounds(v) {
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const p of v) for (let i = 0; i < 3; i++) {
    if (p[i] < lo[i]) lo[i] = p[i];
    if (p[i] > hi[i]) hi[i] = p[i];
  }
  return { lo, hi, size: [hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]],
           mid: [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2] };
}

/// Roblox applies Orientation as Y, then X, then Z.
function rotate(p, deg) {
  const [rx, ry, rz] = deg.map((d) => (d * Math.PI) / 180);
  let [x, y, z] = p;
  let c = Math.cos(ry), s = Math.sin(ry);
  [x, z] = [x * c + z * s, -x * s + z * c];
  c = Math.cos(rx); s = Math.sin(rx);
  [y, z] = [y * c - z * s, y * s + z * c];
  c = Math.cos(rz); s = Math.sin(rz);
  [x, y] = [x * c - y * s, x * s + y * c];
  return [x, y, z];
}

function draw(c, model, files, opts = {}) {
  const key = opts.key;

  const cache = {};
  const meshFor = (name) => {
    if (cache[name]) return cache[name];
    const file = files[name];
    if (!file) return null;
    const full = path.join(MODELS, file);
    if (!fs.existsSync(full) || !file.endsWith(".obj")) return null;   // GLB not parsed here
    cache[name] = parseObj(fs.readFileSync(full, "utf8"));
    return cache[name];
  };
  // The picture the part's TextureID names, falling back to the model's face.
  const pictures = {};
  const pictureFor = (name) => {
    const file = files[name] || files.face;
    if (!file || !file.endsWith(".png") || !fs.existsSync(path.join(MODELS, file))) return null;
    return (pictures[file] ||= decode(fs.readFileSync(path.join(MODELS, file))));
  };

  // A MeshPart's mesh is recentred on its bounding box and stretched to fill Size, per axis.
  const world = [];
  const walk = (n) => {
    const p = n.properties || {};
    if (n.className === "MeshPart" && p.MeshId) {
      const mesh = meshFor(p.MeshId);
      if (mesh) {
        const b = bounds(mesh.v);
        const fit = [0, 1, 2].map((i) => (b.size[i] > 1e-6 ? p.Size[i] / b.size[i] : 1));
        const rot = p.Orientation || [0, 0, 0];
        const texture = p.TextureID ? pictureFor(p.TextureID) : null;
        const textured = !!texture;
        const colour = (p.Color && p.Color.Color3uint8) || [255, 255, 255];
        for (const tri of mesh.tris) {
          world.push({
            pts: tri.map((c) => {
              const src = mesh.v[c.v];
              const local = [0, 1, 2].map((i) => (src[i] - b.mid[i]) * fit[i]);
              const r = rotate(local, rot);
              return [r[0] + p.Position[0], r[1] + p.Position[1], r[2] + p.Position[2]];
            }),
            uv: tri.map((c) => (c.t >= 0 ? mesh.vt[c.t] : null)),
            textured, texture, colour,
            alpha: 1 - (p.Transparency || 0),
          });
        }
      }
    }
    (n.children || []).forEach(walk);
  };
  walk(model);
  if (world.length === 0) throw new Error("nothing to draw (GLB meshes are not parsed here)");

  // opts.back looks from behind: x mirrors, and the depth test flips with it.
  const all = [];
  for (const t of world) all.push(...t.pts);
  const b = bounds(all);
  const S = c.w, pad = opts.pad === undefined ? 24 : opts.pad;
  const scale = Math.min((S - pad * 2) / b.size[0], (c.h - pad * 2) / b.size[1]);
  const flip = opts.back ? -1 : 1;
  const sx = (x) => S / 2 + (x - b.mid[0]) * scale * flip;
  const sy = (y) => c.h / 2 - (y - b.mid[1]) * scale;

  if (opts.background) c.fill(0, 0, c.w, c.h, () => opts.background);
  const depth = new Float64Array(c.w * c.h).fill(Infinity);

  for (const tri of world) {
    const xs = tri.pts.map((p) => sx(p[0])), ys = tri.pts.map((p) => sy(p[1]));
    const zs = tri.pts.map((p) => p[2] * flip);
    const x0 = Math.max(0, Math.floor(Math.min(...xs))), x1 = Math.min(S - 1, Math.ceil(Math.max(...xs)));
    const y0 = Math.max(0, Math.floor(Math.min(...ys))), y1 = Math.min(S - 1, Math.ceil(Math.max(...ys)));
    const area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0]);
    if (Math.abs(area) < 1e-9) continue;
    for (let y = y0; y <= y1; y++) {
      for (let x = x0; x <= x1; x++) {
        const w1 = ((x - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (y - ys[0])) / area;
        const w2 = ((xs[1] - xs[0]) * (y - ys[0]) - (x - xs[0]) * (ys[1] - ys[0])) / area;
        const w0 = 1 - w1 - w2;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        const z = zs[0] * w0 + zs[1] * w1 + zs[2] * w2;
        const at = y * c.w + x;
        if (z >= depth[at]) continue;
        let rgb = tri.colour;
        if (tri.textured && tri.uv[0] && tri.uv[1] && tri.uv[2]) {
          const texture = tri.texture;
          const u = tri.uv[0][0] * w0 + tri.uv[1][0] * w1 + tri.uv[2][0] * w2;
          const vv = tri.uv[0][1] * w0 + tri.uv[1][1] * w1 + tri.uv[2][1] * w2;
          const tx = Math.min(texture.w - 1, Math.max(0, Math.round(u * (texture.w - 1))));
          const ty = Math.min(texture.h - 1, Math.max(0, Math.round((1 - vv) * (texture.h - 1))));
          const i = (ty * texture.w + tx) * 4;
          rgb = [texture.px[i], texture.px[i + 1], texture.px[i + 2]];
        }
        if (tri.alpha < 1) {
          const bg = c.px.slice((at) * 4, at * 4 + 3);
          rgb = rgb.map((v, i) => Math.round(v * tri.alpha + bg[i] * (1 - tri.alpha)));
        } else {
          depth[at] = z;
        }
        c.set(x, y, rgb);
      }
    }
  }
  return { triangles: world.length };
}


module.exports = { draw, parseObj, bounds, MODELS };
