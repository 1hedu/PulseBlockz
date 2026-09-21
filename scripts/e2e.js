// End-to-end: run against a local anvil after deploy.js and with the reference
// bridge up.   node scripts/e2e.js
// Proves nobody administers the ledger: a creator registers their own assets, picks the ERC-20s
// they accept per asset and is paid directly, and the bridge only relays gas.
// Player = anvil #2, Creator = anvil #3.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { contracts, tokens, erc20, buildForwardRequest, signForwardRequest, signPermit, provider } = require("../bridge/src/chain");
const cfg = require("../bridge/src/config");
const assets = require("./assets");

const BRIDGE = process.env.BRIDGE_URL || `http://127.0.0.1:${cfg.port}`;
// ethers caches eth_getTransactionCount briefly, which breaks back-to-back sends from one wallet.
const nm = (k) => { const w = new ethers.Wallet(k, provider); const s = new ethers.NonceManager(w); s.address = w.address; return s; };
// Keys: anvil defaults locally; DEPLOYER_KEY / PLAYER_KEY / CREATOR_KEY on a testnet (scripts/testnet.js sets them).
const deployer = nm(process.env.DEPLOYER_KEY || "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"); // anvil #0
const player   = nm(process.env.PLAYER_KEY   || "0x5de4111afa1a4b94908f83103eb1f1706367c2e68ca870fc3fb9a804cdab365a");
const creator  = nm(process.env.CREATOR_KEY  || "0x7c852118294e51e653712a81e05800f419141751be58f605c371e15141b007a6");
const post = (p, b) => fetch(BRIDGE + p, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(b) }).then((r) => r.json());
const get  = (p) => fetch(BRIDGE + p).then((r) => r.json());
const ok = (c, m) => { if (!c) throw new Error("FAIL: " + m); console.log("  ✓", m); };
const relay = async (signer, contract, fn, args) => {
  const r = await buildForwardRequest(signer.address, contract, fn, args);
  return post("/relay", { request: ser(r), signature: (await signForwardRequest(signer, r)).signature });
};
const errorIface = new ethers.Interface([...Object.values(contracts), erc20(ethers.ZeroAddress)].flatMap((c) => c.interface.fragments.filter((f) => f.type === "error")));
const reverts = async (p, re) => {
  try { await p; console.log("  (did not revert)"); return false; }
  catch (e) {
    let decoded = ""; try { const d = errorIface.parseError(e.data); decoded = `${d.name}(${d.args.map(String).join(",")})`; } catch {}
    const text = [decoded, e.revert?.name, e.reason, e.shortMessage, e.message].filter(Boolean).join(" | ");
    if (!re.test(text)) console.log("  (reverted with unexpected error: " + text.slice(0, 300) + ")");
    return re.test(text);
  }
};
const deployMock = async (name, symbol, decimals) => {
  const art = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "artifacts", "MockERC20.json")));
  const c = await new ethers.ContractFactory(art.abi, art.bytecode, creator).deploy(name, symbol, decimals);
  await c.waitForDeployment(); return c;
};
const hasFn = (c, name) => { try { return !!c.interface.getFunction(name); } catch { return false; } };

(async () => {
  const { mUSD, mWPLS } = tokens;
  const USD = cfg.addresses.Tokens.mUSD, WPLS = cfg.addresses.Tokens.mWPLS;
  const usd = (v) => ethers.parseUnits(v, 6), pls = (v) => ethers.parseEther(v);
  const market = contracts.marketplace, ugc = contracts.ugc, M = cfg.addresses.Marketplace;

  console.log("0. nobody is in charge");
  ok(!hasFn(market, "owner") && !hasFn(market, "pause") && !hasFn(market, "setFee"), "Marketplace has no owner, pause or fee setter");
  ok(!hasFn(ugc, "hasRole") && !hasFn(ugc, "setDelisted") && !hasFn(ugc, "register(address,string,uint32,uint96)"), "UGC1155 has no roles, no delist switch, no publisher");
  ok((await ugc.minter()) === M, "the only privileged address is the Marketplace (mint on sale)");
  ok((await market.feeBps()) === 0n, "fee is 0 (immutable); FEE_BPS/FEE_RECIPIENT at deploy time are the only knobs, ever");

  console.log("1. wallet login (reference bridge; a creator's server can run its own)");
  const { message } = await post("/auth/nonce", { address: player.address });
  const sess = await post("/auth/verify", { address: player.address, signature: await player.signMessage(message) });
  ok(sess.token, "session token issued");
  ok((await get(`/auth/session?token=${sess.token}`)).ok, "session validates");

  console.log("2. player holds some tokens (local faucet)");
  await (await mUSD.connect(player).mint(player.address, usd("100"))).wait();
  await (await mWPLS.connect(player).mint(player.address, pls("100"))).wait();
  // A testnet keeps state between runs, so every value below is a delta from here.
  const before = {
    playerUsd: await mUSD.balanceOf(player.address), playerWpls: await mWPLS.balanceOf(player.address),
    creatorUsd: await mUSD.balanceOf(creator.address), creatorWpls: await mWPLS.balanceOf(creator.address),
  };

  console.log("3. creator registers their own assets — gaslessly, no publisher, no review");
  const hatId = await ugc.nextId();
  let out = await relay(creator, ugc, "register", ["ipfs://bafy.../hat-001.json", 100, 500]);
  ok(out.status === 1, `hat registered as id ${hatId} (creator paid 0 gas)`);
  const capeId = await ugc.nextId();
  out = await relay(creator, ugc, "register", ["ipfs://bafy.../cape-001.json", 0, 500]);
  ok(out.status === 1, `cape registered as id ${capeId}`);
  ok((await ugc.creatorOf(hatId)) === creator.address, "the signer is the creator; nobody can register on someone's behalf");

  console.log("4. creator decides what they accept for the hat: 25 mUSD or 10 mWPLS — gaslessly");
  out = await relay(creator, market, "setPrice", [hatId, USD, usd("25")]);
  ok(out.status === 1, "price in mUSD relayed");
  out = await relay(creator, market, "setPrice", [hatId, WPLS, pls("10")]);
  ok(out.status === 1, "price in mWPLS relayed");
  const listing = await get(`/listing/${hatId}`);
  ok(listing.prices.map((p) => `${p.price} ${p.symbol}`).join(", ") === "25.0 mUSD, 10.0 mWPLS", "bridge shows the creator's accepted tokens and prices");

  console.log("5. player pays in mUSD: one real approve per token, then gasless buys");
  await (await mUSD.connect(player).approve(M, ethers.MaxUint256)).wait();
  out = await relay(player, market, "buy", [hatId, USD, 1n]);
  ok(out.status === 1, "buy relayed");
  ok((await mUSD.balanceOf(creator.address)) - before.creatorUsd === usd("25"), "creator was paid the full 25 mUSD directly (no fee)");
  ok((await mUSD.balanceOf(M)) === 0n && (await mUSD.balanceOf(deployer.address)) === 0n, "marketplace and deployer hold nothing");

  console.log("6. pay in mWPLS with a permit: zero transactions from the player");
  const txCountBefore = await provider.getTransactionCount(player.address);
  const permit = await signPermit(player, mWPLS, M, pls("10"));
  out = await relay(player, market, "buyWithPermit", [hatId, WPLS, 1n, permit.deadline, permit.v, permit.r, permit.s]);
  ok(out.status === 1, "buyWithPermit relayed");
  ok((await mWPLS.balanceOf(creator.address)) - before.creatorWpls === pls("10"), "creator was paid 10 mWPLS");
  ok((await provider.getTransactionCount(player.address)) === txCountBefore, "player sent no transaction at all");

  console.log("7. game server checks inventory on join");
  const inv = await get(`/inventory/${player.address}?ids=${hatId},${capeId}&tokens=${USD},${WPLS}`);
  ok(inv.owned.length === 1 && inv.owned[0].id === hatId.toString() && inv.owned[0].qty === "2", `player owns 2x ${inv.owned[0].uri}`);
  const spentUsd = before.playerUsd - (await mUSD.balanceOf(player.address));
  const spentWpls = before.playerWpls - (await mWPLS.balanceOf(player.address));
  const reported = Object.fromEntries(inv.tokens.map((t) => [t.symbol, t.balance]));
  ok(spentUsd === usd("25") && spentWpls === pls("10"), "player paid exactly 25 mUSD and 10 mWPLS");
  ok(reported.mUSD === ethers.formatUnits(await mUSD.balanceOf(player.address), 6) &&
     reported.mWPLS === ethers.formatEther(await mWPLS.balanceOf(player.address)),
     `bridge reports the chain's balances for the tokens asked about (${reported.mUSD} mUSD, ${reported.mWPLS} mWPLS)`);

  console.log("8. the creator's list is theirs alone");
  out = await relay(creator, market, "setPrice", [hatId, WPLS, 0n]);
  ok(out.status === 1, "creator stops accepting mWPLS");
  ok((await market.tokensFor(hatId)).length === 1, "tokensFor() updated");
  out = await relay(player, market, "buy", [hatId, WPLS, 1n]);
  ok(/NotForSale/.test(out.error), "buying in a token the creator doesn't accept: " + out.error);
  const own = await deployMock("Creator Coin", "CC", 18);
  const CC = await own.getAddress();
  out = await relay(creator, market, "setPrice", [capeId, CC, pls("1")]);
  ok(out.status === 1, "creator prices the cape in their own token; nobody has a say");
  await (await own.connect(player).mint(player.address, pls("5"))).wait();
  const p2 = await signPermit(player, own, M, pls("1"));
  out = await relay(player, market, "buyWithPermit", [capeId, CC, 1n, p2.deadline, p2.v, p2.r, p2.s]);
  ok(out.status === 1 && (await own.balanceOf(creator.address)) === pls("1"), "sold for 1 CC, all of it to the creator");

  console.log("9. creator controls their metadata, then gives that up too");
  out = await relay(creator, ugc, "setUri", [hatId, "ipfs://bafy.../hat-001-v2.json"]);
  ok(out.status === 1 && (await ugc.uri(hatId)).endsWith("v2.json"), "creator re-points the hat's metadata");
  out = await relay(player, ugc, "setUri", [hatId, "ipfs://evil"]);
  ok(/NotCreator/.test(out.error), "nobody else can");
  out = await relay(creator, ugc, "freeze", [hatId]);
  ok(out.status === 1, "creator freezes it");
  out = await relay(creator, ugc, "setUri", [hatId, "ipfs://bafy.../hat-001-v3.json"]);
  ok(/Frozen/.test(out.error), "not even the creator can change it now: holders can trust it");

  console.log("10. guardrails");
  out = await relay(player, market, "setPrice", [hatId, USD, 1n]);
  ok(/NotCreator/.test(out.error), "only the creator can price their asset");
  ok(await reverts(ugc.connect(player).mint.staticCall(player.address, hatId, 1n), /NotMinter/), "only the Marketplace can mint (on sale)");
  let r = await buildForwardRequest(player.address, market, "buy", [hatId, USD, 1n]);
  const sig = (await signForwardRequest(creator, r)).signature; // wrong signer
  out = await post("/relay", { request: ser(r), signature: sig });
  ok(/mismatch/.test(out.error), "relayer refuses forged signature");
  await (await mUSD.connect(player).approve(M, 0n)).wait();
  out = await relay(player, market, "buy", [hatId, USD, 1n]);
  ok(/ERC20InsufficientAllowance/.test(out.error), "buy without allowance fails cleanly, decoded: " + out.error);
  ok(await reverts(market.connect(player).buy.staticCall(hatId, USD, 0n), /BadQty/), "qty 0 reverts on-chain");

  console.log("11. asset bytes on chain: AssetStore chunks, hash verified on chain and by the reader");
  const seed = (n, salt) => { const b = Buffer.alloc(n); let x = salt; for (let i = 0; i < n; i++) { x = (Math.imul(x, 1103515245) + 12345) >>> 0; b[i] = x >>> 24; } return b; };
  const small = Buffer.from(JSON.stringify({ name: "Wizard Hat", kind: "accessory", attachment: "HatAttachment", license: "CC0-1.0" }));
  const s1 = await assets.storeBytes(small, { mime: "application/json", signer: creator });
  ok(s1.chunks.length === 1 && s1.txs.length === 1, `small blob: 1 chunk, 1 tx, blobId ${s1.blobId}`);
  const big = seed(assets.MAX_CHUNK * 5 + 123, 7);   // 6 chunks -> storeMany x2 + publish
  const s2 = await assets.storeBytes(big, { mime: "application/octet-stream", signer: creator });
  const expectChunks = Math.ceil(big.length / assets.CHUNK_BYTES);
  ok(s2.chunks.length === expectChunks && s2.txs.length === assets.batches(assets.chunk(big)).length + 1,
     `big blob (${big.length} bytes): ${s2.chunks.length} chunks over ${s2.txs.length} txs, each within the ${assets.MAX_TX_DATA}-byte calldata limit, blobId ${s2.blobId}`);
  const f1 = await assets.fetchBlob(s1.uri);
  ok(Buffer.compare(f1.bytes, small) === 0 && f1.info.mime === "application/json" && f1.info.publisher === creator.address, "small blob reads back by URI, hash + mime + publisher intact");
  const f2 = await assets.fetchBlob(s2.blobId);
  ok(Buffer.compare(f2.bytes, big) === 0, "big blob reads back byte-exact through read()");
  const codes = []; for (const c of s2.chunks) codes.push(ethers.getBytes(await provider.getCode(c)));
  ok(codes.every((c) => c[0] === 0) && Buffer.compare(Buffer.concat(codes.map((c) => c.subarray(1))), big) === 0, "chunk code is STOP || data; eth_getCode assembly matches too");
  const dedupeId = await contracts.assetStore.blobOf(s2.contentHash);
  ok(dedupeId !== 0n && dedupeId <= s2.blobId && Buffer.compare((await assets.fetchBlob(dedupeId)).bytes, big) === 0,
     `blobOf(hash) -> blob ${dedupeId} with identical bytes (the first publish of this content, which on a persistent chain may predate this run)`);
  const edge = assets.CHUNK_BYTES;   // the first chunk boundary
  const range = ethers.getBytes(await contracts.assetStore.readRange(s2.blobId, edge - 10, 20));
  ok(Buffer.compare(range, big.subarray(edge - 10, edge + 10)) === 0, "readRange spans a chunk boundary");
  const wrongHash = ethers.keccak256(Buffer.from("not these bytes"));
  ok(await reverts(contracts.assetStore.connect(creator).publish.staticCall(wrongHash, small.length, "x", s1.chunks), /HashMismatch/), "publishing with a wrong hash reverts: the chain checks");
  ok(await reverts(contracts.assetStore.connect(creator).publish.staticCall(s1.contentHash, small.length + 1, "x", s1.chunks), /SizeMismatch/), "wrong size reverts");
  ok(await reverts(contracts.assetStore.connect(creator).store.staticCall(seed(assets.MAX_CHUNK + 1, 1)), /ChunkTooLarge/), "a chunk over 24,575 bytes is refused");
  ok(await reverts(contracts.assetStore.read.staticCall(999n), /UnknownBlob/), "unknown blob reverts");
  const parsed = assets.parseUri(s1.uri);
  ok(parsed.contentHash === s1.contentHash && parsed.chain.blobId === s1.blobId && parsed.mime === "application/json", "pblockz:// URI round-trips: " + s1.uri);

  console.log("12. a token whose metadata lives on chain");
  const metaId = await ugc.nextId();
  out = await relay(creator, ugc, "register", [s1.uri, 10, 0]);
  ok(out.status === 1 && (await ugc.uri(metaId)) === s1.uri, "registered with a pblockz:// uri; a client resolves it through AssetStore and verifies the hash");

  console.log("\nALL PASSED");
})().catch((e) => { console.error(e); process.exit(1); });

function ser(r) { return { ...r, value: r.value.toString(), gas: r.gas.toString(), nonce: r.nonce.toString() }; }
