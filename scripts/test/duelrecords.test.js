// DuelRecords, run in an in-process EVM: node --test scripts/test/duelrecords.test.js
// Needs artifacts/DuelRecords.json (node scripts/compile.js). Signatures are made the way a
// wallet's eth_signTypedData_v4 makes them, so what passes here holds against the deployed one.
const test = require("node:test");
const assert = require("node:assert");
const fs = require("fs"), path = require("path");
const { ethers } = require("ethers");
const { createEVM } = require("@ethereumjs/evm");
const { createAddressFromString } = require("@ethereumjs/util");

const art = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "..", "artifacts", "DuelRecords.json"), "utf8"));
const iface = new ethers.Interface(art.abi);
const hex = (b) => "0x" + Buffer.from(b).toString("hex");
const bytes = (h) => Uint8Array.from(Buffer.from(h.replace(/^0x/, ""), "hex"));

const alice = new ethers.Wallet(ethers.id("alice")), bob = new ethers.Wallet(ethers.id("bob"));
const carol = new ethers.Wallet(ethers.id("carol")), dave = new ethers.Wallet(ethers.id("dave"));
const eve = new ethers.Wallet(ethers.id("eve"));

const TYPES = { Result: [
  { name: "id", type: "bytes32" }, { name: "teamA", type: "address[]" }, { name: "teamB", type: "address[]" },
  { name: "killsA", type: "uint16[]" }, { name: "killsB", type: "uint16[]" }, { name: "limit", type: "uint16" },
  { name: "winner", type: "uint8" }, { name: "endedAt", type: "uint64" },
] };

async function deploy() {
  const evm = await createEVM();
  const res = await evm.runCall({ caller: createAddressFromString(alice.address), data: bytes(art.bytecode), gasLimit: 10_000_000n });
  assert.ok(!res.execResult.exceptionError, "deploys");
  const at = res.createdAddress;
  const call = async (from, fn, args) => {
    const r = await evm.runCall({ caller: createAddressFromString(from), to: at, data: bytes(iface.encodeFunctionData(fn, args)), gasLimit: 5_000_000n });
    if (r.execResult.exceptionError) {
      let reason = String(r.execResult.exceptionError.error);
      try {
        const e = iface.parseError(hex(r.execResult.returnValue));
        if (e) reason = e.name === "Error" ? String(e.args[0]) : e.name;
      } catch {}
      return { ok: false, reason };
    }
    return { ok: true, out: iface.decodeFunctionResult(fn, hex(r.execResult.returnValue)), logs: r.execResult.logs || [] };
  };
  return { address: at.toString(), call };
}

function result(over = {}) {
  return { id: ethers.id("duel one"), teamA: [alice.address], teamB: [bob.address], killsA: [5], killsB: [3], limit: 5,
    winner: 1, endedAt: 1757800000, ...over };
}
const domain = (address) => ({ name: "PulseBlockz Duels", version: "1", chainId: 1, verifyingContract: address });
const sign = (wallet, address, r) => wallet.signTypedData(domain(address), TYPES, r);
const args = (r, sig) => [r.id, r.teamA, r.teamB, r.killsA, r.killsB, r.limit, r.winner, r.endedAt, sig];

test("the contract's digest is the one a wallet signs", async () => {
  const c = await deploy();
  const r = result();
  const got = await c.call(alice.address, "digest", args(r).slice(0, 8));
  assert.equal(got.out[0], ethers.TypedDataEncoder.hash(domain(c.address), TYPES, r));
});

test("signerOf says who signed, and zero for junk, so a town can check before keeping one", async () => {
  const c = await deploy();
  const r = result();
  const sig = await sign(bob, c.address, r);
  assert.equal((await c.call(alice.address, "signerOf", args(r, sig))).out[0], bob.address);
  assert.equal((await c.call(alice.address, "signerOf", args(result({ killsB: [4] }), sig))).out[0] === bob.address, false,
    "the same signature over different numbers is somebody else");
  assert.equal((await c.call(alice.address, "signerOf", args(r, "0x1234"))).out[0], ethers.ZeroAddress);
  assert.equal((await c.call(alice.address, "signerOf", args(r, "0x" + "00".repeat(65)))).out[0], ethers.ZeroAddress);
});

test("sent by one side and signed by the other, it is recorded and counted", async () => {
  const c = await deploy();
  const r = result();
  const sent = await c.call(alice.address, "record", args(r, await sign(bob, c.address, r)));
  assert.ok(sent.ok, sent.reason);
  assert.equal((await c.call(eve.address, "recorded", [r.id])).out[0], true);
  assert.equal((await c.call(eve.address, "kills", [alice.address])).out[0], 5n);
  assert.equal((await c.call(eve.address, "kills", [bob.address])).out[0], 3n);
  assert.equal((await c.call(eve.address, "wins", [alice.address])).out[0], 1n);
  assert.equal((await c.call(eve.address, "losses", [bob.address])).out[0], 1n);
  const log = iface.parseLog({ topics: sent.logs[0][1].map(hex), data: hex(sent.logs[0][2]) });
  assert.equal(log.name, "Recorded");
  assert.equal(log.args.submittedBy, alice.address);
  assert.equal(log.args.signedBy, bob.address);
});

test("the loser can send it too, with the winner's signature", async () => {
  const c = await deploy();
  const r = result();
  const sent = await c.call(bob.address, "record", args(r, await sign(alice, c.address, r)));
  assert.ok(sent.ok, sent.reason);
});

test("a signature from your own side is not agreement", async () => {
  const c = await deploy();
  const r = result({ teamA: [alice.address, carol.address], teamB: [bob.address, dave.address], killsA: [3, 2], killsB: [1, 2] });
  const sent = await c.call(alice.address, "record", args(r, await sign(carol, c.address, r)));
  assert.equal(sent.ok, false);
  assert.match(sent.reason, /other team/);
});

test("somebody outside the duel cannot send it", async () => {
  const c = await deploy();
  const r = result();
  const sent = await c.call(eve.address, "record", args(r, await sign(bob, c.address, r)));
  assert.equal(sent.ok, false);
  assert.match(sent.reason, /not in this duel/);
});

test("a signature for different numbers does not carry over", async () => {
  const c = await deploy();
  const r = result();
  const sig = await sign(bob, c.address, r);
  const sent = await c.call(alice.address, "record", args({ ...r, killsB: [0] }, sig));
  assert.equal(sent.ok, false);
});

test("a signature for another contract does not carry over", async () => {
  const c = await deploy();
  const r = result();
  const sig = await sign(bob, "0x000000000000000000000000000000000000dEaD", r);
  const sent = await c.call(alice.address, "record", args(r, sig));
  assert.equal(sent.ok, false);
});

test("each duel is recorded once", async () => {
  const c = await deploy();
  const r = result();
  const sig = await sign(bob, c.address, r);
  assert.ok((await c.call(alice.address, "record", args(r, sig))).ok);
  const again = await c.call(alice.address, "record", args(r, sig));
  assert.equal(again.ok, false);
  assert.match(again.reason, /already recorded/);
});

test("a 2v2 counts each player's own kills", async () => {
  const c = await deploy();
  const r = result({ teamA: [alice.address, carol.address], teamB: [bob.address, dave.address], killsA: [4, 1], killsB: [0, 3] });
  assert.ok((await c.call(bob.address, "record", args(r, await sign(carol, c.address, r)))).ok);
  for (const [who, n] of [[alice, 4n], [carol, 1n], [bob, 0n], [dave, 3n]])
    assert.equal((await c.call(eve.address, "kills", [who.address])).out[0], n);
});

const witnessArgs = (r, sigs) => [r.id, r.teamA, r.teamB, r.killsA, r.killsB, r.limit, r.winner, r.endedAt, sigs];
const frank = new ethers.Wallet(ethers.id("frank")), gina = new ethers.Wallet(ethers.id("gina"));

test("witnessed: sent by somebody in it, signed by bystanders, counted apart", async () => {
  const c = await deploy();
  const r = result();
  const sigs = [await sign(eve, c.address, r), await sign(frank, c.address, r)];
  const sent = await c.call(alice.address, "recordWitnessed", witnessArgs(r, sigs));
  assert.ok(sent.ok, sent.reason);
  assert.notEqual((await c.call(gina.address, "witnessedOf", [r.id])).out[0], ethers.ZeroHash);
  assert.equal((await c.call(gina.address, "recorded", [r.id])).out[0], false, "not on the agreed ledger");
  assert.equal((await c.call(gina.address, "witnessedKills", [alice.address])).out[0], 5n);
  assert.equal((await c.call(gina.address, "witnessedWins", [alice.address])).out[0], 1n);
  assert.equal((await c.call(gina.address, "witnessedLosses", [bob.address])).out[0], 1n);
  assert.equal((await c.call(gina.address, "kills", [alice.address])).out[0], 0n, "the agreed kills are untouched");
  const log = iface.parseLog({ topics: sent.logs[0][1].map(hex), data: hex(sent.logs[0][2]) });
  assert.equal(log.name, "Witnessed");
  assert.deepEqual([...log.args.witnesses], [eve.address, frank.address]);
});

test("a witness cannot be somebody in the duel, or sign twice", async () => {
  const c = await deploy();
  const r = result();
  assert.match((await c.call(alice.address, "recordWitnessed", witnessArgs(r, [await sign(bob, c.address, r)]))).reason, /was in the duel/);
  assert.match((await c.call(alice.address, "recordWitnessed", witnessArgs(r, [await sign(alice, c.address, r)]))).reason, /was in the duel/);
  const e = await sign(eve, c.address, r);
  assert.match((await c.call(alice.address, "recordWitnessed", witnessArgs(r, [e, e]))).reason, /signed twice/);
  assert.match((await c.call(eve.address, "recordWitnessed", witnessArgs(r, [await sign(frank, c.address, r)]))).reason, /not in this duel/);
  assert.match((await c.call(alice.address, "recordWitnessed", witnessArgs(r, []))).reason, /witness count/);
});

test("witnessed once, and agreeing afterwards still counts on the agreed ledger", async () => {
  const c = await deploy();
  const r = result();
  const sigs = [await sign(eve, c.address, r)];
  assert.ok((await c.call(alice.address, "recordWitnessed", witnessArgs(r, sigs))).ok);
  assert.match((await c.call(alice.address, "recordWitnessed", witnessArgs(r, sigs))).reason, /already witnessed/);
  assert.ok((await c.call(alice.address, "record", args(r, await sign(bob, c.address, r)))).ok);
  assert.equal((await c.call(gina.address, "kills", [alice.address])).out[0], 5n);
});

test("nobody on both teams, no frags past the limit, a kill count for everybody", async () => {
  const c = await deploy();
  const both = result({ teamA: [alice.address], teamB: [bob.address, alice.address], killsB: [2, 1] });
  assert.match((await c.call(alice.address, "record", args(both, await sign(bob, c.address, both)))).reason, /on both teams/);
  const over = result({ killsA: [6] });
  assert.match((await c.call(alice.address, "record", args(over, await sign(bob, c.address, over)))).reason, /past the limit/);
  const short = result({ teamA: [alice.address, carol.address], killsA: [5] });
  assert.match((await c.call(alice.address, "record", args(short, await sign(bob, c.address, short)))).reason, /kill count for each/);
});
