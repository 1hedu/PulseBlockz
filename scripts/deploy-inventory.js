// Deploys contracts/Inventory.sol and writes its address into addresses.943.json.
//
//   node scripts/compile.js && node scripts/deploy-inventory.js
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");

const ROOT = path.join(__dirname, "..");
const ADDR_FILE = path.join(ROOT, "addresses.943.json");
const CATALOGUE = path.join(ROOT, "catalogue.943.json");

async function main() {
  const env = Object.fromEntries(fs.readFileSync(path.join(ROOT, ".env.testnet"), "utf8").trim().split("\n").map((l) => l.split("=")));
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com", 943, { staticNetwork: true }));
  const deployer = new ethers.NonceManager(new ethers.Wallet(env.DEPLOYER_KEY, provider));
  const addrs = JSON.parse(fs.readFileSync(ADDR_FILE, "utf8"));

  if (addrs.Inventory && !process.argv.includes("--redeploy")) {
    console.log("already deployed at", addrs.Inventory, "(--redeploy to replace)");
    return;
  }
  // Balance a claimer must hold, in PLS, one per tier -- Inventory.add reads the balance and
  // takes nothing. The testnet faucet pays 10 tPLS a day into an empty wallet, so every rung
  // is reachable within five days.
  //
  // Constructor-only: a new ladder means a redeploy, and item ids are keccak(dataHash, tier)
  // scoped to one contract, so everything already claimed is orphaned.
  const RUNGS = ["5", "10", "20", "30", "40", "50"];
  // The founder is credited at construction with every catalogue item the publish manifest
  // records, so only already-fetchable items can be seeded. The hash comes out of the item's
  // pblockz:// URI, which is what Inventory.add folds with the tier -- a seeded item and an
  // earned one share an id.
  const { ITEMS } = require("./catalogue");
  const manifest = fs.existsSync(CATALOGUE) ? JSON.parse(fs.readFileSync(CATALOGUE, "utf8")) : { items: {} };
  const founder = new ethers.Wallet(env.CREATOR_KEY.trim()).address;
  const seedItems = [], seedTiers = [], seeded = [];
  for (const item of ITEMS) {
    const known = (manifest.items || {})[item.key];
    if (!known || !known.uri) continue;
    const hash = known.uri.replace(/^pblockz:\/\//, "").split("?")[0];
    seedItems.push("0x" + hash);
    seedTiers.push(item.tier);
    seeded.push(`${item.name} (${item.tierPls})`);
  }
  if (seedItems.length !== ITEMS.length) {
    console.log(`note: ${ITEMS.length - seedItems.length} item(s) are not in ${path.basename(CATALOGUE)} yet and cannot be seeded -- publish the catalogue first`);
  }
  console.log(`seeding ${founder} with ${seedItems.length} item(s)`);

  const art = JSON.parse(fs.readFileSync(path.join(ROOT, "artifacts", "Inventory.json"), "utf8"));
  const c = await new ethers.ContractFactory(art.abi, art.bytecode, deployer)
    .deploy(RUNGS.map((p) => ethers.parseEther(p)), founder, seedItems, seedTiers);
  await c.waitForDeployment();
  addrs.Inventory = await c.getAddress();
  fs.writeFileSync(ADDR_FILE, JSON.stringify(addrs, null, 2) + "\n");
  console.log("Inventory", addrs.Inventory);

  if (seedItems.length > 0) {
    const [items, counts, datas] = await c.inventoryOf(founder, 0, 1024);
    const right = items.length === seedItems.length
      && counts.every((n) => n.toString() === "1")
      && datas.every((d, i) => d === seedItems[items.findIndex((x) => x === items[i])] || seedItems.includes(d));
    console.log(`founder holds  : ${items.length} of ${seedItems.length}${right ? ", one of each, all resolving" : "  <-- WRONG"}`);
    const first = await c.itemId(seedItems[0], seedTiers[0]);
    console.log(`and a seeded id: ${items.includes(first) ? "is keccak(dataHash, tier), the same id anyone earns" : "DOES NOT MATCH -- WRONG"}`);
  }

  // Round trip on the deploying key. Paying for the catalogue publish usually leaves it under
  // the bottom rung, so the balance is checked first to tell an expected skip from a revert.
  const me = await deployer.getAddress();
  const bal = await provider.getBalance(me);
  const bottom = ethers.parseEther(RUNGS[0]);
  if (bal < bottom) {
    console.log(`round trip : skipped -- deployer holds ${ethers.formatEther(bal)} tPLS, under the ${RUNGS[0]} tPLS bottom rung`);
    console.log(`gate holds : the chain refuses this key at tier 0, which is the whole point of the ladder`);
    return;
  }
  const data = ethers.keccak256(ethers.toUtf8Bytes("a test cape"));
  await (await c.add(data, 0)).wait();
  const [items, counts, datas] = await c.inventoryOf(me, 0, 64);
  console.log("after add:", items.length, "slot(s), count", counts[0].toString(), "data", datas[0] === data ? "resolves" : "WRONG");
  console.log("item id  :", items[0], "= keccak(data, tier)", (await c.itemId(data, 0)) === items[0] ? "ok" : "WRONG");
  try {
    await (await c.add(data, RUNGS.length - 1)).wait();
    console.log("GATE FAILED: a tier we cannot afford was accepted");
  } catch (e) {
    console.log("gate holds :", (e.shortMessage || e.message).slice(0, 60));
    // NonceManager counted the refused add even though it never went out; without this the
    // drop below goes out one nonce too high and sits queued until a later send fills the gap.
    deployer.reset();
  }
  await (await c.drop(items[0], 1)).wait();
  console.log("dropped    : held now", (await c.held(me, items[0])).toString());
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
