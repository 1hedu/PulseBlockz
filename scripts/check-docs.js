// Checks the mechanically checkable claims in the repo's .md files: file paths, npm scripts,
// node scripts, --path and res:// targets, contracts/<Name>.sol, and 0x addresses against
// addresses.943.json. Findings print as file:line; exit 1 if there are any.
//
//   node scripts/check-docs.js           every .md
//   node scripts/check-docs.js README.md SERVER.md
const fs = require("fs"), path = require("path");

const ROOT = path.join(__dirname, "..");
const SKIP_DIRS = new Set(["node_modules", ".git", "builds", "build", ".godot", "promo", "out"]);
// Vendored whole: their docs describe their own tree, not this one.
const VENDORED = ["luau/luau/", "luau/secp256k1/", "luau/godot-cpp/", "thirdparty/"];

function docs(dir = ROOT, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (SKIP_DIRS.has(e.name)) continue;
    const full = path.join(dir, e.name);
    const rel = path.relative(ROOT, full).replace(/\\/g, "/");
    if (VENDORED.some((v) => rel.startsWith(v))) continue;
    if (e.isDirectory()) docs(full, out);
    else if (e.name.toLowerCase().endsWith(".md")) out.push(full);
  }
  return out;
}

const addresses = JSON.parse(fs.readFileSync(path.join(ROOT, "addresses.943.json"), "utf8"));
const pkg = JSON.parse(fs.readFileSync(path.join(ROOT, "package.json"), "utf8"));

/** Lowercased address -> the dotted name it is recorded under in addresses.943.json. */
function knownAddresses(o = addresses, prefix = "", out = new Map()) {
  for (const [k, v] of Object.entries(o)) {
    if (typeof v === "string" && /^0x[0-9a-fA-F]{40}$/.test(v)) out.set(v.toLowerCase(), prefix + k);
    else if (v && typeof v === "object") knownAddresses(v, k + ".", out);
  }
  return out;
}
const KNOWN = knownAddresses();

const exists = (rel) => fs.existsSync(path.join(ROOT, rel));

/// True if any file under node_modules has this basename. Walked once, then remembered.
let depFiles = null;
function inNodeModules(name) {
  const base = path.posix.basename(name);
  if (depFiles === null) {
    depFiles = new Set();
    const nm = path.join(ROOT, "node_modules");
    if (fs.existsSync(nm)) (function walk(d, depth) {
      if (depth > 6) return;
      for (const e of fs.readdirSync(d, { withFileTypes: true })) {
        const full = path.join(d, e.name);
        if (e.isDirectory()) walk(full, depth + 1);
        else depFiles.add(e.name);
      }
    })(nm, 0);
  }
  return depFiles.has(base);
}
/// Resolves `p` against the repo root, the doc's own directory, and any root the doc declares:
/// luau/README.md saying `gdextension/demo2` means luau/gdextension/demo2.
const existsNear = (p, file, roots = []) =>
  exists(p)
  || fs.existsSync(path.resolve(path.dirname(file), p))
  || roots.some((r) => exists(path.posix.join(r, p)));

// Never a path in this repo: URLs, packages, Roblox and Godot paths, absolute paths, globs.
const NOT_OURS = [
  /^https?:/, /^@/, /^user:\/\//, /^res:\/\//, /^pblockz:/, /^\//, /^[A-Z]:/,
  /^~/, /^\.\.?$/, /^game\./, /^workspace\./, /^rbx/, /^\$/, /\*/,
];

function check(file) {
  const rel = path.relative(ROOT, file).replace(/\\/g, "/");
  const text = fs.readFileSync(file, "utf8");
  const lines = text.split("\n");
  // Three docs declare their own root: SHOWCASE.md `luau/gdextension/demo2/scripts/src/`,
  // SAFETY.md and SERVER.md `luau/gdextension/`. Without this branch every relative path in
  // those three reads as a missing file.
  const declared = [];
  for (const m of text.matchAll(/[Pp]aths?[^`\n]{0,60}?(?:under|relative to)\s+`([^`]+)`/g)) declared.push(m[1]);
  const found = [];
  const say = (i, what) => found.push(`${rel}:${i + 1}  ${what}`);

  lines.forEach((line, i) => {
    // ---- repo paths in backticks, with or without a line number after them. A line number
    // is reported: it moves with every edit above it, and a wrong one reads worse than none.
    // Before this matched the suffix, a `file:123` was skipped entirely, and four wrong paths
    // sat behind line numbers through a whole docs pass.
    for (const m of line.matchAll(/`([^`\s:]+\.(?:js|cpp|h|gd|luau|lua|sol|json|md|ps1|sh|py|tscn))(:[\d,-]+)?`/g)) {
      const p = m[1];
      if (m[2]) say(i, `cites a line number, which moves: \`${p}${m[2]}\` -- name the function instead`);
      if (NOT_OURS.some((r) => r.test(p))) continue;
      if (existsNear(p, file, declared)) continue;
      // A bare name may be a suffix convention rather than a path: `model.json` and
      // `.meta.json` are how Rojo names a kind of file.
      if (!p.includes("/")) {
        let suffixed = false;
        (function walk(d) {
          if (suffixed) return;
          for (const e of fs.readdirSync(d, { withFileTypes: true })) {
            if (SKIP_DIRS.has(e.name) || suffixed) continue;
            const full = path.join(d, e.name);
            if (e.isDirectory()) walk(full);
            else if (e.name.endsWith(p) && e.name !== p) { suffixed = true; return; }
          }
        })(ROOT);
        if (suffixed) continue;
      }
      // A bare filename names a file anywhere in the tree; fail only if nothing matches.
      if (!p.includes("/")) {
        const hits = [];
        (function walk(d) {
          for (const e of fs.readdirSync(d, { withFileTypes: true })) {
            if (SKIP_DIRS.has(e.name)) continue;
            const full = path.join(d, e.name);
            if (e.isDirectory()) walk(full);
            else if (e.name === p) hits.push(full);
          }
        })(ROOT);
        if (hits.length) continue;
      }
      // A dependency's own file, named by name: OpenZeppelin's Bytes.sol and the like.
      if (inNodeModules(p)) continue;
      say(i, `no such file: ${p}`);
    }

    // ---- 0x addresses: must appear in addresses.943.json
    for (const m of line.matchAll(/0x[0-9a-fA-F]{40}\b/g)) {
      const a = m[0].toLowerCase();
      if (KNOWN.has(a)) continue;
      if (/0x0{40}/.test(a)) continue;                       // the zero address
      if (a === "0x000000000000000000000000000000000000dead") continue;   // the burn address
      if (/^0x[0-9a-f]{40}$/.test(a) && /example|e\.g\.|placeholder|0xStore|0xabc/i.test(line)) continue;
      say(i, `address is in no deployment record: ${m[0]}`);
    }

    // ---- npm run <script>
    for (const m of line.matchAll(/npm run ([a-z0-9:_-]+)/gi)) {
      if (!pkg.scripts || !(m[1] in pkg.scripts)) say(i, `package.json has no script "${m[1]}"`);
    }

    // ---- node scripts/<file>
    for (const m of line.matchAll(/node\s+(scripts\/[A-Za-z0-9._-]+\.js)/g)) {
      if (!existsNear(m[1], file, declared)) say(i, `no such script: ${m[1]}`);
    }

    // ---- godot --path <dir>
    for (const m of line.matchAll(/--path\s+([A-Za-z0-9._\/-]+)/g)) {
      const p = m[1];
      if (p === "." || NOT_OURS.some((r) => r.test(p))) continue;
      if (!existsNear(p, file, declared)) say(i, `no such directory: ${p}`);
    }

    // ---- res:// scripts the docs tell you to run
    for (const m of line.matchAll(/res:\/\/([A-Za-z0-9._\/-]+\.gd)/g)) {
      // res:// is relative to a project; accept it if any project has it.
      const projects = ["luau/gdextension/demo", "luau/gdextension/demo2", "luau/gdextension/studio",
        "luau/gdextension/player", "luau/gdextension/publisher"];
      const here = path.dirname(file);
      if (fs.existsSync(path.join(here, m[1]))) continue;                       // beside the doc
      if (fs.existsSync(path.join(here, "..", m[1]))) continue;                 // the project above it
      if (!projects.some((pr) => exists(path.posix.join(pr, m[1])))) say(i, `no project has res://${m[1]}`);
    }

    // ---- contracts named as contracts/<Name>.sol
    for (const m of line.matchAll(/contracts\/([A-Za-z0-9_]+)\.sol/g)) {
      if (!existsNear(`contracts/${m[1]}.sol`, file, declared)) say(i, `no such contract: contracts/${m[1]}.sol`);
    }
  });
  return found;
}

function main() {
  const only = process.argv.slice(2).filter((a) => !a.startsWith("-"));
  const files = only.length ? only.map((f) => path.join(ROOT, f)) : docs();
  let total = 0;
  for (const f of files.sort()) {
    const found = check(f);
    if (!found.length) continue;
    total += found.length;
    for (const l of found) console.log(l);
  }
  console.log(total === 0
    ? `\nchecked ${files.length} document(s): every path, script and address in them is real.`
    : `\n${total} finding(s) across ${files.length} document(s).`);
  process.exitCode = total ? 1 : 0;
}

main();
