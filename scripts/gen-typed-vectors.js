// Writes luau/gdextension/demo2/tests/typed_vectors.json: EIP-712 typed data with the digest and
// signature ethers produces for it. tests/typed_data_test.gd checks that host/TypedData.gd reaches
// the same digest and that a signature the host makes recovers to the same signer.
//
//   node scripts/gen-typed-vectors.js
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");

const key = ethers.id("pulseblockz typed data vectors");
const wallet = new ethers.Wallet(key);
const out = [];

async function add(name, domain, types, primaryType, message) {
  const digest = ethers.TypedDataEncoder.hash(domain, types, message);
  const signature = await wallet.signTypedData(domain, types, message);
  out.push({ name, typed: { domain, types, primaryType, message }, digest, signature });
}

async function main() {
  await add("the EIP's own Mail", { name: "Ether Mail", version: "1", chainId: 1, verifyingContract: "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC" },
    { Person: [{ name: "name", type: "string" }, { name: "wallet", type: "address" }],
      Mail: [{ name: "from", type: "Person" }, { name: "to", type: "Person" }, { name: "contents", type: "string" }] },
    "Mail",
    { from: { name: "Cow", wallet: "0xCD2a3d9F938E13CD947Ec05AbC7FE734Df8DD826" },
      to: { name: "Bob", wallet: "0xbBbBBBBbbBBBbbbBbbBbbbbBBbBbbbbBbBbbBBbB" }, contents: "Hello, Bob!" });

  // The Result struct as DuelRecords.sol hashes it.
  await add("a duel result", { name: "PulseBlockz Duels", version: "1", chainId: 943, verifyingContract: "0x1111111111111111111111111111111111111111" },
    { Result: [{ name: "id", type: "bytes32" }, { name: "teamA", type: "address[]" }, { name: "teamB", type: "address[]" },
      { name: "killsA", type: "uint16[]" }, { name: "killsB", type: "uint16[]" }, { name: "limit", type: "uint16" },
      { name: "winner", type: "uint8" }, { name: "endedAt", type: "uint64" }] },
    "Result",
    { id: ethers.id("duel"), teamA: ["0x6181C6099D50DCab711e6531BAe4a37b069B1F01", "0xCD2a3d9F938E13CD947Ec05AbC7FE734Df8DD826"],
      teamB: ["0xe84706a9f8056742013D2972469903DB2f692757", "0xbBbBBBBbbBBBbbbBbbBbbbbBBbBbbbbBbBbbBBbB"],
      killsA: [3, 2], killsB: [4, 0], limit: 5, winner: 1, endedAt: 1757800000 });

  await add("the rest of the types", { name: "Everything", chainId: 369 },
    { Item: [{ name: "tag", type: "bytes4" }, { name: "on", type: "bool" }, { name: "n", type: "int32" }],
      Bag: [{ name: "items", type: "Item[]" }, { name: "pair", type: "uint8[2]" }, { name: "blob", type: "bytes" },
        { name: "big", type: "uint256" }, { name: "note", type: "string" }] },
    "Bag",
    { items: [{ tag: "0xdeadbeef", on: true, n: -7 }, { tag: "0x00000001", on: false, n: 12 }], pair: [3, 4],
      blob: "0x0102030405", big: "115792089237316195423570985008687907853269984665640564039457584007913129639935", note: "mind the gap" });

  const file = path.join(__dirname, "..", "luau", "gdextension", "demo2", "tests", "typed_vectors.json");
  fs.writeFileSync(file, JSON.stringify({ signer: wallet.address, key, vectors: out }, null, 2) + "\n");
  console.log("wrote", out.length, "vectors to", file);
}

main().catch((e) => { console.error(e); process.exit(1); });
