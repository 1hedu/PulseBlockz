# PulseBlockz

A Roblox-shaped game engine and Studio, with places, assets and ownership on PulseChain.

Build a place in the Studio, or import one from Roblox. Publish it and it lives on chain,
named by the hash of its own bytes. Anyone can open it in the Player, see what it is and what
it may ask of their wallet, and play it — alone, hosted, or on a server. No account, no
platform, no one who can take it down.

## The apps

| | |
|---|---|
| **PulseBlockz Studio.exe** | Edit a place: explorer, properties, handles, terrain, rigs, animation, scripts, a debugger. Imports `.rbxl` / `.rbxlx`. |
| **PulseBlockz.exe** | The Player. Open published places and join servers. Shows what a place is before it runs. |
| **PulseBlockz Publisher.exe** | Put a place on chain. Shows every file and the cost, then asks once. |

All three run on one engine: Godot 4 with a sandboxed Luau VM that speaks Roblox's API —
79% of its classes, 92% of its properties (`PARITY.md`). A place's scripts cannot reach the
disk, the network or a key.

## Run it

Download the release for your machine and run any of the three apps. They share the engine
library beside them, so keep a folder together. On Linux, `chmod +x *.x86_64` first.

**Importing a Roblox place:** File → Open your `.rbxl`. Meshes and textures are private on
Roblox, so the Studio fetches them signed in as you. Put your `.ROBLOSECURITY` cookie in
`%APPDATA%\Godot\app_userdata\PulseBlockz Studio\roblox_cookie.txt` on Windows, or
`~/.local/share/godot/app_userdata/PulseBlockz Studio/roblox_cookie.txt` on Linux — the Studio
sends it only to Roblox, never logs it, and never writes it anywhere else. `SAFETY.md` has
the details.

## Build it

Godot 4.3-stable, MSVC or clang, CMake, `pip install scons`. `luau/README.md` says how to get
Luau, godot-cpp 4.3 and libsecp256k1 (`make luau-libs`). Then:

```
cd luau/gdextension && scons platform=windows target=template_release
godot --headless --path luau/gdextension/studio --export-release "Windows Desktop"
```

and the same export for `player` and `publisher`. For Linux, `scons platform=linux
target=template_release` and the preset named `Linux`; `luau/tools/linux-build.sh` builds the
library on an Ubuntu box or under WSL. A fresh checkout first needs one
`godot --headless --editor --path <app> --quit` per app to register the extension.

Suites: `godot --headless --path <app> -s res://tests/<name>.gd`. Contracts: `npm install`,
`node scripts/compile.js`, then `node scripts/deploy.js` — against a local node by default;
`RPC_URL` and `DEPLOYER_KEY` point it at the testnet.

## On chain

Testnet v4 today (`addresses.943.json`). What the apps themselves stand on is four ownerless
contracts — `AssetStore` holds the bytes a place is made of, `UGC1155` the items, `Inventory`
what a player is carrying, `Forwarder` a signature somebody else pays to send — with no admin,
no delist and no upgrade. A creator prices in whatever tokens they accept and is paid directly.
`Marketplace` is deployed beside them for selling between wallets; nothing in these three apps
goes through it yet. A game's own contracts are the game's: the town's fishing, duelling and
noticeboard live in its repository, not here. `ARCHITECTURE.md` and `SERVER.md` explain why a
server is only for what cannot be on chain.

## Docs

`ARCHITECTURE.md` the design · `SAFETY.md` what you trust and what is not safe yet ·
`SERVER.md` hosting · `ASSETS.md` the `pblockz://` scheme · `PARITY.md` what Roblox has that
this does not · `SHOWCASE.md` the town as examples · `WHERE.md` where things live and why.

## Licence

`LICENSE`: free for anything, with no chain or on PulseChain; not on another chain.
`THIRD_PARTY.txt` lists what the binaries contain: Godot, godot-cpp, Luau, libsecp256k1 and
meshoptimizer (MIT), Draco (Apache-2.0).
