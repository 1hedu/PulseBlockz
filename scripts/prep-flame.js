// One tongue of flame: Roblox's Fire is this one sprite drawn a few hundred times over.
//
//   node scripts/prep-flame.js
//
// White, because a ParticleEmitter's Color multiplies through it -- one sprite serves every
// fire. The engine falls back to a soft round spot for a texture it cannot load, and a
// blurred circle reads as bubbles at any rate or squash.
const fs = require("fs");
const path = require("path");
const { Canvas } = require("./png");

const SIZE = 128;

const c = new Canvas(SIZE, SIZE);
for (let y = 0; y < SIZE; y++) {
  // 0 at the tip, 1 at the base.
  const v = 1 - y / (SIZE - 1);
  for (let x = 0; x < SIZE; x++) {
    const u = (x / (SIZE - 1)) * 2 - 1;          // -1..1 across
    // Half-width at this height. The exponent gives a waist; a linear taper is a triangle.
    const width = Math.pow(Math.max(0, 1 - v), 0.65);
    if (width <= 0.001) continue;
    const across = Math.abs(u) / width;
    if (across >= 1) continue;
    const body = Math.pow(1 - across * across, 1.6);
    // Eased off at the very base, so the sprite has no hard horizontal edge.
    const along = Math.min(1, (1 - v) * 6) * (0.55 + 0.45 * Math.pow(v, 0.5));
    const a = Math.max(0, Math.min(1, body * along));
    if (a <= 0.004) continue;
    // A hotter core, so overlapping tongues build a bright centre.
    const core = Math.pow(body, 3);
    const white = 235 + Math.round(20 * core);
    c.set(x, y, [white, white, white, Math.round(a * 255)]);
  }
}
const png = c.toPNG();
fs.mkdirSync(path.join(__dirname, "models"), { recursive: true });
fs.writeFileSync(path.join(__dirname, "models", "flame.png"), png);
console.log("flame.png  %d square, %d bytes", SIZE, png.length);
