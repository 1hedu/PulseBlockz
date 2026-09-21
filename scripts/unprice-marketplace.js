// Clears every Marketplace price set by CREATOR_KEY: setPrice(id, token, 0) is how the Marketplace
// stops accepting a token, and it drops the token from tokensFor(id) too. --dry sends nothing.
// Wait until the town whose shelf reads the item registry is live -- it grants on PLS held, not on
// a price -- or any shelf still built from tokensFor sits empty from the moment this runs.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");

const ROOT = path.join(__dirname, "..");
const RPC = "https://rpc.v4.testnet.pulsechain.com";
const dry = process.argv.includes("--dry");

async function main() {
  process.env.CHAIN = "testnet";
  process.env.ADDRESSES_FILE = path.join(ROOT, "addresses.943.json");
  const env = Object.fromEntries(fs.readFileSync(path.join(ROOT, ".env.testnet"), "utf8").trim().split("\n").map((l) => l.split("=")));
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(RPC, 943, { staticNetwork: true }));
  const creator = new ethers.NonceManager(new ethers.Wallet(env.CREATOR_KEY.trim(), provider));
  const me = await creator.getAddress();
  const { contracts } = require("../bridge/src/chain");
  const erc20 = (a) => new ethers.Contract(a, ["function symbol() view returns (string)"], provider);

  const nextId = Number(await contracts.ugc.nextId());
  const todo = [];
  for (let id = 1; id < nextId; id++) {
    const tokens = await contracts.marketplace.tokensFor(id);
    if (tokens.length === 0) continue;
    const creatorOf = await contracts.ugc.creatorOf(id);
    if (creatorOf.toLowerCase() !== me.toLowerCase()) {
      console.log(`#${id}  priced, but its creator is ${creatorOf} -- not ours to change`);
      continue;
    }
    for (const t of tokens) todo.push({ id, token: t, symbol: await erc20(t).symbol().catch(() => "?") });
  }
  const bySymbol = {};
  for (const one of todo) bySymbol[one.symbol] = (bySymbol[one.symbol] || 0) + 1;
  console.log(`${todo.length} price(s) on ${new Set(todo.map((t) => t.id)).size} item(s):`, bySymbol);
  console.log("creator", me, ethers.formatEther(await provider.getBalance(me)), "tPLS");
  if (dry || todo.length === 0) return;

  // Sent in one pass, waited on in a second: awaiting each receipt in turn costs a block apiece.
  const sent = [];
  for (const one of todo) {
    const tx = await contracts.marketplace.connect(creator).setPrice(one.id, one.token, 0);
    sent.push({ ...one, tx });
    console.log(`  sent  #${one.id} ${one.symbol}  ${tx.hash}`);
  }
  let failed = 0;
  for (const one of sent) {
    const rc = await one.tx.wait();
    if (rc.status !== 1) { failed++; console.log(`  FAILED #${one.id} ${one.symbol}`); }
  }
  let left = 0;
  for (let id = 1; id < nextId; id++) left += (await contracts.marketplace.tokensFor(id)).length;
  console.log(`\n${sent.length - failed} cleared, ${failed} failed; ${left} price(s) left on the Marketplace`);
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
