// Publishes an experience's code to the chain and prints the content hash that names it. A
// Script's Source is NoReplicate, so a server cannot hand a client the code it runs: the
// client fetches the published place by hash instead.
//
//   node scripts/publish-experience.js luau/gdextension/demo2/scripts "PulseBlockz Town" [--dry]
//
// The manifest holds the flattened tree, not the Rojo project: `$path` is resolved here, so a
// client verifies (instance path, content hash) pairs without reading a project file.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");

const ROOT = path.join(__dirname, "..");
const RPC = process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com";
const CHAIN_ID = 943;

// Suffixes ScriptSync keeps on an instance's name, longest first so ".server.luau" wins over
// ".luau"; the matched suffix stays in the manifest path.
const SUFFIXES = [".server.luau", ".server.lua", ".client.luau", ".client.lua",
                  ".luau", ".lua", ".model.json", ".meta.json", ".rbxmx", ".rbxm"];

const suffixOf = (file) => SUFFIXES.find((s) => file.endsWith(s)) || "";

function walk(base, rel = "") {
  const out = [];
  const here = rel ? path.join(base, rel) : base;
  if (!fs.existsSync(here)) return out;
  for (const entry of fs.readdirSync(here, { withFileTypes: true })) {
    const child = rel ? path.join(rel, entry.name) : entry.name;
    if (entry.isDirectory()) out.push(...walk(base, child));
    else if (suffixOf(entry.name)) out.push(child.split(path.sep).join("/"));
  }
  return out;
}

/** The meta.json a project node describes, or "" when it says nothing. */
function nodeMeta(node) {
  const meta = {};
  if (node.$className) meta.className = node.$className;
  if (node.$properties) meta.properties = node.$properties;
  if (node.$attributes) meta.attributes = node.$attributes;
  return Object.keys(meta).length ? JSON.stringify(meta) : "";
}

/**
 * Rojo project -> `instance path -> {file} | {inline}`, mounted the way ScriptSync.gd does:
 * where the two disagree, a client mounts a tree other than the published one.
 */
function mount(node, inst, dir, files) {
  if (!node || typeof node !== "object") return;
  let isFile = false;
  if (typeof node.$path === "string") {
    const disk = path.join(dir, node.$path);
    if (fs.existsSync(disk) && fs.statSync(disk).isFile()) {
      files[inst + suffixOf(disk)] = { file: disk };
      isFile = true;
    } else {
      for (const rel of walk(disk)) files[inst ? `${inst}/${rel}` : rel] = { file: path.join(disk, rel) };
    }
  }
  if (inst) {
    const meta = nodeMeta(node);
    if (meta) {
      const name = isFile ? `${inst}.meta.json` : `${inst}/init.meta.json`;
      if (!files[name]) files[name] = { inline: meta };
    }
  }
  for (const key of Object.keys(node)) {
    if (!key.startsWith("$")) mount(node[key], inst ? `${inst}/${key}` : key, dir, files);
  }
}

function resolve(dir) {
  const files = {};
  const project = path.join(dir, "default.project.json");
  if (fs.existsSync(project)) {
    const tree = JSON.parse(fs.readFileSync(project, "utf8"));
    if (!tree.tree) throw new Error(`${project} has no tree`);
    mount(tree.tree, "", dir, files);
  } else {
    for (const rel of walk(dir)) files[rel] = { file: path.join(dir, rel) };
  }
  return files;
}

function flag(which) {
  const at = process.argv.indexOf(which);
  return at > 0 && process.argv[at + 1] ? process.argv[at + 1] : "";
}

async function main() {
  const dir = process.argv[2];
  const named = process.argv[3] && !process.argv[3].startsWith("--") ? process.argv[3] : "";
  const name = named || path.basename(path.dirname(dir || ""));
  if (!dir) throw new Error("usage: publish-experience.js <scripts dir> <name> [--thumbnail <img>] [--splash <img>] [--uses chain,transact,sign,pulsex,scan,market,mirror] [--description <text>] [--assets <...>] [--server host:port] [--undeclared-ok] [--contracts <Contracts.luau>] [--dry]");
  const thumbPath = flag("--thumbnail");
  const splashPath = flag("--splash");
  // The capabilities a client will grant this place; it refuses any other (host/Uses.gd).
  const USES = ["chain", "transact", "sign", "pulsex", "scan", "market", "mirror"];
  const uses = flag("--uses").split(",").map((s) => s.trim()).filter(Boolean);
  for (const u of uses) if (!USES.includes(u)) throw new Error(`--uses: "${u}" is not one of ${USES.join(", ")}`);
  if (splashPath && !fs.existsSync(splashPath)) throw new Error(`no splash at ${splashPath}`);
  const description = flag("--description");
  // Addresses read out of the module the place names them in (demo2/scripts/src/shared/
  // Contracts.luau) rather than retyped, so the manifest shows them before the place is run.
  const contractsPath = flag("--contracts");
  const declared = {};
  if (contractsPath) {
    const text = fs.readFileSync(contractsPath, "utf8");
    for (const m of text.matchAll(/^\s*(\w+)\s*=\s*"(0x[0-9a-fA-F]{40})"/gm)) declared[m[1]] = m[2];
    const chain = /^\s*ChainId\s*=\s*(\d+)/m.exec(text);
    if (chain) declared.ChainId = Number(chain[1]);
    if (Object.keys(declared).length === 0) throw new Error(`--contracts: no addresses in ${contractsPath}`);
  }
  // A real-time host:port carried in the manifest, the publisher's word and no more: what that
  // server sends a client is not checked against this hash.
  const server = flag("--server");
  if (server && !/^[A-Za-z0-9.\-]+:\d{1,5}$/.test(server)) {
    throw new Error(`--server: "${server}" is not host:port`);
  }
  // Asset uris by name, from the place's own place-assets.json beside its scripts dir. They
  // ride in the manifest, so a client fills ReplicatedStorage.PlaceAssets (host/Wallet.gd
  // publish_place_assets) with no local json.
  const assetsPath = flag("--assets");
  const placeAssets = {};
  if (assetsPath) {
    const raw = JSON.parse(fs.readFileSync(assetsPath, "utf8"));
    for (const [k, v] of Object.entries(raw)) {
      if (k.startsWith("_")) continue;
      if (typeof v !== "string" || !(v.startsWith("pblockz://"))) throw new Error(`--assets: ${k} is not a pblockz:// uri`);
      placeAssets[k] = v;
    }
  }
  if (thumbPath && !fs.existsSync(thumbPath)) throw new Error(`no thumbnail at ${thumbPath}`);

  const files = resolve(dir);
  // Published, a dev chain's overrides (scripts/dev-chain.js) point every player at contracts
  // that exist on one machine.
  const dev = Object.keys(files).find((name) => /(^|\/)ContractsDev\.lua[u]?$/.test(name));
  if (dev) {
    console.error(`refusing to publish: ${dev} is a dev chain's override. Stop the dev chain (node scripts/dev-chain.js stop) first.`);
    process.exit(1);
  }
  const paths = Object.keys(files).sort();
  if (paths.length === 0) throw new Error(`nothing to publish under ${dir}`);

  // Read up front: the manifest is one snapshot of the tree, not a crawl across a changing one.
  const contents = {};
  let total = 0;
  for (const p of paths) {
    const entry = files[p];
    const bytes = entry.inline ? Buffer.from(entry.inline) : fs.readFileSync(entry.file);
    contents[p] = bytes;
    total += bytes.length;
  }
  process.env.CHAIN = "testnet";
  process.env.ADDRESSES_FILE = path.join(ROOT, "addresses.943.json");
  // Read-only, and above the summary so --dry weighs what a player would download as well.
  const assets = require("./assets");
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(RPC, CHAIN_ID, { staticNetwork: true }));
  console.log(`${name}: ${paths.length} files, ${total} bytes of scripts, models and meta`);
  // A guess from the source, not the grant: a capability left off --uses surfaces here rather
  // than as a refusal in front of a player.
  const text = paths.filter((p) => /\.lua[u]?$/.test(p)).map((p) => contents[p].toString("utf8")).join("\n");
  const looks = new Set();
  const ACTIONS = { chain: ["read", "fetch", "who"], transact: ["write", "store"], sign: ["sign_typed"],
    pulsex: ["quote", "swap", "positions", "add_token", "forget_token", "add_liquidity", "remove_liquidity"] };
  for (const [cap, acts] of Object.entries(ACTIONS))
    if (acts.some((a) => new RegExp(`action\\s*=\\s*"${a}"`).test(text))) looks.add(cap);
  if (/AskScan/.test(text)) looks.add("scan");
  if (/AskMarket/.test(text)) looks.add("market");
  // `mirror` shows in the asset uris, not the scripts: ?src= is a fetch from off the chain.
  if (Object.values(placeAssets).some((u) => /[?&]src=/.test(u))) looks.add("mirror");
  console.log(`uses (declared): ${uses.join(", ") || "nothing"}`);
  if (server) console.log(`server:         ${server}`);
  for (const [k, v] of Object.entries(declared)) if (k !== "ChainId") console.log(`contract:       ${k.padEnd(14)} ${v}`);
  // Weighed, not guessed: the store reports a blob's size without sending it, so what a player
  // will actually download can be said here rather than discovered on their machine.
  let assetBytes = 0;
  if (assetsPath) {
    const ABI = ["function blob(uint256) view returns (address,bytes32,uint256,string,uint256)"];
    const stores = {};
    let weighed = 0;
    for (const uri of Object.values(placeAssets)) {
      // Each uri names the store it is in, so a place whose assets sit in more than one is
      // weighed the same way as one whose assets are all in ours.
      const at = (assets.parseUri(uri) || {}).chain;
      if (!at || !at.store) continue;
      stores[at.store] = stores[at.store] || new ethers.Contract(at.store, ABI, provider);
      try {
        const [, , size] = await stores[at.store].blob(at.blobId);
        assetBytes += Number(size);
        weighed++;
      } catch { /* a uri naming no blob of that store is left out of the weight */ }
    }
    console.log(`place assets: ${Object.keys(placeAssets).length} named, ${weighed} weighed, ${assetBytes} bytes`);
    console.log(`a player downloads: ${paths.length + weighed} file(s), ${total + assetBytes} bytes`);
  }
  // Refused, not warned: an undeclared capability publishes a place whose every such ask a client
  // turns down, which looks to a player like the chain being down rather than a flag left off.
  // --undeclared-ok to publish anyway, for a place that really does not want the grant.
  const missing = [...looks].filter((u) => !uses.includes(u));
  if (missing.length && !process.argv.includes("--undeclared-ok"))
    throw new Error(`the scripts use ${missing.join(", ")}, which --uses does not declare, so a client would refuse every one of those asks.
`
      + `  publish with: --uses ${[...new Set([...uses, ...missing])].join(",")}
`
      + `  or --undeclared-ok to publish without the grant.`);
  if (missing.length) console.log(`undeclared (allowed by --undeclared-ok): ${missing.join(", ")}`);
  for (const p of paths) console.log(`  ${String(contents[p].length).padStart(6)}  ${p}`);

  if (process.argv.includes("--dry")) {
    console.log("\n--dry: nothing published");
    return;
  }

  const env = Object.fromEntries(fs.readFileSync(path.join(ROOT, ".env.testnet"), "utf8").trim().split("\n").map((l) => l.split("=")));
  const creator = new ethers.NonceManager(new ethers.Wallet(env.CREATOR_KEY, provider));
  console.log("\npublisher", await creator.getAddress());

  // Bytes already in the store are not stored twice: the uri names them by hash, so a
  // republish pays only for the files that changed.
  const { contracts } = require("../bridge/src/chain");
  let reused = 0;
  const storeOnce = async (bytes, mime) => {
    const hash = ethers.keccak256(bytes);
    const id = await contracts.assetStore.blobOf(hash);
    if (id !== 0n) {
      const [, , size, kept] = await contracts.assetStore.blob(id);
      if (kept === mime && Number(size) === bytes.length) {
        reused++;
        return { uri: assets.toUri(hash, { blobId: id, mime }), reused: true };
      }
    }
    return assets.storeBytes(bytes, { mime, signer: creator });
  };

  const manifestFiles = [];
  for (const p of paths) {
    const bytes = contents[p];
    const stored = await storeOnce(bytes, mimeFor(p));
    manifestFiles.push({ path: p, uri: stored.uri, bytes: bytes.length });
    console.log(`  ${stored.reused ? "kept  " : "stored"} ${String(bytes.length).padStart(6)}  ${p}`);
  }

  // The picture a place is listed by, stored as an asset of its own and named in the manifest.
  let thumbnail = "";
  if (thumbPath) {
    const bytes = fs.readFileSync(thumbPath);
    const ext = path.extname(thumbPath).toLowerCase();
    const mime = ext === ".jpg" || ext === ".jpeg" ? "image/jpeg" : ext === ".webp" ? "image/webp" : "image/png";
    const shot = await storeOnce(bytes, mime);
    // Bare content hash: which store and blob hold a copy belongs to this deployment, so the
    // client asks AssetStore.blobOf for the blob and takes the mime off it. File entries keep
    // their whole uris -- those are a fetch plan, read in bulk on join.
    thumbnail = assets.parseUri(shot.uri).contentHash;
    console.log(`thumbnail ${bytes.length} bytes  ${mime}`);
  }

  // The card shown while the rest of the place arrives; the client reads the manifest first.
  let splash = "";
  if (splashPath) {
    const bytes = fs.readFileSync(splashPath);
    const ext = path.extname(splashPath).toLowerCase();
    const mime = ext === ".jpg" || ext === ".jpeg" ? "image/jpeg" : "image/png";
    splash = assets.parseUri((await storeOnce(bytes, mime)).uri).contentHash;
    console.log(`splash ${bytes.length} bytes  ${mime}`);
  }

  const manifest = Buffer.from(JSON.stringify({
    kind: "experience",
    name,
    description,
    thumbnail,
    splash,
    uses,
    ...(server ? { server } : {}),
    ...(Object.keys(declared).length ? { contracts: declared } : {}),
    published: Math.floor(Date.now() / 1000),
    publisher: await creator.getAddress(),
    files: manifestFiles,
    ...(Object.keys(placeAssets).length ? { assets: placeAssets } : {}),
  }));
  const stored = await assets.storeBytes(manifest, { mime: "application/json", signer: creator });

  console.log(`\n${reused} file(s) were already on chain and kept`);
  console.log(`manifest ${manifest.length} bytes`);
  console.log(`\n${name} is:\n  ${stored.uri}`);
  console.log("\nThat hash is the experience. Anything that changes -- one character in one");
  console.log("script -- makes a different one, which is the point.");

  const out = path.join(ROOT, "experiences.943.json");
  const all = fs.existsSync(out) ? JSON.parse(fs.readFileSync(out, "utf8")) : {};
  all[name] = { uri: stored.uri, files: manifestFiles.length, bytes: total,
    ...(assetBytes ? { asset_bytes: assetBytes, download_bytes: total + assetBytes } : {}),
    published: Math.floor(Date.now() / 1000), ...(server ? { server } : {}) };
  fs.writeFileSync(out, JSON.stringify(all, null, 2) + "\n");
  console.log(`\nwrote ${path.basename(out)}`);
}

function mimeFor(p) {
  if (p.endsWith(".json")) return "application/json";
  if (p.endsWith(".rbxm") || p.endsWith(".rbxmx")) return "application/octet-stream";
  return "text/x-lua";
}

main().catch((e) => { console.error(e.message || e); process.exit(1); });
