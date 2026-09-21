# What the town shows you how to do

`demo2` — PulseBlockz Home, the town — is one place built to prove the kit. Most of what is in it
is also an example: a technique to lift, a published model to reuse, or a kind of game running end
to end. This is the index, with the file to open for each.

Paths below are under `luau/gdextension/demo2/scripts/src/` unless they start with `scripts/`,
`contracts/` or `luau/gdextension/`. `WHERE.md` says which of the kit's five places something
belongs in; `SERVER.md` says what a server is for; `SAFETY.md` says what running a place asks a
player to trust. Read those before copying the chain patterns.

**Before you copy anything, the three rules the town keeps:**

1. **Everyone pays for their own actions, from their own wallet.** The server holds no key and signs
   nothing. A desk that wants a player to do something on chain asks that player's wallet.
2. **The server reads the chain only to judge** — whose hit landed, whether a fish was really
   caught, whether a result is really on chain. Everything a player only looks at, their own
   machine reads for itself.
3. **Do it the Roblox way.** Where Roblox has an API for a thing, the town uses it and the engine
   implements it; nothing is a scripted imitation of a missing feature.

---

## 1. Reusing what is already published

Everything published is named by the hash of its own bytes (`ASSETS.md`), so anybody can fetch it,
check it and use it: no permission, no registry to ask.

| What | How to get it | Look at |
|---|---|---|
| **A catalogue model** — the hats, capes, shoes, the Pup, the White Roma, BFS 9000... | `catalogue.943.json` holds each item's metadata document by `pblockz://` URI, which is also what its UGC1155 token's uri points at. The document names its model and thumbnail by bare content hash (`0x<keccak>`, not a link); `AssetStore.blobOf` finds the blob and the blob carries its own mime. Ask the client for the model: `OwnWallet.request({ action = "fetch", uris = { uri }, as = "model" })` builds it into `ReplicatedStorage.OnChain`, ready to `:Clone()`. A model can only be data: anything that could carry code is refused. | `server/Chain.server.luau` (hash to uri), `server/Wardrobe.server.luau` (wearing), `server/Pets.luau` (a model that follows you) |
| **A picture, a sound, a mesh, a font** | `as = "file"` gives back a local path a `Decal`, `Sound`, `MeshPart` or font takes directly. | `client/Assets.client.luau` |
| **The town's place assets** — the NES waveforms, the skybox, the rocket, the tree | Named in `luau/gdextension/demo2/place-assets.json`; the host fetches them into `ReplicatedStorage.PlaceAssets` as `StringValue`s holding paths. | `server/Sky.server.luau`, `shared/NesEngine.luau` |
| **Models a game builds for itself**, with no chain round trip | A model described as JSON (`className`, `properties`, `children`) built into Instances. The fish and the rod are made this way. | `shared/ModelJson.luau`, `shared/Fish.luau`, `server/Made.server.luau` |
| **A thumbnail of such a model** | Photographed live in a `ViewportFrame`. | `shared/ModelPicture.luau` |
| **Ownership of an item** | Read `Inventory.held(address, item)`; an item's id is `keccak256(dataHash, tier)`. Your place can honour what somebody already owns elsewhere. | `server/Chain.server.luau`, `contracts/Inventory.sol` |

**Licences.** Being fetchable is not the same as being yours to redistribute. Check an item's
metadata (`license`, `attribution`, `source`). Two things in the town are not free to reuse:
*Reactor7*, the font, is CC BY-NC-SA 3.0 and its licence archive has to travel with it
(`_licence` in that same `place-assets.json`); and *The Moon* (`shared/TheMoon.luau`) is Capcom's music from
DuckTales.

**Preparing your own models the way the town's were prepared** (`scripts/`):
`prep-pup.js` and `prep-tree.js` turn a GLB into a welded OBJ plus its colour atlas (faces reversed
for the engine's winding); `prep-roma.js` cuts wheels out as their own meshes so they can turn;
`prep-splash.js` shrinks a picture for the chain; `catalogue.js` describes an item as a model;
`render-pet.js` draws it without the engine; `stage-preview.js` with `luau/gdextension/demo2/tests/Preview.gd` puts an
unpublished item into a running town to look at before it costs gas.

---

## 2. Kinds of game you can see working

Each of these is the whole loop — world, rules, chain, interface — in a handful of files.

### A collecting game with an on-chain leaderboard — *space fishing*

Cast off the edge of the map, wait for the bite, reel it in. Every cast and reel is the player's own
transaction; which fish bites is decided on chain by a random-number contract; the leaderboard is
read straight off the contract by each player's machine. One rare discovery (the Everliving Fish)
can be claimed once, by whoever finds it.

- `contracts/Fishing.sol` — cast, reel, catches per angler, the one discovery.
- `shared/SpaceFishing.luau` — where a charged cast lands, and checking it was fair.
- `client/Fishing.client.luau` — charge, cast, bite, reel, resume an unfinished line, cancel.
- `server/Fishing.server.luau` — what everyone else sees; landing the fish only after reading the chain.
- `client/FishBoard.client.luau` — a leaderboard with no server behind it.
- `scripts/dev-chain.js` — a local fork to play it before it is deployed, with a `discover` command.

### A PvP arena with ranked results — *the Funmaster's duels*

Hearts, knockback and streaks decided by the server; ranked 1v1 and 2v2 matches that seal the
fighters off from everyone else; results agreed by both sides with EIP-712 signatures, or witnessed
by bystanders when a side will not agree, then put on chain by whoever wants to pay for it.

- `server/Health.luau` — hearts, hits, knockback that grows as you weaken, streaks.
- `server/Weapons.server.luau` — melee and a charged wand shot, judged on the server.
- `server/Duels.luau` — challenges, sealing, the town's record, signatures checked against the
  contract before they are kept, and the ranked board (an `OrderedDataStore`).
- `contracts/DuelRecords.sol` — a result nobody can write alone; `signerOf` so a server can check a
  signature with a read. `contracts/Duelling.sol` — the opt-in flag.
- `client/Overhead.client.luau` — names, hearts and rank over heads, hidden by line of sight.

### A shop and a wardrobe — *Finch and the paper doll*

Items gated by what you hold (a ladder of balances, checked by the contract itself), taken with
one transaction, then worn from a bag with sockets per body slot and an action bar.

- `server/Shop.server.luau`, `client/Shop.client.luau` — the shelf.
- `contracts/Inventory.sol` — holdings-gated claims, gifts, a founder seed in the constructor.
- `server/Wardrobe.server.luau`, `client/Wardrobe.client.luau` — wearing what the chain says you own.
- `server/ActionBar.luau`, `client/ActionBar.client.luau`, `shared/Carry.luau` — a layout you arrange by dragging.
- `server/ItemActions.luau`, `server/Handouts.luau` — what an item does, and things handed out
  that are never on chain (candles).

### A creator tool inside the game — *Kara's drawing table*

Draw on a grid, and the drawing is encoded to a PNG in Luau, stored on chain, wrapped in a cape
model, and recorded as yours — five of the player's own transactions, each one explained.

- `client/Paint.client.luau` — the table, with an `EditableImage` preview and a live try-on in a `ViewportFrame`.
- `shared/Png.luau`, `shared/Base64.luau` — pixels to bytes, in the place.
- `shared/Cape.luau`, `server/Tailor.server.luau` — the `store` chain: picture, model, thumbnail, description, record.

### DeFi in a world — *the trading house and the Hall of Records*

A PulseX swap floor, a pool screener, and a block explorer drawn as a place you can walk into.

- `client/Trade.client.luau`, `server/Trade.server.luau` — quote, swap, liquidity, through the player's wallet.
- `server/Screener.server.luau`, `client/Screener.client.luau`, `shared/Dexscreener.luau`, `shared/Market.luau` — a screener.
- `client/Explorer.client.luau`, `shared/Scan.luau`, `server/Records.server.luau` — an explorer, read by the machine looking at it.
- `shared/Engram.luau`, `server/Engrams.luau`, `server/Showcase.server.luau` — a transaction you can hold and show somebody. A link, not a mint.

### A world that moves with the market — *the braziers*

The server reads what five tokens traded on DexScreener, ranks them, and every client makes the
busier fires taller and the torches burn faster.

- `server/Ranks.server.luau` (`HttpService:GetAsync`, server only, only when whoever runs it allows it;
  six-hour volume on each token's deepest pool) → `shared/Ranks.luau` → `server/Brazier.server.luau`,
  `server/Weapons.server.luau`, `shared/Flames.luau` (`ParticleEmitter`s).

### A social hub — *the square*

- `client/Chat.client.luau` — `TextChatService` in the place's own clothes.
- `server/Whisper.server.luau`, `client/Whisper.client.luau` — `/w` on a private `TextChannel`.
- `client/Squelch.client.luau` — muting that never reaches the server.
- `server/Emotes.server.luau`, `shared/Emotes.luau` — `/wave /sit /lay /roll` as `TextChatCommand`s.
- `contracts/Announcements.sol`, `client/Announcements.client.luau`, `shared/TownOwner.luau` — an
  owner-only notice board, one at a time in chat with a pinned one above, and the owner's gold name
  following an address the chain names rather than a name anybody could copy.

---

## 3. Chain patterns, one each

| Pattern | Where |
|---|---|
| A player's own wallet, asked from their own machine | `shared/OwnWallet.luau` |
| A server desk asking a player to sign or pay | `server/Ledger.luau` (`askPlayer`, `askFor`, `requestFor`), `client/WalletBridge.client.luau` |
| Many reads in one round trip | `OwnWallet.request({ action = "read", calls = {...} })`, e.g. `client/FishBoard.client.luau` |
| Big numbers — wei, token amounts — without losing digits | `shared/Units.luau` |
| Signing in: a player *is* their wallet | the host's sign-in (`luau/gdextension/src/pulseblockz_world.cpp`), `Player` attribute `WalletAddress` |
| Consensus instead of proof — both sides sign | `server/Duels.luau`, `contracts/DuelRecords.sol` |
| Randomness from a contract | `contracts/Fishing.sol` (`Random()`), `contracts/DevRandom.sol` for local testing |
| An owner who can only run the notice board — announce, pin, unpin, and nothing else | `contracts/Announcements.sol` |
| Gating by holdings, not spending | `contracts/Inventory.sol` |
| Publishing content from inside a game | `server/Tailor.server.luau` |
| Declaring what a game uses, so the client can hold it to that | `scripts/publish-experience.js --uses`, `luau/gdextension/host/Uses.gd` |
| Saying what a transaction or signature can cost, in red on the wallet's prompt (approvals, sends, permits, orders) | `luau/gdextension/host/Danger.gd` |
| Which contracts a place trusts, named once | `shared/Contracts.luau` |

---

## 4. Roblox features the town leans on

A place written against Roblox's API should find these behave as documented.

| Feature | Used for |
|---|---|
| `TextChatService`, `TextChannel`, `TextChatCommand` | chat, whispers, emotes, system messages |
| `StarterGui:SetCore("SendNotification")` | "You got a Fishing Rod" (`client/Notify.client.luau`) |
| `BillboardGui` sized in studs, `DistanceLowerLimit` | names and hearts that shrink with distance |
| `ViewportFrame` | the paper doll, the try-on, model thumbnails |
| `EditableImage` | the drawing table |
| `GeometryService` (solid modelling) | the float split into a red half and a white half (`server/Fishing.server.luau`) |
| `Model:ScaleTo` | the caught fish held up big for the camera |
| The Audio API (`AudioPlayer`, `AudioEmitter`, effects) | the wand's charge swelling into its room (`server/Weapons.server.luau`) |
| `DataStoreService`, `OrderedDataStore` | duel results and the ranked board, rod grants, the action bar |
| `ParticleEmitter` | braziers, torches, streak fire |
| `UIScale` | the size buttons on every window (`shared/Theme.luau`) |
| `HttpService` (server) | the market data behind the braziers and Doug's board |

---

## 5. Sound and presentation

- **An NES in Luau.** `shared/NesEngine.luau` plays four of the 2A03's channels — two pulses, the
  triangle and the noise — from single-cycle samples: six waveforms in all, because a pulse has
  four duty settings (`scripts/prep-nes.js`). There is no DMC channel and nothing plays a
  recording. `shared/Vgm.luau` replays a VGM log through those four (`client/Jukebox.client.luau`);
  `shared/Chime.luau` plays a chord on it with no clip of its own; `shared/Mixer.luau` puts music
  and effects on their own channels.
- **One look.** `shared/Theme.luau` — one palette, one type scale, one window, for every panel.
  `shared/Glyphs.luau` is the font baked to pixels for things drawn cell by cell.
- **NPCs that talk.** `Ledger.onTalk` plus `client/Dialog.client.luau`: the server decides what is
  said, the client only draws it. Every desk in town is built on it.
- **Arriving.** `client/Intro.client.luau` — the game's splash, a fade, a rocket, a loading bar that
  waits on real work rather than a timer; `server/Arrival.server.luau` — nobody is placed in the
  world before they can see it.
- **Movement.** `client/Controls.client.luau` — WoW-style mouse and keyboard; `server/Trampoline.server.luau`.

---

## 6. Tooling around a place

| Tool | What it does |
|---|---|
| `scripts/dev-chain.js` | A local fork of the testnet with the undeployed contracts on it, and commands to fund, announce, pin, and queue a discovery |
| `scripts/publish-experience.js` | Publishes a place: every script by hash, plus `--thumbnail`, `--splash` and `--uses` |
| `scripts/publish-catalogue.js`, `scripts/publish-place-assets.js` | Items and place assets |
| `scripts/deploy-*.js` | Each contract; `deploy-inventory.js` seeds the founder in the constructor |
| `luau/gdextension/demo2/tests/run.ps1` | The test suite, scheduled around shared ports |

---

## Not worth copying

- `server/Angling.luau`'s `_setCatches` is a test hook that ships in the place.
- `server/Chain.server.luau` caches item URIs in a DataStore across restarts. It is checked against
  the chain every start, but a place that does not need the speed should not keep chain data at all.
- `HttpService` on the server is not yet one of the capabilities a manifest declares (`--uses`).
