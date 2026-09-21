// Deploys the whole ledger to PulseChain testnet v4 (chainId 943) and runs the e2e against it;
// `status` only prints balances and addresses. Keys are generated into .env.testnet (gitignored)
// on first run; the contracts are ownerless, so addresses.943.json is all there is to keep.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path"), { spawn } = require("child_process");
const { applyFeeOverrides } = require("../bridge/src/fees");

const ROOT = path.join(__dirname, "..");
const ENV = path.join(ROOT, ".env.testnet");
const CHAIN_ID = 943;
const RPC = process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com";
const EXPLORER = "https://scan.v4.testnet.pulsechain.com";
const FAUCET = "https://faucet.v4.testnet.pulsechain.com";
const MIN_DEPLOYER = ethers.parseEther("2");   // covers deploy + fund + e2e at testnet gas prices
const SUB_FUND = ethers.parseEther("0.3");      // player / creator / relayer each

function loadEnv() {
  const env = {};
  if (fs.existsSync(ENV)) for (const line of fs.readFileSync(ENV, "utf8").split("\n")) { const m = /^(\w+)=(.*)$/.exec(line.trim()); if (m) env[m[1]] = m[2]; }
  let changed = false;
  for (const k of ["DEPLOYER_KEY", "PLAYER_KEY", "CREATOR_KEY", "RELAYER_KEY"]) {
    if (!env[k]) { env[k] = ethers.Wallet.createRandom().privateKey; changed = true; }
  }
  if (changed) {
    fs.writeFileSync(ENV, Object.entries(env).map(([k, v]) => `${k}=${v}`).join("\n") + "\n", { mode: 0o600 });
    console.log("wrote", ENV, "(gitignored; back it up if you care about these testnet wallets)");
  }
  return env;
}

const run = (cmd, args, extraEnv, opts = {}) => new Promise((resolve, reject) => {
  const p = spawn(cmd, args, { cwd: ROOT, stdio: opts.quiet ? ["ignore", "pipe", "pipe"] : "inherit", env: { ...process.env, ...extraEnv } });
  let out = "";
  if (opts.quiet) { p.stdout.on("data", (d) => (out += d)); p.stderr.on("data", (d) => (out += d)); }
  p.on("exit", (code) => (code === 0 ? resolve(out) : reject(new Error(`${cmd} ${args.join(" ")} exited ${code}\n${out}`))));
  if (opts.detach) resolve(p);
});

async function main() {
  const env = loadEnv();
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(RPC, CHAIN_ID, { staticNetwork: true }));
  const wallets = Object.fromEntries(["DEPLOYER", "PLAYER", "CREATOR", "RELAYER"].map((n) => [n, new ethers.Wallet(env[n + "_KEY"], provider)]));
  const addrFile = path.join(ROOT, `addresses.${CHAIN_ID}.json`);

  let net;
  try { net = await provider.getNetwork(); } catch (e) { throw new Error(`cannot reach ${RPC}: ${e.shortMessage || e.message}`); }
  if (Number(net.chainId) !== CHAIN_ID) throw new Error(`RPC is chain ${net.chainId}, expected ${CHAIN_ID}`);

  const bal = {};
  for (const [n, w] of Object.entries(wallets)) bal[n] = await provider.getBalance(w.address);
  console.log(`PulseChain testnet v4 via ${RPC}, block ${await provider.getBlockNumber()}`);
  for (const [n, w] of Object.entries(wallets)) console.log(`  ${n.padEnd(9)} ${w.address}  ${ethers.formatEther(bal[n])} tPLS`);
  if (fs.existsSync(addrFile)) console.log("  deployed:", JSON.stringify(JSON.parse(fs.readFileSync(addrFile)), null, 0));
  if (process.argv[2] === "status") return;

  if (bal.DEPLOYER < MIN_DEPLOYER) {
    console.log(`\nFund the deployer with at least ${ethers.formatEther(MIN_DEPLOYER)} tPLS, then run this again:\n  ${FAUCET}\n  address: ${wallets.DEPLOYER.address}`);
    return;
  }

  // The e2e sends real transactions from these three, and the relayer pays its own gas. SUB_FUND
  // is sized for the throwaway ERC-20 the e2e deploys from CREATOR, not for its approves and mints
  const deployer = new ethers.NonceManager(wallets.DEPLOYER);
  for (const n of ["PLAYER", "CREATOR", "RELAYER"]) {
    if (bal[n] < SUB_FUND / 2n) {
      console.log(`funding ${n} with ${ethers.formatEther(SUB_FUND)} tPLS...`);
      await (await deployer.sendTransaction({ to: wallets[n].address, value: SUB_FUND })).wait();
    }
  }

  // DEPLOY_MOCKS: the e2e needs mintable permit tokens
  if (!fs.existsSync(addrFile) || process.argv.includes("--redeploy")) {
    console.log("\ndeploying...");
    await run("node", ["scripts/compile.js"], {});
    await run("node", ["scripts/deploy.js"], { RPC_URL: RPC, DEPLOYER_KEY: env.DEPLOYER_KEY, RELAYER_ADDRESS: wallets.RELAYER.address, DEPLOY_MOCKS: "1" });
    fs.copyFileSync(path.join(ROOT, "addresses.json"), addrFile);
    console.log("wrote", addrFile);
  } else {
    fs.copyFileSync(addrFile, path.join(ROOT, "addresses.json"));
    console.log("\nusing existing deployment from", addrFile, "(--redeploy to replace)");
  }
  const A = JSON.parse(fs.readFileSync(addrFile));
  for (const k of ["Forwarder", "Marketplace", "UGC1155", "AssetStore"]) console.log(`  ${k.padEnd(12)} ${EXPLORER}/address/${A[k]}`);

  const bridgeEnv = { CHAIN: "testnet", RPC_URL: RPC, RELAYER_KEY: env.RELAYER_KEY, PORT: "8787" };
  console.log("\nstarting the reference bridge on :8787 (testnet)...");
  const bridge = spawn("node", ["bridge/src/server.js"], { cwd: ROOT, stdio: "inherit", env: { ...process.env, ...bridgeEnv } });
  try {
    for (let i = 0; i < 40; i++) { try { const r = await fetch("http://127.0.0.1:8787/health"); if (r.ok) break; } catch {} await new Promise((r) => setTimeout(r, 500)); }
    console.log("\nrunning e2e against the testnet (10 s blocks: expect several minutes)...\n");
    await run("node", ["scripts/e2e.js"], { ...bridgeEnv, DEPLOYER_KEY: env.DEPLOYER_KEY, PLAYER_KEY: env.PLAYER_KEY, CREATOR_KEY: env.CREATOR_KEY, TX_TIMEOUT_MS: "180000" });
  } finally {
    bridge.kill();
  }
  console.log(`\nDone. Everything above is live on testnet v4; browse it at ${EXPLORER}. Commit ${path.basename(addrFile)}.`);
}

main().catch((e) => { console.error(e.message || e); process.exit(1); });
