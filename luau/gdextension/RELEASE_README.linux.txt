PulseBlockz 0.9-beta
====================

Three apps, one engine. Run any of the three. They share the .so in this folder, so keep them together.
Mark them executable first:  chmod +x *.x86_64

  PulseBlockz Studio.x86_64      Edit a place. Opens .rbxl and .rbxlx files from Roblox.
  PulseBlockz.x86_64             The Player. Opens published places and joins servers.
  PulseBlockz Publisher.x86_64   Puts a place on chain.

Importing from Roblox
---------------------
A place's meshes and textures are private on Roblox: it serves them only to their owner. The
Studio fetches them signed in as you. Put your .ROBLOSECURITY cookie in

  ~/.local/share/godot/app_userdata/PulseBlockz Studio/roblox_cookie.txt

The Studio sends it to Roblox and nowhere else, never logs it, and never writes it anywhere.
It is your whole account, so treat that file as a password. Without it a place imports with
every mesh as a grey block.

Playing
-------
The Player takes a pasted pblockz:// link or a server address, and keeps what you opened on
its home page. Before anything runs it shows what the place is, who published it, and what it
may ask of your wallet. Play alone runs it here; Host it opens a port for people you give your
address to; Join puts you on the creator's server. A joined server may not ask your wallet for
anything unless you tick the box that says it may.

Your wallet
-----------
Each app keeps its key in its own data folder under ~/.local/share/godot/app_userdata/, never in
this folder. It is an Ethereum keystore: the key encrypted under a passphrase you choose, in
the same format other wallets read. The app asks for it once a launch and holds the key in
memory for that run only; say no and you play as a guest, where everything reads and nothing
signs. That file is your backup -- copy it, and it opens on any machine with the same
passphrase. Nothing can recover the passphrase itself. The Studio holds no key.

Licence: LICENSE. Third-party software and licences: THIRD_PARTY.txt.
