// PulseBlockz curation lists — reference implementation of SPEC.md.
// Sign (maintainer side), verify + merge + match (client side). Engine-agnostic Node;
// the Godot client mirrors the client half in luau/gdextension/demo/Curation.gd.
const { ethers } = require("ethers");

const DOMAIN = { name: "PulseBlockzCuration", version: "1" };
const TYPES = { CurationList: [
  { name: "mode", type: "string" }, { name: "chainId", type: "uint256" }, { name: "registry", type: "address" },
  { name: "name", type: "string" }, { name: "updated", type: "uint64" }, { name: "ttl", type: "uint64" },
  { name: "entriesHash", type: "bytes32" },
] };
const KINDS = new Set(["asset", "creator", "server", "uri"]);
const REASONS = new Set(["abuse", "copyright", "scam", "malware", "spam", "other"]);

function canonical(v) {
  if (Array.isArray(v)) return "[" + v.map(canonical).join(",") + "]";
  if (v && typeof v === "object") return "{" + Object.keys(v).sort().map((k) => JSON.stringify(k) + ":" + canonical(v[k])).join(",") + "}";
  return JSON.stringify(v);
}
const entriesHash = (entries) => ethers.keccak256(ethers.toUtf8Bytes(canonical(entries)));
const message = (l) => ({ mode: l.mode, chainId: BigInt(l.chainId), registry: ethers.getAddress(l.registry), name: l.name, updated: BigInt(l.updated), ttl: BigInt(l.ttl), entriesHash: entriesHash(l.entries) });

function validateShape(l) {
  const bad = (m) => { throw new Error("invalid curation list: " + m); };
  if (l.version !== 1) bad("version must be 1");
  if (l.mode !== "block" && l.mode !== "allow") bad("mode must be block|allow");
  if (!Number.isInteger(l.chainId) || l.chainId <= 0) bad("chainId");
  if (!ethers.isAddress(l.registry)) bad("registry");
  if (!ethers.isAddress(l.maintainer)) bad("maintainer");
  if (typeof l.name !== "string" || !l.name) bad("name");
  if (!Number.isInteger(l.updated) || !Number.isInteger(l.ttl) || l.ttl < 0) bad("updated/ttl");
  if (!Array.isArray(l.entries)) bad("entries");
  for (const e of l.entries) {
    if (!KINDS.has(e.kind)) bad("entry kind " + e.kind);
    if (!REASONS.has(e.reason)) bad("entry reason " + e.reason);
    if (e.kind === "asset" && !/^\d+$/.test(String(e.id))) bad("asset id");
    if (e.kind === "creator" && !ethers.isAddress(e.address)) bad("creator address");
    if (e.kind === "server" && (typeof e.endpoint !== "string" || !e.endpoint)) bad("server endpoint");
    if (e.kind === "uri" && (typeof e.prefix !== "string" || !e.prefix)) bad("uri prefix");
  }
}

/** Maintainer side: sign a list body (everything but `signature`) with an ethers Signer. */
async function signList(body, signer) {
  const l = { ...body, maintainer: await signer.getAddress() };
  validateShape({ ...l, signature: "" });
  const signature = await signer.signTypedData(DOMAIN, TYPES, message(l));
  return { ...l, signature };
}

/** Client side: shape + signature check. Throws on any problem. Returns the list. */
function verifyList(l) {
  validateShape(l);
  if (typeof l.signature !== "string") throw new Error("invalid curation list: signature");
  const signer = ethers.verifyTypedData(DOMAIN, TYPES, message(l), l.signature);
  if (signer.toLowerCase() !== l.maintainer.toLowerCase()) throw new Error("curation list signature does not match maintainer");
  return l;
}

const isFresh = (l, now = Math.floor(Date.now() / 1000)) => now <= l.updated + l.ttl;

/**
 * Client side: the merged view over a set of verified lists.
 *   subject: { asset?: {id, creator, uri}, creator?: address, server?: endpoint }
 * A subject is hidden if any enabled block list matches it, or if allow lists exist and none matches.
 */
class Curation {
  constructor({ chainId, registry }) {
    this.chainId = chainId; this.registry = ethers.getAddress(registry);
    this.lists = new Map(); // maintainer(lower) -> { list, enabled }
  }
  /** Adds a verified list; keeps the newest per maintainer; ignores lists for other registries. */
  add(list, { enabled = true } = {}) {
    verifyList(list);
    if (list.chainId !== this.chainId || ethers.getAddress(list.registry) !== this.registry) return false;
    const key = list.maintainer.toLowerCase();
    const cur = this.lists.get(key);
    if (cur && cur.list.updated >= list.updated) return false;
    this.lists.set(key, { list, enabled: cur ? cur.enabled : enabled });
    return true;
  }
  setEnabled(maintainer, enabled) { const e = this.lists.get(maintainer.toLowerCase()); if (e) e.enabled = enabled; }
  remove(maintainer) { this.lists.delete(maintainer.toLowerCase()); }
  stale(now) { return [...this.lists.values()].filter((e) => !isFresh(e.list, now)).map((e) => e.list.maintainer); }

  /** Which entries of `list` match `subject`. */
  static matches(list, subject) {
    const out = [];
    const a = subject.asset, creator = (subject.creator || a?.creator || "").toLowerCase(), server = (subject.server || "").toLowerCase();
    for (const e of list.entries) {
      if (e.kind === "asset" && a && String(e.id) === String(a.id)) out.push(e);
      else if (e.kind === "creator" && creator && e.address.toLowerCase() === creator) out.push(e);
      else if (e.kind === "server" && server && e.endpoint.toLowerCase() === server) out.push(e);
      else if (e.kind === "uri" && a?.uri && a.uri.startsWith(e.prefix)) out.push(e);
    }
    return out;
  }

  /** { hidden: bool, by: [{maintainer, mode, entry}], allowRequired: bool } */
  decide(subject) {
    const by = []; let allowRequired = false, allowed = false;
    for (const { list, enabled } of this.lists.values()) {
      if (!enabled) continue;
      const m = Curation.matches(list, subject);
      if (list.mode === "block") { for (const entry of m) by.push({ maintainer: list.maintainer, mode: "block", entry }); }
      else { allowRequired = true; if (m.length) { allowed = true; for (const entry of m) by.push({ maintainer: list.maintainer, mode: "allow", entry }); } }
    }
    const blocked = by.some((b) => b.mode === "block");
    return { hidden: blocked || (allowRequired && !allowed), by, allowRequired };
  }
  hidden(subject) { return this.decide(subject).hidden; }
  /** Filter an /inventory `owned` array (each {id, creator, uri}). */
  filterAssets(assets) { return assets.filter((a) => !this.hidden({ asset: a })); }

  /** Report URLs of enabled lists that accept reports. */
  reportTargets() { return [...this.lists.values()].filter((e) => e.enabled && e.list.report).map((e) => ({ maintainer: e.list.maintainer, url: e.list.report })); }
}

/** Fetch + verify a list from a URL (Node 18+). Returns the list or throws. */
async function fetchList(url, { fetchImpl = globalThis.fetch } = {}) {
  const r = await fetchImpl(url, { headers: { accept: "application/json" } });
  if (!r.ok) throw new Error(`curation list ${url}: HTTP ${r.status}`);
  return verifyList(await r.json());
}

/** Build a report payload for a subject; the client POSTs it to each reportTargets() url. */
function buildReport(subject, reason, note = "", reporter = undefined) {
  if (!REASONS.has(reason)) throw new Error("bad reason");
  const kind = subject.asset ? "asset" : subject.creator ? "creator" : "server";
  const ref = subject.asset ? { id: String(subject.asset.id) } : subject.creator ? { address: subject.creator } : { endpoint: subject.server };
  return { version: 1, kind, ...ref, reason, note, ...(reporter ? { reporter } : {}), at: Math.floor(Date.now() / 1000) };
}

module.exports = { DOMAIN, TYPES, KINDS, REASONS, canonical, entriesHash, signList, verifyList, isFresh, Curation, fetchList, buildReport };
