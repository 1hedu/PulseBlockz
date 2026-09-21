// Orthographic views of an accessory's actual parts, against the head it hangs off. Shared by
// scripts/preview-model.js and by catalogue thumbnails, so a thumbnail cannot disagree with the
// geometry it is drawn from.
const { shade } = require("./png");

const SKIN = [214, 176, 140];

/// The head in worn space: the origin is the top of the skull and the head hangs below it,
/// one stud across at the crown and two at its widest.
const HEAD = { top: 0, bottom: -1.73, widestY: -0.87, crown: 0.5, widest: 1.0, depth: 0.5 };

function headHalfWidth(y) {
  if (y > HEAD.top || y < HEAD.bottom) return 0;
  if (y >= HEAD.widestY) {
    const t = (HEAD.top - y) / (HEAD.top - HEAD.widestY);
    return HEAD.crown + (HEAD.widest - HEAD.crown) * t;
  }
  const t = (y - HEAD.bottom) / (HEAD.widestY - HEAD.bottom);
  return HEAD.crown + (HEAD.widest - HEAD.crown) * t;
}

function colourOf(node) {
  const c = node.properties && node.properties.Color;
  return c && Array.isArray(c.Color3uint8) ? c.Color3uint8 : [200, 200, 200];
}

function partsOf(model) {
  const out = [];
  const walk = (n) => {
    const p = n.properties || {};
    if (p.Position && p.Size)
      out.push({ name: n.name, pos: p.Position, size: p.Size,
        rot: p.Orientation || [0, 0, 0], colour: colourOf(n) });
    (n.children || []).forEach(walk);
  };
  walk(model);
  return out;
}

const VIEWS = {
  // Facing someone, their right hand is on your left: the front view negates worn-space x so
  // that it matches what the game shows.
  front: { hx: (p) => -p.pos[0], hw: (p) => p.size[0], depth: (p) => p.pos[2] },
  side: { hx: (p) => -p.pos[2], hw: (p) => p.size[2], depth: (p) => -Math.abs(p.pos[0]) },
};

/// Paints `model` into `c` in place, scaled to fit. `opts`: pad in pixels, head (draw the skull
/// behind it -- off for anything not worn on the head), view "front" or "side".
function draw(c, model, opts = {}) {
  const view = VIEWS[opts.view || "front"];
  const pad = opts.pad === undefined ? 3 : opts.pad;
  const withHead = opts.head !== false;
  const parts = partsOf(model);
  if (parts.length === 0) return;

  // The skull is inside the bounds, so a long accessory is not cropped and a short one not lost.
  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  const see = (x, y) => {
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  };
  for (const p of parts) {
    const w = view.hw(p) / 2, h = p.size[1] / 2;
    see(view.hx(p) - w, p.pos[1] - h);
    see(view.hx(p) + w, p.pos[1] + h);
  }
  if (withHead) {
    const half = opts.view === "side" ? HEAD.depth : HEAD.widest;
    see(-half, HEAD.bottom);
    see(half, HEAD.top);
  }

  const scale = Math.min((c.w - pad * 2) / (maxX - minX), (c.h - pad * 2) / (maxY - minY));
  const ox = pad + (c.w - pad * 2 - (maxX - minX) * scale) / 2;
  const oy = pad + (c.h - pad * 2 - (maxY - minY) * scale) / 2;
  const sx = (x) => ox + (x - minX) * scale;
  const sy = (y) => oy + (maxY - y) * scale;

  if (withHead) {
    for (let py = 0; py < c.h; py++) {
      const y = maxY - (py - oy) / scale;
      const half = opts.view === "side"
        ? (y <= HEAD.top && y >= HEAD.bottom ? HEAD.depth : 0)
        : headHalfWidth(y);
      if (half <= 0) continue;
      for (let px = Math.round(sx(-half)); px <= Math.round(sx(half)); px++) c.set(px, py, SKIN);
    }
  }

  // Back to front, and the skull occludes any part that does not reach in front of it: hair
  // hanging down the back must not paint over the face.
  const order = [...parts].sort((a, b) => view.depth(b) - view.depth(a));
  for (const p of order) {
    const w = view.hw(p) * scale, h = p.size[1] * scale;
    const x0 = sx(view.hx(p)) - w / 2;
    const y0 = sy(p.pos[1]) - h / 2;
    const lean = ((opts.view || "front") === "front" ? -(p.rot[2] || 0) : 0) * Math.PI / 180;
    const inFront = (opts.view || "front") === "front"
      ? p.pos[2] - p.size[2] / 2 < -HEAD.depth + 0.001
      : true;
    for (let dy = 0; dy < h; dy++) {
      const shift = Math.tan(lean) * (dy - h / 2);
      for (let dx = 0; dx < w; dx++) {
        const px = Math.round(x0 + dx + shift), py = Math.round(y0 + dy);
        if (px < 0 || px >= c.w || py < 0 || py >= c.h) continue;
        if (withHead && !inFront) {
          const y = maxY - (py - oy) / scale;
          const half = headHalfWidth(y);
          if (px > sx(-half) && px < sx(half)) continue;
        }
        c.set(px, py, shade(p.colour, 1.06 - 0.22 * (dy / Math.max(1, h))));
      }
    }
  }
}

module.exports = { draw, partsOf, headHalfWidth, HEAD, SKIN };
