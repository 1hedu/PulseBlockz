// Writes the town's mesh props into its map, as models rather than as scripts.
//
//   node scripts/make-props.js
//
// The host resolves a pblockz:// content hash on the way into a place's tree, so a prop can name
// its mesh by hash in the map rather than be assembled by a server script at run time.
//
// The URIs come out of place-assets.json, written by publish-place-assets.js, so the order is:
// publish the meshes, run this, publish the experience.
const fs = require("fs");
const path = require("path");

const ROOT = path.join(__dirname, "..");
const MANIFEST = path.join(ROOT, "luau", "gdextension", "demo2", "place-assets.json");
const MAP = path.join(ROOT, "luau", "gdextension", "demo2", "scripts", "src", "map");
const MODELS = path.join(__dirname, "models");

const assets = JSON.parse(fs.readFileSync(MANIFEST, "utf8"));

const meshPart = (name, props) => ({ className: "MeshPart", name, properties: props });

const PROPS = {
  Rocket: () => {
    // What the mesh itself measures; the prop stands at three times it.
    const size = [4.04, 7.04, 2.635].map((n) => +(n * 3).toFixed(4));
    return meshPart("Rocket", {
      Anchored: true, CanCollide: true, Material: "SmoothPlastic",
      // Position is the part's centre, so half the height stands it on the ground.
      Position: [-49.769, +(size[1] / 2).toFixed(4), -48.526],
      Orientation: [0, 35, 0],
      Size: size,
      MeshId: assets.RocketMesh,
      TextureID: assets.RocketColors,
    });
  },

  Tree: () => {
    // Two meshes, cut apart by prep-tree.js: Material is per-part, and the canopy is Neon.
    const box = JSON.parse(fs.readFileSync(path.join(MODELS, "tree.json"), "utf8"));
    const SCALE = 2;                       // twice what the mesh measures
    const at = [40.787, 0, 40.448];
    const piece = (name, part, asset, material) => meshPart(name, {
      Anchored: true, CanCollide: true, Material: material,
      Position: [0, 1, 2].map((i) => +(at[i] + part.mid[i] * SCALE).toFixed(4)),
      Orientation: [0, 20, 0],
      Size: part.size.map((n) => +(n * SCALE).toFixed(4)),
      MeshId: assets[asset],
      TextureID: assets.TreeColors,
    });
    return {
      className: "Model", name: "Tree",
      children: [
        piece("Trunk", box.trunk, "TreeTrunkMesh", "Wood"),
        piece("Canopy", box.canopy, "TreeCanopyMesh", "Neon"),
      ],
    };
  },
};

/** Property names whose MeshId or TextureID is absent or not a pblockz:// URI. The host draws a
 *  part whose MeshId it cannot resolve as a grey box, so a prop with any of these is not written. */
function missing(node, out = []) {
  if (Array.isArray(node)) { for (const n of node) missing(n, out); return out; }
  if (node && typeof node === "object") {
    for (const [k, v] of Object.entries(node)) {
      if (v === undefined || (typeof v === "string" && (v.startsWith("pblockz://")) === false && /^(MeshId|TextureID)$/.test(k))) out.push(k);
      else missing(v, out);
    }
  }
  return out;
}

let wrote = 0;
for (const [name, build] of Object.entries(PROPS)) {
  const doc = build();
  const gaps = missing(doc);
  if (gaps.length) {
    console.error(`${name}: skipped -- ${gaps.join(", ")} not published yet`);
    continue;
  }
  const file = path.join(MAP, `${name}.model.json`);
  fs.writeFileSync(file, JSON.stringify(doc, null, 2) + "\n");
  console.log(`${name.padEnd(8)} -> src/map/${name}.model.json`);
  wrote++;
}
console.log(`\n${wrote} of ${Object.keys(PROPS).length} prop(s) written into the map.`);
