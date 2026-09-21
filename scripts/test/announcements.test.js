// Announcements, run in an in-process EVM: node --test scripts/test/announcements.test.js
//
// Needs artifacts/Announcements.json (node scripts/compile.js).
const test = require("node:test");
const assert = require("node:assert");
const fs = require("fs"), path = require("path");
const { ethers } = require("ethers");
const { createEVM } = require("@ethereumjs/evm");
const { createAddressFromString } = require("@ethereumjs/util");

const ART = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "..", "artifacts", "Announcements.json"), "utf8"));
const iface = new ethers.Interface(ART.abi);
const hex = (b) => "0x" + Buffer.from(b).toString("hex");
const bytes = (h) => Uint8Array.from(Buffer.from(h.replace(/^0x/, ""), "hex"));

const owner = ethers.getAddress("0x00000000000000000000000000000000000d4e7e");
const stranger = ethers.getAddress("0x0000000000000000000000000000000000000b0b");

function block(n) {
  return { header: { number: BigInt(n), coinbase: createAddressFromString(ethers.ZeroAddress), timestamp: BigInt(1700000000 + n * 10),
    difficulty: 0n, prevRandao: new Uint8Array(32), gasLimit: 30_000_000n, baseFeePerGas: 0n, getBlobGasPrice: () => undefined } };
}

async function world() {
  const evm = await createEVM();
  const made = await evm.runCall({ caller: createAddressFromString(owner), data: bytes(ART.bytecode), gasLimit: 5_000_000n, block: block(1) });
  assert.ok(!made.execResult.exceptionError);
  const at = made.createdAddress.toString();
  const call = async (from, fn, args, n = 2) => {
    const r = await evm.runCall({ caller: createAddressFromString(from), to: createAddressFromString(at),
      data: bytes(iface.encodeFunctionData(fn, args)), gasLimit: 5_000_000n, block: block(n) });
    if (r.execResult.exceptionError) {
      let reason = String(r.execResult.exceptionError.error);
      try { const e = iface.parseError(hex(r.execResult.returnValue)); if (e) reason = String(e.args[0]); } catch {}
      return { ok: false, reason };
    }
    return { ok: true, out: iface.decodeFunctionResult(fn, hex(r.execResult.returnValue)) };
  };
  return { call };
}

test("only the owner announces, and every announcement is kept in order", async () => {
  const w = await world();
  assert.equal((await w.call(stranger, "owner", [])).out[0], owner);
  assert.equal((await w.call(stranger, "count", [])).out[0], 0n);

  const refused = await w.call(stranger, "announce", ["I am not the owner"]);
  assert.equal(refused.ok, false);
  assert.match(refused.reason, /only the owner/);

  assert.ok((await w.call(owner, "announce", ["Space fishing is open."], 5)).ok);
  assert.ok((await w.call(owner, "announce", ["The Everliving Fish is still out there."], 9)).ok);
  assert.equal((await w.call(stranger, "count", [])).out[0], 2n);

  const first = (await w.call(stranger, "announcement", [0n])).out;
  assert.equal(first[0], "Space fishing is open.");
  assert.equal(first[1], BigInt(1700000000 + 5 * 10));
  assert.equal((await w.call(stranger, "announcement", [1n])).out[0], "The Everliving Fish is still out there.");

  assert.match((await w.call(owner, "announce", [""])).reason, /1 to 2000/);
  assert.ok((await w.call(owner, "announce", ["x".repeat(2000)])).ok, "a long one, paragraphs of it");
  assert.match((await w.call(owner, "announce", ["x".repeat(2001)])).reason, /1 to 2000/);
});

test("the owner pins one to keep it up, and unpins it", async () => {
  const w = await world();
  assert.deepEqual([...(await w.call(stranger, "pinned", [])).out], [false, 0n]);
  assert.match((await w.call(owner, "pin", [0n])).reason, /no such announcement/);

  assert.ok((await w.call(owner, "announce", ["Welcome to pBlockz Home."])).ok);
  assert.ok((await w.call(owner, "announce", ["Space fishing is open."])).ok);
  assert.match((await w.call(stranger, "pin", [0n])).reason, /only the owner/);
  assert.match((await w.call(owner, "pin", [2n])).reason, /no such announcement/);

  assert.ok((await w.call(owner, "pin", [0n])).ok);
  assert.deepEqual([...(await w.call(stranger, "pinned", [])).out], [true, 0n]);
  assert.ok((await w.call(owner, "pin", [1n])).ok, "pinning another replaces it");
  assert.deepEqual([...(await w.call(stranger, "pinned", [])).out], [true, 1n]);

  assert.match((await w.call(stranger, "unpin", [])).reason, /only the owner/);
  assert.ok((await w.call(owner, "unpin", [])).ok);
  assert.deepEqual([...(await w.call(stranger, "pinned", [])).out], [false, 0n]);
  assert.match((await w.call(owner, "unpin", [])).reason, /nothing is pinned/);
  assert.equal((await w.call(stranger, "count", [])).out[0], 2n, "unpinning removes nothing");
});
