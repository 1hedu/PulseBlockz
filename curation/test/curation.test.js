const test = require("node:test");
const assert = require("node:assert/strict");
const { ethers } = require("ethers");
const { signList, verifyList, isFresh, Curation, canonical, buildReport, fetchList } = require("../src/curation");

const REGISTRY = "0xd8058efe0198ae9dD7D563e1b4938Dcbc86A1F81";
const maintainer = ethers.Wallet.createRandom();
const other = ethers.Wallet.createRandom();
const badCreator = ethers.Wallet.createRandom().address;
const now = 1_757_000_000;

const body = (over = {}) => ({
  version: 1, mode: "block", chainId: 369, registry: REGISTRY, name: "community blocklist",
  description: "test", report: "https://example.org/report", updated: now, ttl: 86400,
  entries: [
    { kind: "asset", id: "7", reason: "copyright", note: "ripped model" },
    { kind: "creator", address: badCreator, reason: "scam" },
    { kind: "server", endpoint: "wss://bad.example:9000", reason: "malware" },
    { kind: "uri", prefix: "ipfs://bafyabuse", reason: "abuse" },
  ], ...over,
});

test("sign / verify round-trip; maintainer is the signer", async () => {
  const l = await signList(body(), maintainer);
  assert.equal(l.maintainer, maintainer.address);
  assert.equal(verifyList(l), l);
});

test("tampering with entries or metadata breaks the signature", async () => {
  const l = await signList(body(), maintainer);
  assert.throws(() => verifyList({ ...l, entries: [...l.entries, { kind: "asset", id: "8", reason: "spam" }] }), /signature/);
  assert.throws(() => verifyList({ ...l, name: "renamed" }), /signature/);
  assert.throws(() => verifyList({ ...l, mode: "allow" }), /signature/);
  assert.throws(() => verifyList({ ...l, maintainer: other.address }), /signature/);
});

test("canonical JSON is key-order independent (mirrors can re-serialize)", () => {
  assert.equal(canonical([{ b: 1, a: { d: "x", c: [1, 2] } }]), canonical([{ a: { c: [1, 2], d: "x" }, b: 1 }]));
});

test("shape validation rejects unknown kinds and reasons", async () => {
  await assert.rejects(signList(body({ entries: [{ kind: "wallet", address: badCreator, reason: "scam" }] }), maintainer), /kind/);
  await assert.rejects(signList(body({ entries: [{ kind: "asset", id: "1", reason: "ugly" }] }), maintainer), /reason/);
  await assert.rejects(signList(body({ mode: "deny" }), maintainer), /mode/);
});

test("block list hides matching assets, creators, servers and uri prefixes", async () => {
  const c = new Curation({ chainId: 369, registry: REGISTRY });
  assert.equal(c.add(await signList(body(), maintainer)), true);
  const good = { id: "1", creator: other.address, uri: "ipfs://bafygood/1.json" };
  assert.equal(c.hidden({ asset: good }), false);
  assert.equal(c.hidden({ asset: { ...good, id: "7" } }), true, "by id");
  assert.equal(c.hidden({ asset: { ...good, creator: badCreator.toLowerCase() } }), true, "by creator, case-insensitive");
  assert.equal(c.hidden({ asset: { ...good, uri: "ipfs://bafyabuse/x.json" } }), true, "by uri prefix");
  assert.equal(c.hidden({ server: "WSS://bad.example:9000" }), true, "server endpoint, case-insensitive");
  assert.equal(c.hidden({ server: "wss://fine.example" }), false);
  assert.equal(c.hidden({ creator: badCreator }), true, "creator alone (their servers too)");
  const d = c.decide({ asset: { ...good, id: "7" } });
  assert.equal(d.by[0].maintainer, maintainer.address);
  assert.equal(d.by[0].entry.reason, "copyright");
  assert.deepEqual(c.filterAssets([good, { ...good, id: "7" }]).map((a) => a.id), ["1"]);
});

test("allow list: only listed things show; a block still wins", async () => {
  const c = new Curation({ chainId: 369, registry: REGISTRY });
  c.add(await signList(body({ mode: "allow", entries: [{ kind: "creator", address: other.address, reason: "other" }] }), maintainer));
  const byOther = { id: "1", creator: other.address, uri: "ipfs://x" }, byNobody = { id: "2", creator: badCreator, uri: "ipfs://y" };
  assert.equal(c.hidden({ asset: byOther }), false);
  assert.equal(c.hidden({ asset: byNobody }), true, "not on any allow list");
  assert.equal(c.decide({ asset: byNobody }).allowRequired, true);
  const blocker = ethers.Wallet.createRandom();
  c.add(await signList(body({ entries: [{ kind: "asset", id: "1", reason: "scam" }] }), blocker));
  assert.equal(c.hidden({ asset: byOther }), true, "allowed by one list, blocked by another -> hidden");
});

test("newest list per maintainer wins; older or equal is ignored; enabled flag survives updates", async () => {
  const c = new Curation({ chainId: 369, registry: REGISTRY });
  c.add(await signList(body({ updated: now }), maintainer));
  c.setEnabled(maintainer.address, false);
  assert.equal(c.hidden({ asset: { id: "7", creator: other.address, uri: "" } }), false, "disabled list has no effect");
  assert.equal(c.add(await signList(body({ updated: now - 10 }), maintainer)), false, "older ignored");
  assert.equal(c.add(await signList(body({ updated: now + 10, entries: [] }), maintainer)), true, "newer replaces");
  c.setEnabled(maintainer.address, true);
  assert.equal(c.hidden({ asset: { id: "7", creator: other.address, uri: "" } }), false, "entries came from the newer list");
});

test("lists for another chain or registry never apply", async () => {
  const c = new Curation({ chainId: 369, registry: REGISTRY });
  assert.equal(c.add(await signList(body({ chainId: 943 }), maintainer)), false);
  assert.equal(c.add(await signList(body({ registry: other.address }), maintainer)), false);
  assert.equal(c.lists.size, 0);
});

test("freshness and report targets", async () => {
  const c = new Curation({ chainId: 369, registry: REGISTRY });
  const l = await signList(body(), maintainer);
  c.add(l);
  assert.equal(isFresh(l, now + 100), true);
  assert.equal(isFresh(l, now + 86401), false);
  assert.deepEqual(c.stale(now + 90000), [maintainer.address]);
  assert.deepEqual(c.reportTargets(), [{ maintainer: maintainer.address, url: "https://example.org/report" }]);
  const r = buildReport({ asset: { id: "7" } }, "copyright", "ripped", other.address);
  assert.equal(r.kind, "asset"); assert.equal(r.id, "7"); assert.equal(r.reporter, other.address);
});

test("fetchList verifies what a mirror serves", async () => {
  const l = await signList(body(), maintainer);
  const okFetch = async () => ({ ok: true, json: async () => JSON.parse(JSON.stringify(l)) });
  assert.equal((await fetchList("https://mirror/list.json", { fetchImpl: okFetch })).maintainer, maintainer.address);
  const tampered = async () => ({ ok: true, json: async () => ({ ...l, entries: [] }) });
  await assert.rejects(fetchList("https://mirror/list.json", { fetchImpl: tampered }), /signature/);
});
