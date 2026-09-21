// Verifies the town's contracts on the testnet v4 explorer: rebuild each from source, compare with
// the code on chain (metadata tail and immutable slots aside), submit the standard-JSON input. A
// contract whose file has changed since is looked for in git history, newest first.
//
//   node scripts/verify-contracts.js [--dry | --status] [--only Fishing,Announcements]
//
// Ours compile as scripts/compile.js does; atropaMath over mainnet's source, on the stack
// scripts/deploy-atropa.js pins -- ATROPA_COMPILER and ATROPA_SETTINGS below mirror it byte for byte.
const { ethers } = require("ethers");
const solc = require("solc");
const fs = require("fs"), path = require("path");
const { execFileSync } = require("child_process");
const { assemble, recordOf } = require("./atropa-source");

const ROOT = path.join(__dirname, "..");
const RPC = "https://rpc.v4.testnet.pulsechain.com";
const API = "https://api.scan.v4.testnet.pulsechain.com/api/v2/";
const MAINNET_API = "https://api.scan.pulsechain.com/api/v2/";
const OURS_COMPILER = "v0.8.28+commit.7893614a";
const ATROPA_COMPILER = "v0.8.21+commit.d9974bed";

const args = process.argv.slice(2);
const dry = args.includes("--dry");
const statusOnly = args.includes("--status");
const onlyAt = args.indexOf("--only");
const only = onlyAt >= 0 ? new Set(args[onlyAt + 1].split(",")) : null;

const provider = new ethers.JsonRpcProvider(RPC, 943, { staticNetwork: true });
const addrs = JSON.parse(fs.readFileSync(path.join(ROOT, "addresses.943.json"), "utf8"));

async function api(base, route, tries = 6) {
  let last;
  for (let i = 0; i < tries; i++) {
    try {
      const r = await fetch(base + route);
      if (r.ok) return r.json();
      // 404 is an answer -- nothing is verified at that address -- and not worth retrying.
      if (r.status === 404) { const e = new Error(`${route}: 404`); e.notFound = true; throw e; }
      last = new Error(`${route}: ${r.status}`);
    } catch (e) {
      if (e.notFound) throw e;
      last = e;
    }
    await new Promise((res) => setTimeout(res, 700 * (i + 1)));
  }
  throw last;
}

/// Runtime code with its CBOR metadata tail removed.
function stripMeta(hex) {
  const h = hex.replace(/^0x/, "").toLowerCase();
  const len = parseInt(h.slice(-4), 16);
  return len > 0 && len * 2 + 4 <= h.length ? h.slice(0, h.length - (len + 2) * 2) : h;
}
/// Positions at which two hex strings differ, over the length of the first.
function differences(a, b) {
  const out = [];
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) out.push(i);
  return out;
}

/// Where each Solidity metadata blob sits, as [from, to) in hex positions. A creation object
/// carries one per contract it embeds -- the Marketplace's own and the UGC1155 it deploys -- so
/// stripping the tail leaves the rest. Found by the CBOR map marker (`a2` then the key `ipfs` or
/// `bzzr0`) and confirmed by the blob's trailing two-byte length pointing back at that marker.
function metadataRanges(hex) {
  const h = hex.toLowerCase();
  const out = [];
  for (const marker of ["a264697066735822", "a265627a7a72"]) {
    let at = h.indexOf(marker);
    while (at >= 0) {
      for (let len = 30; len <= 200; len++) {
        const end = at + len * 2;
        if (end + 4 > h.length) break;
        if (parseInt(h.slice(end, end + 4), 16) === len) { out.push([at, end + 4]); break; }
      }
      at = h.indexOf(marker, at + 2);
    }
  }
  return out;
}

const inAny = (i, ranges) => ranges.some(([from, to]) => i >= from && i < to);

/// Zeroes the byte ranges deployment writes into: immutables, and a library's own address.
function blank(hex, ranges) {
  let h = hex;
  for (const { start, length } of ranges) h = h.slice(0, start * 2) + "0".repeat(length * 2) + h.slice((start + length) * 2);
  return h;
}

// ---- our own contracts ----------------------------------------------------------------------------

/// Resolves imports the way scripts/compile.js does, so the rebuild sees the same sources.
function importsResolver(fileContents) {
  return (p) => {
    if (fileContents[p] !== undefined) return { contents: fileContents[p] };
    for (const base of [path.join(ROOT, "contracts"), path.join(ROOT, "node_modules")]) {
      const full = path.join(base, p);
      if (fs.existsSync(full)) return { contents: fs.readFileSync(full, "utf8") };
    }
    return { error: "not found: " + p };
  };
}

const OURS_SETTINGS = { optimizer: { enabled: true, runs: 200 }, evmVersion: "shanghai" };

/// Compiles one contract's source text and returns its outputs plus the standard-JSON input to
/// submit, whose sources are pruned to the ones the contract's metadata lists.
function compileOurs(file, name, text) {
  const sources = { [file]: { content: text } };
  const input = { language: "Solidity", sources, settings: { ...OURS_SETTINGS,
    outputSelection: { "*": { "*": ["abi", "metadata", "evm.bytecode.object", "evm.deployedBytecode.object",
      "evm.deployedBytecode.immutableReferences"] } } } };
  const loaded = {};
  const resolver = importsResolver({});
  const out = JSON.parse(solc.compile(JSON.stringify(input), { import: (p) => { const r = resolver(p); if (r.contents) loaded[p] = r.contents; return r; } }));
  const errors = (out.errors || []).filter((e) => e.severity === "error");
  if (errors.length) throw new Error(errors.map((e) => e.formattedMessage).join("\n"));
  const c = out.contracts[file] && out.contracts[file][name];
  if (!c) throw new Error(`${name} is not in ${file}`);
  const meta = JSON.parse(c.metadata);
  const all = { [file]: text, ...loaded };
  const used = {};
  for (const src of Object.keys(meta.sources)) used[src] = { content: all[src] };
  const standard = { language: "Solidity", sources: used, settings: { ...OURS_SETTINGS,
    outputSelection: { "*": { "*": ["abi", "evm.bytecode", "evm.deployedBytecode", "metadata"] } } } };
  return { c, standard };
}

/// The versions of a file, newest first: the working tree, then each commit that changed it.
function versionsOf(file) {
  const rel = "contracts/" + file;
  const out = [{ label: "working tree", text: fs.readFileSync(path.join(ROOT, rel), "utf8") }];
  let log = "";
  try { log = execFileSync("git", ["log", "--format=%h", "--", rel], { cwd: ROOT, encoding: "utf8" }); } catch {}
  for (const sha of log.split("\n").filter(Boolean)) {
    try { out.push({ label: sha, text: execFileSync("git", ["show", `${sha}:${rel}`], { cwd: ROOT, encoding: "utf8" }) }); } catch {}
  }
  return out;
}

/// The code this address was created with, constructor arguments on the end. The explorer has it
/// even for a contract another contract made (UGC1155, by the Marketplace), whose transaction
/// input is its creator's.
async function creationCode(address) {
  // The only place the creation code is to be had: a contract the Marketplace deployed in its own
  // constructor has no transaction of its own to read it from. Worth waiting out a bad minute on
  // the explorer, because without it a contract with constructor arguments cannot be submitted.
  try {
    const sc = await api(API, "smart-contracts/" + address, 14);
    if (sc.creation_bytecode) return sc.creation_bytecode;
  } catch {}
  return null;
}

function show(v) {
  if (Array.isArray(v) || (v && typeof v.toArray === "function")) {
    const a = Array.from(v);
    return a.length > 3 ? `[${a.length} items]` : `[${a.map(show).join(",")}]`;
  }
  const s = String(v);
  return s.length > 44 ? s.slice(0, 20) + "…" : s;
}

async function prepareOurs(label, address, file, name) {
  const onChain = await provider.getCode(address);
  if (onChain === "0x") return { label, address, problem: "no code at this address" };
  const input = await creationCode(address);
  // The first version differing only inside metadata blobs: same instructions, source text edited.
  let metaOnly = null;
  for (const v of versionsOf(file)) {
    let built;
    try { built = compileOurs(file, name, v.text); } catch { continue; }
    const imm = Object.values(built.c.evm.deployedBytecode.immutableReferences || {}).flat();
    const mine = stripMeta(built.c.evm.deployedBytecode.object);
    const theirs = stripMeta(onChain);
    if (mine.length !== theirs.length || blank(mine, imm) !== blank(theirs, imm)) continue;
    const ctor = new ethers.Interface(built.c.abi).deploy;
    let ctorArgs = "", argsFrom = "";
    if (input) {
      const creation = built.c.evm.bytecode.object.toLowerCase();
      const data = input.replace(/^0x/, "").toLowerCase();
      // The creation code up to its metadata must be ours too, or the tail is not the arguments.
      const head = stripMeta(creation.slice(0, creation.length));
      if (data.slice(0, head.length) !== head) {
        // A metadata blob hashes the source TEXT, so an edited comment changes it while the
        // instructions stay identical. Differences confined to metadata blobs therefore mean
        // the same contract; one outside means a different one. An older version may still
        // match exactly, so keep this as a partial and go on looking.
        const off = differences(head, data);
        const outside = off.filter((i) => !inAny(i, metadataRanges(creation)));
        if (outside.length) {
          const at = off[0];
          const word = (h, i) => h.slice(i - (i % 2), i - (i % 2) + 64);
          return { label, address, problem: `creation code differs from the rebuild, from byte ${Math.floor(at / 2)} of ${head.length / 2}`
            + ` (${outside.length} differing byte(s) outside any metadata blob)`
            + `\n                  rebuilt  ${word(head, at)}\n                  on chain ${word(data, at)}` };
        }
        if (!metaOnly) metaOnly = { v, built, creation, data, bytes: off.length };
        continue;
      }
      ctorArgs = data.slice(creation.length);
      argsFrom = "creation code";
    } else if (ctor.inputs.length && name === "MockERC20") {
      // No creation record for this one: recover the arguments from the token's own getters.
      const t = new ethers.Contract(address, ["function name() view returns (string)", "function symbol() view returns (string)", "function decimals() view returns (uint8)"], provider);
      ctorArgs = ethers.AbiCoder.defaultAbiCoder().encode(["string", "string", "uint8"], [await t.name(), await t.symbol(), await t.decimals()]).slice(2);
      argsFrom = "its getters (explorer has no creation record)";
    } else if (ctor.inputs.length) {
      return { label, address, problem: "has constructor arguments and the explorer has no creation code for it" };
    }
    // A tail that does not re-encode byte for byte is not the constructor's arguments.
    let decoded = [];
    try {
      decoded = ethers.AbiCoder.defaultAbiCoder().decode(ctor.inputs, "0x" + ctorArgs);
      if (ethers.AbiCoder.defaultAbiCoder().encode(ctor.inputs, decoded).slice(2) !== ctorArgs) throw new Error("re-encodes differently");
    } catch (e) {
      return { label, address, problem: `constructor arguments do not decode (${e.shortMessage || e.message})` };
    }
    return { label, address, kind: "ours", compiler: OURS_COMPILER, name, file, source: v.label,
      standard: built.standard, contractName: `${file}:${name}`, ctorArgs, argsFrom,
      shownArgs: ctor.inputs.map((p, i) => `${p.name}=${show(decoded[i])}`).join(" ") };
  }
  if (metaOnly) {
    const { v, built, creation, data, bytes } = metaOnly;
    const ctor = new ethers.Interface(built.c.abi).deploy;
    const ctorArgs = data.slice(creation.length);
    let decoded = [];
    try {
      decoded = ethers.AbiCoder.defaultAbiCoder().decode(ctor.inputs, "0x" + ctorArgs);
      if (ethers.AbiCoder.defaultAbiCoder().encode(ctor.inputs, decoded).slice(2) !== ctorArgs) throw new Error("re-encodes differently");
    } catch (e) {
      return { label, address, problem: `constructor arguments do not decode (${e.shortMessage || e.message})` };
    }
    return { label, address, kind: "ours", compiler: OURS_COMPILER, name, file, source: `${v.label}, metadata only (${bytes / 2} byte(s))`,
      partial: true, standard: built.standard, contractName: `${file}:${name}`, ctorArgs, argsFrom: "creation code",
      shownArgs: ctor.inputs.map((p, i) => `${p.name}=${show(decoded[i])}`).join(" ") };
  }
  return { label, address, problem: `no version of contracts/${file} in the working tree or git history compiles to the code on chain` };
}

// ---- atropaMath's stack ------------------------------------------------------------------------------

const ATROPA_SETTINGS = { optimizer: { enabled: false, runs: 200 }, evmVersion: "shanghai" };
const MAINNET_RNG = "0xa96BcbeD7F01de6CEEd14fC86d90F21a36dE2143";
const MAINNET_MATH = "0xB680F0cc810317933F234f67EB6A9E923407f05D";   // atropaMath 1.1, the one deploy-atropa.js
// puts up. It pointed at 1.0 here, so the rebuild could not match what was deployed and the
// contract stayed unverifiable: the two scripts must name the same source or neither is right.

async function prepareAtropa(compiler) {
  const rngRecord = recordOf(await api(MAINNET_API, "smart-contracts/" + MAINNET_RNG));
  const mathRecord = recordOf(await api(MAINNET_API, "smart-contracts/" + MAINNET_MATH));
  const libs = addrs.atropaLibraries || {};
  const ours = { ...libs };
  // The copy here points at our own RNG, not mainnet's, and that one edit is the only difference
  // between what is deployed here and the source mainnet verified.
  const hardcoded = `RNG(${MAINNET_RNG})`;
  mathRecord.source = mathRecord.source.replace(hardcoded, `RNG(${ethers.getAddress(addrs.RNG)})`);
  if (mathRecord.source.includes(hardcoded) || !mathRecord.source.includes(ethers.getAddress(addrs.RNG)))
    throw new Error("atropaMath: the hardcoded RNG address was not where it was expected");
  const rngSources = assemble("RNG.sol", rngRecord);
  const mathSources = assemble("atropaMath.sol", mathRecord);
  const items = [];
  const one = async (label, address, file, sources, name, libraries) => {
    const settings = { ...ATROPA_SETTINGS, libraries: libraries ? { [file]: libraries } : {},
      outputSelection: { "*": { "*": ["abi", "evm.bytecode", "evm.deployedBytecode", "metadata"] } } };
    const standard = { language: "Solidity", sources, settings };
    const out = JSON.parse(compiler.compile(JSON.stringify(standard)));
    const bad = (out.errors || []).filter((e) => e.severity === "error");
    if (bad.length) throw new Error(`${label}: ${bad.map((e) => e.formattedMessage).join("\n")}`);
    const c = out.contracts[file] && out.contracts[file][name];
    if (!c) throw new Error(`${label}: ${name} is not in ${file}`);
    const onChain = (await provider.getCode(address)).slice(2).toLowerCase();
    // A library's code opens with a PUSH20 of its own address (the call guard): blank both sides.
    const guard = name !== "RNG" && name !== "atropaMath" || (file === "RNG.sol" && name === "atropaMath")
      ? [{ start: 1, length: 20 }] : [];
    const same = blank(stripMeta(c.evm.deployedBytecode.object), guard) === blank(stripMeta(onChain), guard);
    items.push(same
      ? { label, address, kind: "atropa", compiler: ATROPA_COMPILER, standard, contractName: `${file}:${name}`, ctorArgs: "", full: false, source: "mainnet's verified source" + (name === "atropaMath" && file === "atropaMath.sol" ? ", RNG address changed" : "") }
      : { label, address, problem: "the rebuild does not match the code on chain" });
  };
  await one("lib atropaMath", libs.atropaMath, "RNG.sol", rngSources, "atropaMath", null);
  await one("lib Conjecture", libs.Conjecture, "RNG.sol", rngSources, "Conjecture", { atropaMath: libs.atropaMath });
  await one("lib Dynamic", libs.Dynamic, "RNG.sol", rngSources, "Dynamic", { atropaMath: libs.atropaMath });
  await one("RNG", addrs.RNG, "RNG.sol", rngSources, "RNG", ours);
  await one("atropaMath", addrs.atropaMath, "atropaMath.sol", mathSources, "atropaMath", null);
  return items;
}

// ---- submitting --------------------------------------------------------------------------------------

async function isVerified(address) {
  try { return !!(await api(API, "smart-contracts/" + address)).is_verified; } catch { return false; }
}

/// The same question asked of the address endpoint, which answers when the other one is sulking.
async function isVerifiedEither(address) {
  if (await isVerified(address)) return true;
  try { return !!(await api(API, "addresses/" + address)).is_verified; } catch { return false; }
}

async function submit(item) {
  const form = new FormData();
  form.append("compiler_version", item.compiler);
  form.append("license_type", "none");   // ours is LicenseRef-PulseBlockz, which the explorer has no value for; the header in the source says
  form.append("autodetect_constructor_args", "false");
  form.append("constructor_args", item.ctorArgs);
  form.append("files[0]", new Blob([JSON.stringify(item.standard)], { type: "application/json" }), "input.json");
  const r = await fetch(`${API}smart-contracts/${item.address}/verification/via/standard-input`, { method: "POST", body: form });
  const text = await r.text();
  if (!r.ok) throw new Error(`${r.status} ${text.slice(0, 200)}`);
  for (let i = 0; i < 40; i++) {
    await new Promise((res) => setTimeout(res, 5000));
    if (await isVerifiedEither(item.address)) return "verified";
  }
  return "submitted, not verified yet: " + text.slice(0, 120);
}

function everything() {
  return [
    ["Forwarder", addrs.Forwarder], ["UGC1155", addrs.UGC1155], ["Marketplace", addrs.Marketplace],
    ["AssetStore", addrs.AssetStore], ["Inventory", addrs.Inventory], ["Duelling", addrs.Duelling],
    ["DuelRecords", addrs.DuelRecords], ["Announcements", addrs.Announcements], ["Fishing", addrs.Fishing],
    ["RNG", addrs.RNG], ["atropaMath", addrs.atropaMath],
    ["lib atropaMath", addrs.atropaLibraries.atropaMath], ["lib Conjecture", addrs.atropaLibraries.Conjecture],
    ["lib Dynamic", addrs.atropaLibraries.Dynamic],
    ["mUSD", addrs.Tokens.mUSD], ["mWPLS", addrs.Tokens.mWPLS],
  ];
}

/// What the explorer says about each, which is the answer that counts: a submission going
/// through is not the explorer having finished with it. Fully verified means the source hashes
/// to the metadata on chain too; partial means identical instructions, source text edited since.
async function status() {
  let done = 0;
  for (const [label, address] of everything()) {
    if (only && !only.has(label)) continue;
    let says = "not yet";
    try {
      const r = await api(API, "smart-contracts/" + address);
      if (r.is_verified) { done++; says = r.is_fully_verified ? "verified" : "verified (partial: source edited since, same code)"; }
    } catch (e) { says = e.notFound ? "not yet" : "the explorer did not answer"; }
    console.log(`${label.padEnd(15)} ${address}  ${says}`);
  }
  console.log(`\n${done} verified`);
}

async function main() {
  if (statusOnly) return status();
  const OURS = [
    ["Inventory", addrs.Inventory, "Inventory.sol", "Inventory"],
    ["Marketplace", addrs.Marketplace, "Marketplace.sol", "Marketplace"],
    ["UGC1155", addrs.UGC1155, "UGC1155.sol", "UGC1155"],
    ["AssetStore", addrs.AssetStore, "AssetStore.sol", "AssetStore"],
    ["Forwarder", addrs.Forwarder, "Forwarder.sol", "Forwarder"],
    ["Duelling", addrs.Duelling, "Duelling.sol", "Duelling"],
    ["DuelRecords", addrs.DuelRecords, "DuelRecords.sol", "DuelRecords"],
    ["Announcements", addrs.Announcements, "Announcements.sol", "Announcements"],
    ["Fishing", addrs.Fishing, "Fishing.sol", "Fishing"],
    ["mUSD", addrs.Tokens.mUSD, "MockERC20.sol", "MockERC20"],
    ["mWPLS", addrs.Tokens.mWPLS, "MockERC20.sol", "MockERC20"],
  ];
  const items = [];
  for (const [label, address, file, name] of OURS) {
    if (only && !only.has(label)) continue;
    items.push(await prepareOurs(label, address, file, name));
  }
  if (!only || [...only].some((l) => /atropa|RNG|lib /.test(l))) {
    const compiler = await new Promise((res, rej) => solc.loadRemoteVersion(ATROPA_COMPILER, (e, c) => (e ? rej(e) : res(c))));
    for (const it of await prepareAtropa(compiler)) if (!only || only.has(it.label)) items.push(it);
  }

  let ok = 0;
  for (const it of items) {
    const already = !it.problem && await isVerifiedEither(it.address);
    if (it.problem) {
      console.log(`${it.label.padEnd(15)} ${it.address}  SKIP -- ${it.problem}`);
      continue;
    }
    const where = `${it.contractName}, ${it.compiler.split("+")[0]}, source: ${it.source}${it.shownArgs ? `; args from ${it.argsFrom}: ${it.shownArgs}` : ""}`;
    if (already) { console.log(`${it.label.padEnd(15)} ${it.address}  already verified`); ok++; continue; }
    if (dry) { console.log(`${it.label.padEnd(15)} ${it.address}  matches -- would submit (${where})`); ok++; continue; }
    try {
      const result = await submit(it);
      console.log(`${it.label.padEnd(15)} ${it.address}  ${result} (${where})`);
      if (result === "verified") ok++;
    } catch (e) {
      console.log(`${it.label.padEnd(15)} ${it.address}  FAILED -- ${e.message}`);
    }
  }
  console.log(`\n${ok} of ${items.length} ${dry ? "ready" : "verified"}.`);
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
