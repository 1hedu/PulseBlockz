// Stores a listing's metadata document again with the URI scheme inside it respelled, and points
// the listing at the new document. The token id keeps; the item id -- keccak(document hash, tier)
// -- does not, so this is only safe for listings nobody holds: the ones here are duplicates of
// catalogue items, absent from catalogue.943.json, and the Inventory is seeded from that file.
// Nothing else respells them -- the catalogue publisher covers only what that file names.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");
const assets = require("./assets");

const ROOT = path.join(__dirname, "..");
const WAS = "pb" + "lox://";      // in two pieces: a sweep of the old name must not rewrite it
const NOW = "pblockz://";

async function main() {
  const ids = process.argv.slice(2).filter((a) => /^\d+$/.test(a)).map(Number);
  const dry = process.argv.includes("--dry");
  if (ids.length === 0) throw new Error("usage: rescheme-documents.js <id> [id...] [--dry]");
  const env = Object.fromEntries(fs.readFileSync(path.join(ROOT, ".env.testnet"), "utf8").trim().split("\n").map((l) => l.split("=")));
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com", 943, { staticNetwork: true }));
  const creator = new ethers.NonceManager(new ethers.Wallet(env.CREATOR_KEY.trim(), provider));
  const addrs = JSON.parse(fs.readFileSync(path.join(ROOT, "addresses.943.json"), "utf8"));
  const reg = new ethers.Contract(addrs.UGC1155, [
    "function uri(uint256) view returns (string)",
    "function setUri(uint256 id, string uri_)",
  ], creator);
  const store = new ethers.Contract(addrs.AssetStore, ["function read(uint256) view returns (bytes)"], provider);

  for (const id of ids) {
    const uri = await reg.uri(id);
    const blob = (uri.match(/:(\d+)&/) || [])[1];
    if (!blob) { console.log(`${id}: not one of ours`); continue; }
    const text = Buffer.from((await store.read(blob)).slice(2), "hex").toString("utf8");
    if (!text.includes(WAS)) { console.log(`${id}: already current`); continue; }
    const now = text.split(WAS).join(NOW);
    console.log(`${id}: ${JSON.parse(text).name || "?"}  ${text.length} bytes`);
    if (dry) continue;
    const stored = await assets.storeBytes(Buffer.from(now, "utf8"), { mime: "application/json", signer: creator });
    const tx = await reg.setUri(id, stored.uri);
    await tx.wait();
    console.log(`    -> ${stored.uri.slice(0, 48)}…`);
  }
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
