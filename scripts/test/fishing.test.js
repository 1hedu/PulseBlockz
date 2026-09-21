// Fishing, in an in-process EVM against a stand-in random: node --test scripts/test/fishing.test.js
// Needs artifacts/Fishing.json and artifacts/MockRandom.json (node scripts/compile.js). The block
// number is set per call, so the bite, the window and a line left too long are all exact.
const test = require("node:test");
const assert = require("node:assert");
const fs = require("fs"), path = require("path");
const { ethers } = require("ethers");
const { createEVM } = require("@ethereumjs/evm");
const { createAddressFromString } = require("@ethereumjs/util");

const load = (name) => JSON.parse(fs.readFileSync(path.join(__dirname, "..", "..", "artifacts", `${name}.json`), "utf8"));
const FISH = load("Fishing"), RNG = load("MockRandom"), STORE = load("AssetStore");
const fishI = new ethers.Interface(FISH.abi), rngI = new ethers.Interface(RNG.abi), storeI = new ethers.Interface(STORE.abi);
const PICTURE = fs.readFileSync(path.join(__dirname, "..", "logos", "everliving-fish.png"));
const hex = (b) => "0x" + Buffer.from(b).toString("hex");
const bytes = (h) => Uint8Array.from(Buffer.from(h.replace(/^0x/, ""), "hex"));

const alice = ethers.getAddress("0x00000000000000000000000000000000000a11ce");
const bob = ethers.getAddress("0x0000000000000000000000000000000000000b0b");

function block(n) {
  return { header: { number: BigInt(n), coinbase: createAddressFromString(ethers.ZeroAddress), timestamp: BigInt(1700000000 + n * 10),
    difficulty: 0n, prevRandao: new Uint8Array(32), gasLimit: 30_000_000n, baseFeePerGas: 0n, getBlobGasPrice: () => undefined } };
}

async function world() {
  const evm = await createEVM();
  const tryDeploy = async (art, iface, args = []) => {
    const data = art.bytecode + (args.length ? iface.encodeDeploy(args).slice(2) : "");
    const r = await evm.runCall({ caller: createAddressFromString(alice), data: bytes(data), gasLimit: 10_000_000n, block: block(1) });
    return r.execResult.exceptionError ? { ok: false, error: r.execResult.exceptionError.error } : { ok: true, address: r.createdAddress.toString() };
  };
  const deploy = async (art, iface, args = []) => {
    const r = await tryDeploy(art, iface, args);
    assert.ok(r.ok, `deploy ${art.contractName}: ${r.error}`);
    return r.address;
  };
  const call = async (to, iface, from, fn, args, at, gas = 5_000_000n) => {
    const r = await evm.runCall({ caller: createAddressFromString(from), to: createAddressFromString(to),
      data: bytes(iface.encodeFunctionData(fn, args)), gasLimit: gas, block: block(at) });
    if (r.execResult.exceptionError) {
      let reason = String(r.execResult.exceptionError.error);
      try { const e = iface.parseError(hex(r.execResult.returnValue)); if (e) reason = e.name === "Error" ? String(e.args[0]) : e.name; } catch {}
      return { ok: false, reason };
    }
    const logs = (r.execResult.logs || []).map((l) => { try { return fishI.parseLog({ topics: l[1].map(hex), data: hex(l[2]) }); } catch { return null; } }).filter(Boolean);
    return { ok: true, out: iface.decodeFunctionResult(fn, hex(r.execResult.returnValue)), logs };
  };
  // The picture in an AssetStore, chunked as the publisher chunks it: blob 1. A text blob is 2.
  const store = await deploy(STORE, storeI);
  const chunks = [];
  for (let i = 0; i < PICTURE.length; i += 24_575) chunks.push(PICTURE.subarray(i, i + 24_575));
  const put = await call(store, storeI, alice, "storeAndPublish", [ethers.keccak256(PICTURE), PICTURE.length, "image/png", chunks], 1, 30_000_000n);
  assert.ok(put.ok, put.reason);
  const words = Buffer.from("not a picture");
  assert.ok((await call(store, storeI, alice, "storeAndPublish", [ethers.keccak256(words), words.length, "text/plain", [words]], 1)).ok);
  const rng = await deploy(RNG, rngI);
  const fishing = await deploy(FISH, fishI, [rng, store, 1n]);
  return {
    fishing, store, rng, tryDeploy,
    fish: (from, fn, args, at, gas) => call(fishing, fishI, from, fn, args, at, gas),
    nextRandom: (v, at = 1) => call(rng, rngI, alice, "setNext", [v], at),
  };
}

/// The roll the contract makes for a reel, recomputed here from the same inputs.
function rollFor(random, angler, nonce, fishing) {
  return BigInt(ethers.keccak256(ethers.AbiCoder.defaultAbiCoder().encode(
    ["uint64", "address", "uint64", "address", "uint256"], [random, angler, nonce, fishing, 1])));
}

test("a cast bites 1 to 3 blocks later, and one line at a time", async () => {
  const w = await world();
  for (const [r, extra] of [[0n, 0n], [1n, 1n], [2n, 2n], [5n, 2n]]) {
    await w.nextRandom(r);
    const angler = ethers.getAddress("0x" + (0x1000 + Number(r)).toString(16).padStart(40, "0"));
    const cast = await w.fish(angler, "cast", [], 10);
    assert.ok(cast.ok, cast.reason);
    const ev = cast.logs.find((l) => l.name === "Cast");
    assert.equal(ev.args.biteBlock, 11n + extra, `random ${r}`);
    assert.equal(ev.args.lastBlock, 11n + extra + 3n);
  }
  await w.nextRandom(0n);
  assert.ok((await w.fish(alice, "cast", [], 20)).ok);
  const again = await w.fish(alice, "cast", [], 21);
  assert.equal(again.ok, false);
  assert.match(again.reason, /already out/);
});

test("reeling before the bite reverts; inside the window it catches; after it, it got away", async () => {
  const w = await world();
  await w.nextRandom(0n);                                   // bite at 11
  assert.ok((await w.fish(alice, "cast", [], 10)).ok);
  const state = (await w.fish(alice, "lineOf", [alice], 10)).out;
  assert.deepEqual([state[0], state[1], state[2], state[3]], [true, 11n, 14n, 10n], "lineOf says where the line and the chain are");
  const early = await w.fish(alice, "reel", [], 10);
  assert.equal(early.ok, false);
  assert.match(early.reason, /nothing has bitten/);

  await w.nextRandom(777n);
  const got = await w.fish(alice, "reel", [], 13);
  assert.ok(got.ok, got.reason);
  const want = Number((rollFor(777n, alice, 1n, w.fishing) >> 16n) % 9n);
  const ev = got.logs.find((l) => l.name === "Caught");
  assert.equal(Number(ev.args.species), want);
  assert.equal(ev.args.discovery, false);
  assert.equal((await w.fish(alice, "caught", [alice, want], 13)).out[0], 1n);
  assert.equal((await w.fish(alice, "reel", [], 13)).reason, "no line out");

  await w.nextRandom(0n);                                   // bite at 21, window to 24
  assert.ok((await w.fish(alice, "cast", [], 20)).ok);
  const late = await w.fish(alice, "reel", [], 25);
  assert.ok(late.ok, late.reason);
  assert.ok(late.logs.some((l) => l.name === "GotAway"));
  assert.ok(!late.logs.some((l) => l.name === "Caught"));
});

test("a line left past its window can be cast over", async () => {
  const w = await world();
  await w.nextRandom(0n);
  assert.ok((await w.fish(alice, "cast", [], 10)).ok);     // bite 11, window to 14
  assert.match((await w.fish(alice, "cast", [], 14)).reason, /already out/);
  assert.ok((await w.fish(alice, "cast", [], 15)).ok);
  assert.equal((await w.fish(alice, "castCount", [alice], 15)).out[0], 2n);
  assert.equal((await w.fish(alice, "anglerCount", [], 15)).out[0], 1n, "still one angler");
});

function discoveringRandom(angler, nonce, fishing, from = 1000n) {
  for (let r = from; r < from + 200000n; r++) if (rollFor(r, angler, nonce, fishing) % 5555n === 0n) return r;
  throw new Error("no discovering random found");
}

test("the Everliving Fish: caught, claimed by choice, the one token minted, and after that an ordinary catch", async () => {
  const w = await world();
  assert.equal((await w.fish(alice, "discovered", [], 1)).out[0], false);

  await w.nextRandom(0n);
  assert.ok((await w.fish(bob, "cast", [], 10)).ok);
  await w.nextRandom(discoveringRandom(bob, 1n, w.fishing));
  const found = await w.fish(bob, "reel", [], 11);
  assert.ok(found.ok, found.reason);
  const ev = found.logs.find((l) => l.name === "Caught");
  assert.equal(Number(ev.args.species), 9);
  assert.equal(ev.args.discovery, true);
  assert.equal((await w.fish(alice, "discovered", [], 11)).out[0], false, "catching it does not unlock it");
  assert.equal((await w.fish(alice, "balanceOf", [bob], 11)).out[0], 0n, "nor mint anything");
  assert.equal((await w.fish(alice, "canClaimDiscovery", [bob], 11)).out[0], true);
  assert.equal((await w.fish(bob, "caughtOf", [bob], 11)).out[0].map(Number)[9], 1, "the fish itself is his");
  assert.match((await w.fish(alice, "claimDiscovery", [], 11)).reason, /not caught/, "nobody else can claim it");
  const claimed = await w.fish(bob, "claimDiscovery", [], 12);
  assert.ok(claimed.ok, claimed.reason);
  assert.ok(claimed.logs.some((l) => l.name === "Discovered"));
  assert.match((await w.fish(bob, "claimDiscovery", [], 12)).reason, /already been discovered/, "and only once");
  assert.equal((await w.fish(alice, "discovered", [], 11)).out[0], true);
  assert.equal((await w.fish(alice, "discoverer", [], 11)).out[0], bob);
  assert.equal((await w.fish(alice, "ownerOf", [1n], 11)).out[0], bob);
  // tokenURI is built on chain, picture and all: a data: URI any wallet can show.
  const uri = (await w.fish(alice, "tokenURI", [1n], 11, 30_000_000n)).out[0];
  assert.ok(uri.startsWith("data:application/json;base64,"), uri.slice(0, 40));
  const meta = JSON.parse(Buffer.from(uri.slice("data:application/json;base64,".length), "base64").toString("utf8"));
  assert.equal(meta.name, "First Catch of the Everliving Fish");
  assert.equal(meta.description, "ROMANS VI:IV");
  assert.ok(meta.image.startsWith("data:image/png;base64,"));
  assert.ok(Buffer.from(meta.image.slice("data:image/png;base64,".length), "base64").equals(PICTURE), "the picture is the game's own bytes");
  assert.equal((await w.fish(alice, "balanceOf", [bob], 11)).out[0], 1n);

  // Once found, a roll landing in the Everliving Fish's share is an ordinary catch.
  let r = 50000n, hit = null;
  for (; r < 60000n; r++) {
    const pick = (rollFor(r, alice, 1n, w.fishing) >> 16n) % 41n;
    if (pick >= 36n) { hit = r; break; }
  }
  await w.nextRandom(0n);
  assert.ok((await w.fish(alice, "cast", [], 20)).ok);
  await w.nextRandom(hit);
  const later = await w.fish(alice, "reel", [], 21);
  const ev2 = later.logs.find((l) => l.name === "Caught");
  assert.equal(Number(ev2.args.species), 9);
  assert.equal(ev2.args.discovery, false);
  assert.equal((await w.fish(alice, "balanceOf", [alice], 21)).out[0], 0n, "no token for an ordinary Everliving Fish");
  assert.equal((await w.fish(alice, "discoverer", [], 21)).out[0], bob, "the discoverer stays the discoverer");
  const counts = (await w.fish(alice, "caughtOf", [alice], 21)).out[0].map(Number);
  assert.equal(counts[9], 1);
  const totals = (await w.fish(alice, "totalsOf", [], 21)).out[0].map(Number);
  assert.equal(totals[9], 2);
});

test("before discovery the nine colours come up evenly", async () => {
  const w = await world();
  const seen = new Array(10).fill(0);
  let at = 10;
  for (let i = 0; i < 900; i++) {
    await w.nextRandom(0n, at);
    const c = await w.fish(alice, "cast", [], at);
    assert.ok(c.ok, c.reason);
    await w.nextRandom(BigInt(100000 + i), at + 1);
    const r = await w.fish(alice, "reel", [], at + 1);
    const ev = r.logs.find((l) => l.name === "Caught");
    seen[Number(ev.args.species)]++;
    at += 2;
  }
  for (let s = 0; s < 9; s++) assert.ok(seen[s] > 60 && seen[s] < 140, `colour ${s} came up ${seen[s]} times in 900`);
});

test("the token's picture has to be a PNG that is really in the store", async () => {
  const w = await world();
  assert.equal((await w.fish(alice, "pictureStore", [], 1)).out[0], ethers.getAddress(w.store));
  assert.equal((await w.fish(alice, "pictureBlob", [], 1)).out[0], 1n);
  assert.equal((await w.tryDeploy(FISH, fishI, [w.rng, w.store, 2n])).ok, false, "a text blob is refused");
  assert.equal((await w.tryDeploy(FISH, fishI, [w.rng, w.store, 99n])).ok, false, "and a blob that is not there");
});
