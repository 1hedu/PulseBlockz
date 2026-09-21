# PulseBlockz — Architecture (v0.4)

A decentralized Roblox: a Godot client with a hardened Luau sandbox, creator-hosted game servers, and ownerless contracts on PulseChain for item ownership and sales. Creators publish, price, host and moderate their own experiences; players pay creators directly; the contracts have no owner, no admin and no upgrade path. This revision (v0.4) records that decision and the ones that led to it.

## 1. Layers

| Layer | Tech | Notes |
|---|---|---|
| Client | Godot 4 + GDExtension (C++) | Rendering, input, prediction/interpolation. Luau VM embedded via GDExtension. **Curation lists** decide what it shows (§8). |
| Creator scripting | Luau (embedded) | Sandboxed. Same VM on client and server; server is authoritative. |
| Game authority | Headless Godot server (C++), **run by the creator** | Physics, movement, replication, chat, and the creator's own moderation. Same binary as the client with the authoritative API surface. |
| Server discovery | On-chain registry (Phase 2) | experience id → endpoint, signed by the creator's key; the client verifies. No lobby service required. |
| Web3 bridge | Node.js + ethers v6, **reference, anyone can run one** | Wallet auth for a creator's server, inventory/listing reads, gasless relay. Nothing on chain requires it. |
| Assets | **On chain by default** (`AssetStore`), mirrors optional | Every asset is named by its content hash (`pblockz://`, §9). Bytes live in contract code, readable for free forever; big files can sit on mirrors that the hash keeps honest. No platform asset service. |
| Moderation | Signed curation lists, published by anyone, subscribed to by clients | Block/allow lists over assets, creators, servers and URIs. Off-chain; nothing in the registry can be delisted or removed, by anyone. The one edit it permits is a creator re-pointing their own asset's `uri` with `setUri`, until they `freeze` it (§8). |
| Ledger | Solidity on PulseChain (EVM), **ownerless** | UGC1155 (PRC-1155 + ERC-2981, permissionless register), Marketplace (per-asset prices in whatever ERC-20s the creator accepts, fixed fee or none), AssetStore (content-addressed bytes in contract code), ERC2771 Forwarder. No platform token. **Live on testnet v4** (§9). |

### Trust boundaries
- The client is trusted with its own character and nothing else. As on Roblox it simulates that and reports it, and the server's packet handler accepts exactly six properties on the two instances the sender owns (§4); every other write it sends is dropped without a word. Beyond that it signs and it asks.
- A creator's game server trusts whichever bridge the creator runs (or none: it can verify wallet signatures and read the chain itself).
- A relayer holds one key that can only pay gas. It cannot forge a request (EIP-712) and cannot prevent anyone from paying their own gas.
- Contracts trust: Marketplace → the only minter of its UGC1155; Forwarder → the only trusted meta-tx entry point. No contract holds user funds, and no address can change any of this after deployment.
- The sandbox is the last line: with no reviewer between a creator and a player's machine, the budgets and capability stripping in §4 are what protect the player.

## 2. Corrections from v0.1 (and what v0.4 removed)

1. **Authoritative physics lives in a headless Godot build, run by the creator.** Movement, parts, and Luau server scripts run in a Godot server process per experience instance. v0.2 had the platform running these behind Nakama; v0.4 hands them to creators, Minecraft-style, with an on-chain registry for discovery.
2. **Luau-in-Godot is the risk.** Binding is easy; the sandbox is the work. See §4. Spike this *first* — if it doesn't work in 2–3 weeks, reconsider the engine choice before writing anything else.
3. **UGC tokens point at a metadata URI, not the binary.** `UGC1155.setUri` lets a creator ship a new version to all holders without re-minting, and `freeze` lets them promise they never will. There is no platform kill-switch (v0.2's `setDelisted` is gone) and no publisher role: creators register their own assets. Curation happens in clients and on creators' servers.
4. **Gasless ≠ frictionless.** EIP-712 meta-transactions remove gas but still pop a wallet signature per action. v1 ships meta-tx because it's audited and simple. v2 should add session keys (ERC-4337 smart account with a spending allowance per session) so a play session needs one signature, not one per purchase.
5. **There is no platform currency, and no platform say in payment tokens.** Each creator decides, per asset, which ERC-20s they accept and the price in each; the Marketplace pays them directly on every sale. The platform issues nothing, custodies nothing and owes nobody a payout. This replaced, in order: a PLS-backed one-way token (v0.2), and a Robux-style closed loop with a gated creator cash-out (implemented and then retired in the branch history, commit 37c7f66). Rationale in §3.
6. **Relayer abuse controls are off-chain and optional.** The forwarder is the unmodified OpenZeppelin contract. The reference bridge enforces a (contract, selector) allowlist, gas cap, zero-value, and per-wallet rate limit before spending any PLS. Anyone can run one; nobody has to.
7. **Nothing is administered.** The Marketplace has no owner: its fee (default 0) and fee address are immutable, and a burn address makes any fee provably unspendable. It deploys its own UGC1155 and is that contract's only privileged caller (mint on sale). The deployer holds no power the moment deployment finishes.

## 3. Economy flow

```
Creator   ──sign register(uri, maxSupply, royalty)──▶ any relayer ──▶ Forwarder ──▶ UGC1155  (gasless; the signer is the creator)
Creator   ──sign setPrice(id, token, price)──▶ any relayer ──▶ Forwarder ──▶ Marketplace     (gasless; once per token they accept)
Player    ──GET /listing/:id──▶ Bridge ──▶ which tokens this asset can be bought with, and prices
Player    ──approve(token)──▶ token                                                          (1 real tx per token; none for permit tokens)
Player    ──sign buy(id,token,qty) / buyWithPermit(...)──▶ any relayer ──▶ Forwarder ──▶ Marketplace
                                                      ├─ token → creator (and the fixed fee address, if any), same tx
                                                      └─ UGC1155.mint(player, id, qty)
Game server ──GET /inventory/:wallet?ids=…&tokens=…──▶ Bridge ──balanceOfBatch──▶ chain
            ◀── owned[{id, uri}], tokens[{symbol, balance}] ── fetch uri from asset service → equip
```

The fee is fixed at deployment (`FEE_BPS`, default 0) and, if non-zero, goes to `FEE_RECIPIENT` forever; `0x…dEaD` burns it. Secondary trading is out of scope for v1; ERC-2981 royalties are set so any standard NFT marketplace on PulseChain honors them.

### No platform token (decided)

The project is for a crypto-native audience, and the goal is a platform nobody operates. Every token design carried a liability someone would own:

- A **transferable token** sold by a treasury is a convertible virtual currency the moment a pool exists, whoever creates the pool. Its issuer is the regulated party.
- A **closed loop** (Robux/DevEx: non-transferable, purchased vs earned balances, a gated cash-out) makes the platform an issuer, a custodian of takings, a payout program with identity verification, and a reserve to manage.
- **No token, no owner** leaves a contract that moves the buyer's chosen token to the creator in one transaction and holds nothing in between, with nobody able to change it. That is the design the pending CLARITY Act developer safe harbor (no control over user assets) is written for, and creators are paid on every sale, so there is no payout program.

What it costs, and what was done about it:

- **Gasless approvals.** Third-party tokens don't know our forwarder, so a buyer sends one real `approve` per token. Tokens with EIP-2612 `permit` need none: `buyWithPermit` makes the whole purchase a single gasless signature. The e2e script proves a buy with zero transactions from the player.
- **Garbage tokens.** A creator can accept anything, including a token they minted themselves. A "sale" in it gives that token volume. The contract never values a token. A platform allowlist was considered and rejected because it made someone the arbiter of which tokens are legitimate. Clients may still curate what they display.
- **One price scale.** Prices display in whatever token the creator chose; a client can quote in USD via a price feed if it wants to.
- **Spending fence.** A closed currency also limited what a wallet could spend. That control now lives in the wallet: session keys with a spending allowance (Phase 4).

### Nothing administered (decided)

"Decentralized" was applied to the whole stack, not just the token:

| v0.2 control point | v0.4 |
|---|---|
| Marketplace owner (fee, pause) | none; fee immutable, default 0 |
| UGC1155 admin, publisher role, delist switch | none; creators register themselves, `freeze` is the only lock and only the creator holds it |
| Backend key that registered assets | gone |
| Platform asset service + moderation queue | creators host (IPFS or own server) and moderate on their own servers |
| Platform game servers + Nakama | creator-hosted servers, on-chain registry for discovery |
| Bridge as the only path to the chain | reference implementation; any wallet can call any function directly |

Whoever deploys the contracts holds no key afterwards, and the pending US safe harbor is written for exactly that. It does not cover the client or the people who ship it: distributing a client that renders arbitrary creator content is where remaining exposure concentrates (EU age assurance, app-store policy), and the design answer is multiple clients, each curating what it shows (§8), plus the sandbox. Renouncing the delist switch also means that when something illegal is registered there is no protocol-level remedy; only clients and servers can refuse to show it.

## 4. Luau sandbox (Phase 1 spec)

Non-negotiable properties of the embedded VM, and what the code does about each:
- **No ambient capabilities.** `io`, `loadstring` and `package` do not exist in this Luau build at all — `linit.cpp` never opens them. `debug`, `getfenv` and `setfenv` are nil'd out in `Sandbox::Sandbox` (`luau/src/luau_sandbox.cpp`), and so is `os` — after which the runtime puts back a three-function `os` of `time`, `clock` and `date` (`luau/src/rbx_runtime.cpp`), because Roblox scripts lean on those. `require` is not removed but replaced: `g_require` (`rbx_runtime.cpp`) takes an Instance that `isA("ModuleScript")` and compiles that instance's own `Source`, so there is no path to a file or to arbitrary text — and since `Source` carries `PluginWrite` (`rbx_instance.cpp`), a place's own scripts cannot put text there either, which is what keeps "no `loadstring`" from being true of the name only. The setter wants plugin capability, and that capability is one thing: the calling script sits under something the host itself recorded as a plugin directly under the DataModel's one `PluginDebugService` (`pluginRoots`, written only by `addPlugin` and `addPluginFile`), or the chunk is the host's own (`hasPluginCapability`, `rbx_runtime.cpp`). The Studio's plugin loader is the only caller; the Player never loads a plugin. Tree position alone is nothing: a script parenting into the service by `Parent`, `Instance.new` or a clone is refused (`inPluginTree`, `rbx_runtime.cpp`), a place file addressed at `PluginDebugService/` is refused by the loader (`ensureContainers`, `rbx_host.cpp`), and a Script a server's replicated tree puts there is not in the record and does not run (`luau/gdextension/demo/tests/plugin_escape_test.gd`; harness R59). The same capability gates the `HttpEnabled` setter (`rbx_api.cpp`). What remains beside the PulseBlockz API is the ordinary Luau standard library — `string`, `table`, `math`, `coroutine`, `utf8`, `bit32`, `buffer`, `vector` — frozen read-only by `luaL_sandbox`.
- **Budgets.** Four, all fields of `Budget` (`luau/src/luau_sandbox.h`): instruction count and a wall-clock deadline checked in the Luau interrupt callback (`luau_sandbox.cpp`), a memory limit in a custom allocator (`luau_sandbox.cpp`), and a per-frame slice. Exceeding any of them kills that one script: the failing task is reported and `kill`ed and nothing else is touched (`rbx_runtime.cpp`). `PulseBlockzWorld`'s constructor (`luau/gdextension/src/pulseblockz_world.cpp`) leaves all but the wall clock at 0, which means unlimited, and sets that to 10 seconds — Roblox's own script-timeout default, chosen so a module that spends 30 ms building its tables is not killed for it. **The host is expected to say what it wants**, and the two that join a stranger's place do: `set_max_memory_mb(64)` in the Player (`luau/gdextension/player/Player.gd`, in both `play()` and `join()`) and in the town (`luau/gdextension/demo2/Main.gd`, in `_enter_tree` — the World builds its runtime in its own `_ready`, so a parent's `_ready` is a frame too late). Sixty-four is measured: the live town peaks at 4.98 MiB of Luau heap, which `Runtime::Stats` now reports from the allocator that was already counting it. Step and frame budgets are still 0 everywhere but the old demo, deliberately, until there is a measured number for them too.
- **Isolation.** One `lua_State` per experience; creator scripts run in child threads with a sandboxed global environment (`luaL_sandbox` / `luaL_sandboxthread`).
- **Off the main thread.** Scripts do execute on a worker thread per experience (`RuntimeThread`, `luau/src/rbx_host.cpp`), and the engine only sees the marshalled change log: nothing a script calls reaches a Godot object from that thread. It is applied on the main thread by polling, not by `call_deferred` — `_process` checks `is_idle()` and then `take()`s the finished frame (`pulseblockz_world.cpp`); the only `call_deferred` left in that file is the sign-in signal. The frame budget that would bound a tick batch exists (`Budget::maxFrameMillis`) but ships at 0, i.e. unlimited, per the bullet above. The **per-call command cap is not in this path at all**: `maxCommandsPerCall` lives in `luau/gdextension/src/luau_script_host.cpp`, a class `register_types.cpp` never registers and no `.gd` or `.tscn` in the repo references. Decided in Phase 1 over "scale the per-call budget by script count", which would keep the stall on the render thread and shrink every script's slice as an experience grows.
- **Determinism where it matters.** Server-side scripts are authoritative, and this one is enforced on the wire rather than by convention. The protocol has no generic client→server property write: a `ClientFrame` carries only what the server's handler whitelists, which is Position / Orientation / AssemblyLinearVelocity on the sender's **own** character root and MoveDirection / Jump / StateName on its own Humanoid, each type-checked, enum-checked and finiteness-checked, everything else silently dropped (`pulseblockz_world.cpp`). A script's `Source` is `NoReplicate | Hidden | PluginWrite` on `LuaSourceContainer`, the base of every script class (`luau/src/rbx_instance.cpp`). What crosses the wire depends on which world is serving. In a play world `Replicator::sendsProperty` (`luau/src/rbx_net.cpp`) refuses a `NoReplicate` property for every class — no exemption for LocalScript or ModuleScript, only `inProcess_`, Play Solo's one-process case with no socket and no second party — and `visible` sends nothing under `ServerStorage`, `ServerScriptService` or `UserInputService` (`replicates`, `luau/src/rbx_instance.cpp`); a joined client fetches the place from the chain by the hash the Welcome packet names. That is the case `SAFETY.md`'s "a server cannot hand you code" section describes. In an edit world — a Studio hosting Team Create — `pulseblockz_world.cpp` calls `setReplicateAll(edit_mode)` when it builds the server runtime, `all_` is true, and both checks pass everything: every service and every property, `Source` included, to every joined Studio, and the host applies the edits they send back (`NetEdit`, applied only while `editMode_`).
- **Hot-reload path.** File-sync tool (Rojo-style: watch dir → diff → push over local websocket) injects into the client VM in dev, and into the server VM in staging.

## 5. Phasing

| Phase | Deliverable | Exit criterion |
|---|---|---|
| 0 — Spike ✅ | Luau sandbox with budgets + GDExtension node (`luau/`) | Runaway loop killed at 4 ms, frame continues — verified by harness |
| 1 — Sandbox | Worker thread + event marshalling ✅, curation lists ✅ (spec, reference lib, client filter), file-sync tool ✅, part instancing ✅, PulseBlockz API surface ✅ (Roblox-shaped: Instance tree, services, placement rules, remotes, replication) | Creator edits a script externally, sees it live in-client; a subscribed blocklist hides an asset |
| 2 — Authority | Creator-hosted headless Godot server ✅, movement replication ✅ (ENet transport, Roblox network ownership), chat ✅ (`TextChatService` in `luau/src/rbx_chat.cpp`), on-chain server registry, **wallet login ✅ (EIP-191 sign-in, `Player.WalletAddress`) + inventory read ✅** | Two clients see each other on a creator's server found via the registry; a wallet holding a test token gets the item equipped on join |
| 3 — Ledger ✅ (chain side) | Contracts on PulseChain testnet v4 (chainId 943), a reference relayer funded, creator publishing flow in-client | The e2e script in this repo passes against testnet ✅ (48 checks in `scripts/e2e.js`, chainId 943, `addresses.943.json`); a creator publishes from the client with no platform step |
| 4 — Economy v2 | Session keys / 4337 with spending allowances, secondary market | One signature per session |

Wallet auth moves into Phase 2 (was Phase 3) so the server's identity model accounts for wallets from the start.

## 6. What's implemented in this repo

- **the town** (`luau/gdextension/demo2`) — pBlockz Home, the place built to prove the kit, and
  the thing the other documents keep using as an example. Seven desks answer from live testnet v4
  data: the teller (holdings, your address, sending), the shopkeeper (the shelf and the holdings
  ladder), the archivist (block height, base fee, every contract by address), the tailor (draw a
  cape, publish it, own it), the trader and the screener (quotes and pools through PulseX), and the
  Funmaster (duels, ranked and recorded). `SHOWCASE.md` is the index of what it shows how to do,
  file by file; `WHERE.md` says which layer a new thing belongs in.
- `curation/` — Curation list format v1 (`curation/SPEC.md`), a reference implementation (sign,
  verify, merge, match, report) run by `node --test curation/test/curation.test.js`, the Godot-side
  filter `luau/gdextension/host/Curation.gd`, and native verification in the GDExtension
  (`PulseBlockzCrypto`, backed by `luau/src/eip712.h` and libsecp256k1).
- `luau/` — the engine: a C++ Luau sandbox with step, time, memory and frame budgets under an
  engine-agnostic Roblox-shaped runtime (`luau/src/rbx_*`), and the Godot GDExtension that draws it
  (`luau/gdextension/src/`). What it implements is too long to list twice and goes stale when it is:
  `luau/README.md` is the reference, `PARITY.md` and `luau/PARITY.md` are the audits of what is
  present and what is missing.
- `contracts/` — twelve Solidity files. Deployed and recorded in `addresses.943.json`: Forwarder,
  UGC1155, Marketplace, AssetStore, Inventory, Duelling, DuelRecords, Announcements, Fishing. Test
  doubles: `MockERC20.sol`, `MockRandom.sol`, `DevRandom.sol`. None has an owner, an admin key or an
  upgrade path except Announcements, whose owner may post, pin and unpin and nothing else.
  `node scripts/verify-contracts.js --status` says which are verified on the explorer.
- `bridge/` — reference Express server: `/auth/*`, `/inventory/:address`, `/listing/:id`, `/relay`,
  `/health`. `/relay` simulates the inner call before paying for it.
- `scripts/` — `deploy.js` deploys and there is nothing to wire; `e2e.js` asserts no owner or roles
  exist and then runs a creator through registering, pricing and a sale; `publish-experience.js` and
  `publish-catalogue.js` put a place and its items on chain; `check-docs.js` checks that the paths,
  scripts and addresses in these documents are real.

The place editor lives beside the client, not in it: `luau/gdextension/studio/` is a
second Godot project, and its loop is Studio's -- edit a tree that is not running, Play
a copy of it, Stop and the edits are still there, Save them into the place. The edit
world is a `PulseBlockzWorld` with `edit_mode` on, where no script starts and nothing is
simulated; over it sit an Explorer of the Instance tree, a Properties panel that reads
and writes it, a free camera that selects what you click (one, several, or a box drawn
round them), Select / Move / Scale / Rotate handles, and the verbs on Roblox's own keys --
cut, copy, paste, duplicate, group, ungroup, rename, insert, delete -- from one table
that a Studio-shaped ribbon, the right-click menus and the keyboard all read. A script
opens in a tabbed CodeEdit over the viewport, whose buffer is its own and whose text goes
back into the tree as a property write, one history entry per edit rather than per
keystroke.

The panels read the world node's mirror of the tree
(`track_properties`, `get_child_ids` / `get_instance` / `get_properties`), so none of
them touches a script thread; a write is queued for the runtime's own thread and comes
back through the change log -- and goes through one command log first, so Ctrl+Z takes
back a property, a whole handle drag, an insert, a delete with its children, or a move
in the tree.

One thing carries both Play and Save: the tree serialized as Rojo's `*.model.json`, the
format the engine already reads. Play builds a second world and hands it that (through
`add_model`, which adds rather than replaces and resolves Refs by where the instance is),
so what runs includes edits never written to disk; Save writes the same text to the
files the place opens from, leaving alone whatever is authored as a `.luau` or `.rbxmx`.
Because an edit world runs nothing, a place whose map is built by a Script opens empty --
so the demo place now keeps its map as `*.model.json` beside the Script that gives it
behaviour, which is also what a Studio-edited place looks like.
`luau/gdextension/studio/tests/studio_test.gd` runs the whole loop headlessly.

The chrome comes in three looks -- Dark, Light and a Clear that shows the place through
its panels -- from one palette of named roles, with the icons rasterised from SVG held in
the source rather than shipped as image files.

Not implemented: dockable panels (the View tab hides and shows them, but they cannot be
moved), writing an `.rbxmx` or an `init.meta.json` back -- a model or a
Tool authored that way is read and then left alone -- the on-chain server registry, and
secondary trading. Chat and wallet login shipped (`luau/src/rbx_chat.cpp`, and the EIP-191
sign-in in `pulseblockz_world.cpp` that sets `Player.WalletAddress`), and so did publishing
without Node: `luau/gdextension/host/Publish.gd` stores a place's files and writes its
manifest from the host layer, with `luau/gdextension/publisher/` the app around it.

## 7. Open items before mainnet
- Audit the Marketplace and UGC1155 (Forwarder and the OZ bases are audited; the glue is not). There is no upgrade path: a bug means a redeploy and a new address.
- Decide `FEE_BPS` and `FEE_RECIPIENT` once. A burn address is the only recipient that is verifiably nobody's; a "wallet I never touch" is a claim, not a proof.
- Reference relayer: key in HSM/KMS, alert on PLS balance, Redis for nonces/rate limits. Or don't run one and let players pay gas.
- Client distribution: multiple clients, each shipping with curation subscriptions (§8) and a report button, and a privacy-preserving age-assurance story for European users (a wallet-bound over-age credential fits the CJEU's proportionality test).
- Server registry design (Phase 2): who can register an endpoint for an experience (its creator), how the client verifies it, and what a malicious server can do to a client (the sandbox answer: nothing outside the presentation API).

## 8. Client curation (blocklists)

There is no delist switch on chain, so moderation is a client concern, and it has to be built
in from the start: app stores require filter/report/block for user content, and takedown
requests will arrive at whoever ships a client. The design is the ad-blocker / DNS one: the
registry lists everything, and a client decides which entries it renders.

- **Lists** (`curation/SPEC.md`): a signed JSON document from a `maintainer` address, in
  `block` or `allow` mode, scoped to one `chainId` + `registry`, with entries over assets
  (by id), creators (by address), servers (by endpoint) and URIs (by prefix, i.e. a content
  hash or a host). EIP-712 signed, so any wallet can maintain one and any mirror can serve it.
- **Subscriptions**: none by default. `subscriptions` in `luau/gdextension/host/Curation.gd`
  starts empty, and no project in the repo adds one outside its tests, so nothing is hidden
  until `subscribe(url, maintainer)` is called or a list is handed to `add_list`;
  `set_enabled` turns one off. `refresh`
  refetches every subscription, `status` reports a list past `updated + ttl` as stale, and
  `add_list` keeps the newest per maintainer.
- **Merge**: hidden if any enabled block list matches; if any allow list is enabled, shown only
  if some allow list matches. Allow mode is how a curated or kids-mode client is built
  without a different protocol.
- **Applied to**: store/listing views, inventory (hidden items are not equipped or rendered),
  the server browser, and asset URIs before they are fetched. Hidden is not deleted.
- **Report**: every asset, creator and server has a report action that posts to each
  subscribed list's `report` endpoint. Lists decide what to do with reports.
- **What it costs**: nothing on chain. What it doesn't do: stop a determined user running a
  client with no lists.

Reference: `curation/src/curation.js` (Node; sign, verify, merge, match, report) with tests, and
`luau/gdextension/demo/Curation.gd` (Godot autoload; fetch, verify, merge, match, report).
Verification in the client is native: `PulseBlockzCrypto.verify_curation_list` in the GDExtension
does keccak-256, JSON canonicalisation identical to the JS reference, the EIP-712 digest and
secp256k1 recovery (bitcoin-core libsecp256k1). The engine-agnostic core (`luau/src/eip712.*`) is
checked in the harness against a list signed by ethers, including tamper and malformed cases, so a
list served by any mirror is trusted on its signature alone.

## 9. Assets on chain

PulseChain gas is cheap enough that the asset bytes themselves, not just a pointer, can
live on chain. Once they do, reads are free forever and a creator's world outlives the
creator's server. The design (`ASSETS.md`):

- **Content hash is the identity.** An asset URI is `pblockz://<keccak256>` plus hints:
  `chain=<chainId>:<AssetStore>:<blobId>` for the on-chain copy, `src=` for mirrors (fetched only for a place that
  declared the `mirror` capability, and only to a host in `ChainAssets.mirrors_allowed`, which
  ships empty — so a place cannot name a URL for your machine to visit),
  `mime=`. A client verifies the hash after every fetch, so any mirror will do.
- **A document names an asset by bare hash.** A metadata document's `model` and
  `thumbnail` are `0x<keccak>` with no scheme and no query: the hints are facts about a
  deployment, and both are already on chain — `AssetStore.blobOf(hash)` gives the blob and
  the blob records its own `mime`. A hash inside a property string (a `Decal`'s `Texture`
  within a model) keeps the scheme, `pblockz://<hash>`, because the engine finds assets by
  scanning property strings for it. A place manifest's `files` keeps whole links on
  purpose: it is a fetch plan read in bulk at join, where naming the blob saves the lookup.
  `node scripts/audit-documents.js` reports how every listing on chain names its parts and
  whether the store can find them.
- **Bytes in contract code, not events.** Logs and calldata are history that nodes may
  prune; code is state that every node keeps as long as the chain exists. `AssetStore`
  writes each ≤ 24,575-byte chunk as a contract whose code is `0x00 || data` (nothing can
  execute it) and a blob as an ordered chunk list. `publish` recomputes the keccak and
  size from the chunks on chain, so a recorded hash is guaranteed, not claimed.
- **One call to read.** `read(blobId)` assembles the blob in an `eth_call`; `readRange`
  streams; or `chunksOf` + `eth_getCode`. The client verifies either way.
- **What goes where.** Roblox-format models, scripts, animations, low-poly meshes and
  small textures fit comfortably; large textures and sounds go behind `src=` mirrors with
  only the hash on chain. Cost is ~200 gas per byte; what bounds one transaction is
  PulseChain's 49,152-byte calldata limit (addendum below), not price.
- **Permanence cuts both ways.** Nothing stored can be removed by anyone, the creator
  included, and every node holds it. Curation lists (§8) are how a client declines to
  show something; they do not make it go away.

A token's metadata JSON is itself a hash-named asset, and names the model and thumbnail
it is made of by bare hash, with `license`, `attribution` and `source` fields so CC-BY
assets are usable and copyright reports have something to check. Thumbnails live in the
`AssetStore` rather than in transaction calldata: calldata is cheaper but findable only
through the transaction that carried it, so a document naming one by hash alone could
never resolve it.

### What the testnet changed (§9 addendum)

Deploying for real surfaced three things a local chain cannot:

1. **PulseChain is Shanghai, not Cancun.** `MCOPY` and `TSTORE` are invalid opcodes there, so
   `scripts/compile.js` targets `shanghai` by default and OpenZeppelin is pinned to 5.0.2;
   5.1+ uses `MCOPY` in OpenZeppelin 5.1's own Bytes.sol and Arrays.sol and will not compile for it.
2. **Every transaction is held to EIP-3860's 49,152-byte initcode limit**, creation or not.
   Batching asset chunks four-at-a-time produced ~98 KB of calldata and was rejected on send
   with `INVALID: initcode too large`, while `eth_estimateGas` had accepted it. Batches are
   now packed by encoded calldata size.
3. **The node's suggested priority fee is 10,000 gwei** against a 7 wei base fee, which would
   have made one deployment cost ~50 PLS. `bridge/src/fees.js` bids 1 gwei.

The e2e also had two assertions that only held on a fresh chain — an absolute token balance
and the `blobOf` dedupe check — both now measure deltas, so the suite is re-runnable against a
chain that keeps its state.
