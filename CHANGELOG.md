# Changelog

## 0.9-beta — 2026-09-21

**The wallet key is a keystore.** It was a bare hex key in a file that anything running as you
could read. It is now an Ethereum keystore — Web3 Secret Storage v3, the JSON other wallets
read — locked with a passphrase you choose. The title screen asks once a launch and holds the
key in memory for that run; decline and you play as a guest, where every read works and nothing
signs. The file is the backup: copy it to another machine and the same words open it there, on
any platform. A key from an older build is put away the first time you launch.

**A joined client whose place was already cached never became a player.** The server named the
place before the join was on record, so a host with nothing left to fetch answered into a guard
that dropped it: the tree arrived, the player never did, and the wait card stayed up. It had
been that way since joining began taking its code off the chain.

**A read that did not answer stopped reading as nothing held.** A failed pools read published a
hole over the last good one, so the trader's token list emptied itself and the teller called a
wallet with money in it empty. The explorer's page asked its three parts one after another and
lost the whole page to the first that died; it asks them at once now and draws what answered.

**A place cannot make itself a plugin.** Plugin capability is what the host recorded, not where
a script sits, so neither a parent write, a published file at that path, nor a replicated tree
can claim it.

**The Player ships no listing.** Its home page is what that machine opened, and Forget takes any
row off it. A player of games carries no one's bookmark.

**Publishing refuses an undeclared capability** rather than warning about it, because a place
whose `--uses` is short is a place whose every chain read a client silently turns down.

**Linux, as well as Windows.** The three apps build and run on x86_64 Linux from the same
checkout, against an engine library built by `luau/tools/linux-build.sh`. The server that
hosts a place for other people has been Linux from the start.

**Everything else, the first time it is offered.** Roblox places import and run:
`.rbxl` and `.rbxlx`, version 7 meshes (Draco), PBR surfaces, custom rigs, Motor6D animation
and terrain, with the Studio signing in to Roblox for a place's own assets. Verified on a
4,000-instance place against Roblox Studio side by side. Roblox API parity is measured against
Roblox's own API dump, class by class and member by member (`PARITY.md`); a member the engine
cannot act on is declared and says so rather than stopping a script. A place's scripts reach no
disk, no network and no key; source never crosses a socket; memory is capped; every claim in
`SAFETY.md` names the test that holds it. Licences ship beside the binaries in
`THIRD_PARTY.txt`, and `LICENSE` says what you may do with this: anything, with no chain or on
PulseChain, and not on another.
