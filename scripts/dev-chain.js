// A chain of your own, to play what is not deployed yet.
//
//   node scripts/dev-chain.js [--block-time 10] [--port 8545] [--owner <address>]  start it
//   node scripts/dev-chain.js discover <angler address>   queue the reel that finds the fish
//   node scripts/dev-chain.js fund <address> [pls]
//   node scripts/dev-chain.js announce "text" | --file path
//   node scripts/dev-chain.js pin <n> | unpin   hold announcement #n above the newest one, or stop
//   node scripts/dev-chain.js stop    clean up after a start that was killed
//
// anvil (Foundry) forking PulseChain testnet v4: every deployed contract, item and balance is there
// at the same addresses and chain id, and nothing sent to it leaves. On top of that go Fishing
// (drawing on DevRandom in place of atropaMath), DuelRecords and Announcements, named for the town
// in shared/ContractsDev.luau -- gitignored, and the experience publisher refuses to run beside it.
const { ethers } = require("ethers");
const { spawn } = require("child_process");
const fs = require("fs"), path = require("path"), os = require("os");

const ROOT = path.join(__dirname, "..");
const ANVIL = process.env.ANVIL || "C:/tools/foundry/anvil.exe";
const FORK = process.env.FORK_URL || "https://rpc.v4.testnet.pulsechain.com";
const OVERRIDE = path.join(ROOT, "luau", "gdextension", "demo2", "scripts", "src", "shared", "ContractsDev.luau");
const STATE = path.join(ROOT, ".dev-chain.json");
const DISCOVERY_ODDS = 5555n;
// The account that publishes the town (Contracts.Donations) and plays in it; it owns the dev
// chain's Announcements unless --owner says otherwise.
const TOWN_OWNER = "0xC8CD89650f12b8b565ba89307e072C10579F6AA6";
let anvilProcess = null;

const art = (name) => JSON.parse(fs.readFileSync(path.join(ROOT, "artifacts", `${name}.json`), "utf8"));
const flag = (name, def) => { const i = process.argv.indexOf(name); return i >= 0 ? process.argv[i + 1] : def; };

function tidy() {
  for (const f of [OVERRIDE, STATE]) if (fs.existsSync(f)) fs.unlinkSync(f);
}

async function waitForNode(url, seconds) {
  const provider = new ethers.JsonRpcProvider(url, undefined, { staticNetwork: false });
  for (let i = 0; i < seconds * 2; i++) {
    try { return { provider, chainId: (await provider.getNetwork()).chainId }; } catch { await new Promise((r) => setTimeout(r, 500)); }
  }
  throw new Error(`anvil did not answer at ${url}`);
}

async function start() {
  const port = flag("--port", "8545");
  const blockTime = flag("--block-time", "10");
  const url = `http://127.0.0.1:${port}`;
  if (!fs.existsSync(ANVIL)) throw new Error(`no anvil at ${ANVIL} (set ANVIL to where it is)`);
  const log = path.join(os.tmpdir(), "pblockz-dev-chain.log");
  const out = fs.openSync(log, "w");
  // Shanghai, as the contracts are compiled for (scripts/compile.js): PulseChain has PUSH0, not Cancun.
  const anvil = spawn(ANVIL, ["--fork-url", FORK, "--port", port, "--block-time", blockTime, "--hardfork", "shanghai"],
    { stdio: ["ignore", out, out] });
  anvilProcess = anvil;
  let stopping = false;
  const stop = () => {
    if (stopping) return;
    stopping = true;
    tidy();
    anvil.kill();
    console.log("\nstopped; shared/ContractsDev.luau removed");
    process.exit(0);
  };
  process.on("SIGINT", stop);
  process.on("SIGTERM", stop);
  anvil.on("exit", (code) => { if (!stopping) { tidy(); console.error(`anvil exited (${code}); see ${log}`); process.exit(1); } });

  const { provider, chainId } = await waitForNode(url, 90);
  console.log(`anvil at ${url}, forking ${FORK}, chain ${chainId}, a block every ${blockTime}s (log: ${log})`);
  const dev = new ethers.NonceManager(await provider.getSigner(0));

  const R = art("DevRandom"), F = art("Fishing");
  const random = await new ethers.ContractFactory(R.abi, R.bytecode, dev).deploy();
  await random.waitForDeployment();
  // The discovery token's picture, a PNG already in the forked AssetStore: the heart icon stands
  // in for the fish's own.
  const heart = JSON.parse(fs.readFileSync(path.join(ROOT, "luau", "gdextension", "demo2", "place-assets.json"), "utf8")).HeartIcon;
  const [, pictureStore, pictureBlob] = heart.match(/chain=943:(0x[0-9a-fA-F]{40}):(\d+)/);
  const fishing = await new ethers.ContractFactory(F.abi, F.bytecode, dev).deploy(await random.getAddress(), pictureStore, BigInt(pictureBlob));
  await fishing.waitForDeployment();
  // anvil sends as the owner without its key, so announcements carry the town's gold nametag here
  // as they will for real.
  const owner = ethers.getAddress(flag("--owner", TOWN_OWNER));
  await provider.send("anvil_impersonateAccount", [owner]);
  await provider.send("anvil_setBalance", [owner, ethers.toQuantity(ethers.parseEther("1000"))]);
  const A = art("Announcements");
  const announcements = await new ethers.ContractFactory(A.abi, A.bytecode, await provider.getSigner(owner)).deploy();
  await announcements.waitForDeployment();
  const D = art("DuelRecords");
  const duelRecords = await new ethers.ContractFactory(D.abi, D.bytecode, dev).deploy();
  await duelRecords.waitForDeployment();
  const addrs = { Fishing: await fishing.getAddress(), DevRandom: await random.getAddress(), Announcements: await announcements.getAddress(),
    DuelRecords: await duelRecords.getAddress(), owner };

  fs.writeFileSync(OVERRIDE, [
    "-- Written by scripts/dev-chain.js for a local chain, and removed when it stops. Gitignored; the",
    "-- experience publisher will not run while this file exists. These addresses mean nothing anywhere else.",
    "return {",
    `\tFishing = "${addrs.Fishing}",`,
    `\tAnnouncements = "${addrs.Announcements}",`,
    `\tDuelRecords = "${addrs.DuelRecords}",`,
    "}",
    "",
  ].join("\n"));
  fs.writeFileSync(STATE, JSON.stringify({ url, ...addrs }, null, 2));
  console.log(`Fishing   ${addrs.Fishing}`);
  console.log(`DevRandom ${addrs.DevRandom}`);
  console.log(`Announcements ${addrs.Announcements}`);
  console.log(`DuelRecords ${addrs.DuelRecords}`);
  console.log("");
  console.log("Wrote shared/ContractsDev.luau. Start every Godot process from a shell with:");
  console.log(`  PowerShell:  $env:PBLOCKZ_RPC_URL = "${url}"`);
  console.log(`  bash:        export PBLOCKZ_RPC_URL=${url}`);
  console.log("");
  console.log("Ctrl+C here stops the chain and removes the override.");
}

function state() {
  if (!fs.existsSync(STATE)) throw new Error("no dev chain running (start one with: node scripts/dev-chain.js)");
  return JSON.parse(fs.readFileSync(STATE, "utf8"));
}

/// Queues the Random() the angler's next reel draws so that it discovers the Everliving Fish.
async function discover(angler) {
  if (!angler || !ethers.isAddress(angler)) throw new Error("usage: node scripts/dev-chain.js discover <angler address>");
  angler = ethers.getAddress(angler);
  const s = state();
  const provider = new ethers.JsonRpcProvider(s.url);
  const dev = await provider.getSigner(0);
  const fishing = new ethers.Contract(s.Fishing, art("Fishing").abi, provider);
  if (await fishing.discovered()) throw new Error(`already found, by ${await fishing.discoverer()}`);
  const line = await fishing.lines(angler);
  if (!line.out) throw new Error("that angler has no line out: cast first, then run this before reeling");
  const { chainId } = await provider.getNetwork();
  const coder = ethers.AbiCoder.defaultAbiCoder();
  for (let r = 1n; r < 50_000_000n; r++) {
    const roll = BigInt(ethers.keccak256(coder.encode(["uint64", "address", "uint64", "address", "uint256"],
      [r, angler, line.nonce, s.Fishing, chainId])));
    if (roll % DISCOVERY_ODDS === 0n) {
      const tx = await new ethers.Contract(s.DevRandom, art("DevRandom").abi, dev).queue([r]);
      await tx.wait();
      console.log(`queued ${r}: ${angler}'s reel on line ${line.nonce} finds the Everliving Fish -- if nobody else casts or reels first`);
      return;
    }
  }
  throw new Error("no discovering number found");
}

async function announce(words) {
  const fileAt = words.indexOf("--file");
  const text = (fileAt >= 0 ? fs.readFileSync(words[fileAt + 1], "utf8") : words.join(" ")).trim();
  if (!text) throw new Error('usage: node scripts/dev-chain.js announce "text" | --file path');
  const s = state();
  const provider = new ethers.JsonRpcProvider(s.url);
  await provider.send("anvil_impersonateAccount", [s.owner]);
  const c = new ethers.Contract(s.Announcements, art("Announcements").abi, await provider.getSigner(s.owner));
  await (await c.announce(text)).wait();
  console.log(`announced on the dev chain: ${text}`);
}

async function pin(index) {
  if (index !== undefined && !/^\d+$/.test(index)) throw new Error("usage: node scripts/dev-chain.js pin <announcement number> | unpin");
  const s = state();
  const provider = new ethers.JsonRpcProvider(s.url);
  await provider.send("anvil_impersonateAccount", [s.owner]);
  const c = new ethers.Contract(s.Announcements, art("Announcements").abi, await provider.getSigner(s.owner));
  if (index === undefined) {
    await (await c.unpin()).wait();
    console.log("unpinned on the dev chain");
  } else {
    await (await c.pin(BigInt(index))).wait();
    console.log(`pinned #${index} on the dev chain`);
  }
}

async function fund(address, pls = "1000") {
  if (!address || !ethers.isAddress(address)) throw new Error("usage: node scripts/dev-chain.js fund <address> [pls]");
  const s = state();
  const provider = new ethers.JsonRpcProvider(s.url);
  await provider.send("anvil_setBalance", [ethers.getAddress(address), ethers.toQuantity(ethers.parseEther(pls))]);
  console.log(`${address} now holds ${pls} PLS on the dev chain`);
}

const [, , cmd, ...rest] = process.argv;
const run = cmd === "discover" ? discover(rest[0])
  : cmd === "fund" ? fund(rest[0], rest[1])
  : cmd === "announce" ? announce(rest)
  : cmd === "pin" ? (rest[0] === undefined ? Promise.reject(new Error("usage: node scripts/dev-chain.js pin <n>")) : pin(rest[0]))
  : cmd === "unpin" ? pin(undefined)
  : cmd === "stop" ? Promise.resolve(tidy()).then(() => console.log("removed shared/ContractsDev.luau"))
  : start();
run.catch((e) => {
  console.error(e.shortMessage || e.message || e);
  if (anvilProcess) { tidy(); anvilProcess.kill(); }
  process.exit(1);
});
