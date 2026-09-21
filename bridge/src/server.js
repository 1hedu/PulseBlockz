// PulseBlockz Web3 Bridge — reference infrastructure anyone can run.
// Nothing on chain requires it: every contract call works from a wallet
// directly. It exists so a creator's game server has a wallet-auth and
// inventory helper, and so players can play without holding gas.
//   1. Wallet auth (sign-a-nonce) -> session token the game server trusts
//   2. Reads: inventory (which UGC does this wallet own?), listings
//   3. Gasless relay (verify signer's EIP-712 signature, pay gas, execute)
// Deliberately stateless except for in-memory nonces/rate limits; swap the
// Map()s for Redis when you go multi-instance.
const express = require("express");
const crypto = require("crypto");
const { ethers } = require("ethers");
const cfg = require("./config");
const { contracts, erc20, relayer, forwarderDomain, forwardTypes } = require("./chain");

const app = express();
app.use(express.json({ limit: "64kb" }));

// ---------------------------------------------------------------------------
// 1. Auth: POST /auth/nonce { address } -> { nonce, message }
//          POST /auth/verify { address, signature } -> { token, expires }
// The game server checks `token` via GET /auth/session?token=... (or share the
// HMAC secret and verify locally to avoid a hop).
const nonces = new Map();     // address -> { nonce, exp }
const sessions = new Map();   // token   -> { address, exp }
const sessionSecret = process.env.SESSION_SECRET || crypto.randomBytes(32).toString("hex");

app.post("/auth/nonce", (req, res) => {
  const address = safeAddr(req.body.address); if (!address) return bad(res, "bad address");
  const nonce = crypto.randomBytes(16).toString("hex");
  nonces.set(address, { nonce, exp: Date.now() + 5 * 60_000 });
  res.json({ nonce, message: loginMessage(address, nonce) });
});

app.post("/auth/verify", (req, res) => {
  const address = safeAddr(req.body.address); if (!address) return bad(res, "bad address");
  const n = nonces.get(address);
  if (!n || n.exp < Date.now()) return bad(res, "nonce expired");
  let recovered;
  try { recovered = ethers.verifyMessage(loginMessage(address, n.nonce), req.body.signature); } catch { return bad(res, "bad signature"); }
  if (recovered.toLowerCase() !== address.toLowerCase()) return bad(res, "signature mismatch");
  nonces.delete(address);
  const exp = Date.now() + cfg.sessionTtlSec * 1000;
  const token = crypto.createHmac("sha256", sessionSecret).update(`${address}:${exp}`).digest("hex");
  sessions.set(token, { address, exp });
  res.json({ token, address, expires: exp });
});

app.get("/auth/session", (req, res) => {
  const s = sessions.get(String(req.query.token));
  if (!s || s.exp < Date.now()) return res.status(401).json({ ok: false });
  res.json({ ok: true, address: s.address });
});

// ---------------------------------------------------------------------------
// 2. Inventory: GET /inventory/:address?ids=1,2,3&tokens=0xA,0xB
// The game server calls this on join and on purchase events. `ids` is the set
// of assets relevant to the current experience (don't scan the whole chain).
// `tokens` (optional) is the set of payment tokens the client wants balances
// for — there is no platform list; take them from /listing.
app.get("/inventory/:address", async (req, res) => {
  const address = safeAddr(req.params.address); if (!address) return bad(res, "bad address");
  const ids = String(req.query.ids || "").split(",").filter(Boolean).map(BigInt);
  if (ids.length === 0 || ids.length > 500) return bad(res, "pass 1-500 ids");
  try {
    const wanted = String(req.query.tokens || "").split(",").filter(Boolean).map(safeAddr);
    if (wanted.includes(null) || wanted.length > 50) return bad(res, "bad tokens");
    const balances = await contracts.ugc.balanceOfBatch(ids.map(() => address), ids);
    const owned = [];
    for (let i = 0; i < ids.length; i++) if (balances[i] > 0n) {
      const a = await contracts.ugc.asset(ids[i]);
      owned.push({ id: ids[i].toString(), qty: balances[i].toString(), uri: a.uri, creator: a.creator });
    }
    const tokens = await Promise.all(wanted.map(async (t) => {
      const c = erc20(t);
      const [symbol, decimals, bal] = await Promise.all([c.symbol(), c.decimals(), c.balanceOf(address)]);
      return { token: t, symbol, balance: ethers.formatUnits(bal, decimals) };
    }));
    res.json({ address, owned, tokens });
  } catch (e) { fail(res, e); }
});

// Listing: GET /listing/:id -> the tokens the creator accepts for this asset, with prices.
app.get("/listing/:id", async (req, res) => {
  let id; try { id = BigInt(req.params.id); } catch { return bad(res, "bad id"); }
  try {
    const [a, toks] = await Promise.all([contracts.ugc.asset(id), contracts.marketplace.tokensFor(id)]);
    const prices = await Promise.all(toks.map(async (t) => {
      const c = erc20(t);
      const [symbol, decimals, p] = await Promise.all([c.symbol(), c.decimals(), contracts.marketplace.price(id, t)]);
      return { token: t, symbol, price: ethers.formatUnits(p, decimals), priceRaw: p.toString() };
    }));
    res.json({ id: id.toString(), creator: a.creator, uri: a.uri, frozen: a.frozen, prices });
  } catch (e) { fail(res, e); }
});

// ---------------------------------------------------------------------------
// 3. Relay: POST /relay { request: {from,to,value,gas,nonce,deadline,data}, signature }
// Player signed this in-client for free; we pay gas. Guardrails:
//   - target+selector allowlist (can't relay arbitrary calls)
//   - gas cap, value must be 0
//   - per-wallet rate limit
//   - forwarder.verify() before spending gas
const allowedSelectors = buildAllowlist();
const rate = new Map(); // address -> timestamps[]

app.post("/relay", async (req, res) => {
  const { request: r, signature } = req.body || {};
  if (!r || !signature) return bad(res, "request + signature required");
  const from = safeAddr(r.from); if (!from) return bad(res, "bad from");
  if (BigInt(r.value || 0) !== 0n) return bad(res, "value must be 0");
  if (BigInt(r.gas) > cfg.relay.maxGasPerCall) return bad(res, "gas too high");

  const key = `${r.to.toLowerCase()}:${String(r.data).slice(0, 10).toLowerCase()}`;
  if (!allowedSelectors.has(key)) return bad(res, "call not allowed via relay");

  if (!rateOk(from)) return res.status(429).json({ error: "rate limited" });

  // Local signature check first (free), then on-chain verify (nonce/deadline).
  try {
    const rec = ethers.verifyTypedData(forwarderDomain, forwardTypes, r, signature);
    if (rec.toLowerCase() !== from.toLowerCase()) return bad(res, "signature mismatch");
  } catch { return bad(res, "bad typed-data signature"); }

  const fr = { from, to: r.to, value: 0n, gas: BigInt(r.gas), deadline: Number(r.deadline), data: r.data, signature };
  try {
    if (!(await contracts.forwarder.verify(fr))) return bad(res, "forwarder rejected (nonce/deadline/domain)");
    // Simulate the inner call as the signer before paying for it. The forwarder
    // swallows the target's revert data, so this is also how the client learns
    // *why* a call would fail (e.g. NotVerified, NotEarned).
    try {
      await relayer.provider.call({ from, to: r.to, data: r.data, gasLimit: fr.gas });
    } catch (e) { return bad(res, "call would revert: " + describeError(e)); }
    const tx = await contracts.forwarder.connect(relayer).execute(fr);
    const rc = await tx.wait();
    res.json({ txHash: rc.hash, status: rc.status });
  } catch (e) { fail(res, e); }
});

// Nonce helper so clients don't need their own forwarder call
app.get("/relay/nonce/:address", async (req, res) => {
  const address = safeAddr(req.params.address); if (!address) return bad(res, "bad address");
  res.json({ nonce: (await contracts.forwarder.nonces(address)).toString() });
});

app.get("/health", async (_req, res) => {
  const bal = await relayer.provider.getBalance(relayer.address);
  res.json({ chainId: cfg.chainId, relayer: relayer.address, relayerPls: ethers.formatEther(bal) });
});

// ---------------------------------------------------------------------------
function loginMessage(address, nonce) {
  return `PulseBlockz login\nWallet: ${address}\nNonce: ${nonce}\nThis signature only proves wallet ownership. It costs nothing and cannot move funds.`;
}
function safeAddr(a) { try { return ethers.getAddress(a); } catch { return null; } }
function bad(res, msg) { return res.status(400).json({ error: msg }); }
function fail(res, e) { console.error(e); return res.status(500).json({ error: describeError(e) }); }
// Decode custom errors from any of our contracts into "Name(args)".
const errorIface = new ethers.Interface([...Object.values(contracts), erc20(ethers.ZeroAddress)].flatMap((c) => c.interface.fragments.filter((f) => f.type === "error")));
function describeError(e) {
  const data = e.data || e.info?.error?.data || e.error?.data;
  if (typeof data === "string" && data.startsWith("0x") && data.length >= 10) {
    try { const p = errorIface.parseError(data); return `${p.name}(${p.args.map(String).join(",")})`; } catch {}
  }
  return e.shortMessage || e.message || String(e);
}
function rateOk(addr) {
  const now = Date.now(), win = 60_000;
  const arr = (rate.get(addr) || []).filter((t) => now - t < win);
  if (arr.length >= cfg.relay.perWalletPerMinute) return false;
  arr.push(now); rate.set(addr, arr); return true;
}
function buildAllowlist() {
  const set = new Set();
  const byName = { Marketplace: contracts.marketplace, UGC1155: contracts.ugc };
  for (const [name, sigs] of Object.entries(cfg.relay.allow)) {
    const c = byName[name];
    for (const s of sigs) set.add(`${cfg.addresses[name].toLowerCase()}:${c.interface.getFunction(s).selector.toLowerCase()}`);
  }
  return set;
}

if (require.main === module) {
  app.listen(cfg.port, () => console.log(`bridge on :${cfg.port} chain=${cfg.chainId} relayer=${relayer.address}`));
}
module.exports = app;
