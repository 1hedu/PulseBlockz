# PulseBlockz asset URIs and on-chain storage

Every asset a UGC1155 token points at, and every file that asset's metadata points at
(model, script, animation, texture, sound), is named by its **content hash**. Where the
bytes live is a hint, not the identity: any mirror that serves bytes matching the hash is
usable, and the chain is the default mirror.

## The URI

```
pblockz://<keccak256 of the bytes, 64 hex>[?chain=<chainId>:<AssetStore>:<blobId>][&tx=<chainId>:<manifestTx>][&mime=<type>][&src=<url>]...
```

| Part | Meaning |
|---|---|
| host | keccak-256 of the exact bytes. The identity. A client MUST verify it after any fetch. |
| `chain=` | the blob in an `AssetStore` on that chain: bytes in contract code, i.e. state. |
| `tx=` | a copy published as transaction calldata (below): cheaper to write, lives in block history. |
| `mime=` | what the bytes are (`model/gltf-binary`, `application/x-rbxm`, `application/json`, ...). Also recorded on chain. |
| `src=` | zero or more mirrors (https, ipfs). Faster than RPC for big files. Repeatable. **A hint, not an instruction**: the host fetches one only if the place declared the `mirror` capability *and* the host name is in `ChainAssets.mirrors_allowed`, which ships empty. A place cannot make your machine visit an address it chose. |

An asset may name several of these at once; they are locations for the same bytes, and
the hash decides whether what came back is right.

**A document names an asset by bare hash.** A field whose whole job is to name an asset --
a metadata document's `model` and `thumbnail` -- holds `0x<keccak>` and nothing else: no
scheme, no `?chain=`, no `mime=`. Which chain, which store and which blob hold a copy is a
fact about a deployment, not about the thing, so a document that writes it down has to be
republished whenever one of them changes. Both dropped facts are on chain beside the bytes:
`AssetStore.blobOf(hash)` says which blob holds them, and the blob records its own `mime`.
A client resolves a bare name with two extra `eth_call`s -- `blobOf` for the blob, then
`blob` for its mime and size -- each batched with the rest of a shelf, so it is two round
trips for a whole shelf rather than two per item (`ChainAssets._locate`).

The query still belongs on a **fetch plan** -- a place manifest's `files`, read in bulk the
moment somebody joins, where naming the blob saves the lookup. In a place manifest only
`thumbnail` and `splash` are bare.

Resolution order for a client: cache by hash → `chain=` via `AssetStore.read(blobId)` (one
`eth_call`) or `chunksOf` + `eth_getCode` per chunk → `tx=` via the manifest transaction and
its chunk transactions → each allowed `src=` in turn. Verify the hash; on mismatch, discard
and try the next source. Consult curation lists (`curation/SPEC.md`) before fetching: a `uri` entry
with a `pblockz://<hash>` prefix hides exactly one asset.

A UGC1155 token's `uri` is one of these, pointing at a metadata JSON blob:

```json
{ "name": "Wizard Hat", "kind": "accessory", "slot": "hat", "blurb": "…",
  "tier": 2, "tier_pls": 1000000,
  "model": "0xab12…",
  "thumbnail": "0xcd34…",
  "license": "CC0-1.0", "attribution": "", "source": "scripts/catalogue.js" }
```

Those are the fields `scripts/publish-catalogue.js` writes and the ones
`luau/gdextension/demo2/scripts/src/server/Chain.server.luau` reads back.

A hash that has to survive inside a *property string*, where a reader is scanning for
something that looks like an asset (a `Decal`'s `Texture` inside a model), keeps the scheme
and drops the rest: `pblockz://cd34…`. Sixty-four loose characters of hex are not something
that scan finds.

`license`, `attribution` and `source` are what make CC-BY assets usable and copyright
reports checkable.

## AssetStore

Ownerless. Anyone stores; nothing changes afterwards.

- **Chunks** are contracts whose code is `0x00 || data` (the SSTORE2 pattern): the leading
  STOP means nothing can execute the data; code is state every node keeps, unlike logs
  and calldata which nodes may prune. A chunk holds at most 24,575 bytes (EIP-170).
- **Blobs** are ordered chunk lists with a `contentHash`, `size` and `mime`. `publish`
  recomputes the hash and size from the chunks' code on chain, so a blob's hash is
  guaranteed, not claimed. `blobOf(hash)` finds an existing blob for dedupe.
- **Reads**: `read(blobId)` returns the whole blob from one `eth_call`; `readRange` streams
  a window; `chunkData(addr)` one chunk; or skip the contract and `eth_getCode` each chunk.
- **Writes**: `storeAndPublish` when the whole asset fits in one transaction; larger ones go
  `storeMany` per batch, then `publish`. Batches are packed by **encoded calldata size**, not
  by chunk count — see the transaction limit below.

Cost is ~200 gas per byte to deploy code plus the transaction overhead, so a 100 KB asset
is roughly 20M gas: on PulseChain a fraction of a cent, on Ethereum mainnet prohibitive.

### What a real chain enforces (measured on testnet v4)

- **Calldata per transaction: 49,152 bytes.** PulseChain applies EIP-3860's initcode limit to
  *every* transaction, not just contract creations, and rejects anything larger at
  `eth_sendRawTransaction` with `INVALID: initcode too large`. `eth_estimateGas` accepts the
  same payload, so this only shows up when you actually send. The writer therefore uses
  24,000-byte chunks (`CHUNK_BYTES`) so two fit in one transaction at 48,196 bytes of
  calldata, and packs batches against `MAX_TX_DATA`. The contract's own 24,575-byte ceiling
  (EIP-170) is unchanged, so chunks may be larger if a chain permits bigger transactions.
- **No Cancun.** PulseChain has `PUSH0` but not `MCOPY` or `TSTORE`. Contracts compile for
  `shanghai` (the default in `scripts/compile.js`) and OpenZeppelin is pinned to 5.0.2,
  because 5.1+ emits `MCOPY` in OpenZeppelin 5.1's own Bytes.sol and Arrays.sol.
- **Fees.** The node suggests a 10,000 gwei priority fee while the base fee is 7 wei and real
  blocks tip ~0.03 gwei. Taking the suggestion literally costs ~50 PLS for one deployment, so
  `bridge/src/fees.js` bids 1 gwei instead (`PRIORITY_FEE_GWEI`, or `FEE_MODE=node` to defer
  to the node). Storing a 123 KB asset then costs well under 0.01 PLS.

What fits comfortably: Roblox-format models (hundreds of bytes to tens of KB), scripts,
animation sequences, low-poly meshes, small textures. What doesn't: large textures and
sounds; put those behind `src=` mirrors and keep only the hash on chain.

## Published as calldata (`tx=`)

An asset can also live in plain transaction calldata rather than contract code. This is the
scheme an existing PulseChain publishing tool already uses, so `tx=` opens documents and
images published with that tool, not only ours.

- **Chunk transaction**: `{"parentHash": <previous chunk hash or 0>, "chunkData": "<base64>",
  "mimeType": "..."}`, sent to the zero address with value 0. It runs nothing; the payload is
  the point.
- **Manifest transaction**: `{"mimeType": "...", "chunkHashes": ["0x...", "0x..."]}`, listing
  its chunks in order. The manifest's transaction hash is what `tx=` names.
- **Reading**: fetch the manifest with `eth_getTransactionByHash`, then each chunk, base64
  decode, concatenate, and check the keccak against the URI. Nothing is trusted because it
  came from a chain; it is trusted because it hashes correctly.

**Which to use.** Calldata costs 16 gas a byte, plus a third more for base64, against roughly
200 for contract code: about ten times cheaper, which matters for images, PDFs and sounds. The
trade is permanence. Contract code is state, which every full node keeps for as long as the
chain exists; calldata is history, which clients may prune and which Ethereum is moving to
expire. PulseChain keeps full history today.

The second constraint is lookup: **anything a document names by bare hash has to be in an
`AssetStore`.** `blobOf(hash)` is the only route from a hash to bytes; a calldata copy is
findable only through the transaction that carried it, which a bare hash does not name.

So: the things a world cannot open without (a model, a script, a metadata document) and
anything a document points at go in `AssetStore`; large things named by a full link
(textures, sounds, documents) go in calldata or on a mirror, with the hash keeping every
copy honest.

## Thumbnails

An asset's metadata may carry a `thumbnail` field: the content hash of a small image, so a
client can draw a store listing or an inventory row without fetching the whole model.

Thumbnails go in the `AssetStore`, not calldata. Calldata is cheaper, but a document names
its thumbnail by hash alone and only `blobOf` resolves that.

```json
{ "name": "Wizard Hat", "kind": "accessory",
  "model": "0xab12…",
  "thumbnail": "0x77ee…" }
```

### Tooling

```
node scripts/assets.js store hat.rbxm                  # into AssetStore; prints the pblockz:// URI
node scripts/assets.js store art.png --via calldata    # as manifest + chunk transactions instead
node scripts/assets.js fetch 17 --out hat.rbxm         # read() + verify
node scripts/assets.js fetch 0x99dc... --out doc.pdf   # by manifest transaction hash
node scripts/assets.js fetch "pblockz://…"         # by URI, same verification
node scripts/audit-documents.js                        # how every listing on chain names its parts
```

`scripts/audit-documents.js` reads every listing in the registry and reports, per document,
whether `model` and `thumbnail` are a bare hash or still a whole link, and for the bare ones
whether `blobOf` can find the bytes. Reads only, sends nothing; it exits non-zero if anything
named would fail to draw.

`scripts/assets.js` is also a module (`storeBytes`, `fetchBlob`, `parseUri`, `toUri`) and
the reference for the C++ resolver in `luau/src/chain_assets.*`, which the Godot client
uses through `PulseBlockzChain`.

### Assets referring to other assets

A model may name further assets by `pblockz://` URI in its own properties — a `Decal` or
`Texture` whose image is a separate blob, say. Those references stay as they are in the
tree, so a model mounted on the host replicates to every client with ids that mean the same
thing there. When the engine needs the bytes to draw one it raises `asset_wanted`; the host
fetches, checks the hash and hands back a local file path
(`luau/gdextension/demo2/host/Wallet.gd`, `_on_asset_wanted` → `_cache_media`). One image
can then be shared by several models and stored once.

Two kinds of image show up in the demo. The PulseBlockz gradient is a *shared* texture: one
blob, referenced by every model that wears it. A cape's emblem is the item's own, since no
two capes wear the same one. The publisher keeps both in `catalogue.943.json` keyed by
slot, and skips storing anything whose bytes already hash to what is recorded there --
without that, fixing one cape rewrote every item in the catalogue, which is seventy of them
(`scripts/catalogue.js`, `ITEMS`).

Godot picks its loader from the file extension, so cached media is written as
`user://media/<first 32 hex of the hash>.<ext>`, the extension taken from the URI's `mime=`
when it carries one and otherwise from the blob's own mime (`ChainAssets.mime_of`). The
mapping is a fixed table in `_cache_media`, and anything whose mime is not in it -- or which
has no mime at all -- is written `.png` regardless. A `.glb` filed as `.png` is a file the
engine never looks at, and it fails silently. This is separate from
`ChainAssets`' own hash-keyed cache of raw bytes under `user://assets`.

### The demo catalogue

```
node scripts/publish-catalogue.js                 # publish or update everything
node scripts/publish-catalogue.js --only tophat   # just one
node scripts/publish-catalogue.js --preview out/  # draw the art, publish nothing
```

`scripts/catalogue.js` defines every wearable the demo puts on chain as geometry plus a
painted thumbnail, and `scripts/capes.js` the emblems that go on the back of a cape. Those
emblems are approximations of third-party marks drawn from simple geometry; to publish real
artwork instead, drop a PNG at `scripts/logos/<key>.png` and it is used verbatim, no code
change. Portrait images around 3:4 fit the cape without stretching.

All of the art is drawn by `scripts/png.js`, a small deterministic raster + PNG encoder:
assets are content-addressed, so the same drawing has to produce the same bytes every run
or it publishes as a different asset.

Token ids live in `catalogue.943.json`, so a second run **updates** the tokens that already
exist rather than minting a duplicate of each: on an ownerless registry there is no admin
and no burn, and a stray token cannot be taken back.

### Permanence, honestly

Bytes in contract code persist as long as the chain does and every node holds them. That
is the property you want for a place or an item, and it is also a commitment: nothing
here can be removed by anyone, including the creator. Curation lists are how a client
declines to show something; they do not make it go away.
