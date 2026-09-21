#!/usr/bin/env node
// Runs sync.js against a scratch Rojo project and checks what a connected world
// would hear: the initial loads, a save, a delete, and a default.project.json
// edit that moves a mount (unload + load) or breaks (old mounts kept).
//
//   node luau/tools/sync.test.js
//
// Needs Node 22+ (global WebSocket). Prints `sync: PASS` or the failures.
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawn } = require("child_process");

const port = 8790 + Math.floor(Math.random() * 1000);
const root = fs.mkdtempSync(path.join(os.tmpdir(), "pulseblockz-sync-"));
const w = (rel, text) => { fs.mkdirSync(path.dirname(path.join(root, rel)), { recursive: true }); fs.writeFileSync(path.join(root, rel), text); };
const project = (tree) => w("default.project.json", JSON.stringify({ name: "t", tree }, null, 2));

w("src/server/Main.server.luau", "print('main')");
w("src/shared/Util.luau", "return {}");
w("src/extra/Extra.server.luau", "print('extra')");
w("src/shared/Wand/init.meta.json", '{"className": "Tool"}');
w("src/shared/Wand/Handle.model.json", '{"className": "Part"}');
w("src/shared/notes.json", "{}");
w("src/shared/Shrine.rbxmx", '<roblox version="4"><Item class="Model" referent="RBX0"><Properties><string name="Name">Shrine</string></Properties></Item></roblox>');
const rbxm = Buffer.from("3c726f626c6f782189ff0d0a1a0a0000010000000100000000000000000000004d4554410000000010000000000000004f", "hex");   // binary, as Studio writes it
w("src/shared/Props.rbxm", rbxm);
project({ $className: "DataModel", ServerScriptService: { $path: "src/server" }, ReplicatedStorage: { $path: "src/shared" } });

let failed = 0;
function check(ok, what) { console.log(`  ${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) failed++; }

const tool = spawn(process.execPath, [path.join(__dirname, "sync.js"), root, "--port", String(port)], { stdio: ["ignore", "pipe", "pipe"] });
let toolOut = "";
tool.stdout.on("data", (d) => { toolOut += d; });
tool.stderr.on("data", (d) => { toolOut += d; });

const heard = [];
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
// Wait until `pred(heard)` holds, or give up after `ms`.
async function until(pred, ms = 3000) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { if (pred(heard)) return true; await sleep(25); }
  return false;
}
const loads = (name) => heard.filter((m) => m.op === "load" && m.name === name);
const unloads = (name) => heard.filter((m) => m.op === "unload" && m.name === name);

async function main() {
  await sleep(400);
  const ws = new WebSocket(`ws://127.0.0.1:${port}`);
  ws.onmessage = (e) => heard.push(JSON.parse(e.data));
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });

  check(await until((h) => h.length >= 4), "a connecting world gets every script under the mounts");
  check(loads("ServerScriptService/Main.server.luau").length === 1 && loads("ReplicatedStorage/Util.luau").length === 1 && loads("Extra.server.luau").length === 0,
        "named by their instance path; files outside the mounts are not sent");
  check(loads("ReplicatedStorage/Wand/init.meta.json").length === 1 && loads("ReplicatedStorage/Wand/Handle.model.json").length === 1 && loads("ReplicatedStorage/notes.json").length === 0 && loads("ReplicatedStorage/Shrine.rbxmx").length === 1,
        "*.meta.json, *.model.json and *.rbxmx go too; other .json files do not");
  check(loads("ReplicatedStorage/Props.rbxm").length === 1 && loads("ReplicatedStorage/Props.rbxm")[0].source === rbxm.toString("base64"), "a binary *.rbxm goes as base64");

  heard.length = 0;
  w("src/server/Main.server.luau", "print('main 2')");
  check(await until(() => loads("ServerScriptService/Main.server.luau").length === 1), "a save reloads the script");
  check(loads("ServerScriptService/Main.server.luau")[0]?.source === "print('main 2')", "with the new source");

  heard.length = 0;
  fs.unlinkSync(path.join(root, "src/shared/Util.luau"));
  check(await until(() => unloads("ReplicatedStorage/Util.luau").length === 1), "a delete unloads it");

  // Move a mount in the project file: ServerScriptService now comes from src/extra.
  heard.length = 0;
  project({ $className: "DataModel", ServerScriptService: { $path: "src/extra" }, Workspace: { Old: { $path: "src/server" } } });
  check(await until(() => unloads("ServerScriptService/Main.server.luau").length === 1 && loads("ServerScriptService/Extra.server.luau").length === 1 && loads("Workspace/Old/Main.server.luau").length === 1),
        "editing default.project.json remounts: the old path unloads, the new ones load, no restart");

  // The new mount is watched too.
  heard.length = 0;
  w("src/extra/More.server.luau", "print('more')");
  check(await until(() => loads("ServerScriptService/More.server.luau").length === 1), "a file added under the new mount is picked up");

  // $className / $properties / $attributes on a node are its meta.json, live like the rest.
  heard.length = 0;
  project({ $className: "DataModel", ServerScriptService: { $path: "src/extra" },
            Lighting: { $className: "Lighting", $properties: { ClockTime: 18 } },
            ReplicatedStorage: { Config: { $className: "Configuration", $attributes: { MaxPlayers: 8 } } } });
  check(await until(() => loads("Lighting/init.meta.json").length === 1 && loads("ReplicatedStorage/Config/init.meta.json").length === 1),
        "a node's $className / $properties / $attributes load as its init.meta.json");
  check(loads("Lighting/init.meta.json")[0]?.source === JSON.stringify({ className: "Lighting", properties: { ClockTime: 18 } })
        && loads("ReplicatedStorage/Config/init.meta.json")[0]?.source === JSON.stringify({ className: "Configuration", attributes: { MaxPlayers: 8 } }), "with the node's contents");
  heard.length = 0;
  project({ $className: "DataModel", ServerScriptService: { $path: "src/extra" }, Lighting: { $className: "Lighting", $properties: { ClockTime: 6 } } });
  check(await until(() => loads("Lighting/init.meta.json").length === 1 && unloads("ReplicatedStorage/Config/init.meta.json").length === 1),
        "editing them reloads the meta; a node removed unloads it");

  // A broken project file keeps the old mounts.
  heard.length = 0;
  w("default.project.json", "{ not json");
  await sleep(400);
  check(heard.length === 0 && /keeping the old mounts/.test(toolOut), "a project file that does not parse is reported and changes nothing");
  w("src/extra/More.server.luau", "print('more 2')");
  check(await until(() => loads("ServerScriptService/More.server.luau").length === 1), "and the old mounts still sync");

  ws.close();
}

main().catch((e) => { console.error(e); failed++; }).finally(() => {
  tool.kill();
  fs.rmSync(root, { recursive: true, force: true });
  console.log(failed ? `sync: ${failed} FAILED` : "sync: PASS");
  process.exit(failed ? 1 : 0);
});
