// What a generated catalogue model must never carry: node --test scripts/test/catalogue.test.js
//
// Properties of the generator's output, checked offline -- nothing is published.
const test = require("node:test");
const assert = require("node:assert");
const { ITEMS } = require("../catalogue.js");

/** Every Part named Handle anywhere in a model, with the path that reached it. */
function handles(node, path = "") {
  const found = [];
  for (const kid of node.children || []) {
    const at = `${path}/${kid.name || kid.className}`;
    if (kid.className === "Part" && kid.name === "Handle") found.push({ at, part: kid });
    found.push(...handles(kid, at));
  }
  return found;
}

// Roblox places a worn Handle by its attachment alone and discards the Handle's own
// Orientation, but places the siblings relative to it including that orientation -- so a turned
// Handle cancels out and the accessory renders unturned. accessory() keeps an untuned speck in
// front as the Handle.
test("no accessory's Handle carries an Orientation", () => {
  const turned = [];
  let checked = 0;
  for (const item of ITEMS) {
    if (typeof item.model !== "function") continue;
    let model;
    // A model that requires texture refs throws on {}; not this test's business.
    try { model = item.model({}); } catch { continue; }
    for (const { at, part } of handles(model)) {
      checked++;
      if (part.properties && part.properties.Orientation)
        turned.push(`${item.key}${at}: ${JSON.stringify(part.properties.Orientation)}`);
    }
  }
  assert.ok(checked > 20, `expected a catalogue's worth of handles, found ${checked}`);
  assert.deepStrictEqual(turned, [],
    `a turned Handle renders its whole accessory unturned:\n  ${turned.join("\n  ")}`);
});

test("every accessory's Handle carries the Attachment it hangs from", () => {
  const missing = [];
  for (const item of ITEMS) {
    if (typeof item.model !== "function" || item.kind !== "accessory") continue;
    let model;
    try { model = item.model({}); } catch { continue; }
    // accessory() names the first Part Handle.
    const first = (model.children || []).find((k) => k.className === "Part");
    if (!first) continue;
    if (!(first.children || []).some((k) => k.className === "Attachment"))
      missing.push(`${item.key}/${first.name}`);
  }
  assert.deepStrictEqual(missing, [],
    `a Handle with no Attachment has nothing to hang on:\n  ${missing.join("\n  ")}`);
});

// The engine finds a Handle by walking entries_, an unordered_map: with two of them, which one
// the body holds is arbitrary. accessory() renaming the first Part can make a second.
test("no accessory has two parts called Handle", () => {
  const doubled = [];
  for (const item of ITEMS) {
    if (typeof item.model !== "function") continue;
    let model;
    try { model = item.model({}); } catch { continue; }
    const named = (model.children || []).filter((k) => k.className === "Part" && k.name === "Handle");
    if (named.length > 1) doubled.push(`${item.key}: ${named.length}`);
  }
  assert.deepStrictEqual(doubled, [], "two Handles is a coin flip over which one the body holds");
});
