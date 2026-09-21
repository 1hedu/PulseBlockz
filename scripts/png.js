// RGBA raster and PNG encoder for this repo's item art, no dependencies. Assets are
// content-addressed, so the same drawing must produce the same bytes every run: nothing here
// uses Math.random, and noise() hashes its coordinates instead.
const zlib = require("zlib");

/** 8-bit RGBA, origin top-left. Colours are [r, g, b] or [r, g, b, a], 0-255. */
class Canvas {
  constructor(w, h = w) {
    this.w = w;
    this.h = h;
    this.px = Buffer.alloc(w * h * 4, 0);
  }

  /** Alpha-blends the colour over what is already there. Off-canvas writes are dropped. */
  set(x, y, c) {
    x |= 0; y |= 0;
    if (x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    const a = (c[3] === undefined ? 255 : c[3]) / 255;
    if (a <= 0) return;
    const i = (y * this.w + x) * 4;
    const was = this.px[i + 3] / 255;
    const out = a + was * (1 - a);
    for (let k = 0; k < 3; k++) this.px[i + k] = Math.round((c[k] * a + this.px[i + k] * was * (1 - a)) / out);
    this.px[i + 3] = Math.round(out * 255);
  }

  /** Forces a region to zero alpha. `set` blends, so painting transparent does nothing. */
  clear(x0, y0, x1, y1) {
    for (let y = Math.floor(y0); y < Math.ceil(y1); y++)
      for (let x = Math.floor(x0); x < Math.ceil(x1); x++) {
        if (x < 0 || y < 0 || x >= this.w || y >= this.h) continue;
        this.px.writeUInt32LE(0, (y * this.w + x) * 4);
      }
  }

  /** Half-open rectangle [x0, x1) x [y0, y1). */
  rect(x0, y0, x1, y1, c) {
    for (let y = Math.floor(y0); y < Math.ceil(y1); y++)
      for (let x = Math.floor(x0); x < Math.ceil(x1); x++) this.set(x, y, c);
  }

  /** Per-pixel rectangle: `fn(x, y, u, v)` gets absolute and 0..1-within-rect coordinates, null skips. */
  fill(x0, y0, x1, y1, fn) {
    const w = x1 - x0, h = y1 - y0;
    for (let y = Math.floor(y0); y < Math.ceil(y1); y++)
      for (let x = Math.floor(x0); x < Math.ceil(x1); x++) {
        const c = fn(x, y, w > 0 ? (x - x0) / w : 0, h > 0 ? (y - y0) / h : 0);
        if (c) this.set(x, y, c);
      }
  }

  /** Filled ellipse. A function colour gets (x, y, u, v), u and v 0..1 across the bounding box. */
  ellipse(cx, cy, rx, ry, c) {
    for (let y = Math.floor(cy - ry); y <= Math.ceil(cy + ry); y++)
      for (let x = Math.floor(cx - rx); x <= Math.ceil(cx + rx); x++) {
        const dx = (x + 0.5 - cx) / rx, dy = (y + 0.5 - cy) / ry;
        if (dx * dx + dy * dy <= 1) this.set(x, y, typeof c === "function" ? c(x, y, (dx + 1) / 2, (dy + 1) / 2) : c);
      }
  }

  tri(ax, ay, bx, by, cx, cy, c) {
    const minX = Math.floor(Math.min(ax, bx, cx)), maxX = Math.ceil(Math.max(ax, bx, cx));
    const minY = Math.floor(Math.min(ay, by, cy)), maxY = Math.ceil(Math.max(ay, by, cy));
    const side = (px, py, x0, y0, x1, y1) => (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
    for (let y = minY; y <= maxY; y++)
      for (let x = minX; x <= maxX; x++) {
        const p = x + 0.5, q = y + 0.5;
        const d1 = side(p, q, ax, ay, bx, by), d2 = side(p, q, bx, by, cx, cy), d3 = side(p, q, cx, cy, ax, ay);
        const neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
        if (!(neg && pos)) this.set(x, y, c);
      }
  }

  toPNG() {
    return encode(this.px, this.w, this.h);
  }
}

// A 5x7 bitmap font: the glyphs the wordmarks and cape numbers need, and no others.
const FONT = {
  "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
  "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
  "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
  "3": ["11111", "00010", "00100", "00010", "00001", "10001", "01110"],
  "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
  "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
  "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
  "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
  "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
  "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
  A: ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
  C: ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
  D: ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
  E: ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
  H: ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
  I: ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
  N: ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
  P: ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
  X: ["10001", "01010", "00100", "00100", "00100", "01010", "10001"],
  " ": ["00000", "00000", "00000", "00000", "00000", "00000", "00000"],
};

/** Pixel width of `str` at `scale`, including the one-pixel gap between glyphs. */
function textWidth(str, scale) {
  return str.length === 0 ? 0 : (str.length * 6 - 1) * scale;
}

/** Draws `str` with its top-left at (x, y). `color` may be a colour or (x, y) => colour. */
function text(c, str, x, y, scale, color) {
  let at = x;
  for (const ch of str.toUpperCase()) {
    const glyph = FONT[ch];
    if (glyph) {
      for (let row = 0; row < glyph.length; row++)
        for (let col = 0; col < glyph[row].length; col++)
          if (glyph[row][col] === "1")
            for (let dy = 0; dy < scale; dy++)
              for (let dx = 0; dx < scale; dx++) {
                const px = at + col * scale + dx, py = y + row * scale + dy;
                c.set(px, py, typeof color === "function" ? color(px, py) : color);
              }
    }
    at += 6 * scale;
  }
  return at - x - scale;
}

/** Scaled blit, averaging the source box. Alpha is premultiplied through the average, so
 * shrinking pulls no dark fringe out of the transparent pixels around the art. */
function blit(dst, src, x0, y0, w, h) {
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const sx0 = Math.floor((x * src.w) / w), sx1 = Math.max(sx0 + 1, Math.floor(((x + 1) * src.w) / w));
      const sy0 = Math.floor((y * src.h) / h), sy1 = Math.max(sy0 + 1, Math.floor(((y + 1) * src.h) / h));
      let r = 0, g = 0, b = 0, a = 0, n = 0;
      for (let sy = sy0; sy < sy1; sy++)
        for (let sx = sx0; sx < sx1; sx++) {
          const i = (sy * src.w + sx) * 4, al = src.px[i + 3] / 255;
          r += src.px[i] * al; g += src.px[i + 1] * al; b += src.px[i + 2] * al;
          a += al; n++;
        }
      if (n === 0 || a === 0) continue;
      dst.set(x0 + x, y0 + y, [Math.round(r / a), Math.round(g / a), Math.round(b / a), Math.round((255 * a) / n)]);
    }
}

/** Deterministic value noise in [0, 1): a hash of (x, y, salt), not a PRNG. */
function noise(x, y, salt = 0) {
  let h = (x | 0) * 374761393 + (y | 0) * 668265263 + (salt | 0) * 2246822519;
  h = (h ^ (h >>> 13)) >>> 0;
  h = Math.imul(h, 1274126177) >>> 0;
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

function mix(a, b, t) {
  const u = Math.max(0, Math.min(1, t));
  return [0, 1, 2].map((k) => Math.round(a[k] + (b[k] - a[k]) * u));
}

/** Samples a list of {t, c} stops. Plain sRGB, unlike the engine's OKLab skin ramp. */
function ramp(stops, t) {
  const u = Math.max(0, Math.min(1, t));
  for (let i = 0; i < stops.length - 1; i++) {
    const a = stops[i], b = stops[i + 1];
    if (u >= a.t && u <= b.t) return mix(a.c, b.c, b.t === a.t ? 0 : (u - a.t) / (b.t - a.t));
  }
  return stops[u <= stops[0].t ? 0 : stops.length - 1].c;
}

function shade(c, f) {
  return [0, 1, 2].map((k) => Math.max(0, Math.min(255, Math.round(c[k] * f))));
}

/** PNG into a Canvas. 8-bit non-interlaced greyscale, RGB or RGBA only; anything else throws.
 * Art loaded from a file shrinks through the same path as art drawn here, so a logo's cape
 * texture (capes.js) and its thumbnail (catalogue.js) cannot disagree. */
function decode(png) {
  if (png.readUInt32BE(0) !== 0x89504e47) throw new Error("not a PNG");
  let at = 8, ihdr = null;
  const idat = [];
  while (at < png.length) {
    const len = png.readUInt32BE(at);
    const type = png.toString("ascii", at + 4, at + 8);
    const body = png.subarray(at + 8, at + 8 + len);
    if (type === "IHDR") ihdr = { w: body.readUInt32BE(0), h: body.readUInt32BE(4), depth: body[8], colour: body[9], interlace: body[12] };
    else if (type === "IDAT") idat.push(body);
    else if (type === "IEND") break;
    at += len + 12;                                  // length + type + data + crc
  }
  if (!ihdr) throw new Error("no IHDR");
  if (ihdr.depth !== 8 || ihdr.interlace !== 0) throw new Error("only 8-bit, non-interlaced PNGs");
  const channels = { 0: 1, 2: 3, 6: 4 }[ihdr.colour];
  if (!channels) throw new Error("unsupported colour type " + ihdr.colour);

  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = ihdr.w * channels;
  const out = new Canvas(ihdr.w, ihdr.h);
  const line = Buffer.alloc(stride);
  const prev = Buffer.alloc(stride);
  for (let y = 0; y < ihdr.h; y++) {
    const filter = raw[y * (stride + 1)];
    raw.copy(line, 0, y * (stride + 1) + 1, (y + 1) * (stride + 1));
    // Undo the row filter: a is the byte one pixel to the left, b the one above, c above-left.
    for (let i = 0; i < stride; i++) {
      const a = i >= channels ? line[i - channels] : 0;
      const b = prev[i];
      const c = i >= channels ? prev[i - channels] : 0;
      let v = line[i];
      if (filter === 1) v += a;
      else if (filter === 2) v += b;
      else if (filter === 3) v += (a + b) >> 1;
      else if (filter === 4) {
        const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      line[i] = v & 0xff;
    }
    for (let x = 0; x < ihdr.w; x++) {
      const i = x * channels;
      const px = channels === 1 ? [line[i], line[i], line[i], 255]
        : channels === 3 ? [line[i], line[i + 1], line[i + 2], 255]
        : [line[i], line[i + 1], line[i + 2], line[i + 3]];
      const o = (y * ihdr.w + x) * 4;
      out.px[o] = px[0]; out.px[o + 1] = px[1]; out.px[o + 2] = px[2]; out.px[o + 3] = px[3];
    }
    line.copy(prev);
  }
  return out;
}

// ---- PNG encoding -------------------------------------------------------------------
let TBL = null;
function crc32(buf) {
  if (!TBL) {
    TBL = new Int32Array(256);
    for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; TBL[n] = c; }
  }
  let c = -1;
  for (const b of buf) c = TBL[(c ^ b) & 0xff] ^ (c >>> 8);
  return c ^ -1;
}

function encode(px, w, h) {
  const stride = w * 4;
  const raw = Buffer.alloc((stride + 1) * h);
  for (let y = 0; y < h; y++) {
    raw[y * (stride + 1)] = 0;                       // filter 0 (None), so the bytes are predictable
    px.copy(raw, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
  }
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const body = Buffer.concat([Buffer.from(type, "ascii"), data]);
    const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(body) >>> 0);
    return Buffer.concat([len, body, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 6;                          // 8 bits per channel, colour type 6 (RGBA)
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk("IHDR", ihdr),
    // A fixed level, never tuned per image: the bytes have to be the same every run.
    chunk("IDAT", zlib.deflateSync(raw, { level: 9 })),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}

module.exports = { Canvas, noise, mix, ramp, shade, encode, decode, text, textWidth, blit, FONT };
