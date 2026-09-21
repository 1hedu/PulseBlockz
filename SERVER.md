# Hosting a town — the smallest server that can exist

The rule this is written to: **as little on the server as possible, as much on the chain as
can be.** Where something is on the server, there is a sentence saying why it cannot be
anywhere else. Paths are relative to `luau/gdextension/` unless they start with `luau/`, which
are from the repo root. Bare names: the `rbx_` C++ is in `luau/src/`, `pulseblockz_world.*` in
`luau/gdextension/src/`, `Serve.gd`, `Join.gd`, `Main.gd` and `scripts/src` in the town's project
`luau/gdextension/demo2/`, `Player.gd` in `luau/gdextension/player/`, `setup.sh` in
`luau/tools/droplet/`, and a `.luau` under the town's `scripts/src`.

## 1. What a server is here

It is the same binary as the client, told which half to be. The engine has three modes
(`pulseblockz_world.h`): Play Solo runs a server and a client in one process, **Server** runs
the server half alone and lets Client worlds join over ENet, and **Client** is fed by a Server.
There is no separate server program: a second implementation of the game is a second thing to
keep in step, and what a creator publishes is what should run.

```
godot --headless --path luau/gdextension/demo2 -s res://Serve.gd -- --port=8800
godot           --path luau/gdextension/demo2 -s res://Join.gd  -- --host=play.safewrap.xyz --port=8800
```

Both are small and neither holds game code. They set the mode before the world enters the tree,
because the world reads it in its own `_ready`, and a node's `_ready` runs when it is added; after
that they only report — joins, leaves and slow frames on the server, frame, socket and texture
counts on the client — save that `Serve.gd`, given `--private`, also stamps `PrivateServerId` and
`PrivateServerOwnerId` on the DataModel once the world is in.

Verified end to end headless: a server on 8802, a client that joined it, the join logged by
name on the server.

## 2. What has to be off chain, and nothing else

| On the server | Why it cannot be on chain |
|---|---|
| Where everyone is, this frame | 20–60 updates a second per player. No chain takes that, and it is worthless a second later. |
| Who hit whom | The result depends on two players' positions at one instant. Only the thing holding both can judge it. |
| Hearts, deaths, respawns, the streak | Downstream of the above, and gone when the session ends. |
| Chat in the moment | A relay. Nothing is stored. |
| Who is in the room | Presence. True only while it is true. |

Everything else stays where it already is:

| On chain | Where |
|---|---|
| Item ownership | UGC1155 |
| Prices, sales, royalties | Marketplace |
| Assets — meshes, textures, sounds, fonts, the skybox | AssetStore, addressed by content hash (`pblockz://`) |
| The experience itself — every script in `scripts/src` | AssetStore |
| The map | Same |
| What a wallet holds | Read by the player's own machine |

The server reads the chain like anybody else and **holds none of it**. What it does keep, in
its DataStores, is the town's own game state that no chain holds: ranked duel results before
anybody puts them on chain, who has been handed a fishing rod and where a line is out, each
player's action bar, and a cache of item uris that is checked against the chain every start. No
accounts and no keys; moving it to another box is copying a directory.

**A game server sends state. It sends no script source and no asset bytes over a socket.** In
`luau/src/rbx_net.cpp`, `Replicator::sendsProperty` returns `inProcess_` for a `NoReplicate`
property (`rbx_net.cpp`) and `Source` is declared `NoReplicate`
(`rbx_instance.cpp`, on `LuaSourceContainer`, so `Script`, `LocalScript` and `ModuleScript`
all have it), so `Source` does not cross. `Replicator::sendContentHolder` is a no-op
(`rbx_net.cpp`) and a `Content` value on the wire is a kind byte, a uri and a ref and never
the bytes (`rbx_wire.cpp`), so bytes a script made do not cross either.

On join the server names its published place in the Welcome packet (`NetPacket::place`, wire
protocol 5, `rbx_wire.h`; sent by `apply_server` and stored by the client in `net_receive`, both
`pulseblockz_world.cpp`). The shipped client — the Player app — fetches that place from the chain, checks every
file against its own hash, and loads the **code** over the instances the server sent:
`Player.gd` `_take_the_code` calls `Experience.mount(uri, code_only)`, which drops every file
that is not `.luau` or `.lua` (`luau/gdextension/host/Experience.gd` and `_is_code`). Only then does the
client become the local player, because becoming it is what copies `StarterPlayerScripts` into
`PlayerScripts` (`PulseBlockzWorld.hold_for_place` / `ready_for_place`,
`pulseblockz_world.cpp`). `ScriptSync.load_place()` is the same filter for a place
on disk (`host/ScriptSync.gd`); nothing in the shipped apps calls it, only `net_test.gd`.

`demo2/Join.gd`, the developer's join script above, does none of this: it names no place, and
nothing in `demo2/Main.gd` listens for the one the server names, so its client is handed the tree
and no code for it. A client world never does the full disk load — `ScriptSync._ready` returns
early for `MODE_CLIENT` (`host/ScriptSync.gd`) — so on a `Join.gd` client no client code runs at
all unless `--place` is given.

A client cannot rebuild the tree itself: a client's `DataModel` hands out **negative** instance
ids, counting down from −1 (`rbx_instance.h`, `nextId_`, set and stepped at
`rbx_instance.cpp`), so nothing it makes can be mistaken for the server's.
Loading a model file on a joined client would *replace* what the server sent rather than fill it
in — `loadSourceFile` destroys whatever stands at that path (`rbx_host.cpp`), where a
script file at a path a script of the same class already holds is reloaded in place
(`rt.reloadSource`, same function) — which is why only code is loaded. Instances are the server's; source is the chain's.

A server that names no published place is still joinable. None of its code runs on the joining
client, and the player is told why (`Player.gd`, `_take_the_code`).

There are two exceptions, and neither is a game server.

In process: Studio's play mode, Play Solo and `LocalSession` are the same program with the same
files and no socket, and set `Replicator::setInProcess(true)` — `pulseblockz_world.cpp` for
`MODE_PLAY_SOLO`, `rbx_net.cpp` for `LocalSession`. The network path never sets it.

Team Create, which is an edit session rather than a game, and which **does** put `Source` on a
socket. A hosting world with `edit_mode` set builds its runtime with `setReplicateAll(true)`
(`ensure_runtime`, `pulseblockz_world.cpp`), and with `all_` set `sendsProperty` returns true for every
property (`rbx_net.cpp`), so a joined Studio is sent every service and every script's text
over ENet; its own edits come back the other way as `NetEdit` `chunk` / `file` ops
(`rbx_wire.h`, applied only by an edit world, `pulseblockz_world.cpp`). This is not a
hole in the game path — a game server leaves `edit_mode` false — but it is source over a wire,
and `luau/gdextension/demo/tests/team_test.gd` asserts it arrives.

The test is `luau/gdextension/demo/tests/no_code_over_the_wire_test.gd`: a real socket, a server
holding a client script that would announce itself, and a client that never receives its text
and never runs it.

## 3. The server needs no key

`Wallet.gd` runs read-only when `PBLOCKZ_PLAYER_KEY` is unset — it says so itself: *"no key;
reads work, buying is off"* (`luau/gdextension/host/Wallet.gd`). So a public server is deployed with **no key
of any kind**: `setup.sh` writes a unit with no `Environment=` line and puts no key file on the
box. It can read the chain and cannot sign anything, which is the correct amount of power for a
thing strangers connect to. And in a server process (`world.mode == 1`) `_load_key` returns
before it looks anywhere (`host/Wallet.gd`), so a key file or a `.env.testnet` left on
the host is never loaded either. Two things hold it shut, not one: with `world.mode == 1` every
action in `PLAYER_ONLY` asked of the server's own channel is refused unasked
(`host/Wallet.gd`), and with no key loaded there is nothing to sign with anyway.

The player's wallet stays on the player's machine: keys are the player's, inventory is read by
the player's own hardware, and the thing they connect to is only told where they are standing. A
server holding an inventory is a server that can take it.

## 4. Putting it on safewrap.xyz

The domain is on GoDaddy; the transport is **ENet, which is UDP**. That rules some things in
and out:

- **DNS only.** An `A` record for `play.safewrap.xyz` → the box's IPv4. GoDaddy's
  forwarding, and Cloudflare's proxy, are both HTTP — they cannot carry this. The name must
  resolve straight to the host.
- **One UDP port open for players**, 8800 by default, plus SSH for whoever runs the box:
  `setup.sh` allows `OpenSSH` and `$PORT/udp` and then turns the firewall on, and nothing else
  (`setup.sh`).
- **No TLS, and no certificate to buy.** ENet is not encrypted here — nothing sets DTLS on the
  peer. No key and no transaction crosses it: a purchase is signed and submitted by the player's
  own machine. **Signatures do cross it**, in the clear, in two places — the sign-in `Proof`
  packet carries the address and the 65-byte signature (`rbx_wire.h`), and a desk that asks a
  player to sign something gets the signature back over a RemoteEvent (`Duels.luau`,
  EIP-712 duel results). Each is bound to what it is for — the sign-in names the server and a
  nonce (§5.1), a duel signature names the duel — so a listener gets something it cannot reuse
  elsewhere, not nothing.
- **A small box.** Headless Godot with no rendering. The town is bigger than "a handful": its
  map files alone declare 176 parts (173 `Part`, 3 `MeshPart`) and `scripts/src` holds 94
  `.luau` files. Start at 1 vCPU / 1GB and watch it.

A unit file, so it comes back after a reboot. This is the unit `luau/tools/droplet/setup.sh`
writes (`setup.sh`), with `PORT=8800`, `PUBLIC=play.safewrap.xyz` and `PLACE` set to
pBlockz Home — what is actually running on the live box cannot be read from this repo:

```ini
# /etc/systemd/system/pblockz-town.service
[Unit]
Description=PulseBlockz town
After=network-online.target
Wants=network-online.target

[Service]
User=pblockz
WorkingDirectory=/opt/pblockz/demo2
ExecStart=/opt/godot/godot --headless --path /opt/pblockz/demo2 -s res://Serve.gd -- --port=8800 --public=play.safewrap.xyz --place 'pblockz://25accbadcd14c6df55859f5c39e4eef6fc53df4a8f33421e3166e25f7bc4db77?chain=943:0x0f9D08e13BE2345856026615d05F7251F07efAfA:1363&mime=application%%2Fjson'
Restart=always
RestartSec=3
# Deliberately no PBLOCKZ_PLAYER_KEY, and no .env.testnet anywhere on this box. The server does not sign.

[Install]
WantedBy=multi-user.target
```

`%%` is systemd's escape for a literal `%`; `setup.sh` doubles them (`${PLACE//%/%%}`,
`setup.sh`). With `--place` the server runs the published place rather than the project's own
scripts (`Main.gd`): at boot it fetches the manifest and all 114 files of
pBlockz Home from the chain and checks each against its hash — 114 is what the publish recorded
in the repo root's `experiences.943.json`, the Publisher's record of what it published. The
Player ships no copy of that record: its home list is what its own machine opened.

### Running a server for somebody else's game

A server does not have to run its own copy of the scripts. Given a published game's uri, it fetches
the manifest and every file from the chain, checks each against its hash, and runs exactly that:

```
godot --headless --path luau/gdextension/demo2 -s res://Serve.gd -- --port=8800 --place 'pblockz://<hash>?chain=943:...'
```

or, on a droplet, `PLACE='pblockz://...' bash setup.sh pblockz-town.tar.gz`. So anybody can run a server for
pBlockz Home and know it is pBlockz Home they are running: the hash names it. What they cannot do is
prove that to a player -- a remote machine cannot show what code it is executing without attested
hardware. What a player can rely on instead is that the server holds no key and cannot move anything
of theirs, that their own client runs code it checked by hash, and (planned, §5.3) that the game's
creator can list on chain the servers they vouch for.

## 5. What is not done yet

1. ~~**Anyone can claim any name.**~~ Built. A player is their wallet: the server sends each
   joiner a nonce, their own wallet signs it (EIP-191) on their own machine, and the server
   recovers the address and puts it on the Player as `WalletAddress`
   (`pulseblockz_world.cpp`). A wrong proof is disconnected; an empty one is a joiner
   saying they are a guest (`net_receive`, `pulseblockz_world.cpp`). The display name is still whatever the
   client offers, so treat the address, not the name, as the person.

   What is signed names the server as well as the nonce:

   ```
   PulseBlockz sign-in
   Server: <host:port the client dialled>
   Nonce: <nonce>
   ```

   (`sign_in_text`, `pulseblockz_world.cpp`.) With the nonce alone, a server you joined
   could hand you another server's challenge and sign in there as you with your answer. A server
   takes a proof only for the port it listens on and a name it answers to: its own interface
   addresses, `localhost`, and the names given with `Serve.gd -- --public=<name>`
   (`accepts_server_name`, `pulseblockz_world.cpp`; `Serve.gd`; the droplet passes
   `play.safewrap.xyz`, `setup.sh`). A server dialled by a name it was not told about refuses
   every sign-in and closes that connection, so give it every name players use.
2. **Position is client-owned.** The client simulates its own character and reports it —
   network ownership, as Roblox does it. A modified client can therefore walk through a wall
   or stand where it likes. The server checks only that a write is for that client's own
   character, is a property a character's owner gets to set, and is finite — there is no clamp
   on speed or distance (`pulseblockz_world.cpp`). Combat is judged server-side,
   which is the half that matters for fairness, but a server-side sanity check on speed and
   reach is the next thing to write.
3. **Discovery is a hostname.** `ARCHITECTURE.md` §1 wants an on-chain registry: experience
   id → endpoint, signed by the creator's key, verified by the client. A DNS name is the
   stand-in until that exists.
4. ~~**Two wallets in one process.**~~ Examined, and settled. The server's `Wallet.gd` holds no
   key and only answers its own scripts' **reads** of the game's contracts — the catalogue,
   and each signed-in player's holdings, read by their address, so it can judge what they may
   wear and swing. It refuses to sign anything (`PLAYER_ONLY` in `host/Wallet.gd`, refused
   for `world.mode == 1` in `_handle_request`, and it has no key to sign with anyway). Everything
   about a player's own wallet — balances, history, pools — is read by that player's machine
   and handed to the desks to show back to them (`Mine.client.luau`), and every write a desk
   wants is asked of that player's wallet (`Ledger.askFor`), which they confirm and pay for.
   `Ledger.data(player)` is one player's picture; there is no town-wide wallet.
5. **Nothing rate-limits a join.** One box, one open port, no cost to connect. A `Hello` is
   checked for protocol and a sane name and nothing else (`pulseblockz_world.cpp`);
   there is no throttle anywhere on the path.
6. **One town, one process.** No instancing. `--max` is the capacity: `Serve.gd` reads it
   into `world.max_clients` (default 32) and the world hands that to ENet when it opens the
   socket (`pulseblockz_world.cpp`, `listen(listenPort_, maxClients_)`). Past it, a joiner is
   refused at the socket.

## 6. What it costs

A VPS and a DNS record. No lobby service, no matchmaker, no accounts database, no asset CDN,
no payout system, nothing that holds anyone's money or keys. If the box disappears, the items
still exist, the assets still exist, the experience still exists, and someone else can start
the same binary behind the same name.
