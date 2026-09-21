// What on chain still uses the old URI scheme, counted both in the registry pointers and
// inside the metadata documents they point at. Decides whether the readers for the old
// spelling can go. Rewriting a document changes its hash, so it changes the item's id too.
//
//   node scripts/audit-scheme.js
//
// Reads only.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");

const ROOT = path.join(__dirname, "..");
const WAS = "pb" + "lox://";      // split so a rename sweep cannot rewrite the old name away
const NOW = "pblockz://";

async function main() {
  const addrs = JSON.parse(fs.readFileSync(path.join(ROOT, "addresses.943.json"), "utf8"));
  const p = new ethers.JsonRpcProvider(process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com", 943, { staticNetwork: true });
  const reg = new ethers.Contract(addrs.UGC1155, ["function uri(uint256) view returns (string)"], p);
  const store = new ethers.Contract(addrs.AssetStore, ["function read(uint256) view returns (bytes)"], p);

  const oldPointers = [], oldDocuments = [], other = [];
  let current = 0;
  for (let id = 1; id <= 400; id++) {
    let uri;
    try { uri = await reg.uri(id); } catch { break; }
    if (!uri) continue;
    if (uri.startsWith(WAS)) oldPointers.push(id);
    else if (!uri.startsWith(NOW)) { other.push(`${id} ${uri.slice(0, 24)}`); continue; }
    else current++;
    const blob = (uri.match(/:(\d+)&/) || [])[1];
    if (!blob) continue;
    try {
      const text = Buffer.from((await store.read(blob)).slice(2), "hex").toString("utf8");
      if (text.includes(WAS)) oldDocuments.push(id);
    } catch { /* unreadable */ }
  }

  console.log(`pointers on the current spelling : ${current}`);
  console.log(`pointers still on the old one    : ${oldPointers.length}${oldPointers.length ? "  ids " + oldPointers.join(",") : ""}`);
  console.log(`documents with the old one inside: ${oldDocuments.length}${oldDocuments.length ? "  ids " + oldDocuments.join(",") : ""}`);
  console.log(`pointers somewhere else entirely : ${other.length}${other.length ? "  " + other.join("; ") : ""}`);
  console.log(oldPointers.length === 0 && oldDocuments.length === 0
    ? "\nNothing on chain needs a reader for the old spelling."
    : "\nSomething still does: the readers stay until these are rewritten.");
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
