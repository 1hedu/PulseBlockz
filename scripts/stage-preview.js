// Writes catalogue items to a directory as the engine wants them, so they can be worn in the
// running game without a chain. A published item's model names its textures and meshes by
// pblockz:// URI and the host swaps those for cached files; this writes the files and the paths.
const fs = require("fs");
const path = require("path");
const { ITEMS, TEXTURES, textureImage } = require("./catalogue");

const [, , which, outDir] = process.argv;
if (!which || !outDir) {
  console.error("usage: node scripts/stage-preview.js <key,key|all> <dir>");
  process.exit(1);
}
fs.mkdirSync(outDir, { recursive: true });

const keys = which === "all" ? ITEMS.map((i) => i.key) : which.split(",");

const wanted = new Set();
for (const k of keys) {
  const item = ITEMS.find((i) => i.key === k);
  if (!item) throw new Error("no catalogue item " + k);
  for (const t of item.uses || []) wanted.add(t);
}
const texPath = {};
for (const name of wanted) {
  const file = path.join(outDir, `tex-${name}.png`);
  fs.writeFileSync(file, textureImage(name));
  texPath[name] = file.replace(/\\/g, "/");
}

const manifest = [];
for (const k of keys) {
  const item = ITEMS.find((i) => i.key === k);
  const refs = { ...texPath };
  for (const [name, make] of Object.entries(item.images || {})) {
    const file = path.join(outDir, `${k}-${name}.png`);
    fs.writeFileSync(file, make());
    refs[name] = file.replace(/\\/g, "/");
  }
  for (const [name, make] of Object.entries(item.assets || {})) {
    const a = make();
    const ext = a.mime.includes("png") ? "png" : a.mime.includes("gltf-binary") ? "glb" : "obj";
    const file = path.join(outDir, `${k}-${name}.${ext}`);
    fs.writeFileSync(file, a.bytes);
    refs[name] = file.replace(/\\/g, "/");
  }
  const model = item.model(refs);
  fs.writeFileSync(path.join(outDir, `${k}.json`), JSON.stringify(model));
  // instance is the name the model gives itself, which is what add_model uses; it is not always
  // the catalogue key. previously is the name the item was published under: an inventory is chain
  // data, so a renamed item is still listed there under the old name.
  manifest.push({ key: k, name: item.name, slot: item.slot || "", kind: item.kind || "accessory",
                  previously: item.previously || "",
                  instance: (model.properties && model.properties.Name) || model.name || k });
}

fs.writeFileSync(path.join(outDir, "manifest.json"), JSON.stringify(manifest, null, 1));
console.log(`staged ${manifest.length} item(s) and ${wanted.size} texture(s) -> ${outDir}`);
for (const m of manifest) console.log(`  ${m.key.padEnd(13)} ${m.name}`);
