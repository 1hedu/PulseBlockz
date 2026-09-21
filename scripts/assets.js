// Asset bytes on chain: store a file in AssetStore chunks, publish the blob, fetch it back.
//   node scripts/assets.js store <file> [--mime <type>] [--key 0x…]
//   node scripts/assets.js fetch <blobId | pblockz://…> [--out <file>]
//   node scripts/assets.js info  <blobId>
// Prints JSON; `store` prints the pblockz:// URI to put in a UGC1155 register(). URI scheme: ASSETS.md.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { provider, contracts } = require("../bridge/src/chain");
const cfg = require("../bridge/src/config");

const MAX_CHUNK = 24_575;      // AssetStore.MAX_CHUNK, i.e. EIP-170 minus the STOP byte

// PulseChain applies EIP-3860's 49,152-byte initcode limit to every transaction, not just contract
// creations: a full-size batch is refused at eth_sendRawTransaction ("INVALID: initcode too large")
// though eth_estimateGas accepts it. 24,000 keeps two chunks in one tx (48,196 bytes of calldata).
const MAX_TX_DATA = Number(process.env.MAX_TX_DATA || 49_152);
const CHUNK_BYTES = Math.min(Number(process.env.CHUNK_BYTES || 24_000), MAX_CHUNK);

function chunk(bytes, size = CHUNK_BYTES) {
  const out = [];
  for (let i = 0; i < bytes.length; i += size) out.push(bytes.subarray(i, i + size));
  return out;
}

const pad32 = (n) => Math.ceil(n / 32) * 32;
/** Exact ABI calldata size for a `bytes[]` argument list, plus `head` bytes of fixed args. */
function calldataSize(sizes, head = 0) {
  return 4 + head + 32 + 32 + sizes.reduce((t, n) => t + 32 + 32 + pad32(n), 0);
}
/** Greedily pack chunks into transactions that stay within MAX_TX_DATA. */
function batches(parts, head = 0) {
  const out = [];
  let cur = [];
  for (const part of parts) {
    if (cur.length && calldataSize([...cur, part].map((b) => b.length), head) > MAX_TX_DATA) { out.push(cur); cur = []; }
    cur.push(part);
  }
  if (cur.length) out.push(cur);
  return out;
}

function toUri(contentHash, { chainId = cfg.chainId, store = cfg.addresses.AssetStore, blobId, manifestTx, txChainId, mime, srcs = [] } = {}) {
  const q = [];
  if (blobId !== undefined) q.push(`chain=${chainId}:${store}:${blobId}`);
  if (manifestTx) q.push(`tx=${txChainId ?? chainId}:${manifestTx}`);
  if (mime) q.push(`mime=${encodeURIComponent(mime)}`);
  for (const s of srcs) q.push(`src=${encodeURIComponent(s)}`);
  return `pblockz://${contentHash.replace(/^0x/, "")}` + (q.length ? "?" + q.join("&") : "");
}

function parseUri(uri) {
  const m = /^pblockz:\/\/([0-9a-fA-F]{64})(?:\?(.*))?$/.exec(uri);
  if (!m) throw new Error("not a pblockz:// asset URI");
  const out = { contentHash: "0x" + m[1].toLowerCase(), chain: null, tx: null, mime: null, srcs: [] };
  for (const kv of (m[2] || "").split("&").filter(Boolean)) {
    const [k, v] = kv.split("=");
    if (k === "chain") { const [chainId, store, blobId] = decodeURIComponent(v).split(":"); out.chain = { chainId: Number(chainId), store, blobId: BigInt(blobId) }; }
    else if (k === "tx") { const d = decodeURIComponent(v); const i = d.indexOf(":"); out.tx = { chainId: Number(d.slice(0, i)), manifestTx: d.slice(i + 1).toLowerCase() }; }
    else if (k === "mime") out.mime = decodeURIComponent(v);
    else if (k === "src") out.srcs.push(decodeURIComponent(v));
  }
  return out;
}

/** Store bytes on chain. Returns { blobId, contentHash, size, chunks, uri, txs }. */
async function storeBytes(bytes, { mime = "application/octet-stream", signer } = {}) {
  const store = contracts.assetStore.connect(signer);
  const contentHash = ethers.keccak256(bytes);
  const parts = chunk(bytes);
  if (parts.length === 0) throw new Error("empty");
  const txs = [];
  let blobId, chunks;
  // storeAndPublish also carries hash + size + mime, so its fixed args eat more of the limit.
  const publishHead = 32 + 32 + 32 + 32 + pad32(Buffer.byteLength(mime));
  if (batches(parts, publishHead).length === 1) {
    const tx = await store.storeAndPublish(contentHash, bytes.length, mime, parts);
    const rc = await tx.wait(); txs.push(rc.hash);
    ({ blobId, chunks } = parseEvents(store, rc));
  } else {
    chunks = [];
    for (const batch of batches(parts)) {
      const tx = await store.storeMany(batch);
      const rc = await tx.wait(); txs.push(rc.hash);
      chunks.push(...parseEvents(store, rc).chunks);
    }
    const tx = await store.publish(contentHash, bytes.length, mime, chunks);
    const rc = await tx.wait(); txs.push(rc.hash);
    blobId = parseEvents(store, rc).blobId;
  }
  return { blobId, contentHash, size: bytes.length, chunks, mime, uri: toUri(contentHash, { blobId, mime }), txs };
}

function parseEvents(store, rc) {
  const chunks = []; let blobId;
  for (const l of rc.logs) {
    let p; try { p = store.interface.parseLog(l); } catch { continue; }
    if (p && p.name === "ChunkStored") chunks.push(p.args.chunk);
    if (p && p.name === "BlobPublished") blobId = p.args.blobId;
  }
  return { chunks, blobId };
}

// ---- the calldata scheme ------------------------------------------------------------------
// One transaction per chunk carrying {"parentHash","chunkData":"<base64>","mimeType"}, then a
// manifest carrying {"mimeType","chunkHashes":[...]}; both go to the zero address and run nothing.
// Calldata is 16 gas a byte against ~200 for code, but lives in block history, not state. The
// scheme an existing PulseChain tool publishes with, so the reader below opens its content too.
const ZERO = "0x0000000000000000000000000000000000000000";
const TX_CHUNK_BYTES = Number(process.env.TX_CHUNK_BYTES || 20_000);   // ~27 KB of calldata once base64 encoded

/** Publish bytes as calldata transactions. Returns { manifestTx, chunkTxs, contentHash, uri }. */
async function storeBytesAsCalldata(bytes, { mime = "application/octet-stream", signer } = {}) {
  const contentHash = ethers.keccak256(bytes);
  const chunkTxs = [];
  let parent = 0;
  for (let i = 0; i < bytes.length; i += TX_CHUNK_BYTES) {
    const part = bytes.subarray(i, i + TX_CHUNK_BYTES);
    const body = JSON.stringify({ parentHash: parent, chunkData: part.toString("base64"), mimeType: mime });
    const rc = await (await signer.sendTransaction({ to: ZERO, value: 0, data: "0x" + Buffer.from(body).toString("hex") })).wait();
    chunkTxs.push(rc.hash);
    parent = rc.hash;
  }
  const manifest = JSON.stringify({ mimeType: mime, chunkHashes: chunkTxs });
  const rc = await (await signer.sendTransaction({ to: ZERO, value: 0, data: "0x" + Buffer.from(manifest).toString("hex") })).wait();
  return { manifestTx: rc.hash, chunkTxs, contentHash, size: bytes.length, mime,
           uri: toUri(contentHash, { manifestTx: rc.hash, mime }) };
}

/** Read an asset published as calldata. Verifies against `verifyHash` when given. */
async function fetchCalldataAsset(manifestTx, { verifyHash, rpc = provider } = {}) {
  const readTx = async (h) => {
    const t = await rpc.getTransaction(h);
    if (!t) throw new Error(`transaction ${h} not found on chain ${cfg.chainId}`);
    return JSON.parse(Buffer.from(t.data.slice(2), "hex").toString("utf8"));
  };
  const manifest = await readTx(manifestTx);
  if (!Array.isArray(manifest.chunkHashes) || manifest.chunkHashes.length === 0) throw new Error("manifest lists no chunks");
  const parts = [];
  for (const h of manifest.chunkHashes) {
    const chunk = await readTx(h);
    if (typeof chunk.chunkData !== "string") throw new Error(`chunk ${h} carries no chunkData`);
    parts.push(Buffer.from(chunk.chunkData, "base64"));
  }
  const bytes = Buffer.concat(parts);
  const got = ethers.keccak256(bytes);
  if (verifyHash && got !== verifyHash.toLowerCase()) throw new Error(`calldata asset does not match its hash: ${got} != ${verifyHash}`);
  return { bytes, info: { manifestTx, chunks: manifest.chunkHashes, mime: manifest.mimeType || null, size: bytes.length, contentHash: got } };
}

/** Fetch + verify a blob by id or pblockz:// URI. Returns { bytes, info }. Tries read() first, then chunk-by-chunk. */
async function fetchBlob(ref, { verifyHash } = {}) {
  let blobId = ref, expect = verifyHash;
  if (typeof ref === "string" && (ref.startsWith("pblockz://"))) {
    const u = parseUri(ref);
    if (!u.chain && u.tx) {
      if (u.tx.chainId !== cfg.chainId) throw new Error(`URI is for chain ${u.tx.chainId}, bridge is on ${cfg.chainId}`);
      return fetchCalldataAsset(u.tx.manifestTx, { verifyHash: u.contentHash });
    }
    if (!u.chain) throw new Error("URI names no chain copy; fetch from one of srcs and verify " + u.contentHash);
    if (u.chain.chainId !== cfg.chainId) throw new Error(`URI is for chain ${u.chain.chainId}, bridge is on ${cfg.chainId}`);
    blobId = u.chain.blobId; expect = u.contentHash;
    if (u.chain.store.toLowerCase() !== cfg.addresses.AssetStore.toLowerCase()) throw new Error("URI names a different AssetStore");
  }
  const store = contracts.assetStore;
  const [publisher, contentHash, size, mime, chunks] = await store.blob(blobId);
  let bytes;
  try {
    bytes = ethers.getBytes(await store.read(blobId));
  } catch {
    // read() over the RPC's response limit: assemble from eth_getCode per chunk, STOP byte off.
    const parts = [];
    for (const c of chunks) parts.push(ethers.getBytes(await provider.getCode(c)).subarray(1));
    bytes = Buffer.concat(parts);
  }
  const got = ethers.keccak256(bytes);
  if (got !== contentHash) throw new Error(`chain data does not match its recorded hash: ${got} != ${contentHash}`);
  if (expect && got !== expect.toLowerCase()) throw new Error(`data does not match the URI's hash: ${got} != ${expect}`);
  if (bytes.length !== Number(size)) throw new Error("size mismatch");
  return { bytes, info: { blobId: BigInt(blobId), publisher, contentHash, size: Number(size), mime, chunks } };
}

async function main() {
  const [cmd, arg, ...rest] = process.argv.slice(2);
  const opt = (name, def) => { const i = rest.indexOf(name); return i >= 0 ? rest[i + 1] : def; };
  if (cmd === "store") {
    const key = opt("--key", process.env.DEPLOYER_KEY || "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"); // anvil #0 dev only
    const signer = new ethers.NonceManager(new ethers.Wallet(key, provider));
    const bytes = fs.readFileSync(arg);
    const mime = opt("--mime", guessMime(arg));
    if (opt("--via") === "calldata") {
      console.log(JSON.stringify(await storeBytesAsCalldata(bytes, { mime, signer }), null, 2));
    } else {
      const r = await storeBytes(bytes, { mime, signer });
      console.log(JSON.stringify({ ...r, blobId: r.blobId.toString() }, null, 2));
    }
  } else if (cmd === "fetch") {
    const ref = /^\d+$/.test(arg) ? BigInt(arg) : arg;
    const { bytes, info } = /^0x[0-9a-fA-F]{64}$/.test(String(ref))
      ? await fetchCalldataAsset(String(ref))
      : await fetchBlob(ref);
    const out = opt("--out");
    if (out) fs.writeFileSync(out, bytes);
    console.log(JSON.stringify({ ...info, blobId: info.blobId ? info.blobId.toString() : undefined, verified: true, wrote: out || null }, null, 2));
  } else if (cmd === "info") {
    const [publisher, contentHash, size, mime, chunks] = await contracts.assetStore.blob(BigInt(arg));
    console.log(JSON.stringify({ blobId: arg, publisher, contentHash, size: Number(size), mime, chunks, uri: toUri(contentHash, { blobId: arg, mime }) }, null, 2));
  } else {
    console.error("usage: assets.js store <file> [--mime t] [--key k] [--via calldata] | fetch <blobId|uri|0xmanifestTx> [--out f] | info <blobId>");
    process.exit(2);
  }
}

function guessMime(file) {
  return { ".glb": "model/gltf-binary", ".gltf": "model/gltf+json", ".rbxm": "application/x-rbxm", ".rbxmx": "application/x-rbxmx",
           ".json": "application/json", ".luau": "text/x-luau", ".png": "image/png", ".wav": "audio/wav", ".ogg": "audio/ogg" }[path.extname(file).toLowerCase()] || "application/octet-stream";
}

module.exports = { MAX_CHUNK, CHUNK_BYTES, MAX_TX_DATA, TX_CHUNK_BYTES, ZERO, chunk, batches, calldataSize, toUri, parseUri, storeBytes, fetchBlob, storeBytesAsCalldata, fetchCalldataAsset };
if (require.main === module) main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
