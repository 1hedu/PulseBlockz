// Copies luau/gdextension/host, the source of truth, into every Godot project that has a host/
// folder: a project's res:// cannot reach outside itself, so the wallet, the asset fetcher and
// the curation lists must be physically present in each. --check, run by the suite, fails on drift.
// luau/gdextension/SConstruct:107 fans the compiled library out the same way, for the same reason:
// a project left out of either fan-out runs against a stale copy.
const fs = require("fs"), path = require("path");

const ROOT = path.join(__dirname, "..");
const SRC = path.join(ROOT, "luau", "gdextension", "host");
const PROJECTS = path.join(ROOT, "luau", "gdextension");
const BANNER = (name) =>
  `# COPY -- do not edit. The original is luau/gdextension/host/${name}; this was put here by\n` +
  `# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.\n` +
  `# Edit the original and run: node scripts/sync-host.js\n`;

const stripBanner = (text) =>
  text.startsWith("# COPY -- do not edit.")
    ? text.slice(text.indexOf("\n", text.indexOf("node scripts/sync-host.js")) + 1)
    : text;

function targets() {
  return fs.readdirSync(PROJECTS, { withFileTypes: true })
    .filter((d) => d.isDirectory() && fs.existsSync(path.join(PROJECTS, d.name, "project.godot")))
    .map((d) => path.join(PROJECTS, d.name, "host"))
    .filter((dir) => fs.existsSync(dir));
}

function main() {
  const check = process.argv.includes("--check");
  const files = fs.readdirSync(SRC).filter((f) => f.endsWith(".gd")).sort();
  // Pictures the host layer draws with, copied as they are: bytes carry no banner.
  const pictures = fs.readdirSync(SRC).filter((f) => f.endsWith(".png")).sort();
  const dirs = targets();
  if (dirs.length === 0) {
    console.log("no project has a host/ folder yet -- nothing to do");
    return;
  }
  let wrong = 0, wrote = 0;
  for (const dir of dirs) {
    const project = path.basename(path.dirname(dir));
    // A .gd the source no longer has still loads in the project, so it goes.
    for (const stale of fs.readdirSync(dir).filter((f) => f.endsWith(".gd") && !files.includes(f))) {
      if (check) { console.log(`  ${project}/host/${stale}  <-- not in host/ any more`); wrong++; }
      else { fs.unlinkSync(path.join(dir, stale)); console.log(`  ${project}/host/${stale}  removed`); }
    }
    for (const f of pictures) {
      const want = fs.readFileSync(path.join(SRC, f));
      const at = path.join(dir, f);
      if (fs.existsSync(at) && fs.readFileSync(at).equals(want)) continue;
      if (check) { console.log(`  ${project}/host/${f}  <-- ${fs.existsSync(at) ? "DRIFTED" : "missing"}`); wrong++; }
      else { fs.writeFileSync(at, want); console.log(`  ${project}/host/${f}`); wrote++; }
    }
    for (const f of files) {
      const want = BANNER(f) + fs.readFileSync(path.join(SRC, f), "utf8");
      const at = path.join(dir, f);
      const have = fs.existsSync(at) ? fs.readFileSync(at, "utf8") : null;
      if (have === want) continue;
      if (check) {
        const why = have === null ? "missing" : stripBanner(have) === stripBanner(want) ? "banner differs" : "DRIFTED";
        console.log(`  ${project}/host/${f}  <-- ${why}`);
        wrong++;
      } else {
        fs.writeFileSync(at, want);
        console.log(`  ${project}/host/${f}`);
        wrote++;
      }
    }
  }
  if (check) {
    if (wrong > 0) {
      console.error(`\n${wrong} copy/copies out of date. Run: node scripts/sync-host.js`);
      process.exit(1);
    }
    console.log(`${files.length} file(s) match across ${dirs.length} project(s)`);
  } else {
    console.log(`${wrote} written, ${dirs.length} project(s)`);
  }
}

main();
