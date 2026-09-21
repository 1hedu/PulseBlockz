// Rewrites every listing's uri in the registry onto the current scheme prefix. Only the prefix
// changes -- the content hash after it still names the same bytes, so the item id, keccak(metadata
// hash, tier), does not move. setUri is the creator's, so only this key's listings can change.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");

const ROOT = path.join(__dirname, "..");
const NOW = "pblockz://";
const WAS = "pb" + "lox://";      // in two pieces: a sweep of the old name must not rewrite it

async function main() {
  const env = Object.fromEntries(fs.readFileSync(path.join(ROOT, ".env.testnet"), "utf8").trim().split("\n").map((l) => l.split("=")));
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com", 943, { staticNetwork: true }));
  const creator = new ethers.NonceManager(new ethers.Wallet(env.CREATOR_KEY, provider));
  const addrs = JSON.parse(fs.readFileSync(path.join(ROOT, "addresses.943.json"), "utf8"));
  const dry = process.argv.includes("--dry");
  const flag = (name, fallback) => {
    const i = process.argv.indexOf(name);
    return i >= 0 && process.argv[i + 1] ? Number(process.argv[i + 1]) : fallback;
  };
  const from = flag("--from", 1), to = flag("--to", 200);

  const reg = new ethers.Contract(addrs.UGC1155, [
    "function uri(uint256) view returns (string)",
    "function setUri(uint256 id, string uri_)",
    "function creatorOf(uint256) view returns (address)",
  ], creator);
  const me = (await creator.getAddress()).toLowerCase();
  console.log("creator", me, dry ? "(dry)" : "");

  let done = 0, skipped = 0, notMine = 0;
  for (let id = from; id <= to; id++) {
    let was;
    try { was = await reg.uri(id); } catch { break; }
    if (!was || !was.startsWith(WAS)) { skipped++; continue; }
    try {
      const owner = (await reg.creatorOf(id)).toLowerCase();
      if (owner !== me) { notMine++; continue; }
    } catch { /* no creatorOf: let setUri decide */ }
    const now = NOW + was.slice(WAS.length);
    if (dry) { console.log(`  ${id}  ${now.slice(0, 64)}…`); done++; continue; }
    const tx = await reg.setUri(id, now);
    await tx.wait();
    done++;
    if (done % 10 === 0) console.log(`  ${done} rewritten (id ${id})`);
  }
  console.log(`${done} listing(s) ${dry ? "would be" : ""} rewritten, ${skipped} already current or not ours to read, ${notMine} somebody else's`);
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
