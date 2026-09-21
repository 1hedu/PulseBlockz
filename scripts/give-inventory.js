// Hand every item one address holds to another.
//
//   node scripts/give-inventory.js CREATOR PLAYER [--dry]
//
// Inventory seeds its founder once, in the constructor, and that is the account that publishes
// the catalogue -- not the one the town signs with. Inventory.give moves an item without creating
// one, so no redeploy is needed, and a redeploy would orphan every record: holdings live in the
// contract, keyed by an id of keccak(dataHash, tier).
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");

const ROOT = path.join(__dirname, "..");
const ABI = [
  "function inventoryOf(address,uint256,uint256) view returns (bytes32[],uint256[],bytes32[])",
  "function give(address to, bytes32 item, uint32 amount) external",
  "function held(address,bytes32) view returns (uint32)",
];

async function main() {
  const [fromName, toName] = process.argv.slice(2).filter((a) => !a.startsWith("--"));
  const dry = process.argv.includes("--dry");
  if (!fromName || !toName) { console.error("usage: give-inventory.js FROM_KEYNAME TO_KEYNAME [--dry]"); process.exit(1); }

  const env = Object.fromEntries(fs.readFileSync(path.join(ROOT, ".env.testnet"), "utf8").trim().split("\n").map((l) => l.split("=")));
  const addrs = JSON.parse(fs.readFileSync(path.join(ROOT, "addresses.943.json"), "utf8"));
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com", 943, { staticNetwork: true }));
  const from = new ethers.NonceManager(new ethers.Wallet(env[`${fromName}_KEY`].trim(), provider));
  const to = new ethers.Wallet(env[`${toName}_KEY`].trim()).address;
  const fromAddr = await from.getAddress();

  const inv = new ethers.Contract(addrs.Inventory, ABI, from);
  const [items, counts] = await inv.inventoryOf(fromAddr, 0, 1024);
  const moving = items.map((id, i) => [id, counts[i]]).filter(([, n]) => n > 0n);
  console.log(`${fromAddr}  ->  ${to}`);
  console.log(`${moving.length} item(s) to move on ${addrs.Inventory}`);
  if (dry) return;
  if (moving.length === 0) return;

  let done = 0;
  for (const [id, n] of moving) {
    const tx = await inv.give(to, id, n);
    await tx.wait();
    process.stdout.write(`\r  moved ${++done}/${moving.length}`);
  }
  console.log();
  const [after] = await inv.inventoryOf(to, 0, 1024);
  const [left] = await inv.inventoryOf(fromAddr, 0, 1024);
  const stillHeld = (await Promise.all(left.map((i) => inv.held(fromAddr, i)))).filter((n) => n > 0n).length;
  console.log(`${to} now holds ${after.length}; ${fromAddr} still holds ${stillHeld}`);
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
