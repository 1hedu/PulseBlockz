# Why this is safe to run

Two programs: **the town** (`luau/gdextension/demo2`), which connects to a chain and holds a
key, and **the Studio** (`luau/gdextension/studio`), which does neither. They share an engine
and a sandbox, and their risks are different.

Every claim below names the file that makes it true, and the last section says what is **not**
safe yet. Paths are relative to `luau/gdextension/` unless they start with `luau/` or
`scripts/`, which are from the repo root.

---

## The three questions

Running somebody else's game asks you to trust them with three things:

1. **Your machine.** Their code runs on it.
2. **Your money.** Their game is standing next to your wallet.
3. **Yourself, among strangers.** Other people are in there with you.

Roblox answers the first with a sandbox, the second by having no wallet at all, and the third
with a moderation department. This answers the first the same way; the second by keeping the
key somewhere a creator's code cannot reach and showing you every signature before it happens;
and the third without a department.

One section each, then the Studio, then what is still wrong.

---

## 1. Your machine: what a creator's script can do

Every script a place ships runs in `luau/src/luau_sandbox.cpp`. It is a fresh `lua_State` whose
globals are stripped in `Sandbox::Sandbox` and then frozen by `luaL_sandbox` in `Sandbox::seal`
**before any creator code loads**. What survives that stripping is not what the sandbox left:
`rbx_runtime.cpp` puts three of them back, deliberately, because Roblox has them.

| Global | Actually |
|---|---|
| `debug` | **Gone.** Nilled at `luau_sandbox.cpp` and never restored; `print(debug)` says `nil` |
| `getfenv` / `setfenv` | **Gone.** `luau_sandbox.cpp`, never restored |
| `os` | **Back**, at `rbx_runtime.cpp`: `os.time`, `os.clock`, `os.date`. A script can read your clock and your date, which is what fingerprints a host |
| `collectgarbage` | **Back** at `rbx_runtime.cpp`, but only `collectgarbage("count")`; anything that would drive the collector raises |
| `gcinfo` | **Back** at `rbx_runtime.cpp`: kilobytes in use, the same number |

Luau ships **no `io`, no `package`, no `loadstring`, no `dofile`** at all (`luaL_openlibs` in
`luau/luau/VM/src/linit.cpp` opens base, coroutine, table, os, string, math, debug, utf8, bit32,
buffer and vector, and `integer` only under the `LuauIntegerLibrary` fast flag, which defaults to
false and nothing in this repository sets; the base library's own list is
`luau/luau/VM/src/lbaselib.cpp`). There is no file system. **A place cannot write itself new code
while it runs.** `require` compiles whatever a `ModuleScript`'s `Source` holds (`g_require`,
`rbx_runtime.cpp`), so what holds is that a place's scripts cannot write `Source`: it carries
`PluginWrite` (`rbx_instance.cpp`), Roblox's PluginSecurity, and the Lua setter refuses a thread
without plugin capability (`rbx_api.cpp`). That capability is one thing — the calling script sits
under something the host itself put directly under the DataModel's **one** `PluginDebugService`,
or the chunk is the host's own (`hasPluginCapability`, `isPluginScript` and the `pluginRoots`
record, `rbx_runtime.cpp`). Sitting under the service is not enough: only `addPlugin` and
`addPluginFile` write the record, both reached from the Studio's plugin loader
(`studio/Studio.gd`, `plugin_add` and `plugin_add_file`) and from nothing in the Player, which
loads no plugins. A script that reaches the service any other way — by `Parent`, by
`Instance.new`'s second argument, by a clone, as a place file whose path begins
`PluginDebugService/`, or in a tree a server replicates — is not in the record and does not run.
The Lua setters refuse the service outright (`inPluginTree`, `rbx_runtime.cpp`; `rbx_api.cpp`,
*"cannot parent to PluginDebugService"*), and the source-file loader refuses that path for
anything but the Studio's own plugin files (`ensureContainers`, `rbx_host.cpp`, *"a place file
cannot go there"*; `import_place` skips the service as well). The host's loaders write `Source`
from C++ and never pass the Lua setter, so published modules load.
`demo2/tests/no_new_code_test.gd` checks both halves, because a place whose own modules stopped
loading would otherwise pass; `luau/gdextension/demo/tests/plugin_escape_test.gd` tries every
route into the service from a place script — thirteen checks: each refused, the script still where
it was, the service still empty, `Source` and `HttpEnabled` still a plugin's to write, the place
file refused and never run; harness section R59 plants a Script under the service by hand and
shows it is no plugin, while `addPlugin` and `addPluginFile` are. `print` is replaced with one that hands the text to the host (`rbx_runtime.cpp`,
`rt.cb.print`) rather than writing to stdout itself — where it goes after that is the host's
choice, and the town's host prints it.

**No `HttpService` from a player's machine.** Roblox places expect the service to exist, so it
does, and every method that would reach outside raises instead: `GetAsync`, `PostAsync` and
`RequestAsync` all go through `httpGuard` (`luau/src/rbx_api.cpp`). What is left —
`JSONEncode`, `JSONDecode`, `GenerateGUID` — is pure string work that touches nothing. There is
no socket API either. **Three** things enforce it, not two, and the third is the one that covers
you when you are the one hosting:

1. The runtime refuses a client's call: `if (!rt.dm.isServer()) luaL_error(…"can only be called
   from the server")` — `rbx_api.cpp`.
2. The host drains the request queue only for a server world: `start_http` is reached from
   `apply_server` and from nowhere else (`pulseblockz_world.cpp`).
3. Neither of those covers **Play Solo or "Host it"**, where your own machine *is* the server
   and `isServer()` is true. What covers that is the Player setting `world.http_enabled = false`
   in both `play()` (solo and hosting) and `join()` (`player/Player.gd`), which `httpGuard`'s
   second check refuses on (`rbx_api.cpp`). The one Lua path that could turn it back on, the
   `HttpEnabled` setter, requires plugin capability (`rbx_api.cpp`, *"cannot set
   'HttpEnabled'"*), which no place script can obtain (above). The town's own project does the
   same for a fetched place in `demo2/Main.gd`, in `_enter_tree`.

**What a place can still make your machine fetch.** Two channels, and both are real:

- **A `src=` mirror on an asset URI — closed, and worth describing, because it was open.** A
  `pblockz://<hash>` may carry `?src=<any url>` (`chain_assets.cpp`), and `ChainAssets.fetch`
  used to GET that URL from your machine with no allowlist at all. The bytes are thrown away
  unless they hash to the content hash, so it could never fetch *code* that way — but the
  request went out. Run here, before the fix: a sandboxed script set `Decal.Texture` to a
  `pblockz://…?src=http://…/?leak=…`, the world asked the host for it
  (`pulseblockz_world.cpp`, `Wallet.gd`), and the listener on the other end logged the
  hit. That was a phone-home carrying whatever the place wrote into the URL. `mirrors_allowed`
  (`host/ChainAssets.gd`) is **empty by default** and `_fetch_url` is only reached for a
  `src=` whose host matches an entry (`_mirror_allowed`); an unlisted mirror is skipped with a
  warning and the asset is fetched from the chain store instead. `player/tests/mirror_test.gd`
  stands up a listener and checks it is never called.
- **`scan` and `market`.** A place that declares those capabilities can have the host query the
  block explorer or DexScreener from your box with a search string of the place's choosing
  (`host/Scan.gd`, up to 128 characters; `host/Market.gd`). The host names the API, so
  the place cannot choose *who* is asked — only what is in the question.

`Uses.gd` is what holds a published place to the capabilities its manifest declared: it is
checked at the wallet's request dispatch (`Wallet.gd`), at the scan (`Scan.gd`) and at the
market. A place that declares none of `transact`, `sign` or `pulsex` cannot put a wallet prompt
in front of you at all — those three capabilities own every action that reaches a `_confirm`
(`Uses.gd`), and `permits` returns false for an undeclared one (`Uses.gd`). A
place that declares one of them can ask for anything of that kind, and what stands between that
and you is the prompt, not the list. A place running off local files declares nothing and is
refused nothing (`Uses.gd`). The mirror channel is behind `mirror`, its own capability, and
that gate and the allowlist answer different questions: the list is the **player** saying whose
servers they are willing to talk to, the capability is the **place** saying it wants fetching
from outside the chain at all. Both have to say yes, so allowlisting a CDN for one game does not
hand it to every other game. Fetching from the chain store itself needs no capability — the host
names the RPC, not the place.

So: a creator's script cannot open a socket, cannot use `HttpService` on your machine, and
cannot fetch code that does not hash to what was asked for. It can cause your machine to make
an outbound request to an address it chose.

**A server cannot hand you code to run, either.** What runs on your machine when you join is
not what the server sent. A game server sends instances and their properties; it never sends
script source (`Replicator::sendsProperty` in `luau/src/rbx_net.cpp` refuses a `NoReplicate`
property unless `inProcess_`, so in a play world `Source` never crosses a socket) and never sends
asset bytes (`Replicator::sendContentHolder` is a no-op). The server names its published place in the Welcome packet (`NetPacket::place`, wire
protocol 5); your client fetches that place from the chain and checks every file against its
own hash before a line of it runs — `Experience.mount(uri, code_only)`, and
`ScriptSync.load_place()`, which loads `.luau` and `.lua` and nothing else. Becoming the local
player waits for that, because becoming it is what copies `StarterPlayerScripts` into
`PlayerScripts` (`PulseBlockzWorld.hold_for_place` / `ready_for_place`). A server that names no
published place is still joinable; none of its code runs, and you are told why
(`player/Player.gd`). There are **two** exceptions, and the second is not a game:

- **In process** — Play Solo, Studio's play mode and `LocalSession` are the same program with
  the same files and no socket. `setInProcess(true)` is called at `rbx_net.cpp` and
  `pulseblockz_world.cpp` (`MODE_PLAY_SOLO`) and nowhere else; the network path never sets
  it.
- **Team Create.** `sendsProperty` returns `true` for everything when `all_` is set, and so
  does `visible` for every service (`rbx_net.cpp`); `all_` is set from `edit_mode` by
  `setReplicateAll` where `pulseblockz_world.cpp` builds the server runtime. A Studio hosting
  a Team Create session (`studio/Studio.gd`) therefore *does* send every joined Studio the
  `Source` of every script over a real socket, unchecked against any hash. Nothing starts in
  an edit world, but pressing Play there runs what the host sent you.
The door is held shut by a test with a real socket:
`luau/gdextension/demo/tests/no_code_over_the_wire_test.gd`, where a server holds a client
script that would announce itself and the client never receives its text and never runs it.

**Two budgets are on, not four.** The sandbox can enforce four (`luau_sandbox.h`:
1,000,000 steps, 4 ms a call, 8 MB), and the interrupt and the allocator that enforce them are
real (`Sandbox::interrupt` and `Sandbox::alloc`, `luau_sandbox.cpp`). But every one of those checks is written
`if (budget_.maxSteps && …)` — zero means off — and `PulseBlockzWorld`'s constructor sets three
of them to zero (`pulseblockz_world.cpp`), leaving it to the host to say what it wants:

- **steps** — `maxSteps = 0`. **Off** in the engine, and no host sets one.
- **memory** — `maxMemory = 0` in the engine; the two apps that join a stranger's place set
  **64 MB** (`player/Player.gd`, applied in both `play()` and `join()`; `demo2/Main.gd`
  `_enter_tree`).
- **wall clock** — `maxMillis = 10000`. **On, at ten seconds**, which is Roblox's script-timeout
  and not the sandbox's 4 ms.
- **frame budget** — `maxFrameMillis = 0`. **Off.**

Which host sets what, because that is the whole of it:

| Host | Steps | Wall clock | Frame | Memory |
|---|---|---|---|---|
| **Player** (`player/Session.tscn`) — the app you join a stranger's place with | none | 10 s | none | **64 MB** |
| **The town** (`demo2/Main.gd`) | none | 10 s | none | **64 MB** |
| **Studio**, edit and Play (`Studio.gd`) | none | 10 s | none | none |
| the old `demo` (`demo/Main.gd`) | none | 4 ms | 8 ms | 32 MB |

Sixty-four is measured, not picked: pBlockz Home joined against the live server peaks at
**4.98 MiB** of Luau heap and sits near 3.3 (`world.get_stats()`'s `memory_peak`, which
`rbx_runtime.cpp` now reports from the allocator that was already counting it). That is the
scripts' own tables — meshes, textures and sounds are Godot's and are not in it — so the cap is
about twelve times what a rich place uses. A place that passes it gets *"not enough memory"*
raised in the script that asked, which a place can catch; it is not a crash. Who sets it:
`player/Player.gd` — `PLACE_MEMORY_MB` is 64, and `play()` and `join()` each assign it to
`world.max_memory_mb`; the engine's own default is no cap. `player/tests/memory_budget_test.gd`
reads that constant and sets `world.max_memory_mb` from it on a world it builds itself — it
runs neither `play()` nor `join()` — then asks for 400 MB and checks the script is stopped,
never holds 400 MB, and the reported peak stays under the budget (63.5 MiB on a run). It
proves the engine holds the cap the Player names; that the Player applies it is those two
assignments in `Player.gd`, not the test. The Studio is deliberately outside this — it runs
your own files, and an import that needs more than 64 MB is not an attack on you.

So: a script that spins is stopped, after ten seconds, and scripts run on a worker thread
(`threaded_ = true`, `pulseblockz_world.h`) rather than the frame's; a script that
allocates forever is stopped at 64 MB in the apps you join strangers with. Step and frame
budgets are still off, and will stay off until there is a measured number for them the way
there is for memory — a step cap guessed at is a cap that kills working places.

A **server** is a different question: it can reach the network, if the person running it says
so. `http_enabled` is off in the engine (`rbx_runtime.h`) and is the operator's decision about
their own box — Roblox's "Allow HTTP Requests", the same shape and for the same reasons. Say the
rest of it plainly, because "off by default" is not true of the town's own server: the town's
scene ships it **on** (`demo2/Main.tscn`), and `demo2/Main.gd` turns it off again only
when that process is *not* a server (`int(w.mode) != 1`). So a box running `Serve.gd` — including
one running somebody else's place by `--place` — has HttpService enabled unless the operator
edits the scene. The Player's "Host it" is the other way round: off, always
(`player/Player.gd`). The town uses it for
one thing: reading what five tokens traded, so that everybody standing in the square sees the
same fires. That request comes from the server's box, not from thirty players' machines, and it
carries nothing about anyone who is playing. Without it a place cannot ask the world anything,
and every such feature has to be built into the client instead — which is what "the braziers
only work in the client that shipped them" looked like before.

When a place needs something from *you* rather than from the internet — a chain read, a
purchase, a block explorer page — it writes a numbered request into the tree and the host
decides. `Ledger.ask` on one side, `Wallet._handle_request` on the other. A script asking to
spend your money is a string in an attribute until a human agrees.

### What it can still do

A script can make things in the world, move them, play sounds, draw GUI, and talk to other
players through the chat. So it can be **annoying** — flashing the screen, blaring audio,
covering the display. It cannot read your disk or your keys, and it learns nothing about you the
game did not already put in the tree. It *can* make your machine send a request to an address it
chose, by the two channels above, and it can read your clock through `os`.

An example of the class, found and fixed this session: clicking the action bar also swung the
weapon, because `Mouse.Button1Down` does not care that a control took the click. The fix was
to honour `gameProcessedEvent` — the same flag Roblox has, implemented rather than worked
around.

---

## 2. Your money: the town

### The key never goes near a script

`Wallet.gd` is GDScript, outside the sandbox, and it is the only thing in the project that
keeps a secret key and signs with it. `_load_key` looks in four places, **none of them inside
`res://`**, and stops at the first that answers:

1. `PBLOCKZ_PLAYER_KEY` in the environment
2. `Wallet.unlocked` — what the title screen opened this launch, in memory only
3. `user://watch.address` — an address with **no key**, which reads and cannot sign
4. `.env.testnet`, gitignored, for development

So the key is never part of the game's own files, never in the repository, and never anywhere
a creator's script could reach even if the sandbox were bypassed. Whatever is found, the
address is **derived from the key** before anything is trusted — the town reports on, and
spends from, the account the key actually controls, so it can never show you one balance and
sign for another.

### The key at rest, and who can open it

`user://player.key` is an **Ethereum keystore** (Web3 Secret Storage v3): the key encrypted
under a passphrase the player chooses, in the JSON geth writes and other wallets read. PBKDF2
over the passphrase, AES-128-CTR over the key, a keccak MAC over the ciphertext, all in
`luau/gdextension/src/keystore.cpp`. Nothing in it is bound to a machine or an operating
system, so the file is the backup: copy it to the next computer and the same words open it
there, on Windows, Linux or macOS alike. `player/tests/keyfile_test.gd` holds it to that,
including the specification's own published vector, so the format is the standard one and not
a lookalike.

`host/Title.gd` asks for the passphrase once a launch, hands the key to
`Wallet.unlocked` — in memory, never written, never logged — and each session takes it from
there. Decline and you play as a guest: every read still works, and nothing can sign. A
passphrase is taken **before** a key is written, whether the key was just made, pasted in, or
found bare from an older build, so a key never reaches the disk unprotected. A wrong passphrase
is refused by the MAC, which also means a keystore somebody altered will not open at all.

`player/Player.gd`'s `wallet_line` derives the address for the home screen and holds the key in
a local that ends with the call; locked, it reads the address off the keystore's face and says
so, without the passphrase. Nothing in a place's tree, and nothing inside the sandbox, reaches
any of those files.

### Every signature is shown, and says what it is

`_confirm` puts a modal in front of every signature a *place* can ask for: transactions
(`chain_write`, `Wallet.gd`), typed-data signatures (`sign_typed`), publishing bytes to the
chain (`chain_store`), and PulseX's swap and liquidity calls (`Pulsex.gd`). What, how much, to whom, on
which network — read off the chain rather than from whatever the asking script claimed. The
donation prompt says in as many words that you get nothing for it and there is no refund. The
buy prompt shows the price it read, not the price it was told.

Two honest footnotes. A swap that needs an ERC-20 approval sends two transactions under one
prompt — the prompt says so in those words ("Two transactions…", `Pulsex.gd`). And
`send_prepared` (`Wallet.gd`) signs with no prompt of its own, for the publisher, which
asks once over the whole plan; a place cannot reach it, because `_handle_request`'s action list
(`Wallet.gd`) does not contain it.

Turn it off only for headless tests (`confirm_purchases`, `Wallet.gd`; every use in the tree
is a test).

### A public server holds no key

`Wallet.gd` runs read-only when no key is set — it says so itself: *"no key; reads work,
buying is off"*. A server is deployed without one, so it reads the chain like anybody else and
cannot sign. It holds no money and no custody of anything: what it stores in its DataStores is
the town's own game state (duel results, rod grants, the action bar) and a cache checked against
the chain. Those records are keyed by the wallet address you signed in with, and keep the name
you played under (`Duels.luau`) — game state about you, on the operator's disk, the
way any Roblox server's DataStore is.

It is **enforced, not just a practice**: in a server process (`world.mode == 1`) `_load_key`
returns before it reads the environment or any file (`Wallet.gd`), so an operator who
left a `user://player.key` or a `.env.testnet` on the host still runs a server with no key.
`Serve.gd` sets that mode before the scene enters the tree, which is what makes the check
land. On top of that, a server's own request channel refuses every signing action
(`PLAYER_ONLY` and the first check in `_handle_request`, `Wallet.gd`). One caveat worth writing down: the
test is `world != null and int(world.mode) == 1`, so it is the wallet finding its `World`
sibling (`Wallet.gd`) that makes it true — in the town's scene and the Player's session
it does.

Your wallet stays on your machine. Your inventory is read by your own hardware.

### Nobody can take anything off you

The five contracts that hold what is yours — `AssetStore`, `UGC1155`, `Marketplace`,
`Inventory`, `Duelling` — have **no admin, no pause, no upgrade, no owner**. Not the author.
There is no delist switch and no role. Read the executable lines, not the headers:
`AssetStore.sol` has no access control at all; `Duelling.setFighting` writes
`optedOut[msg.sender]` and nothing else (`Duelling.sol`); `Marketplace.setPrice` reverts
unless `ugc.creatorOf(id) == _msgSender()` (`Marketplace.sol`); `Inventory.forget` reverts
unless the count is already zero and the list is your own (`Inventory.sol`), and `give` and
`add` move only `msg.sender`'s.

One privileged address does exist, and it is not a person: `UGC1155.minter` is fixed at
construction to the `Marketplace` and is the only thing allowed to `mint`
(`UGC1155.sol`). Registering an asset is open to anybody (`register`, `UGC1155.sol`), so there
is still no publisher who decides what may exist.

The town is not five contracts, either. `addresses.943.json` also records `DuelRecords`,
`Fishing` and `Announcements` from `contracts/`, and `RNG`, `atropaMath` and the three
libraries `RNG` links, which are in no file of this repository: `scripts/deploy-atropa.js`
rebuilds them from mainnet's verified source and deploys them for `Fishing` to draw its random
number from (`rng`, immutable, `Fishing.sol`). Of the twelve files in `contracts/`, none uses
`Ownable` or `onlyOwner`, and `Announcements.sol` is the one that names an owner: the deployer,
immutable, the only address that can post, pin or unpin. It holds nothing of yours — it is the
town's noticeboard. `RNG` and `atropaMath` are both `Ownable` and the deployer owns them, and
`scripts/deploy-atropa.js`'s account of that ownership is that it gates nothing beyond
transferring or renouncing itself; their source is not here to read, so that is the script's
word. "No privileged address anywhere" would be false, so it is said here instead.

The one exception is stated where it lives: `Inventory`'s constructor credits a founder with a
list, **once**, before the contract has an address anybody can call, and leaves no function
behind that could do it again. You can read the deployment transaction and see exactly what
was seeded and to whom.

The rest of the repository's contracts are `MockERC20` and `MockRandom` (test doubles),
`DevRandom`, and `Forwarder`. `Forwarder` is OpenZeppelin's audited `ERC2771Forwarder` unchanged,
for gasless meta-transactions — the town does not use it today (`Wallet.gd` signs and sends
directly), and if it ever does, note what that costs you: a relayer cannot forge your
signature or change what you signed, but it **does see what you are about to do and can
decline to send it**. Rate limiting and allowlists live in the relayer, off chain,
deliberately.

### Assets are checked, not trusted

Everything the world loads arrives as `pblockz://<content hash>`. `ChainAssets.gd` fetches it —
from the chain, from calldata, or from a URL — and **throws it away if the bytes do not hash
to what was asked for**. A hostile mirror can serve you rubbish and it will not load. You do
not have to trust this town's copy of anything, including this town's.

---

## 3. Other players

### Nobody in this town can hit you who you have not agreed to

`Duelling.sol` holds one bit per address. It is ownerless like the rest — you set your own
(`setFighting`, `Duelling.sol`), nobody sets it for you, and there is no list of exceptions.
`Health.luau` checks it before anything lands: `Health.mayHurt`, called from `Health.hit`,
`Health.burn` and `died` (which runs `killerOf`'s answer through it before crediting a kill); and the
Funmaster in the square is just a door onto `setFighting`
(`Funmaster.server.luau`), which also reads your flag back off the chain when you sign in
(`readFlag`, a `fighting(address)` read through `Ledger.request`).

What that is **not** is enforcement. The check runs in this town's own server script, so it
binds this town and no other. The contract says so itself: *"a statement of intent that servers
honour"* (`Duelling.sol`). Your bit is readable everywhere and honoured wherever somebody
wrote the check — a place that skips it is a place where it does nothing, and nothing on chain
can stop that. A guest who has not signed in has no address to read, and plays by the town's
default.

The flag defaults to **in**: this is a square with wooden weapons and hearts that refill, not a
place with anything to lose. Turning it off is one click, and it is remembered for every town
that bothers to look.

### The client never decides anything

A client that judged whether a blow landed is a client that judges it always lands. So it does
not judge. It sends *"I clicked"* and the server decides which technique that was, whether the
weapon's head was inside its cutting window, who was standing in the sweep, and what it did.
Both players are then told what to draw. `Weapons.client.luau` is the shape everything copies:
it checks whether a click is even worth sending, and the server checks again whether it was
allowed.

The same rule covers the shelf, though it is worth being exact about where the check is. The
client sends which listing it wants and nothing else; the tier and the content hash come off the
server's own published listing, not off the message (`Shop.server.luau`). Then the
**chain checks your balance itself**: `Inventory.add` reverts unless
`msg.sender.balance >= thresholds[tier]` (`Inventory.sol`), and the tier is folded into
the item id, so claiming a lower one buys a record of something nobody recognises. A modified
client cannot pick its own rung, and could not profit from it if it did, because that check is
not running on their machine.

The split runs the other way too: **instances are the server's, source is the chain's.** A
client's `DataModel` hands out negative instance ids, counting down from −1, so nothing a
client makes can be mistaken for the server's, and a joined client loads only code over the
tree it was sent — loading a model file would *replace* what the server sent rather than fill
it in.

### What you see is your own choice

The chain has no delist switch — nobody can make an asset stop existing, which is the point
and also the problem. `Curation.gd` is the answer: signed blocklists, published by anybody,
that a client subscribes to and merges. Signatures are verified in the GDExtension
(keccak-256 + EIP-712 + secp256k1 recovery), so a list cannot be forged in transit; without
the extension, only lists from `trusted_urls` are accepted and they are marked *unverified*.
Hidden is not deleted — the wallet still holds the token, this client just declines to draw
it. `ChainAssets.fetch` refuses a hidden URI before it fetches a byte.

It is not moderation: there is no authority, and nothing is subscribed by default. It is a way
to decline, not a way to remove.

---

## 4. The Studio

The Studio's position is simpler on the part that matters most: **it has no wallet, no key, and
does not sign.** There is no `Wallet.gd` in the project and no autoloads at all
(`studio/project.godot`). There is no money in it to lose.

It is not true that it talks to no network. **Team Create** opens a socket both ways —
`world.listen(TEAM_PORT, 32)` when you host (`Studio.gd`), a Client world when you join
(`Studio.gd`) — and, as §1 says, a Team Create host sends every joined Studio the `Source`
of every script, checked against nothing. HttpService stays off for scripts either way
(`http_enabled` is never set, and the engine's default is false).

Its other exposure is opening somebody else's place — an `.rbxl`, an `.rbxlx`, an `.rbxmx` —
and pressing Play, which runs their scripts, in the same sandbox as the town's and on the same
budgets — though the Studio sets no memory cap, where the Player and the town set 64 MB. The
edit world is `edit_mode = true`, which sets
`opts_.runScripts = false` (`pulseblockz_world.cpp`) and stops `shouldRun` at
`rbx_runtime.cpp`.

**"Opening a file runs nothing at all" was false, and was tested here.** `shouldRun` exempts
plugin scripts one line *earlier* than the `runScripts` check — anything under
`PluginDebugService` runs in an edit world, whatever the world says, which is right for a plugin
you installed. The bug was how "under `PluginDebugService`" was decided: by an ancestor's **class
name**. Importing a place merges the file's own services into the world's by class name
(`rbx_host.cpp`, `dm.getService(className)`), and `PluginDebugService` is a registered service
(`rbx_instance.cpp`) — so a hand-written `.rbxlx` whose root child was a
`PluginDebugService` holding a `Script` printed from inside the sandbox the moment it was
imported into an edit world, with no Play button pressed. A script parenting its own second
instance of the class was the same door.

Both are shut. `isPluginScript` walks to the DataModel's **one** `PluginDebugService` — the
instance `addPlugin` puts plugins in — and compares pointers (`rbx_runtime.cpp`), so another
instance of the class confers nothing; and import skips that service rather than merging it
(`rbx_host.cpp`), reporting it as skipped. Either fix alone would close the file door;
having both means neither is load-bearing. The third door, a script putting itself or a clone
*into* the real service, is the one §1 describes: the Lua setters refuse it for want of
plugin capability (`inPluginTree`, `rbx_runtime.cpp`; `rbx_api.cpp`), so the capability rests
on the host being the only thing that parents there. `studio/tests/plugin_provenance_test.gd`
drives a place file and a script down each of the first two paths and checks an installed
plugin still runs, so the capability cannot quietly disappear instead;
`luau/gdextension/demo/tests/plugin_escape_test.gd` tries the third from a place script.

Which parts are weaker than the town's:

| | Town | Studio |
|---|---|---|
| Sandbox, no `HttpService` for scripts | Yes | Yes, the same |
| Budgets | 10 s a call, 64 MB | **10 s only** — no memory cap |
| Opening a file runs code | — | No — both plugin doors closed (`rbx_runtime.cpp`, `rbx_host.cpp`) |
| A key to steal | Yes, guarded | **None** — unless a Roblox cookie is set (below) |
| Content-hash verification | Yes | Not applicable — local files |
| Source over a socket | Never | **Team Create sends it** |
| Curated blocklists | `Curation.gd` | **Not wired up** |

### What the Studio needs, honestly

1. **Curation is demo2-only.** `Curation.gd` — signed blocklists a player opts into — is not
   in the Studio. Less pressing than it sounds, because the Studio loads files you chose off
   your own disk rather than things strangers published, but it is a gap.
2. **Play-mode budgets are the defaults.** A place you are *authoring* wants a generous
   budget; a place you are *inspecting* wants a mean one. They are the same today — both are
   `script_timeout * 1000` (`Studio.gd`), and no step or memory cap is set in
   either.
3. **No "open read-only".** There is no mode that guarantees Play cannot be reached for a file
   you merely want to look inside.
4. **`PluginDebugService` was a hole, and is closed.** A place file could put a `Script` there
   and the edit world would run it, with Play never pressed, because `isPluginScript` matched on
   an ancestor's **class name** — so a second instance of the class, carried in by a file or
   parented by a script, conferred plugin reach. It matches on **identity** now
   (`rbx_runtime.cpp`): the one service `addPlugin` uses, a child of the DataModel. Import
   also drops the service outright rather than merging it (`rbx_host.cpp`), and a script
   cannot parent anything into the real one (`inPluginTree`, `rbx_runtime.cpp`), so the doors
   are shut independently. `studio/tests/plugin_provenance_test.gd` tries the first two and
   checks an installed plugin still runs; `luau/gdextension/demo/tests/plugin_escape_test.gd`
   tries the third.

The first three are conveniences that would make inspecting a stranger's place as comfortable
as inspecting your own. The fourth was a bug, and is fixed.

---

## What is not safe yet

- **This is a testnet.** The keys in `.env.testnet` are development keys in a plaintext file
  used by automated scripts. Do not put real money behind an address whose key has lived
  there. Generate a fresh one for anything that matters.
- **The sandbox has not been attacked by anybody but its author.** Everything above describes
  what it is built to do. It has not had an adversarial review, and it should have one before
  anybody stakes anything real on it.
- **A script can still be a nuisance.** Sound, screen, chat. Nothing stops a place being
  unpleasant.
- **Only two of the four budgets are on.** Memory is capped at 64 MB and the wall clock at ten
  seconds in the apps you join a stranger's place with (see §1); steps and the frame budget are
  not capped anywhere, and the Studio caps neither memory nor steps. A place cannot take the
  machine's memory any more; a place that computes hard inside the ten seconds still gets the
  whole of a worker thread.
- **Curation is opt-in and signed, not a moderation system.** There is no authority deciding
  what exists, by design. That cuts both ways.
- **Chat is not moderated, because nothing here is.** No filter, no report button, no
  department. A shared blocklist can hide an asset or a creator; it cannot hide what somebody
  types at you. Anybody putting this in front of people who are not adults should read that
  sentence twice.
- **A server can reach the internet, and the town's own server does by default.**
  `http_enabled` is false in the engine but true in `demo2/Main.tscn`, and a `Serve.gd` process
  keeps it — including when it is hosting somebody else's place by `--place`. A place you host
  is a place that can make requests from your box, with your address on them. Read what it asks
  for, or set that property false before you host. `HttpService` itself stays unreachable from a
  player's client.
- **A place can still make your machine ask an address a question, if it declared the
  capability.** Not through `HttpService`, and not through a `src=` mirror unless the place
  declared `mirror` **and** the player allowlisted that host — the list ships empty (§1). What is
  left is `scan` and `market`: the host picks who is asked, the place picks what is in the
  question. That is the remaining outbound channel, and it is a declared one.
- **A Roblox session cookie, if you give the Studio one.** A place file names its meshes and
  textures by asset id, and Roblox serves an uploaded mesh only to a session with rights to
  it — anonymous fetches answer 401, so a place imports as untextured blocks. The Studio reads
  a cookie from `PBLOCKZ_ROBLOX_COOKIE` or `user://roblox_cookie.txt` (`Studio.gd`) and hands
  it to the world. That cookie is full account access, so what holds it in: it is never a
  scene property (methods only, so it cannot be saved into a `.tscn`), there is no getter for
  the value, nothing logs it, and it is sent **only** when `cloud_fetch_base` still points at
  `https://assetdelivery.roblox.com/` — a base pointed anywhere else gets no cookie at all
  (`pulseblockz_world.cpp`). `studio/tests/roblox_cookie_test.gd` points the base at a local
  listener and checks the request arrives with no `Cookie:` header and the value nowhere in
  it. None of that makes holding one free: it is a credential on your machine, in a file, and
  Roblox's terms are Roblox's terms. It is off unless you put one there.
- **The equip race is mitigated, not proven fixed.** A freshly claimed item sometimes needed
  re-equipping to appear in hand. The dirty-flag fix and the one-frame yield make it much
  rarer; `equip_probe.gd` is the way back in. A cosmetic bug, but an unproven fix is an
  unproven fix.
- **`WalletConnect` is not built.** Connecting a wallet means pasting a key or watching an
  address. The safe half — watch-only, which reads your holdings and cannot sign — is offered
  first and covers everything but taking things off the shelf. Signing from a phone, so the
  key never reaches the machine at all, is the right answer and is not done.

---

## The short version

A creator's code runs on your machine with no file system, no `debug` library, no sockets and no
`HttpService`, a ten-second clock on each call, and sixty-four megabytes — about twelve times
what the town itself uses. It cannot write itself new code as it runs, so the hash you fetched
is what executes for the whole session, and it cannot name a URL for your machine to fetch: a
mirror has to be on a list that ships empty. A server you join cannot hand you code: it
sends state, you fetch the place from the chain and check every file by hash — except a Team
Create host, which sends the whole source to another Studio. The one thing that can sign lives
outside that sandbox, keeps its key outside the game's files, and shows a human every signature
a place can ask for. A public server holds no key. Nobody — including whoever wrote the
contracts — can take an item off you, freeze a sale, or change a price that is not theirs.
Everything the world loads is checked against its own hash before it is used. No client decides
anything a server has not checked, and nobody in *this* town can hit you who you have not agreed
to, because this town's server checks the flag you set on chain; another town is another town's
code.

The Studio is the same sandbox, and opening a file no longer runs anything in it — but it
sets no memory cap, because it runs files you chose, and it is the one app that can be given a
credential: a Roblox session cookie, so a place's own meshes can be fetched. Off unless you
provide one.

What is missing is a wallet you can sign from without pasting a key, an adversarial look at the
sandbox by somebody who did not write it, step and frame budgets measured the way memory was,
and — for the Studio — the curation and the tighter budgets.
