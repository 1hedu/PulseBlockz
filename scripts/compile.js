// Compiles contracts/ with bundled solc-js (no compiler download needed) and
// writes ABI + bytecode to artifacts/. Usage: node scripts/compile.js
const fs = require("fs"), path = require("path"), solc = require("solc");
const root = path.join(__dirname, "..");
const sources = {};
for (const f of fs.readdirSync(path.join(root, "contracts")))
  if (f.endsWith(".sol")) sources[f] = { content: fs.readFileSync(path.join(root, "contracts", f), "utf8") };

function findImports(p) {
  for (const base of [path.join(root, "contracts"), path.join(root, "node_modules")]) {
    const full = path.join(base, p);
    if (fs.existsSync(full)) return { contents: fs.readFileSync(full, "utf8") };
  }
  return { error: "not found: " + p };
}
const input = { language: "Solidity", sources,
  // Shanghai: PulseChain mainnet and testnet v4 have PUSH0 but not Cancun's MCOPY/TSTORE.
  // Shanghai bytecode also runs on a Cancun chain; EVM_VERSION overrides.
  settings: { optimizer: { enabled: true, runs: 200 }, evmVersion: process.env.EVM_VERSION || "shanghai",
    outputSelection: { "*": { "*": ["abi", "evm.bytecode.object"] } } } };
const out = JSON.parse(solc.compile(JSON.stringify(input), { import: findImports }));
const errs = (out.errors || []).filter(e => e.severity === "error");
for (const e of out.errors || []) console.error(e.formattedMessage);
if (errs.length) process.exit(1);
fs.mkdirSync(path.join(root, "artifacts"), { recursive: true });
for (const f of Object.keys(sources)) for (const [name, c] of Object.entries(out.contracts[f] || {}))
  fs.writeFileSync(path.join(root, "artifacts", name + ".json"),
    JSON.stringify({ contractName: name, abi: c.abi, bytecode: "0x" + c.evm.bytecode.object }, null, 2));
console.log("compiled:", Object.keys(sources).join(", "));
