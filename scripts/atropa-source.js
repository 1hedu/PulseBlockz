// Rebuilding a standard-json source set out of what an explorer hands back for a verified
// contract, for the atropa stack this project deploys copies of.
//
// A verified record is one "main" source plus a bag of extra files filed under names the explorer
// made up -- "/_1", "/", "/extensions/ERC20.sol" -- none of which are the paths the main file
// imports. Putting them back where the imports expect them is the whole job here.
//
// deploy-atropa.js still carries its own copy of this and should come here instead: the two
// drifting apart is exactly how atropaMath ended up deployed from 1.1 and verified against 1.0.

/// What a file declares, as the name after contract/library/interface.
const declaredIn = (src) => (src.match(/\b(?:abstract\s+contract|contract|library|interface)\s+(\w+)/g) || [])
  .map((d) => d.split(/\s+/).pop());

/// Every path a file imports, and the names it asked for by name.
function importsOf(src) {
  const out = [];
  for (const m of src.matchAll(/import\s+(?:\{([^}]*)\}\s+from\s+)?"([^"]+)"\s*;/g))
    out.push({ names: (m[1] || "").split(",").map((n) => n.trim().split(/\s+as\s+/)[0]).filter(Boolean), path: m[2] });
  return out;
}

/// A relative import resolved against the file that asked for it; anything else is left alone,
/// so "@openzeppelin/..." stays the key solc will look for.
function resolve(from, path) {
  if (!path.startsWith(".")) return path;
  const parts = from.split("/").slice(0, -1);
  for (const bit of path.split("/")) {
    if (bit === ".") continue;
    else if (bit === "..") parts.pop();
    else parts.push(bit);
  }
  return parts.join("/");
}

/// The standard-json sources for a verified record. Each import is matched to the file that
/// declares what the import asked for -- by the names in its braces, else by the basename.
/// Wrong guesses cannot pass unnoticed: the rebuilt runtime code is compared against the chain's
/// before anything is sent, and a file put in the wrong place does not compile at all.
function assemble(name, record) {
  const sources = { [name]: { content: record.source } };
  const spare = record.extra.slice();
  const queue = [[name, record.source]];
  while (queue.length) {
    const [file, src] = queue.shift();
    for (const imp of importsOf(src)) {
      const want = resolve(file, imp.path);
      if (sources[want]) continue;
      const wanted = imp.names.length ? imp.names : [want.split("/").pop().replace(/\.sol$/, "")];
      let at = spare.findIndex((s) => declaredIn(s).some((d) => wanted.includes(d)));
      // A file of nothing but constants -- addresses.sol is one -- declares no contract to match
      // on, so it is found by elimination, and only while exactly one such file is left.
      if (at < 0) {
        const bare = spare.map((s, i) => [s, i]).filter(([s]) => declaredIn(s).length === 0);
        if (bare.length !== 1) throw new Error(`${file} imports ${imp.path} and no verified source declares ${wanted.join(", ")}`);
        at = bare[0][1];
      }
      const [taken] = spare.splice(at, 1);
      sources[want] = { content: taken };
      queue.push([want, taken]);
    }
  }
  return sources;
}

/// The main source and its extras, as assemble() wants them.
const recordOf = (j) => ({ source: j.source_code, extra: (j.additional_sources || []).map((a) => a.source_code) });

module.exports = { declaredIn, importsOf, resolve, assemble, recordOf };
