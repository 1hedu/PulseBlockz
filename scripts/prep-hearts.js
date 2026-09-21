// The heart the health meter is drawn from: the Heart Cape's heart, drawn by
// scripts/capes.js, so the two cannot drift apart.
//
//   node scripts/prep-hearts.js
//
// White, because an ImageLabel's ImageColor3 multiplies: one asset covers full, empty and
// every streak colour. The shape shades itself -- outline at 45% of the colour it is given,
// glint in the top-left -- so tinting keeps the outline dark and the glint bright.
const fs = require("fs");
const path = require("path");
const { Canvas } = require("./png");
const { heart } = require("./capes");

const SCALE = 8;                 // the grid is 7 across, so this lands on 56 of 64
const SIZE = 64;

const c = new Canvas(SIZE, SIZE);
heart(c, SIZE / 2, SIZE / 2, SCALE, [255, 255, 255]);
const png = c.toPNG();
fs.mkdirSync(path.join(__dirname, "models"), { recursive: true });
fs.writeFileSync(path.join(__dirname, "models", "heart.png"), png);
console.log("heart.png  %d square, %d bytes", SIZE, png.length);
