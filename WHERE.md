# What happens where

Five places anything in this project can live. Every row below says which one, and why it
cannot be somewhere else. `ARCHITECTURE.md` argues the design; `SERVER.md` covers hosting;
this is the map for deciding where a new thing goes.

The rule everything follows: **as much on the chain as can be, as little on the server as
possible, and nothing on the client that the client could lie about.**

**A second axis this map does not have.** All five places below are inside the KIT — the
Studio, the engine, the host layer, the contracts. Cutting across them is whether a given file
belongs to the kit at all or to `demo2`, one place built to prove the kit. Something can be in
the right one of the five and still be in the wrong product; `README.md` has that split, and
the test is **"would a place that is not the town need this?"** Answer it by what the thing is
FOR, not by how generic it looks -- demo2's PNG encoder, base64 and token maths all read like
utilities and all three are the town's.

---

## The five places

| | What it is | What it can do | What it cannot |
|---|---|---|---|
| **1. Chain** | Contracts on PulseChain (`addresses.943.json`), ownerless but for `Announcements`, whose deployer alone announces, pins and unpins (`contracts/Announcements.sol`, an immutable `owner` with no transfer), and `RNG` and `atropaMath`, rebuilt from mainnet's verified source and Ownable in a way that gates nothing beyond ownership itself (`scripts/deploy-atropa.js`) | Remember things forever, for everybody | Know where you are standing, or anything that is true for four seconds |
| **2. Host** | GDScript in `luau/gdextension/host/`, copied into each Godot project by `node scripts/sync-host.js` | Reach the network, hold a key, sign | Be trusted by other players — it is the player's own machine |
| **3. Engine** | C++ in `luau/gdextension/src/` | Physics, rendering, replication, the Roblox instance tree | Be changed by a creator |
| **4. Place — server** | Luau in `scripts/src/server/` | Judge what happened, for everyone at once | Reach the network, except through `HttpService`, which only whoever runs the server can turn on |
| **5. Place — client** | Luau in `scripts/src/client/` | Draw, and ask | Decide anything that matters |

`scripts/src/shared/` is Luau both halves read. It decides nothing; it is where a number that
must be the same on both ends lives, so it cannot be two numbers.

---

## 1. On the chain

| Thing | Contract | Why it cannot be anywhere else |
|---|---|---|
| The bytes of every mesh, texture, sound and script | `AssetStore` | Addressed by content hash, so the client can check what it got. A server holding these is a server you have to trust |
| What items exist | `UGC1155` | Anybody can register one. A registry on a server is a registry with a gatekeeper |
| Prices, sales, royalties | `Marketplace` | Set by whoever made the thing, enforced by nobody |
| Who holds what, and the holdings ladder | `Inventory` | `add` checks `msg.sender.balance` itself. A check in a client is theatre: anyone running their own skips it |
| Whether you are up for a fight | `Duelling` | One bit that should follow you to every town, including ones nobody has written yet |

**Not on the chain, deliberately:** who hit whom, hearts, deaths, the streak, where anybody is
standing, chat, who is in the room, and whether you are currently carrying a candle. Every one
of them is true for seconds and decided by two players' positions at one instant.

### The two seeming exceptions

**The candles.** A candle's model, thumbnail and metadata are on chain like everything else.
Only the *holding* is not: it is one bit on the server (`Handouts.luau`), because a candle is
spent the moment it lands and recording that on chain means a signature in the middle of a
fight for a fact that is true for four seconds. Sal hands you one if you have not got one;
using it takes it away.

**The founder's items.** `Inventory`'s constructor credits one address with a stated list,
once, before the contract has an address anybody can call — and leaves **no function behind
that could do it again**. Not a restricted one, not an ownable one, not one behind a flag. The
seeded id is `keccak(dataHash, tier)`, the same id anyone earns by standing on that rung, so
it is the thing you can hold and not a special edition.

---

## 2. On the host — the only thing with a key

Everything here is GDScript, outside the sandbox, on **the player's own machine**: the shared host
layer in `luau/gdextension/host/`, plus each project's own top script: the town's `Main.gd`,
`Serve.gd` and `Join.gd` in `luau/gdextension/demo2/`, the Player's `Player.gd`, the Publisher's
`Publisher.gd`, the Studio's `Studio.gd` and the kit's own test project's `Main.gd` in
`luau/gdextension/demo/`.

| File | What it does | Why the host |
|---|---|---|
| `Wallet.gd` | The request dispatch — `read`, `write`, `store`, `fetch`, `who` and the named actions still on its allowlist — and its own account | The one thing that touches a key. Every signature shows the player what it is before it happens |
| `ChainAssets.gd` | Fetches `pblockz://` and verifies the hash | Bytes that do not hash to what was asked for are thrown away here, before the world sees them |
| `Scan.gd` | The block explorer | A public read, no key, signs nothing — so it does not queue behind a purchase |
| `Market.gd` | DexScreener searches, on a channel only a place's **client** scripts write (`MarketRequest` / `MarketResult`) | Each player's own machine answers their own question, on their own bandwidth. It ranks nothing: the fires are ranked in the place, by `Ranks.server.luau` |
| `Curation.gd` | Signed blocklists | A choice the player makes about their own client |
| `Uses.gd` | What a published place declared it uses, and the refusal when it asks for more | The client enforces it at the dispatch, before any of it runs |
| `Title.gd` | Start / Connect Wallet / New Wallet | Key handling. A creator's script must never be near it |
| `Main.gd` | Wires the above together, publishes the splash | — |
| `Serve.gd` / `Join.gd` | Which half of the engine to be | Small, and neither holds any game code: they set `world.mode` before the world enters the tree and then report |
| `Tx.gd`, `Abi.gd` | Signing and encoding | — |

**A creator's CLIENT script cannot reach the network, ever.** `httpGuard` errors on
`!isServer()` before it looks at anything else (`luau/src/rbx_api.cpp`), and
`HttpService`'s three request methods are the only network primitives in the whole Luau API.
There is no flag that turns that off. A creator's SERVER script can reach it, and only then:
`rt.opts.httpEnabled` defaults to false (`luau/src/rbx_runtime.h`) and is the
`http_enabled` property whoever runs the box sets — the Player sets it false on both worlds
it builds (`luau/gdextension/player/Player.gd`), so a player's own machine never
lets a place out, hosting included.

That is the sandbox and it is the point. When a script needs anything else from outside, it
writes a numbered request into the tree and the host decides — `Ledger.ask` on one side,
`_handle_request` on the other.

**What the host does NOT know.** There were eighteen named actions here once — buy, claim,
tailor, keepsake, duel — one town's feature list living inside the client, which meant a
creator's place could not ask for a nineteenth thing: the allowlist simply refused it. Five
verbs are the vocabulary that replaces them; what is left beside those five is `sign_typed`
and the eight PulseX actions, fourteen entries in all today, and each leaves the list as the
town moves off it. The client's
`ADDRESSES` now holds exactly one contract: the store it writes to by default, which is
configuration rather than a rule. Which contracts a town runs on is the town's, in
`Contracts.luau`, where changing one means republishing the place.

The line, everywhere it comes up: **deciding what to ask for is the place's, going and
getting it is the client's.** It is the same line that put the PNG encoder in the place and
left the decoder in the client — see `luau/gdextension/demo2/scripts/COLLECTORS.md`.

---

## 3. In the engine

C++, and a creator cannot change it. Physics, rendering, the instance tree, ENet replication,
the Luau sandbox itself, and every Roblox class the place uses.

The rule here is short: **anything Roblox has and this does not is a bug to fix, not to work
around.** Found and implemented that way: `BillboardGui`'s `PlayerToHideFrom`,
`ParticleEmitter.Size` (never read — `billboard_keep_scale`), `ParticleEmitter.Squash`, and the
skybox's quarter-turned Y faces.

---

## 4. The place, server half

One authority per question. If two players could disagree about it, it is here.

| File | Question it answers |
|---|---|
| `Health.luau` | Hearts, damage, burns, knockback, deaths, the streak, and who may fight whom |
| `Weapons.server.luau` | What a click does: which technique, when the head is cutting, what it lands |
| `Wardrobe.server.luau` | What you are wearing, and what you own |
| `Shop.server.luau` | What is on the shelf and whether you may take it |
| `Handouts.luau` | Whether you are carrying a candle |
| `Bank`, `Records`, `Tailor`, `Trade`, `Screener`, `Funmaster` | What each NPC says |
| `Whisper.server.luau` | Which TextChannel a pair of people share, made when they first speak |
| `Brazier`, `Torch`, `Sky` | Building the world out of what arrived. The rocket and the tree are not scripts at all — they are `Rocket.model.json` and `Tree.model.json` in the place's `map` folder |
| `Ranks.server.luau` | Which of the five tokens is busiest. One of the town's only two `HttpService` callers — the other is `Dexscreener.luau`, behind Doug's board |
| `Chain.server.luau` | What the town reads off the chain: the shelf, your bags, the rung |
| `Engrams.luau` | Taking an Engram: two documents stored and one record written |
| `Ledger.luau` | The request channel to the host |

**Why the server and not the client:** a client that decided whether a blow landed is a client
that decides it always lands. The client is told the outcome; it never proposes one.

---

## 5. The place, client half

Drawing, and asking. Nothing here is trusted.

| File | Draws |
|---|---|
| `Intro.client.luau` | The card, the dark, the rocket, the loading bar |
| `Hearts.client.luau` / `ActionBar.client.luau` | The HUD |
| `Overhead.client.luau` | The name and the hearts over everyone else's head |
| `Weapons.client.luau` | Turns a click into a request — the server decides if it is a swing |
| `Combat.client.luau` | What a fight sounds like, from what the server said |
| `Dialog`, `Shop`, `Wardrobe`, `Screener`, `Trade`, `Paint`, `Showcase`, `Explorer` | The panels |
| `Chat`, `Jukebox` | Talking, and the tune |
| `Whisper.client.luau` | Reads `/w <name> <message>` and sends it on the pair's own channel |
| `Squelch.client.luau` | `/squelch` — who this machine has stopped reading. Never the server's |

`Weapons.client.luau` is the shape to copy: it checks whether a click is worth sending, and the
server checks again whether it was allowed. A client saying it is armed proves nothing.

---

## Shared, and why it has to be

`scripts/src/shared/` holds the numbers that must be identical on both ends. Each of these was
two copies once, and each pair drifted:

- **`Weapons.luau`** — which accessories can be swung. The candles went in on the server only,
  so clicking with one did nothing and nothing said why.
- **`Flames.luau`** — seven palettes: the five token fires (pulse, pulsex, hex, provex, inc) plus
  ice and ember for anyone who wants an ordinary one. A PLSX flame has to be the same flame in a
  brazier and in a hand.
- **`HeartRow.luau`** — hearts are drawn twice, on your screen and over everyone's head.
- **`Ranks.luau`** — what `Ranks.server.luau` published about the five tokens. It reads an
  attribute; nothing in it reaches the network.
- **`Theme.luau`** — one palette. There were four, and they had already drifted.
- **`Mixer.luau`**, **`NesEngine.luau`**, **`Vgm.luau`** — the sound.

---

## Two flows, end to end

**Taking something off the shelf.** The client presses a card → the server checks the rung it
published against the rung you stand on → writes a request → the host shows you what it is
about to sign → `Inventory.add(dataHash, tier)` → the chain checks your balance itself →
the wallet's next read sees it → the wardrobe dresses you. Nothing is paid; holdings are what
qualify you and spending them drops you back down.

**Landing a blow.** The client clicks → the server decides which technique, and whether the
head is inside its cutting window → it sweeps for anyone standing there → `Health.hit` or
`Health.burn` → both players are told what to draw and what to hear. Nothing touches the
chain, and nothing is recorded anywhere after the session.

---

## Where a new thing goes

Ask in this order, and stop at the first yes.

1. **Should it still be true in a year, to somebody who never met you?** → chain.
2. **Does it need a key, or the network?** → host.
3. **Could two players disagree about it?** → place, server.
4. **Is it a picture of something already decided?** → place, client.
5. **Is it a number both halves have to agree on?** → shared.

If it is on the server, there should be a sentence saying why it cannot be on the chain.
