// How each on-chain document names its model and thumbnail: by bare content hash, or by a
// whole link. The hash is the convention -- AssetStore.blobOf finds the blob and the blob
// carries its own mime -- so bare hashes are also checked against the store. A whole link is
// a finding because it pins a chain, a store and a blob that move; rewriting the document to
// follow them changes its hash, and with it the item's id (scripts/audit-scheme.js).
//
//   node scripts/audit-documents.js
//
// Reads only.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");

const ROOT = path.join(__dirname, "..");
const FIELDS = ["model", "thumbnail"];

const isHash = (v) => typeof v === "string" && /^(0x)?[0-9a-f]{64}$/i.test(v);
const isLink = (v) => typeof v === "string" && v.includes("://");

async function main() {
  const addrs = JSON.parse(fs.readFileSync(path.join(ROOT, "addresses.943.json"), "utf8"));
  const p = new ethers.JsonRpcProvider(process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com", 943, { staticNetwork: true });
  const reg = new ethers.Contract(addrs.UGC1155, ["function uri(uint256) view returns (string)"], p);
  const store = new ethers.Contract(addrs.AssetStore, [
    "function read(uint256) view returns (bytes)",
    "function blobOf(bytes32) view returns (uint256)",
  ], p);

  let documents = 0, bare = 0, empty = 0;
  const stillLinked = [], unfindable = [], unlisted = [];
  for (let id = 1; id <= 400; id++) {
    let uri;
    try { uri = await reg.uri(id); } catch { break; }
    if (!uri) continue;
    const blob = (uri.match(/:(\d+)&/) || [])[1];
    if (!blob) continue;
    let doc;
    try { doc = JSON.parse(Buffer.from((await store.read(blob)).slice(2), "hex").toString("utf8")); }
    catch { continue; }   // not one of our documents

    // The shelf only offers documents that carry a tier (Chain.server.luau: `doc.tier ~= nil`),
    // so one without a tier is a listing nothing reads. Counted and named, but not judged.
    if (doc.tier === undefined || doc.tier === null) { unlisted.push(`${id} ${doc.name || "?"}`); continue; }

    documents++;
    for (const field of FIELDS) {
      const v = doc[field];
      if (v === undefined || v === "") { empty++; continue; }
      if (isLink(v)) { stillLinked.push(`${id} ${field}`); continue; }
      if (!isHash(v)) { stillLinked.push(`${id} ${field} (neither)`); continue; }
      bare++;
      const held = await store.blobOf(v.startsWith("0x") ? v : "0x" + v);
      if (held === 0n) unfindable.push(`${id} ${field} ${v.slice(0, 12)}…`);
    }
  }

  const show = (label, list) => console.log(`${label}${list.length ? "  " + list.slice(0, 12).join(", ") + (list.length > 12 ? ` (+${list.length - 12})` : "") : ""}`);
  console.log(`listed documents read            : ${documents}`);
  show(`documents nothing lists          : ${unlisted.length}`, unlisted);
  console.log(`fields naming content by hash    : ${bare}`);
  show(`fields still naming a whole link : ${stillLinked.length}`, stillLinked);
  console.log(`fields a document left empty     : ${empty}`);
  show(`hashes the store cannot find     : ${unfindable.length}`, unfindable);
  console.log(unfindable.length === 0 && stillLinked.length === 0
    ? "\nEvery document names what it is made of, and the store can find all of it."
    : "\nSomething here would not draw: the lines above are what to republish.");
  process.exitCode = unfindable.length === 0 && stillLinked.length === 0 ? 0 : 1;
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
