#!/usr/bin/env node
// PulseBlockz script sync — Rojo-style hot reload for creator scripts.
//
//   node luau/tools/sync.js [dir | project.json] [--port 8790]
//
// Watches a scripts tree and serves it over a local WebSocket. A client
// (ScriptSync.gd in the demo) gets every script on connect and each one again the
// moment it is saved; a deleted file unloads its script. Script output (print,
// load errors, budget kills) comes back over the same socket and is printed here,
// so the creator's terminal is the console for the running client.
//
// The tree is laid out like a Rojo project. With a `default.project.json` (or a
// *.project.json given directly) the project's `tree` says which directory is
// which service — {"ServerScriptService": {"$path": "src/server"}} — exactly as
// Rojo reads it; files outside every $path are ignored. Without one, the
// directory itself is the DataModel: ServerScriptService/Main.server.luau. Either
// way a script's name on the wire is its path under the DataModel, and the world
// turns that into the Instance (rbx_host.h).
//
// Wire protocol: one JSON object per text frame.
//   server -> client   {op:"load", name, source}      load or replace a script
//                      {op:"unload", name}            remove it
//   client -> server   {op:"log", level, name, text}  level: print|error|killed|info
//
// No dependencies: the WebSocket server below is the ~60 lines of RFC 6455 a
// localhost dev tool needs (text frames, ping/pong, close). Node 20+.
"use strict";
const fs = require("fs");
const path = require("path");
const http = require("http");
const crypto = require("crypto");

const args = process.argv.slice(2);
let dir = ".";
let port = 8790;
for (let i = 0; i < args.length; i++) {
  if (args[i] === "--port") port = Number(args[++i]);
  else if (args[i] === "-h" || args[i] === "--help") { usage(); process.exit(0); }
  else dir = args[i];
}
dir = path.resolve(dir);
let projectFile = null;
if (fs.existsSync(dir) && fs.statSync(dir).isFile()) { projectFile = dir; dir = path.dirname(dir); }
else if (fs.existsSync(path.join(dir, "default.project.json"))) projectFile = path.join(dir, "default.project.json");
if (!fs.existsSync(dir) || !fs.statSync(dir).isDirectory()) {
  console.error(`not a directory: ${dir}`);
  usage();
  process.exit(1);
}

// ---- Rojo project: $path mounts -> instance paths ---------------------------------
// mounts: [{disk (absolute), inst ("ServerScriptService" / "Workspace/Map"), isFile}]
// longest disk path first, so a nested mount wins over the one it sits in.
// A node's $className / $properties / $attributes are the instance's meta.json
// (projectMetas: instance path -> JSON; a directory's own init.meta.json wins).
// Throws on a bad project file; the caller decides whether that is fatal.
const projectMetas = new Map();
function readMounts() {
  if (!projectFile) return [{ disk: dir, inst: "", isFile: false }];
  const project = JSON.parse(fs.readFileSync(projectFile, "utf8"));
  const base = path.dirname(projectFile);
  const out = [];
  projectMetas.clear();
  (function visit(node, inst) {
    if (!node || typeof node !== "object") return;
    let isFile = false;
    if (typeof node.$path === "string") {
      const disk = path.resolve(base, node.$path);
      try { isFile = fs.statSync(disk).isFile(); } catch { /* missing: watched anyway */ }
      out.push({ disk, inst, isFile });
    }
    const meta = {};
    if ("$className" in node) meta.className = node.$className;
    if ("$properties" in node) meta.properties = node.$properties;
    if ("$attributes" in node) meta.attributes = node.$attributes;
    if (inst && Object.keys(meta).length) projectMetas.set(isFile ? `${inst}.meta.json` : `${inst}/init.meta.json`, JSON.stringify(meta));
    for (const [k, v] of Object.entries(node)) if (!k.startsWith("$")) visit(v, inst ? `${inst}/${k}` : k);
  })(project.tree, "");
  out.sort((a, b) => b.disk.length - a.disk.length);
  if (!out.length) throw new Error("no $path in tree");
  return out;
}
let mounts;
try { mounts = readMounts(); }
catch (e) { console.error(`${projectFile}: ${e.message}`); process.exit(1); }

const SCRIPT_EXT = /\.(server|client)\.luau?$|\.luau?$|\.(meta|model)\.json$|\.rbxmx?$/;
// A file's source as the runtime takes it: text, or a binary .rbxm as base64.
const readSource = (file) => file.endsWith(".rbxm") ? fs.readFileSync(file).toString("base64") : fs.readFileSync(file, "utf8");
function isScriptFile(name) { return SCRIPT_EXT.test(name); }

// A file's path under the DataModel, or null if no mount covers it. A file mount
// ("Main": {"$path": "src/main.server.luau"}) keeps its class suffix and takes
// the tree's name.
function instancePath(file) {
  for (const m of mounts) {
    if (m.isFile || file === m.disk) {
      if (file !== m.disk) continue;
      const suffix = (file.match(SCRIPT_EXT) || [""])[0];
      return m.inst + suffix;
    }
    const rel = path.relative(m.disk, file);
    if (rel.startsWith("..") || path.isAbsolute(rel)) continue;
    const under = rel.split(path.sep).join("/");
    return m.inst ? `${m.inst}/${under}` : under;
  }
  return null;
}

function usage() {
  console.error("usage: node luau/tools/sync.js [dir | project.json] [--port 8790]   (dir defaults to .)");
}

// ---- minimal WebSocket server ---------------------------------------------------
const WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const clients = new Set();

function encodeText(str) {
  const payload = Buffer.from(str, "utf8");
  let header;
  if (payload.length < 126) {
    header = Buffer.from([0x81, payload.length]);
  } else if (payload.length < 65536) {
    header = Buffer.alloc(4); header[0] = 0x81; header[1] = 126; header.writeUInt16BE(payload.length, 2);
  } else {
    header = Buffer.alloc(10); header[0] = 0x81; header[1] = 127; header.writeBigUInt64BE(BigInt(payload.length), 2);
  }
  return Buffer.concat([header, payload]);
}

// Parses as many complete frames as `buf` holds. Returns [frames, remainder].
function decodeFrames(buf) {
  const frames = [];
  let off = 0;
  for (;;) {
    if (buf.length - off < 2) break;
    const b0 = buf[off], b1 = buf[off + 1];
    const opcode = b0 & 0x0f;
    const masked = (b1 & 0x80) !== 0;
    let len = b1 & 0x7f;
    let p = off + 2;
    if (len === 126) { if (buf.length - p < 2) break; len = buf.readUInt16BE(p); p += 2; }
    else if (len === 127) { if (buf.length - p < 8) break; len = Number(buf.readBigUInt64BE(p)); p += 8; }
    let mask = null;
    if (masked) { if (buf.length - p < 4) break; mask = buf.subarray(p, p + 4); p += 4; }
    if (buf.length - p < len) break;
    const payload = Buffer.from(buf.subarray(p, p + len));
    if (mask) for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
    frames.push({ opcode, payload });
    off = p + len;
  }
  return [frames, buf.subarray(off)];
}

const server = http.createServer((req, res) => {
  res.writeHead(426, { "Content-Type": "text/plain" });
  res.end("PulseBlockz script sync: WebSocket only\n");
});

server.on("upgrade", (req, socket) => {
  const key = req.headers["sec-websocket-key"];
  if (!key || (req.headers.upgrade || "").toLowerCase() !== "websocket") { socket.destroy(); return; }
  const accept = crypto.createHash("sha1").update(key + WS_GUID).digest("base64");
  socket.write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
               `Sec-WebSocket-Accept: ${accept}\r\n\r\n`);
  socket.setNoDelay(true);
  clients.add(socket);
  const who = `${socket.remoteAddress}:${socket.remotePort}`;
  log("info", `client connected (${who}); sending ${scripts.size} script(s)`);
  for (const [name, source] of scripts) socket.write(encodeText(JSON.stringify({ op: "load", name, source })));

  let pending = Buffer.alloc(0);
  socket.on("data", (chunk) => {
    pending = Buffer.concat([pending, chunk]);
    const [frames, rest] = decodeFrames(pending);
    pending = rest;
    for (const f of frames) {
      if (f.opcode === 0x1) onClientMessage(f.payload.toString("utf8"));
      else if (f.opcode === 0x9) socket.write(Buffer.concat([Buffer.from([0x8a, f.payload.length]), f.payload])); // ping -> pong
      else if (f.opcode === 0x8) socket.end(Buffer.from([0x88, 0]));
    }
  });
  const drop = () => { if (clients.delete(socket)) log("info", `client disconnected (${who})`); };
  socket.on("close", drop);
  socket.on("error", drop);
});

function broadcast(msg) {
  const frame = encodeText(JSON.stringify(msg));
  for (const s of clients) s.write(frame);
}

// ---- console -------------------------------------------------------------------
const COLORS = { print: "\x1b[36m", error: "\x1b[31m", killed: "\x1b[33m", info: "\x1b[90m", sync: "\x1b[32m" };
function log(level, text, name) {
  const t = new Date().toTimeString().slice(0, 8);
  const tag = name ? `${name}: ` : "";
  const c = process.stdout.isTTY ? COLORS[level] || "" : "";
  const r = c ? "\x1b[0m" : "";
  console.log(`${c}${t} [${level}] ${tag}${text}${r}`);
}

function onClientMessage(text) {
  let msg;
  try { msg = JSON.parse(text); } catch { return; }
  if (msg && msg.op === "log") log(String(msg.level || "info"), String(msg.text ?? ""), msg.name ? String(msg.name) : "");
}

// ---- files ---------------------------------------------------------------------
const scripts = new Map();   // name (relative path, forward slashes) -> source

function* walk(d) {
  let ents;
  try { ents = fs.readdirSync(d, { withFileTypes: true }); } catch { return; }
  for (const ent of ents) {
    const p = path.join(d, ent.name);
    if (ent.isDirectory()) { if (!ent.name.startsWith(".")) yield* walk(p); }
    else if (ent.isFile() && isScriptFile(ent.name)) yield p;
  }
}

// Every script file the mounts cover.
function* scriptFiles() {
  for (const m of mounts) {
    if (m.isFile) { if (fs.existsSync(m.disk)) yield m.disk; }
    else yield* walk(m.disk);
  }
}

// Read after a short settle so editors that write-then-rename, or write in two
// chunks, are seen once with the final contents.
const timers = new Map();
function schedule(file) {
  clearTimeout(timers.get(file));
  timers.set(file, setTimeout(() => { timers.delete(file); refresh(file); }, 60));
}

function refresh(file) {
  const name = instancePath(file);
  if (name === null) return;
  let source = null;
  try { source = readSource(file); } catch (e) { if (e.code !== "ENOENT") throw e; }
  if (source === null) {
    if (scripts.delete(name)) { log("sync", `unload ${name}`); broadcast({ op: "unload", name }); }
    return;
  }
  if (scripts.get(name) === source) return;
  const isNew = !scripts.has(name);
  scripts.set(name, source);
  log("sync", `${isNew ? "load" : "reload"} ${name} (${source.length} bytes)`);
  broadcast({ op: "load", name, source });
}

for (const [name, source] of projectMetas) scripts.set(name, source);
for (const file of scriptFiles()) {
  const name = instancePath(file);
  if (name !== null) scripts.set(name, readSource(file));
}

// recursive fs.watch: native on Windows/macOS, Node 20+ on Linux. One watcher per
// mount (a file mount watches its directory).
const watchers = new Map();   // dir -> FSWatcher
function watchMounts() {
  const want = new Set();
  for (const m of mounts) {
    const d = m.isFile ? path.dirname(m.disk) : m.disk;
    if (fs.existsSync(d)) want.add(d);
  }
  for (const [d, w] of watchers) if (!want.has(d)) { w.close(); watchers.delete(d); }
  for (const d of want) {
    if (watchers.has(d)) continue;
    const recursive = mounts.some((m) => !m.isFile && m.disk === d);
    watchers.set(d, fs.watch(d, { recursive }, (_event, filename) => {
      if (!filename || !isScriptFile(filename)) return;
      schedule(path.join(d, filename));
    }));
  }
}
watchMounts();

// The project file changed: re-read the mounts and move the world to match. A
// script whose instance path is gone is unloaded, one whose path is new is
// loaded, one that merely moved on disk but kept its path is left alone. A
// project that does not parse is reported and the old mounts stay.
function reloadProject() {
  let next;
  try { next = readMounts(); }
  catch (e) { log("error", `${path.basename(projectFile)}: ${e.message} (keeping the old mounts)`); return; }
  mounts = next;
  const fresh = new Map(projectMetas);
  for (const file of scriptFiles()) {
    const name = instancePath(file);
    if (name === null) continue;
    try { fresh.set(name, readSource(file)); } catch { /* raced a delete */ }
  }
  let changed = 0;
  for (const name of [...scripts.keys()]) {
    if (fresh.has(name)) continue;
    scripts.delete(name); changed++;
    log("sync", `unload ${name}`); broadcast({ op: "unload", name });
  }
  for (const [name, source] of fresh) {
    if (scripts.get(name) === source) continue;
    const isNew = !scripts.has(name);
    scripts.set(name, source); changed++;
    log("sync", `${isNew ? "load" : "reload"} ${name} (${source.length} bytes)`);
    broadcast({ op: "load", name, source });
  }
  watchMounts();
  log("info", `${path.basename(projectFile)} reloaded: ${mounts.length} mount(s), ${changed} script(s) changed`);
}
if (projectFile) {
  // Watch its directory, not the file: editors that save by rename would
  // otherwise leave a watcher on the old inode.
  let timer = null;
  fs.watch(path.dirname(projectFile), (_event, filename) => {
    if (filename !== path.basename(projectFile)) return;
    clearTimeout(timer); timer = setTimeout(reloadProject, 100);
  });
}

server.listen(port, "127.0.0.1", () => {
  const how = projectFile ? `${path.relative(process.cwd(), projectFile)}: ${mounts.length} mount(s)` : `${dir}`;
  log("info", `watching ${how} (${scripts.size} script(s)); ws://127.0.0.1:${port}`);
  for (const name of scripts.keys()) log("info", `  ${name}`);
});
