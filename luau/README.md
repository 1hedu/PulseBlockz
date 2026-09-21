# PulseBlockz — the Roblox-shaped runtime

Structurally a Roblox place: the same Instance tree, the same services, the same rule that
*where a script lives decides where it runs*, the same API names, `task.*`, RemoteEvents,
Rojo's file layout. Underneath it is a hardened Luau sandbox with hard budgets, an
engine-agnostic runtime that owns the tree and the scheduler, and a Godot 4 GDExtension
node (`PulseBlockzWorld`) that renders the tree and drives characters.

```
src/luau_sandbox.*     one lua_State, budgets (steps / time / memory), isolation
src/script_worker.*    the sandbox on a worker thread; job queue in, event queue out
src/rbx_instance.*     the Instance tree (DataModel): classes, typed properties, services,
                       change log, snapshot/rebuild.  Knows nothing about Luau or Godot.
src/rbx_types.cpp      Roblox value types for Luau: Vector3 (Luau's own vector), Vector2, UDim, Color3,
                       BrickColor, Content, CFrame, Enum, Ray, TweenInfo, Random, Font, the sequences,
                       and JSON for HttpService
src/rbx_api.cpp        the Instance API as scripts see it: game, workspace, Instance.new, members on
                       tagged userdata, signals and connections, and the service methods
src/rbx_runtime.*      one DataModel + one sandbox + the scheduler = a Runtime: task.*, wait, signals
                       firing, script lifecycle, players, tweens, step()
src/rbx_chat.cpp       both of Roblox's chat APIs over one remote: TextChatService routes a
                       TextChatMessage through channels and callbacks, LegacyChatService sends the text alone
src/rbx_spatial.cpp    spatial queries against the tree's BaseParts: GetPartBoundsInBox / InRadius /
                       GetPartsInPart, Blockcast / Spherecast / Shapecast, GetTouchingParts
src/rbx_pathfinding.cpp PathfindingService: Path:ComputeAsync, an A* over a 2-stud grid built on the
                       fly from the same queries Raycast and GetPartsInPart use
src/rbx_editable_mesh.* EditableMesh: a mesh a script builds and edits, one position per vertex and
                       normal / UV / colour per face corner with ids of their own
src/rbx_audio.h        audio volume curves (SetDistanceAttenuation / SetAngleAttenuation), shared by
                       GetAudibilityFor and the host mixer
src/rbx_internal.h     internals shared by the binding files (userdata tags, Runtime::Impl); not public
src/rbx_host.*         RuntimeThread (frames on a worker) and Rojo file conventions -> instances;
                       HttpService requests leave a frame as an HttpAsk and the answer rides back in
src/rbx_net.*          Replicator (server tree -> clients), RemoteMsg, LocalSession
src/rbx_wire.*         the packets on the socket: Hello / Welcome / ServerFrame / ClientFrame, wire protocol 5
src/eip712.*           keccak-256, canonical JSON, EIP-712 recovery (curation lists, ../curation/SPEC.md)
src/chain_assets.*     pblockz:// asset URIs -> AssetStore calldata, ABI decoding, chunk
                      assembly, content-hash verification (../ASSETS.md)
gdextension/src/       pulseblockz_world.*    the PulseBlockzWorld node: the Runtimes on worker threads,
                                              their tree mirrored into the scene; Play Solo / Server / Client
                       pulseblockz_crypto.*   PulseBlockzCrypto (static): keccak, canonical JSON, recover /
                                              sign_digest / address_from_key, curation-list verify
                       pulseblockz_chain.*    PulseBlockzChain: pblockz:// calldata, decoding, hash checks (static)
                       pulseblockz_tremolo.*  the one Roblox sound effect Godot has no equivalent of
                       luau_script_host.*     the pre-Instance-tree LuauScriptHost node; not in the build
gdextension/host/      the host scripts the town, the Player and the Publisher share (Wallet.gd, Pulsex.gd,
                       Scan.gd, Tx.gd, Experience.gd, ...); ../scripts/sync-host.js copies them into every
                       project that has a host/ folder (demo keeps its own older Wallet.gd / ChainAssets.gd /
                       Curation.gd; the Studio holds none)
gdextension/demo/      Godot project: Main.tscn, ScriptSync.gd (hot reload), scripts/ (a Rojo project),
                       tests/play_solo_test.gd, tests/net_test.gd, tests/team_test.gd (headless end-to-end)
gdextension/demo2/     the town: a Rojo project of desks that read the chain (below)
gdextension/studio/    the place editor (below)
gdextension/player/    the Player: title and wallet doors, a home screen of published experiences, a session
gdextension/publisher/ the form around host/Publish.gd that puts a place on chain
tools/sync.js          watches a Rojo project on disk, pushes saves into the running game
tools/sync.test.js     its test: loads, saves, deletes and project.json edits over a socket
test/                  harness: 737 checks, no Godot needed
attic/                 the pre-Instance-tree LuauScriptHost node, kept for reference only
```

## The model, kept exactly

- **Everything is an Instance.** `game` is the DataModel; `Workspace`, `ReplicatedStorage`,
  `ServerScriptService`, `ServerStorage`, `StarterPlayer/StarterPlayerScripts`, `Players`,
  `RunService`, `TweenService`, `Debris`, `HttpService`, `Lighting`, `CollectionService`,
  `UserInputService`, `DataStoreService` … are its children (`game:GetService`). Properties are typed and
  declared per class; a wrong type or unknown property errors the way Roblox does.
- **Placement decides execution.** A `Script` runs on the server if it is under
  `Workspace` or `ServerScriptService`; a `LocalScript` runs on a client if it is under
  `StarterPlayerScripts` (copied into `PlayerScripts`) or the player's character/GUI;
  a `ModuleScript` runs where it is `require`d; anything in `ServerStorage` or
  `ReplicatedStorage` sits still. `Disabled` and `Destroy()` end a script's threads and
  connections.
- **Server is authoritative.** The server's tree flows down to every client (everything
  except `ServerStorage`, `ServerScriptService` and script source); nothing a client
  does to its tree flows back up. The only way up is `RemoteEvent:FireServer` /
  `RemoteFunction:InvokeServer`; the only way down is `FireClient(s)` / `InvokeClient`.
  A remote's arguments carry what Roblox's do: numbers, strings, booleans, nested tables,
  `Instance` references, `Vector3` / `Vector2` / `CFrame` / `Color3` / `UDim` / `UDim2` /
  `EnumItem` / `Font`, and `NumberRange` / `NumberSequence` / `ColorSequence`.
- **Scheduler.** `task.spawn / defer / delay / wait / cancel`, `wait()`, `RunService.Heartbeat /
  Stepped / RenderStepped`, `Instance:WaitForChild`, `Signal:Wait`, tweens and `Debris`
  all advance in `Runtime::step(dt)`, one frame at a time, on whichever thread owns the
  runtime. A `WaitForChild` on the client wakes once the replicated batch has landed, so
  `workspace:WaitForChild("Sign").Tag.Label` finds the whole subtree, as on Roblox.
- **Players and characters.** `Players.PlayerAdded / PlayerRemoving`, `Player.Character`,
  `CharacterAdded`, an R6 rig (`HumanoidRootPart`, `Head`, `Torso`, four limbs) — or the R15
  one (`UpperTorso` / `LowerTorso`, an arm and a leg in three parts each) when
  `StarterPlayer.CharacterRigType` says `R15`, `Humanoid.RigType` saying which it is; both wear
  Roblox's attachment points, and the engine turns each arm and leg as one chain from the
  shoulder or the hip — spawned
  at a `SpawnLocation`, `Humanoid` with `Health / MaxHealth / WalkSpeed / JumpPower /
  MoveDirection / Jump / WalkToPoint`, `TakeDamage`, `Move`, `MoveTo` (+ `MoveToFinished`),
  `Died`, `RespawnTime`. The engine moves the rig; the tree sees the result. Any Model
  with a Humanoid and a HumanoidRootPart is a character the engine moves — an NPC is
  a rig plus `Humanoid:MoveTo`, as on Roblox (straight there; a route round things is
  `PathfindingService`, below).
- **Files are the place** (Rojo conventions, verbatim):
  ```
  ServerScriptService/Main.server.luau              -> Script
  StarterPlayer/StarterPlayerScripts/UI.client.luau -> LocalScript
  ReplicatedStorage/Util.luau                       -> ModuleScript
  ReplicatedStorage/Shared/init.luau                -> ModuleScript named Shared
  Workspace/Map/                                    -> Folder
  Workspace/Map/Lava.model.json                     -> the instance tree the file describes
  Workspace/Map/Shrine.rbxmx                        -> a model saved from Studio (Save to File, .rbxmx)
  ReplicatedStorage/Fixtures.rbxm                   -> the same, binary (.rbxm: Studio's default, rojo build's output)
  StarterPack/Wand/init.meta.json                   -> the Wand directory is a Tool (className)
  ServerScriptService/Main.meta.json                -> properties for Main.server.luau
  ```
  With a `default.project.json` the `tree` is read the way Rojo reads it: each `$path`
  mounts a directory (or a single file) at that place in the DataModel; the demo uses
  `src/server`, `src/client`, `src/shared`, `src/map`, `src/tools`. A node's `$className`,
  `$properties` and `$attributes` are the instance's `init.meta.json` (a file mount's
  `.meta.json`) — `"Lighting": {"$properties": {"ClockTime": 18}}` sets the service,
  `"Config": {"$className": "Configuration", "$attributes": {...}}` makes the instance —
  and editing them in the project file applies live; `$ignoreUnknownInstances` has
  nothing to say here. Without a project file, the directory *is* the DataModel.
  Reloading a file restarts its script (or refreshes the module cache); deleting it
  removes the instance and everything the script made.

  A `*.model.json` is `{"className", "properties", "attributes", "children": [{"name", ...}]}`;
  a `*.meta.json` (or `init.meta.json` for its directory) carries `className`, `properties`
  and `attributes` for an instance that already stands there — a Folder becomes the class
  named, keeping its children, so `src/tools/Wand/` with `init.meta.json`, `Handle.model.json`
  and `Swing.server.luau` is a Tool with a Part and a Script. Properties take Rojo's implicit
  forms by the property's type (`[x, y, z]`, `[r, g, b]` as floats or 0–255, `"Neon"` or
  `"Enum.Material.Neon"`, `[[xs, xo], [ys, yo]]`) and the explicit `{"Vector3": [...]}`,
  `{"Color3": ...}`, `{"Enum": 1280}`, `{"Float32": ...}`, `{"BrickColor": 21}` ones (a
  `TeamColor` or a Part's `BrickColor` also takes a name, `"Bright red"`); a `CFrame` sets
  Position and Orientation. A Ref (`Part0`, `Adornee`, `PrimaryPart`) is where the
  instance is: a path down from the model's own root (`"Chassis"`, `"Map/Chassis"`) or
  from the DataModel (`"game.Workspace.Map.Chassis"`), resolved once the whole model is
  built and in place, so a weld may name a part that comes after it in the file. Siblings
  sharing a name are told apart by ordinal (`"Simple Door/Part[3]"`: the third child called
  Part, counted in the tree's order) -- a Roblox door is three parts all named Part, and a
  path by name alone would put every weld on the first of them. An unknown property or a value of the wrong type is reported as a script
  error naming the file and the rest still applies. A meta file that arrives before its
  file waits for it.

  A `*.rbxm` or `*.rbxmx` is a model as Studio saves it (right-click → Save to File; the
  binary `.rbxm` is the default, `.rbxmx` the XML one — `rojo build` writes `.rbxm` too):
  its one root instance is the instance, named after the file like Rojo does (several sit
  in a Folder of that name), with every property this runtime carries read by its
  serialized name and type (`size`, `shape`, `Color3uint8`, `CFrame`s with their rotation
  ids, `token`s, `Font`, `Source`); `Ref`s (`PrimaryPart`, an `ObjectValue`) resolve by
  referent. What the file carries that this runtime does not — internal properties, a
  class it lacks (loaded as a Folder so the rest still does), sequences — is skipped
  quietly; its attributes (Studio's `AttributesSerialize` blob) come through as numbers,
  strings, booleans, Vector3s, Vector2s, Color3s, BrickColors, UDims, UDim2s and Fonts.
  Saving the file rebuilds the tree, deleting it removes it. A binary file's LZ4 chunks are
  read here, and the zstd ones a current Studio writes through Godot's own zstd. The
  demo's `src/map/Shrine.rbxmx` and `src/shared/Fixtures.rbxm` are one each; `test/fixtures/`
  holds rbx-test-files' real ones, which is how the property type ids are known to be
  Studio's (a ParticleEmitter's sequences, a CustomPhysicalProperties with an
  AcousticAbsorption, the six BodyMovers, CFrameValues, UIGradients, UnionOperations).

- **DataStoreService stores on this machine.** `GetDataStore(name, scope)` returns a
  `GlobalDataStore` with `GetAsync` / `SetAsync` / `UpdateAsync` / `IncrementAsync` /
  `RemoveAsync`, server only, any JSON-encodable value. The store is one JSON file
  (`data_store_path`, default `user://datastores.json`), written after a frame that
  changed it and read back when the world starts, so a player's data survives restarts
  the way it survives servers on Roblox. `GetOrderedDataStore(name)` gives an
  `OrderedDataStore` (integers only, its own namespace) whose `GetSortedAsync(ascending,
  pageSize, min, max)` returns `DataStorePages` — `GetCurrentPage()` / `AdvanceToNextPageAsync()`
  / `IsFinished` — for leaderboards. No request budget: the `*Async` calls return at once.
  Every write is a version, as on Roblox: `SetAsync` hands back the version it wrote,
  `GetAsync` / `UpdateAsync` / `RemoveAsync` hand back a `DataStoreKeyInfo` (`Version`,
  `CreatedTime`, `UpdatedTime`) beside the value, `GetVersionAsync(key, version)` reads an
  older one, `ListVersionsAsync(key, sortDirection, minDate, maxDate, pageSize)` lists them as
  `DataStoreObjectVersionInfo`s (a removal is a version marked `IsDeleted`), and
  `RemoveVersionAsync` drops one. `ListKeysAsync(prefix, pageSize)` lists a store's keys as
  `DataStoreKey`s and `DataStoreService:ListDataStoresAsync(prefix, pageSize)` lists the stores
  as `DataStoreInfo`s — all `Pages`, paged the same way. The versions live in the same file as
  the values.
- **Input reaches LocalScripts.** Keys, mouse buttons, the wheel and mouse motion go from
  the engine to the client runtime: `ContextActionService:BindAction(name, handler,
  createTouchButton, ...keys)` / `BindActionAtPriority` see an input first (highest
  priority, then most recent; a handler that returns `Enum.ContextActionResult.Pass` lets
  the next one have it, anything else sinks it), then `UserInputService.InputBegan` /
  `InputChanged` / `InputEnded` fire with an `InputObject` (`KeyCode`, `UserInputType`,
  `UserInputState`, `Position`, `Delta`) and `gameProcessedEvent`; Space also fires
  `JumpRequest`. `IsKeyDown`, `IsMouseButtonPressed`, `GetMouseLocation`, `GetMouseDelta`,
  `GetKeysPressed`, `GetLastInputType`, `UnbindAction`, `GetAllBoundActionInfo` all
  answer. `UserInputService.MouseBehavior` (`LockCenter` / `LockCurrentPosition`) and
  `MouseIconEnabled` reach the engine's mouse mode. The full `Enum.KeyCode` keyboard;
  `Vector2` is a Value type (attributes, tweens, the wire).
- **GUI is drawn, and clicked.** `StarterGui` is copied into the local player's `PlayerGui`
  when they join — and again each time their character respawns: a `ScreenGui` /
  `BillboardGui` with `ResetOnSpawn` (the default) is destroyed, its LocalScripts with it,
  and copied fresh, while one with `ResetOnSpawn = false` stays through deaths — and
  whatever hangs there — from files or `Instance.new` in a LocalScript — is on screen: `ScreenGui` (`Enabled`, `DisplayOrder`), `Frame`, `TextLabel`, `TextButton`,
  `TextBox`, `ImageLabel` / `ImageButton` with `Position` / `Size` as `UDim2`, `AnchorPoint`,
  `BackgroundColor3` / `BackgroundTransparency`, `BorderSizePixel`, `Visible`, `ZIndex`,
  `Rotation`, `ClipsDescendants`; text with `TextColor3`, `TextSize`, `TextScaled`,
  `TextWrapped`, `TextXAlignment` / `TextYAlignment`, `TextStrokeTransparency`,
  `TextTruncate`; `UICorner`, `UIPadding`, `UIStroke` and `UIListLayout` (`FillDirection`,
  `Padding`, `HorizontalAlignment` / `VerticalAlignment`, `SortOrder` + `LayoutOrder`) /
  `UIGridLayout` (`CellSize` / `CellPadding`, `FillDirectionMaxCells`, `StartCorner`;
  `AbsoluteCellCount` / `AbsoluteCellSize`); `UIScale` (about the `AnchorPoint`),
  `UIAspectRatioConstraint` (`AspectType`, `DominantAxis`), `UISizeConstraint` and
  `UITextSizeConstraint` (caps `TextScaled`); a `ScrollingFrame` clips a window onto a
  canvas of `CanvasSize` or `AutomaticCanvasSize`, scrolled by the wheel, its bars
  (`ScrollBarThickness` / `ScrollBarImageColor3`, `VerticalScrollBarPosition`) or a
  write to `CanvasPosition` (clamped and reported back with `AbsoluteCanvasSize` /
  `AbsoluteWindowSize`), along `ScrollingDirection` while `ScrollingEnabled`.
  The engine reports `AbsolutePosition` / `AbsoluteSize` (and a layout's
  `AbsoluteContentSize`) back, and fires `MouseEnter` / `MouseLeave`, `MouseButton1Down` /
  `Up` / `Click`, `InputBegan` / `InputEnded` and `Activated` on the client;
  `AutoButtonColor` darkens a hovered button. A ScreenGui is a `CanvasLayer`, every
  GuiObject a `Panel` (with a `Label` for its text), so the scene tree reads like the
  instance tree. `UDim` / `UDim2` are Value types (attributes, `TweenService`, the wire).
- **TextBoxes take typing.** A `TextBox` is a `LineEdit` drawn over its Panel. A click (or
  `box:CaptureFocus()`) focuses it: `Focused` fires, `UserInputService.TextBoxFocused`
  too, `ClearTextOnFocus` (the default) empties it, `CursorPosition` becomes 1 (it is -1
  unfocused); `PlaceholderText` / `PlaceholderColor3` show while it is empty. Keys typed
  write `Text` and `CursorPosition` (so `Changed` / `GetPropertyChangedSignal("Text")`
  fire) and still reach `UserInputService.InputBegan` as processed — they do not walk the
  character. Enter fires `FocusLost(true)`; Escape, a click elsewhere or
  `box:ReleaseFocus(enterPressed)` fire `FocusLost(false)`, and
  `UserInputService.TextBoxFocusReleased` either way. `TextEditable = false` shows but
  will not take keys; `box:IsFocused()` and `UserInputService:GetFocusedTextBox()` say
  whose turn it is. A script writing `Text` or `CursorPosition` moves the box and the
  caret. `MultiLine = true` makes it a `TextEdit` instead: Enter puts in a newline rather
  than letting the focus go, `Text` carries the lines with `\n` between them (and
  `TextWrapped` wraps them), and `CursorPosition` still counts characters over the whole
  text, from 1.
- **ZIndex stacks them.** A `GuiObject`'s `ZIndex` orders it among its siblings, its
  children over it — Roblox's `Sibling` default, which is Godot's relative z. A `ScreenGui` /
  `BillboardGui` / `SurfaceGui` set to `ZIndexBehavior.Global` weighs every `ZIndex` on it
  against every other, whatever the tree says.
- **Images.** An `ImageLabel` / `ImageButton`'s `Image` is a file in the project like a
  `SoundId`: `images/logo.png` (`.jpg`, `.webp`, `.bmp`, `.tga`, `.svg`, or an imported
  `res://` texture) under `asset_root`; `rbxassetid://` is warned about. It is a
  `TextureRect` over the Panel: `ImageColor3` / `ImageTransparency` tint it, `ScaleType`
  `Stretch` / `Fit` / `Crop` / `Tile` fit it (`Slice` stretches), `ResampleMode.Pixelated`
  keeps pixels crisp, `ImageRectOffset` / `ImageRectSize` cut a sprite out of a sheet, and a
  button shows its `HoverImage` / `PressedImage` in those states. The engine writes
  `IsLoaded` and `ContentImageSize` back once it has read the file. `Player:GetMouse().Icon`
  is an image the same way and becomes the pointer (centred on it); `""` is the arrow again.
- **Meshes.** A `MeshPart`'s `MeshId` is a mesh file in the project the same way:
  `meshes/rock.obj`, `.glb` / `.gltf` (a scene's meshes flattened in place), or an imported
  `res://` mesh or scene, under `asset_root`. As on Roblox the file is fitted to `Size`
  (its bounds scaled to it, centred on the part) and the engine writes its own bounds back
  as `MeshSize`; `TextureID` is its image. `CollisionFidelity` `Default` / `Hull` collide
  with the mesh's convex hull, `Box` with the part's box, `PreciseConvexDecomposition` with
  the exact triangles on an anchored part (a moving body gets the hull). A `SpecialMesh`
  (`BlockMesh` / `CylinderMesh`) in any part draws instead of its `Shape`: `MeshType`
  `FileMesh` with a `MeshId` in the part's own units times `Scale`, or `Sphere` / `Head` /
  `Cylinder` / `Wedge` / `Brick` the part's `Size` times `Scale`, moved by `Offset`, its
  `TextureId` the image; collision stays the part's own, as on Roblox.
- **SurfaceGui.** A `SurfaceGui` in a part (or with an `Adornee`) paints its GuiObjects
  onto one `Face` of it: a `CanvasSize`-pixel canvas (or `PixelsPerStud` of them, with
  `SizingMode`) stretched over the face, a hair off it plus `ZOffset`, hidden past
  `MaxDistance` (0, Roblox's default and what every saved place says: no limit) or when
  not `Enabled`. Its buttons work as a ScreenGui's do — the engine
  routes the pointer over the face into the canvas, so `Activated` / `MouseEnter` /
  `MouseButton1Click` fire on the client that clicked, and `TextLabel` changes show on the
  part. `LightInfluence` 0 (the default) draws it unlit, `Brightness` scales it,
  `AlwaysOnTop` draws it over everything. In `Workspace` it is everyone's; in the
  `PlayerGui` with an `Adornee` it is the one player's, as on Roblox.
- **Pathfinding.** `PathfindingService:CreatePath({ AgentRadius, AgentHeight, AgentCanJump,
  WaypointSpacing, Costs })` gives a `Path`: `ComputeAsync(start, finish)` (points or
  parts) sets `Status` (`Enum.PathStatus`) and `GetWaypoints()` returns `{ Position,
  Action, Label }` every `WaypointSpacing` studs, `Action` `Jump` where a Humanoid must;
  `CheckOccupancyAsync(point)`, `Blocked` / `Unblocked` with the waypoint index as parts
  move onto / off the way, and the older `FindPathAsync`. Roblox bakes a navmesh; this
  walks a 2-stud grid over the tree on the fly with the same queries `workspace:Raycast`
  uses — the agent's box must stand, a step higher than a Humanoid walks up (under 2.5
  studs, which the character does step up when its walk is stopped short: a kerb, a
  stair, a toolbox chair's seat -- that is how walking into one seats you) is a jump, a
  drop too far is no move. `Costs` are per `Material` name or `PathfindingModifier`
  `Label` (`math.huge`: never); a `PassThrough` modifier makes its part air; characters
  are never in the way. The demo's `Patrol` guard walks between two posts round the lava,
  waypoint by waypoint, with `Humanoid:MoveTo` / `MoveToFinished`.
- **Fonts.** `TextLabel` / `TextButton` / `TextBox` carry both `Font` (`Enum.Font`) and
  `FontFace` (a `Font`: `Family`, `Weight`, `Style`, `Bold`), coupled the way Roblox couples
  them — set `Font = Enum.Font.GothamBold` and `FontFace` is `GothamSSm` at `Bold`, set a
  `FontFace` and `Font` is the enum that names it or `Unknown`. `Font.new(family, weight,
  style)`, `Font.fromEnum`, `Font.fromName`, `Font.fromId`, `Enum.FontWeight` (`Thin`…
  `Heavy`, 100–900) and `Enum.FontStyle`; Rojo's `{ family, weight, style }` in a
  `model.json`. The engine draws each face as a system font: the family mapped to the
  desktop names that carry it (`SourceSansPro` → Source Sans, `GothamSSm` → Gotham /
  Montserrat, `Inconsolata` → Consolas…) with the weight and italic asked for, falling
  back to the platform's sans.
- **Raycasts and the mouse.** `workspace:Raycast(origin, direction, params)` walks the tree
  itself — blocks as oriented boxes, balls as spheres, `CanQuery` respected — and returns
  the `RaycastResult` shape (`Instance`, `Position`, `Normal`, `Distance`, `Material`) or
  nil; `RaycastParams.new()` with `FilterDescendantsInstances`, `FilterType`
  (`Enum.RaycastFilterType.Exclude` / `Include`), `RespectCanCollide`, `:AddToFilter()`;
  `Ray.new` with `Unit`, `:ClosestPoint`, `:Distance`, and the classic
  `FindPartOnRay` / `FindPartOnRayWithIgnoreList`. The camera does the pixel math:
  `Camera.ViewportSize` (the engine writes it), `ScreenPointToRay` / `ViewportPointToRay`,
  `WorldToScreenPoint` / `WorldToViewportPoint` (position + depth, `inView`).
  `player:GetMouse()` is the classic `Mouse`: `X` / `Y` / `ViewSizeX`, `Target`, `Hit`,
  `UnitRay`, `Origin`, `TargetFilter`, `Move`, `Button1Down` / `Up`, `Button2Down` / `Up`,
  `WheelForward` / `Backward`. A `ClickDetector` in a part or a Model fires
  `MouseHoverEnter` / `MouseHoverLeave` and `MouseClick(player)` / `RightMouseClick`
  within `MaxActivationDistance` — on the clicking client and on the server, where a Script
  hears it with the `Player`. Clicks the GUI took first do not reach the world.
- **Spatial queries.** `workspace:GetPartBoundsInBox(cframe, size, params)`,
  `GetPartBoundsInRadius(position, radius, params)` — the parts whose world bounds reach
  the box or sphere — and `GetPartsInPart(part, params)` / `part:GetTouchingParts()` — the
  parts whose shapes actually overlap — return arrays of parts, `CanQuery` respected, the
  queried part not counting itself. `OverlapParams.new()` carries
  `FilterDescendantsInstances`, `FilterType`, `RespectCanCollide`, `MaxParts` (20; 0 is
  all) and `BruteForceAllSlow`, and is not a `RaycastParams` (nor the other way round).
  `Blockcast(cframe, size, direction, params)`, `Spherecast(position, radius, direction,
  params)` and `Shapecast(part, direction, params)` sweep an oriented box, a sphere or the
  part's own shape and return the same `RaycastResult` a `Raycast` does — the contact
  point and normal on the part hit, `Distance` how far the shape moved — or nil; a shape
  that starts inside a part misses it, as a ray that starts inside does.
- **ProximityPrompts.** A `ProximityPrompt` in a part (or a Model — its `PrimaryPart`) is
  shown when the character is within `MaxActivationDistance` and, with
  `RequiresLineOfSight`, its Head can see the part: `PromptShown(inputType)` /
  `PromptHidden` on the prompt and on `ProximityPromptService`, and the engine draws
  `ObjectText` over `[key]  ActionText` on the part (`Style = Custom` draws nothing — a
  LocalScript's own GUI presses it with `InputHoldBegin` / `InputHoldEnd`). Nearest first,
  `Exclusivity` (`OnePerButton`, `OneGlobally`, `AlwaysShow`) and the service's
  `MaxPromptsVisible` decide which. `KeyboardKeyCode` (E) fires `Triggered(player)` on the
  client at once and on the server after its own range check, `TriggerEnded` when the key
  comes up; a `HoldDuration` waits with `PromptButtonHoldBegan` / `PromptButtonHoldEnded`
  first. `Enabled` on the prompt or the service turns them off.
- **Sounds.** A `Sound`'s `SoundId` is a file in the project: `sounds/thud.ogg` (or `.wav` /
  `.mp3`) under the world's `asset_root` (`res://` by default), `rbxasset://` the same;
  `rbxassetid://` (or a `roblox.com/asset` URL, or `rbxthumb://`) is a cloud asset: the
  first three of a kind are named in the Output and the rest counted, once a frame, so a
  place full of them does not bury a real error; a missing `rbxasset://` file is named as
  one of Roblox's own built-ins, not shipped here. `Volume`, `PlaybackSpeed`, `Looped`,
  `Playing`, `TimePosition`, `PlayOnRemove`, `RollOffMinDistance` / `RollOffMaxDistance` /
  `RollOffMode`; `Play` / `Stop` / `Pause` / `Resume` fire `Played` / `Stopped` / `Paused` /
  `Resumed`, `Ended` at the end, `DidLoop(soundId, n)` each time round, and `Loaded` when the
  engine has the file and fills in `TimeLength` / `IsLoaded` / `IsPlaying`. Both sides run
  the clock, so `Ended` fires in a server Script too; a Play replicates like any property, a
  LocalScript's Play is heard on its client only, and `SoundService:PlayLocalSound(sound)`
  plays one that is not even in the tree. In a part the sound is 3D from that part, in an
  `Attachment` from where the attachment sits on it; anywhere else it is everywhere. A
  `SoundGroup` (its own `Volume`, and groups nest through their own `SoundGroup`) turns
  every `Sound` that names it up or down together.
- **BillboardGuis.** A `BillboardGui` in a part, or anywhere in Workspace / the PlayerGui
  with an `Adornee` (a part, or a Model's first part), hangs over it facing the camera with
  Frames / TextLabels / buttons inside, laid out as in a ScreenGui. `Size`'s scale is studs
  and its offset pixels, so `UDim2.new(4, 0, 1, 0)` shrinks with distance and
  `UDim2.fromOffset(200, 50)` does not; `StudsOffset` / `ExtentsOffset` move it along the
  camera's axes (`...WorldSpace` along the world's), `SizeOffset` in its own size,
  `MaxDistance` hides it far away and `AlwaysOnTop` draws it through walls (otherwise a part
  in the way hides it). `DistanceLowerLimit` / `DistanceUpperLimit` / `DistanceStep` clamp
  and step the distance it is sized for. The engine writes `AbsolutePosition` /
  `AbsoluteSize` each frame. A server's BillboardGui replicates with its part; one in
  StarterGui copies into each PlayerGui.
- **Tools and the Backpack.** A `Tool` is a Model with a `Handle`. `StarterPack` (and the
  player's `StarterGear`) fill the `Backpack` on every spawn, on the server, and the tools
  replicate down; a `Script` in a tool runs there, a `LocalScript` on its owner's client.
  The hotbar keys `1`–`9` equip (the held tool's key unequips), `Backspace` drops
  (`CanBeDropped`), a click fires `Activated` / `Deactivated` — on the client at once, on
  the server a frame later. Moving a tool into a character is equipping it: `Equipped(mouse)`
  / `Unequipped` fire on both sides, one tool in hand at a time, and `Humanoid:EquipTool` /
  `UnequipTools`, `tool.Parent = character` and `Tool:Activate()` all work from a Script.
  The engine hangs the Handle off the right arm, held out in front, the tool's other parts
  keeping their offsets from it; the arm's pose carries them. `Tool.Grip` moves it in the
  hand, as Roblox's RightGrip weld does (`Handle.CFrame = RightArm.CFrame * C0 * Grip⁻¹`):
  a CFrame made of `GripPos`, `GripForward`, `GripRight` and `GripUp` — set either side and
  the other follows, in a script or in `init.meta.json` (`"GripPos": [0, 0, -1]` holds the
  Handle a stud from its back end; `GripForward` / `GripUp` of `[0, 1, 0]` / `[0, 0, 1]`
  point a Handle built along -Z forward from the hand). The engine draws the hotbar
  along the bottom, as Roblox's CoreGui does — a slot per tool, numbered, the held one lit,
  a click equips — and `StarterGui:SetCoreGuiEnabled(Enum.CoreGuiType.Backpack, false)`
  (`GetCoreGuiEnabled`) hides it, so a game's own inventory GUI can take over.
- **Accessories are worn.** An `Accessory` (an `Accoutrement`, with an `AccessoryType`) is a
  `Handle` part carrying an `Attachment`; parented into a character — `Humanoid:AddAccessory`
  does that — its `Handle` sits where the limb's `Attachment` of the same name is, and rides
  there, the accessory's other parts keeping their offsets from it. An R6 rig wears Roblox's
  own points: `HatAttachment` / `HairAttachment` / `FaceFrontAttachment` /
  `FaceCenterAttachment` / `NeckAttachment` on the Head, `NeckAttachment` /
  `BodyFrontAttachment` / `BodyBackAttachment` / the collars and the waist on the Torso,
  `LeftShoulderAttachment` / `LeftGripAttachment` (and the right) on the arms,
  `LeftFootAttachment` / `RightFootAttachment` on the legs. A hat on the Head sits still; a
  bracelet on an arm swings with it. `Humanoid:GetAccessories` lists them,
  `Humanoid:RemoveAccessories` takes them all off.
- **Animations play from KeyframeSequences.** An `Animation`'s `AnimationId` names a
  `KeyframeSequence` by its path in the tree (`"ReplicatedStorage.animations.Wave"`) — where a
  `.rbxmx` / `.rbxm` saved out of Studio's animation editor lands, PulseBlockz's stand-in for an
  asset id. `humanoid.Animator:LoadAnimation(anim)` (or `humanoid:LoadAnimation`) returns an
  `AnimationTrack`: `Play(fadeTime, weight, speed)`, `Stop`, `AdjustSpeed`, `AdjustWeight`,
  `GetTimeOfKeyframe`, `IsPlaying` / `Length` / `Looped` / `Speed` / `TimePosition` /
  `Priority`, and `KeyframeReached` / `DidLoop` / `Stopped` / `Ended` as the engine's clock runs
  it — at `Speed`, looping or ending at `Length`, `TimePosition` reported back.
  `Animator:GetPlayingAnimationTracks` lists what is playing. The engine poses the R6 limbs
  from the `Keyframe`s either side of the clock, a `Pose` turning the limb it names about the
  joint it hangs from; the default walk/idle animation stands aside while a track plays.
  Tracks layer as they do in Studio: the highest `Priority` posing a limb takes it (an `Action`
  wave over the `Movement` shrug underneath), tracks of the same priority mix by weight, and
  `Play(fadeTime)` / `Stop(fadeTime)` / `AdjustWeight(weight, fadeTime)` fade `WeightCurrent`
  toward `WeightTarget` over that time — a faded `Stop` ends the track when it reaches zero.
- **Names over heads.** The engine draws every other character's `Humanoid.DisplayName` a
  little over its Head, the same size at any distance, with a health bar under it while
  it is hurt — Roblox's name tag. `Humanoid.NameDisplayDistance` / `HealthDisplayDistance`
  (a new character's copy `StarterPlayer`'s) say how far they show, `DisplayDistanceType`
  whose distances count (`Viewer`, the default; `Subject`; `None` hides the name),
  `HealthDisplayType` when the bar shows (`DisplayWhenDamaged`, `AlwaysOn`, `AlwaysOff`)
  and `NameOcclusion = OccludeAll` (the default) hides a tag behind a wall —
  `EnemyOcclusion` only the tags of players not on your Team. Your own name is never shown
  to you. NPCs — any Model with a Humanoid — get one too, so
  `hum.DisplayName = "Guard"` labels a guard. The tags live in the CoreGui layer
  (`CoreGui/PlayerNames/<character name>`, next to `CoreGui/Backpack`), not in any
  PlayerGui, as on Roblox.
- **The player list and your health.** The engine draws Roblox's player list at the top
  right — every Player's `DisplayName` with its `TeamColor`, grouped under the `Teams`'
  names in their colours when there are Teams (Neutral players last), your own row lit —
  and your own Humanoid's health as a bar above it while you are hurt. **leaderstats**
  work the Roblox way: a Folder named `leaderstats` under the Player, its
  `IntValue` / `NumberValue` / `StringValue` / `BoolValue` children become the columns
  (four at most, in the order they were made; `12345` shows as `12.3K+`) and every
  change of a `Value` updates the row on every client.
  `StarterGui:SetCoreGuiEnabled(Enum.CoreGuiType.PlayerList | Health, false)` hides each.
  They live at `CoreGui/PlayerList` and `CoreGui/Health`.
- **The top bar and the escape menu.** The top bar (a menu button and a chat toggle)
  sits at the top left. Escape, or the menu button, opens the menu: the player list,
  Resume, Reset Character and Leave — the last two ask first. While it is up the
  character and camera let go of the keys and mouse and every input reaches scripts
  with `gameProcessed = true`; `GuiService.MenuIsOpen` says so and `MenuOpened` /
  `MenuClosed` fire on the client. Reset Character asks the server, which sets the
  Humanoid's Health to 0 and respawns as usual; Leave disconnects from the server and
  the world emits `leave_game` (the demo's `Main.gd` quits on it).
- **Chat.** Press `/` (or click the box at the top left), type, Enter. The server fires
  `Player.Chatted(message, recipient)` — filter, log, run commands there — and every
  client gets the line in its window
  (`CoreGui/Chat/Window/Box/Messages`) and a bubble over the speaker's head for 15 s
  (`CoreGui/BubbleChat/<name>`, off with `Chat.BubbleChatEnabled = false`). Messages are
  trimmed and cut at 200 characters; the keys are the box's while you type, so W does not
  walk. `Chat:Chat(part | character, text, Enum.ChatColor | Color3)` puts a bubble over a
  part (a character's Head) — on every client from a Script, on this one from a
  LocalScript. `StarterGui:SetCore("ChatMakeSystemMessage", {Text = ..., Color = ...})`
  writes a system line. `SetCore("ResetButtonCallback", bindableEvent | false)` takes the
  menu's Reset Character over (or disables it); `SetCore("TopbarEnabled", bool)` and
  `SetCore("ChatActive", bool)` show or hide the top bar and the chat window, and
  `GetCore` reads those two back. The other `SetCore` names are accepted and ignored.
  `Chat:FilterStringAsync` / `FilterStringForBroadcast` return the text as it is (no
  filter here). `Enum.CoreGuiType.Chat` hides the window.

  **TextChatService** (the current API, `TextChatService.ChatVersion` defaults to it) is
  what the box actually goes through. The server makes `TextChatService.TextChannels`
  with `RBXGeneral` and `RBXSystem` and a `TextSource` (its `UserId`, `CanSend`) per
  player in each, plus the `TextChatCommands` folder. A LocalScript sends with
  `channel:SendAsync(text, metadata)` and gets the `TextChatMessage` back (`Status`
  `Sending`, then `Success` once the server's copy returns with its `MessageId`);
  `TextChatService.SendingMessage` fires first. On the server the message fires
  `TextChannel.MessageReceived`, `TextChatService.MessageReceived` and `Player.Chatted`,
  then `TextChannel.ShouldDeliverCallback(message, textSource)` decides per recipient
  (return `false` to keep it from one). Each client runs `TextChannel.OnIncomingMessage`
  and then `TextChatService.OnIncomingMessage` — return a `TextChatMessageProperties`
  with `PrefixText` / `Text` to change what the window draws (the default prefix is the
  DisplayName in its classic chat colour, as rich text: `<font color="#hex">Name</font>: `)
  — fires `MessageReceived` on both and `Player.Chatted`, and draws `PrefixText .. Text`
  (`<font color/size>`, `<b>`, `<i>`, `<u>`, `<s>`, `<br/>` and the entities; what a
  player typed is shown as typed). `channel:DisplaySystemMessage(text, metadata)` is a
  message with no `TextSource` on that client only. A `TextChatCommand` under
  `TextChatCommands` with `PrimaryAlias` / `SecondaryAlias` (`/hello`) fires
  `Triggered(textSource, text)` on the sender and the server instead of a message. The
  server makes more channels with `Instance.new("TextChannel")` under `TextChannels` and
  `channel:AddUserAsync(userId)` → `TextSource, true`; a message reaches that channel's
  TextSources only (`SendAsync` elsewhere is `InvalidTextChannelPermissions`).
  `TextChatService:DisplayBubble(part | character, text)` is a client-side bubble;
  `CanUserChatAsync` / `CanUsersChatAsync` say yes. `ChatVersion = LegacyChatService`
  sends the box's lines the old way (`Player.Chatted` only, no `TextChatMessage`).
  Not here: `TextChatService.ChatWindowConfiguration` / `BubbleChatConfiguration` /
  `ChatInputBarConfiguration` (the window is the engine's), `OnBubbleAdded` /
  `OnChatWindowAdded` (settable, never called), `Translation`, `MessageTooLong` /
  `Floodchecked` statuses, the default commands (`/whisper`, `/mute`, `/e`).
- **BrickColor.** Roblox's palette: `BrickColor.new("Bright red")`, `BrickColor.new(21)`,
  `BrickColor.new(0, 0, 1)` or `BrickColor.new(someColor3)` (the nearest entry), with
  `.Name`, `.Number`, `.Color` (a Color3), `.r/.g/.b`, and `BrickColor.Red()`, `.Blue()`,
  `.Gray()`, `.palette(n)`, `.random()`. An unknown name is `Medium stone grey`, as on
  Roblox. `Part.BrickColor` is `Part.Color` seen on the palette — set either, read either;
  `GetPropertyChangedSignal("BrickColor")` is `Color`'s. A `BrickColor`, its name or a
  Color3 all assign. (The classic LEGO numbers and the 1001–1032 set are exact; the rest of
  the 300-series is close, not verbatim.)
- **Teams.** `Team`s under the `Teams` service with `TeamColor` (a BrickColor) and
  `AutoAssignable`; a new player lands on the AutoAssignable team with the fewest
  players. `Player.Team` and `Player.TeamColor` are one choice, coupled as on Roblox:
  setting `Team` sets `TeamColor` and `Neutral = false`; `Team = nil` makes the player
  Neutral; setting `TeamColor` finds the Team wearing that colour (or none). Each `Team`
  fires `PlayerAdded` / `PlayerRemoved` and has `GetPlayers()`; `Teams:GetTeams()` lists
  them. A `SpawnLocation` has `TeamColor`, `Neutral` (the default) and
  `AllowTeamChangeOnTouch`: a player spawns at `RespawnLocation` if that is an enabled
  SpawnLocation, else takes turns among the enabled ones that are Neutral or match its
  team's colour; a Neutral player only uses Neutral ones. Touching an enabled pad with
  `AllowTeamChangeOnTouch` puts you on its colour's team.
- **Lighting is the scene's light.** `Lighting.ClockTime` / `TimeOfDay` (one value in two
  forms) turn a sun that rises in the east at 6, is overhead at 12 and sets at 18, dimming
  the sky and the ambient with it; `Brightness`, `GlobalShadows`, `Ambient` / `OutdoorAmbient`,
  `FogStart` / `FogEnd` / `FogColor` and `ExposureCompensation` reach the sun, the
  environment and its fog. A Script's Lighting writes replicate; a LocalScript's show on
  its own screen.
- **Lighting's children are the look of the place.** A `BloomEffect` (`Intensity`, `Size`,
  `Threshold`) is the environment's glow, a `ColorCorrectionEffect` (`Brightness`,
  `Contrast`, `Saturation`, `TintColor`) its adjustments, a `DepthOfFieldEffect`
  (`FocusDistance` ± `InFocusRadius` sharp, `NearIntensity` / `FarIntensity` the blur
  beyond) the camera's depth of field, a `BlurEffect` (`Size` in pixels) the whole screen
  softened under the GUIs; each obeys `Enabled` and counts under `Lighting` or the
  `CurrentCamera`, the first of a kind winning as in Studio. An `Atmosphere` (`Density`,
  `Color`, `Haze`, `Glare`) replaces `FogStart` / `FogEnd` with a haze that thickens with
  distance and tints the sky, and a `Sky`'s `SunAngularSize` / `MoonAngularSize` /
  `CelestialBodiesShown` size the sun's and the moon's disks. A `SunRaysEffect` is shafts of
  light through the air, drawn with volumetric fog (`Intensity` how much light is in the
  air, `Spread` its scatter); a `Sky`'s six `Skybox*` faces become a cubemap (all six or
  none), its `SunTextureId` / `MoonTextureId` the discs that follow `ClockTime`, and
  `StarCount` the share of sky cells that get a star -- a sky shader takes over from the
  procedural one only for what that one cannot do. An `Atmosphere`'s `Decay` / `Offset`
  are accepted and not drawn.
- **Lights in parts.** A `PointLight` (`Brightness`, `Color`, `Range`, `Enabled`, `Shadows`)
  or a `SpotLight` / `SurfaceLight` (plus `Angle`, the whole cone, and `Face`, the side of
  the part it shines out of) parented to a part is a light in the scene that rides the part
  — a lamp head, a falling brick, the tip of a held tool — and goes with it.
- **Joints hold parts together.** A `Weld` / `Snap` / `ManualWeld` (`Part0`, `Part1`, `C0`,
  `C1`: `Part1.CFrame = Part0.CFrame * C0 * C1:Inverse()`), a `Motor6D` (plus
  `DesiredAngle` / `MaxVelocity` / `CurrentAngle`, the angle chasing its target about
  `C0`'s Z each frame) or a `WeldConstraint` (holds the parts where they were when it took
  hold) makes the parts one assembly: it moves as one — through any of its parts'
  `CFrame`, a tween of `C0`, or the physics — and falls as one when nothing in it is
  anchored, with every part still colliding. `C0` / `C1` are CFrames like `CFrame` is
  (`GetPropertyChangedSignal("C0")`, `<CoordinateFrame name="C0">` in an rbxmx, a tween's
  target); an `Attachment` in a part has `Position` / `Orientation` in the part and
  `WorldPosition` / `WorldCFrame` / `WorldAxis` through it.
- **Decals and Textures.** A `Decal` (`Texture`, `Face`, `Transparency`, `Color3`) in a part
  is its image on that face, fitted to it — a sign, a `face` in a character's Head — and a
  `Texture` tiles it every `StudsPerTileU` x `StudsPerTileV` studs from `OffsetStudsU` /
  `OffsetStudsV`. Both ride the part like a light does.
- **Particles.** A `ParticleEmitter` in a part, or in an `Attachment` in one, emits from the
  part's volume (`Shape` Box, Sphere, Cylinder or Disc; `ShapeStyle` throughout it or off its
  skin, `ShapeInOut` the way the particles leave) toward `EmissionDirection` within
  `SpreadAngle`, `Rate` a second, each particle living `Lifetime` seconds at `Speed` plus
  `VelocityInheritance` of the part's, pulled by `Acceleration`, slowed
  by `Drag`, spinning at `Rotation` / `RotSpeed`, drawn `Size` big in `Color` at `Transparency`
  along its life (`NumberRange`, `NumberSequence` + `NumberSequenceKeypoint`, `ColorSequence` +
  `ColorSequenceKeypoint`, with Studio's keypoint rules), `Squash`ed along its velocity (which
  costs it the billboard), `Texture` a file from the project (the default a soft dot) or a
  flipbook of it (`FlipbookLayout` / `FlipbookMode` / `FlipbookFramerate` / `FlipbookStartRandom`),
  `LightEmission` blending additively, `Brightness`, `TimeScale`,
  `LockedToPart`, `Enabled`; `Emit(n)` bursts and `Clear()` clears. `Fire`, `Smoke` and
  `Sparkles` are the fixed looks (`Color` / `SecondaryColor` / `Heat` / `Size`, `Opacity` /
  `RiseVelocity`, `SparkleColor`). Rojo's `model.json` spellings, `.rbxmx` and `.rbxm` load them;
  attributes hold the datatypes.
- **Explosions.** An `Explosion` parented into the Workspace detonates once at its `Position`:
  `Hit(part, distance)` for every part within `BlastRadius`, the joints within
  `DestroyJointRadiusPercent` of it break (a character there dies), the engine flings the loose
  bodies by `BlastPressure` and draws a burst (`Visible`); it removes itself once played.
- **Highlights.** A `Highlight` tints and outlines its `Adornee` (its parent when nil — a
  Model's parts, a part, a character) in `FillColor` / `FillTransparency` and `OutlineColor` /
  `OutlineTransparency`, through walls unless `DepthMode` is `Occluded`; `Enabled` takes it off.
- **Seats.** A character's limb touching a `Seat` sits it: a `SeatWeld` from the seat to the
  `HumanoidRootPart` (the torso's bottom on the seat's top, the engine pins it there, legs out),
  `Humanoid.Sit` / `SeatPart`, `Seated(true, seat)`, the seat's `Occupant`. A jump hops it off,
  `Sit = false` or dying stands it up, destroying the seat or the weld too; a moment's grace
  before it sits again. `Disabled` seats nobody, `Seat:Sit(humanoid)` sits one now. A
  `VehicleSeat` reads its occupant's controls into `Throttle` / `Steer` (and the floats), forward
  along the seat, and drives: its assembly is pushed along the seat's look vector (kept flat)
  toward `Throttle * MaxSpeed` and turned at `Steer * TurnSpeed`, each reached at `Torque`
  studs/s per second, with no wheel in sight, as Roblox's does. A script may set
  `Throttle` / `Steer` itself. Every part has Plastic's friction (0.3) so a push isn't
  swallowed by the ground.
- **Constraints turn parts on each other.** A `HingeConstraint` between two `Attachment`s
  (`Attachment0` / `Attachment1`, `Enabled`) lets their parts turn about `Attachment0`'s axis
  through the engine's physics: `ActuatorType` `Motor` spins it at `AngularVelocity` under
  `MotorMaxTorque`, `Servo` turns it to `TargetAngle` at `AngularSpeed` under `ServoMaxTorque`,
  `LimitsEnabled` bounds it between `LowerAngle` and `UpperAngle` (a motor stalls there), and the
  engine reports `CurrentAngle` (between the attachments' secondary axes, in degrees). A
  `BallSocketConstraint` pins the attachments together, the parts free to swing — within a cone
  of `UpperAngle` and a twist of `TwistLowerAngle`..`TwistUpperAngle` when `LimitsEnabled` /
  `TwistLimitsEnabled`. Either joins whole assemblies (a welded door on a hinge), and does
  nothing between two anchored parts.
- **A prismatic slides, a cylinder slides and turns.** A `PrismaticConstraint` holds its
  attachments on `Attachment0`'s axis, free to slide along it: `ActuatorType` `Motor` pushes at
  `Velocity` under `MotorMaxForce`, `Servo` slides to `TargetPosition` at `Speed` under
  `ServoMaxForce`, `LimitsEnabled` bounds it between `LowerLimit` and `UpperLimit`, and the
  engine reports `CurrentPosition` (how far Attachment1 sits along the axis, in studs). A
  `CylindricalConstraint` is that plus the hinge's turn about the same axis, with its own
  `AngularActuatorType` / `AngularVelocity` / `MotorMaxTorque` / `AngularSpeed` /
  `ServoMaxTorque` / `TargetAngle` / `AngularLimitsEnabled` / `LowerAngle` / `UpperAngle` and
  `CurrentAngle`.
- **Ropes, rods and springs hold a distance.** A `RopeConstraint` keeps its attachments no
  farther apart than `Length` (slack below it; `Restitution` bounces the snap; `WinchEnabled`
  reels `Length` toward `WinchTarget` at `WinchSpeed`), a `RodConstraint` keeps them exactly
  `Length` apart, a `SpringConstraint` pushes and pulls them toward `FreeLength` with
  `Stiffness` and `Damping` (capped by `MaxForce`; `LimitsEnabled` clamps it between `MinLength`
  and `MaxLength`). The engine holds them with impulses at the attachment points each physics
  step — an anchored side is immovable, gravity and a swing's pull anticipated so a hung load
  sits at `Length`, not below it — and reports `CurrentDistance` / `CurrentLength`.
- **The movers push a part.** Each acts on `Attachment0`'s assembly every physics step: a
  `VectorForce` (`Force`, in `Attachment0`'s / `Attachment1`'s / the world's frame per
  `RelativeTo`, at the attachment or `ApplyAtCenterOfMass`) and a `Torque`; a `LinearVelocity`
  (`VectorVelocity`, or a `Line` / `Plane` of it) and an `AngularVelocity` that reach their
  velocity under `MaxForce` / `MaxTorque`; an `AlignPosition` / `AlignOrientation` that pull toward
  `Attachment1` (`TwoAttachment`) or a `Position` / `CFrame` (`OneAttachment`) as a critically
  damped spring of rate `Responsiveness` under `MaxForce` / `MaxTorque` and `MaxVelocity` /
  `MaxAngularVelocity`, or straight there with `RigidityEnabled` (`PrimaryAxisOnly` aligns just
  the X axis). A loose body weighs what `GetMass()` says — `Size`'s volume at its material's
  density, an assembly the sum — so `part:GetMass() * workspace.Gravity` is the force that holds
  it up, as in Studio, and a hinge's `MotorMaxTorque` means what it does there.
- **A material weighs and rubs.** Every `Enum.Material` carries Roblox's density, friction and
  elasticity: Plastic 0.7 / 0.3 / 0.5, Wood 0.35, Metal 7.85, Ice's 0.02 friction, Concrete's
  0.7. `GetMass()` and the body's mass follow the density (a `Massless` part adds none), and the
  body rubs at the material's friction — a crate slides across Ice and stops on Plastic.
  `CustomPhysicalProperties = PhysicalProperties.new(density, friction, elasticity)` (or
  `PhysicalProperties.new(Enum.Material.Ice)`) overrides both; `nil` hands the part back to its
  material.

What is *not* there yet: a `Pose`'s `Weight` / `EasingStyle` / `EasingDirection` / `MaskWeight`
(keyframes interpolate linearly, every Pose at full weight),
an R15 rig's own animation set (it swings the way R6 does), `Humanoid.BodyDepthScale` and the
other scales, an `Accessory`'s legacy `AttachmentPoint` (accepted, not applied; the older
`AttachmentPos` / `AttachmentForward` / `AttachmentRight` / `AttachmentUp` quartet does hang a
Handle from the Head), an accessory's own `Handle`
collision (it is drawn, not felt), a slider's `Restitution` and a cylinder's `AngularRestitution` /
`InclinationAngle` (accepted, not applied), a material's elasticity (nothing bounces, and
Roblox's friction / elasticity weights are not modelled), a hinge's or ball socket's `Restitution`
(accepted, not applied), a `TorsionSpringConstraint` / `UniversalConstraint` (declared, not
simulated), a rope / rod / spring drawn (no `Thickness` / `Coils`), the parts a constraint joins still
colliding with each other (the engine excludes the pair), `Visible` constraints and attachments (nothing is drawn), a constraint on a network client (the server owns the physics), Roblox's 31-`Highlight` cap (the rest are drawn), an `Explosion`'s `ExplosionType` (nothing craters) and
`TimeScale`, a fling of the characters in one (they die instead), a `ParticleEmitter`'s `ZOffset`, `Orientation` and
`LightInfluence` (accepted, not applied),
a `Decal`'s `ZIndex` and one on a `MeshPart` / `SpecialMesh` face
(placed as on the part's box), a joint on a network client (the server tells it where the parts
are), the `Attachment`-based
`PathfindingLink`, `AgentCanClimb` and `ClosestNoPath` / `ClosestOutOfRange` (accepted, not applied: a path is `Success`, `NoPath`, `FailStartNotEmpty` or `FailFinishNotEmpty`), an
`Atmosphere`'s `Decay` / `Offset` (accepted, not drawn), raycasts see a `MeshPart` as its box, a `BevelMesh`'s
`Bevel` / `Bulge` / `Roundness` (accepted, not applied),
`MeshType` `Prism` / `Pyramid` / the ramps / `CornerWedge` (drawn as a brick),
the chat's `ChatWindowConfiguration` / `BubbleChatConfiguration` (recorded, not acted on: the engine draws its own window) / the default `/whisper`-style commands (`TextChatCommands` starts empty), `Team.ChildOrder`, `BillboardGui.LightInfluence` /
`Brightness` (accepted, not applied), `SurfaceGui.Active` (accepted, not applied), `ProximityPrompt`
gamepad and touch prompts, `AmbientReverb` / `DopplerScale`,
`PlaybackLoudness` for anything but a `.wav`, `.wav` files other than 8/16-bit PCM, an `.rbxm` / `.rbxmx`'s `NumberSequence` / `ColorSequence` / `NumberRange` / `Rect` / `CFrame` / `EnumItem` attributes (stepped over in the `AttributesSerialize` blob; a type it does not know ends the read, since only the type says how wide the value is), `ScaleType.Slice`
/ `SliceCenter` and `TileSize` (accepted, not applied), a `MultiLine` box's `TextXAlignment`
(the lines run from the left), `ScrollingFrame.ElasticBehavior` (accepted, not applied), `Font.fromId` faces
(accepted, drawn with the fallback face), Roblox's 30-day `DataStore` version window (they are kept), nothing
behind the `MarketplaceService` stub.

## Budgets — what the sandbox guarantees

The world node's defaults are Roblox's: a script runs until it yields, and one that has
not yielded for ten seconds is stopped (the same threshold as Studio's "Script timeout:
exhausted allowed execution time"); there is no step cap, no memory cap and no frame
budget. The Studio's `script_timeout` (Roblox Studio's "Script Timeout Length") sets it
for the command bar and Play alike, and the four budget properties apply to a running
world from its next call on. The tighter budgets below are what the sandbox can hold a
host to when the host asks (the demo sets 4 ms a call and 8 ms a frame, which is how its
Evil script is caught); `max_steps` / `max_memory_mb` of 0 mean unlimited.
| Threat | Mechanism | Harness |
|---|---|---|
| Infinite loop / long resumption | wall-clock + step count checked in Luau's interrupt callback | 2, 3, 9, R8 |
| Memory bomb | custom allocator refuses past limit → catchable `not enough memory` | 4 |
| Unbounded recursion | Luau stack limit → catchable `stack overflow` | 5 |
| Process access (os/io/debug/loadstring/fenv) | stripped before seal; Luau has no io/require of its own (`require` here is the Instance one) | 6 |
| Cross-script leakage | one thread + own global env per script (`luaL_sandboxthread`) | 7 |
| Patching stdlib / host API | `luaL_sandbox` freezes globals; host tables are read-only | 7 |
| Poisoned sandbox after a kill | the offending thread is reset; other scripts and the tree are untouched | 3, 3b, 8, R8 |
| N scripts × per-call budget | frame budget: each resumption gets `min(maxMillis, frame remaining)`, the rest are skipped, round-robin start | 10 |
| Scripts stalling the render thread | the runtime runs on a worker; the engine never waits for a frame | 11, R13 |

Measured on this box: a `while true do end` costs the worker ~4 ms per frame (the budget)
and the main thread ~0.01 ms; ~200k `math.sin` iterations fit in a 4 ms resumption.

## Build

Tools: a C++17 compiler (MSVC 2019 Build Tools on Windows), CMake, Python 3 with SCons (`pip install
scons`), Godot **4.3-stable** (the export templates are 4.3), and GNU make for the dependency step
(on Windows: `choco install make`, or run the two `git clone`s and the four `cmake` lines in
`Makefile` by hand).

### Dependencies (once) Luau and libsecp256k1 (with the recovery module), both static, and
godot-cpp: ``` make luau-libs                                                       # clones +
builds both (~1 min) git clone -b 4.3 https://github.com/godotengine/godot-cpp
gdextension/../godot-cpp ``` `Makefile` pins Luau to `LUAU_REF` (bb15c71, 0.736 + 2 commits, what
the tree is built against) and libsecp256k1 to v0.6.0. Windows: godot-cpp links the static CRT
(`/MT`), and mixing runtimes fails the link with `LNK2038`, so add
`-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_POLICY_DEFAULT_CMP0091=NEW` to the two `cmake
-S` lines in `Makefile` first.

### Harness (no Godot) ``` make test                                   # Linux / macOS
build_harness.bat && harness.exe            # Windows (VS 2019 Build Tools) ``` Sections 1-17 are
the sandbox, worker, EIP-712, `pblockz://` assets and calldata manifests; R1-R58 are the Roblox
model: tree, scheduler, signals, require, placement, math types, HttpService, budgets,
TweenService/Debris/Players, bindables, snapshot, Disabled/Destroy, Rojo layout, replication +
remotes, RuntimeThread, the wire codec, budget retries, DataStoreService, input, GUI, raycasts + the
mouse, tools, Rojo JSON files, lights, ProximityPrompts, Sound, the hotbar, BillboardGui, the name
over the head, BrickColor + Teams, chat, TextChatService, the UI layout family, images, meshes,
SurfaceGui, pathfinding, fonts, the escape menu, rbxmx, rbxm, Lighting effects, joints, decals,
particles, explosions, highlights, seats, constraints, ropes / rods / springs, the movers,
materials, sliders, accessories, animations, DataStore versions, the R15 rig, plugins. Solid
modelling and Terrain are the Godot suites' (`gdextension/studio/tests/solid_*.gd` and the town's),
not the harness's. It ends with `811 passed, 0 failed`.

### GDExtension
```
cd gdextension
scons platform=linux target=template_debug
scons platform=windows target=template_debug -j4      # MSVC: -j8 runs out of heap; -j4 is what the release was built with
```
Output lands in `demo/bin/` and is copied beside every app; `target=template_release` builds
the release DLL the same way. On Windows the link fails with `Access is denied` while a Godot
window has the DLL loaded -- close the game first. The SConstruct defines `SECP256K1_STATIC`
and picks up MSVC's `Release/` subdirectories.

### First run of a fresh checkout
Godot registers a new `.gdextension` and imports assets in an editor run, not a headless
script run, so once per app before its first suite or export:
```
godot --headless --editor --path gdextension/demo --quit
```
The first such run may end in errors or a crash report after writing
`.godot/extension_list.cfg`; the second is clean, and headless suites work from then on.

### Run the demo
```
godot --path <repo>/luau/gdextension/demo
```
or open `gdextension/demo/` in the editor. WASD walk, space jump, right-drag orbit, wheel
zoom.

The same scene is a dedicated server or a client from the command line (`Main.gd`
reads the user args after `--`):
```
godot --headless --path . -- --server=8800                      # Server mode, listens
godot --path . -- --connect=127.0.0.1:8800 --name=Ann --user=9   # Client mode, joins
```
`--server` alone listens on 8800; `--connect=HOST` alone uses 8800. Every client that
connects gets a Player, a character and the map; the server prints joins and leaves.

Headless end-to-end (joins, walks, jumps, dies in lava via `Touched`, respawns, checks
the skin and animation, then `Humanoid:MoveTo` walks the character and `Humanoid:Move`
walks an NPC built by a script):
```
cd gdextension/demo
godot --headless --path . -s res://tests/play_solo_test.gd     # prints "play solo: PASS"
godot --headless --path . -s res://tests/net_test.gd           # prints "net: PASS"
godot --headless --path . -s res://tests/team_test.gd          # prints "team: PASS" (Team Create: a host and a guest on loopback)
godot --headless --path . -s res://tests/cloud_test.gd         # prints "cloud: PASS" (rbxassetid fetching, against its own tiny HTTP server)
```
`play_solo_test.gd` waits on the place rather than on the clock: its clock starts when
the character is up, its walking windows are counted in physics ticks from the tick the
body actually set off (the character is stepped at a fixed 60 Hz while the script
workers run on the render frame, so a wall-clock window is not a window of walking),
and every phase hands on when the thing it is about to check has actually happened --
with a cap, so a condition that never arrives fails the check rather than hanging.

`net_test.gd` runs a Server world and two Client worlds in one process over ENet on
loopback: both join, the map replicates, the two characters spawn beside each other,
one walks on its client and is tracked on the server and seen by the other client, a
client-reported lava touch kills it, it respawns, a RemoteEvent crosses the wire, and
the second client leaves cleanly (`client_left`, its character gone everywhere).
The headless renderer never compiles shaders; to check those run windowed with
`--quit-after 120` and grep the output for `shader`.

### The Studio

`gdextension/studio/` is a second Godot project: the place editor, next to the game
client rather than inside it. Its loop is Studio's -- **edit a tree that is not
running, Play a copy of it, Stop and your edits are still there, Save them into the
place.**

```
& C:\tools\godot\Godot_v4.3-stable_win64.exe --path <repo>\luau\gdextension\studio
godot --path gdextension/studio -- --place=<dir> --assets=<dir>    # open a place from the command line
```

`scons` copies the shared library into `studio/bin/` as well as `demo/bin/`, because a
Godot project cannot reach outside its own `res://`. With no `--place` it opens the
demo place next door. `--assets` is where the files a place names
(`meshes/rock.obj`, `sounds/creak.wav`) are rooted; unset, it takes the directory above
the project if the usual asset folders are there. A place outside the editor's own
project was never imported by Godot, so its meshes, sounds and images are read straight
off disk by the engine's own loaders.

**The edit world** is a `PulseBlockzWorld` with `edit_mode` on: no script ever starts and
nothing is simulated, so the tree is the place's files and stays exactly where you put
it -- an unanchored boulder hangs in the air until you play. Nobody is in it either
(`auto_join` off), and the Studio flies its own camera. A place whose map is *built by a
Script* opens with an empty Workspace, as it would in Studio; the demo place keeps its
map as `*.model.json` beside the Script that gives it behaviour.

- **Explorer**: the Instance tree, name and class, unfolded on demand. It reads the world node's own
  mirror, so no panel ever touches a script thread. - **Properties**: every property the selected
  instance's class declares, in the order it declares them, with the value the mirror holds -- or
  the class default, dimmed, for one nothing has written. Editable where a script could write it: a
  checkbox for a boolean, a dropdown of the enum's items for an `EnumItem`, and Roblox's own text
  for the rest (`5, 1, 2` for a Vector3, `163, 162, 165` for a Color3). A write is queued for the
  runtime's thread and comes back through the change log, so the cell settles when the world does; a
  value the class will not take is refused and the old one comes back. - **The viewport**:
  right-drag to look, WASD to fly (Q and E down and up, shift to hurry), the wheel to move forward
  and back as Studio's does (shift+wheel changes how fast WASD flies), left-click to select what is
  under the pointer -- which reveals it in the Explorer -- and everything selected is outlined where
  it is drawn. Drag in empty space for a box, and it takes whatever it covers on the screen. An
  instance with `Locked` set is left alone by both; it is still selectable in the Explorer. -
  **Selecting several**: shift to add, ctrl to take one back out, in the viewport or the Explorer.
  The handles then box the lot on the world's axes, since parts share no axes of their own: a move
  offsets each of them, a turn swings them about the middle of that box, and resize stays out of it,
  because Size is along axes a selection does not have in common. The Properties panel lists the
  last one picked and writes to them all; a row they do not agree on reads `--` (a half-ticked box
  would read as "off"). - **The handles**: Move, Scale and Rotate (the toolbar, or 2, 3 and 4; 1 is
  Select, which draws none), on the part's own axes, as Roblox's are. Drag an arrow to move, a face
  block to resize (the face you have hold of moves and the far one stays), a ring to turn. `snap` is
  the grid a move or resize lands on, 0 for none; a turn goes in 15 degree steps. What they write is
  what a script would: Position and Orientation in world terms, Size in the part's. A drag reads the
  part once, when it starts, because the writes land a frame or two later and a drag that fed on its
  own output would chase itself. - **Play / Stop**: Play builds a second world, hands it the edit
  tree as the same `*.model.json` Save writes (`add_model`, so two children may share a name and
  Refs come across), and joins it as a player, with a character, the default controls and the game's
  own camera. The edit world stands down -- hidden, not stepping -- and the panels follow whatever
  is running, so the Explorer shows the live DataModel while you play. Stop throws that world away
  and brings the edit one back untouched: unsaved edits and all, mid-edit, and nothing the game did
  comes back with it. - **Undo and redo**: Ctrl+Z and Ctrl+Shift+Z (or Ctrl+Y), 200 deep. Every edit
  goes through `History.gd` -- the panels and the handles write nothing directly -- and each one is
  recorded before it is applied. A handle drag is one entry, not the hundred writes it makes on the
  way; a delete keeps the whole subtree as the same `*.model.json` Save writes, so undoing one
  brings the children back too. An instance restored that way is a new instance with a new id, so an
  entry re-finds its own id afterwards -- the child of the parent that was not there a moment ago --
  which is why applying an entry is a coroutine. Nothing is recorded while a place is playing: that
  world is a copy. - **Save**: the tree into the place, as the files it can be opened from again --
  a `*.model.json` for an instance, and for a script the `.luau` it came from. An instance the place
  keeps files *inside* is descended into rather than written whole, so a folder mounted as a
  directory, or a Tool like the demo's Wand, is not flattened into one model file with every script
  under it orphaned. A model authored as an `.rbxmx` goes back as one, not as a `.model.json` beside
  it. Files Save owns but no longer writes are removed. Something the project has nowhere to put is
  not lost: Rojo's project file says which directory is which part of the tree, so Save makes one
  and writes the `$path` that points at it, at the service, which is the level Rojo mounts at. That
  happens once per service, the first time you put something there, and is the only thing the Studio
  writes outside the place's own files. An existing directory of that name is taken only if it is
  empty. A Camera and Terrain are never written at all -- the runtime makes those for itself. A file
  that already says exactly what Save was about to write is left alone rather than rewritten, so its
  timestamp does not move, and the Output says how many of the files it saved actually changed. -
  **Recent places** in the File menu, newest first, in the same `user://studio.cfg` the look and the
  layout live in; one that is not there any more drops out the moment you reach for it rather than
  on a sweep at startup. **Autosave** is beside it, off unless you ask for it: it is exactly the
  Save the menu runs, so it never fires over a place that is playing and does nothing at all when
  there is nothing unsaved. - **Dragging a brick** lands it flush on whatever is under the pointer
  and turns it to lie there. It is the part's own body that is dragged, not an arrow -- the six
  arrows stay a plain constrained slide, as Roblox's do. The turn is the shortest swing taking the
  plane it was lying on onto the plane it is landing on, applied to the orientation it already had:
  a brick that was yawed keeps that yaw as roll about the new face. The grab point is kept, the grid
  runs along the face rather than the world, and the dragged parts are excluded from the cast by
  RID, since their colliders are still at the old pose while the writes are in flight. - **The
  panels drag**: splitters between the docks and the viewport, with the widths remembered. A
  `SplitContainer` paints nothing at all, so the middle stays transparent and clicks still reach the
  place everywhere but the grab band itself. - **A command bar** under the Output runs a line of
  Luau against the world in front of you, on the server, with what you typed kept on the up arrow. -
  **Filters** over the Explorer (which keeps the ancestors of a match, or the match would be
  orphaned) and over the Properties list (on the name, never the value -- a value changes under you
  as the place runs), and the Output filtered by level. - **Writing an `.rbxmx`** is the one thing
  the Studio cannot do in GDScript, so `modelJsonToRbxmx` in `rbx_host.cpp` does it: the inverse of
  the reader, taking the same `*.model.json` everything else here moves trees as and putting it out
  in Studio's own spelling. Which of those shapes a property takes is the class table's business,
  and the class table is in C++ -- an enum is a numeric token, a `Vector3` is `<X><Y><Z>` but a
  `UDim` is `<S><O>`, a BasePart's colour is one packed integer under a name that is not the
  property's, its `Size` is `size` in lower case, and its Position and Orientation are one
  `CoordinateFrame` built from both -- as are a JointInstance's `C0`/`C1` and a Pose's, each of
  which this runtime keeps as its own pair. Refs become referents, a Script's text a
  `ProtectedString` in a CDATA section, and attributes the base64 `AttributesSerialize` blob Studio
  writes. Referents count up rather than being random, so the same tree written twice is the same
  file twice -- which is what lets Save leave a file it would not change alone. A model file is only
  ever rewritten in the kind it was: a Script kept as its own `.rbxmx` is written back as XML, never
  as raw Lua over the top of it. - **A Model is what you build with.** Clicking a part inside one
  selects the Model, as Studio does, and Alt reaches past it to the part. The handles then take
  every BasePart underneath it -- move, turn, drag onto a surface, zoom to, and a uniform scale
  about the middle, which is what `Model:ScaleTo` means. "Is a part" is having a Position and a
  Size, not having something drawn: a part welded to another is a limb with no body of its own, and
  in an edit world nothing simulates, so a drag that did not write its Position would leave it
  behind. For the same reason a weld makes no limb at all while a place is being edited: every
  welded part has its own body to click, box and move. - **A place's UI while you build it.** A
  ScreenGui is only ever real under a player's PlayerGui, and an edit world has no player.
  `gui_preview` builds StarterGui with the same builder from the same mirror, laid out in the hole
  the docks leave round the viewport, so it is anchored and scaled the way it will be in the game;
  nothing in it is ever a hit target, and `gui_at(point)` says which GuiObject is under the pointer
  -- innermost first, and only what the place authored, since the engine's own hotbar and player
  list cover most of the screen. A click in the viewport tries that before the 3D ray, so a Frame
  selects into the Explorer and the Properties panel like anything else. A selected GuiObject gets
  handles: the preview is laid out at one pixel per pixel, so dragging the body moves its Position
  by exactly the pixels dragged, in offset, and a grip grows its Size -- the left and top grips
  moving Position by the same amount so the far edge stays put -- each as one thing to undo. -
  **Terrain**, on a Terrain tab: Create lays a flat field, and a brush adds, subtracts, smooths or
  flattens it. It is **voxels**, as Roblox's is -- a density per sample on a regular grid, solid
  from the midpoint up -- so Subtract under the surface digs a cave with a roof over it, and an
  overhang or a tunnel is just more of the same; none of that is expressible with one height per
  column. The voxels are Roblox's occupancies, read the way Roblox reads them: occupancy is matter
  round the voxel centre, and a voxel of occupancy o has its surface o cells out from its centre (a
  column 1, 1, 0.75, 0 tops out at the 0.75 voxel's centre + 3 studs). Measured against 363 of
  Roblox's own raycasts on a real place's ground, matching to a median 0.01 studs. The surface is
  found by surface nets: one vertex per cell the surface passes through, at the mean of its edge
  crossings (each crossing placed by that rule from the edge's own two voxels), and a quad across
  every crossed edge, wound from the solid cell toward the air one. Face normals shade it, so hills
  shade like hills. FillBlock and FillBall write covered fractions like Roblox's, so a block filled
  to 8 tops out at 10 as Roblox's does; this engine's own Flatten, heightfield import and Flatten
  brush write the occupancy that puts the surface at the height they were given. Water is its own
  field: the voxels whose material is Water leave the ground and make a second, translucent surface
  in the Terrain's `WaterColor` at `1 - WaterTransparency` (never fully clear: the alpha floors at
  0.05), rippling `WaterWaveSize` studs high (0 is flat) at `WaterWaveSpeed` and as glossy as
  `WaterReflectance` says, with no collision. A character in it swims as Roblox's does -- no
  gravity, floated up until the head is out, a stroke going where the camera looks and Space
  swimming up, `Humanoid:GetState()` saying Swimming -- and a loose part floats or sinks by its
  material's density against water's (Plastic floats, Metal sinks), dragged as it goes. `GetState`
  follows the engine now: Running, Freefall, Swimming, Seated, Dead. `Terrain.Heights` carries the
  field as a base64 blob, run-length encoded -- almost every sample is all-air or all-solid, so a
  sculpted field with a cave in it is about four kilobytes and fits in a property and the replicator
  like anything else. The exact triangles are the collision shape. The runtime makes one Terrain per
  Workspace at a fixed id, never `Instance.new`, as in Roblox; a place saved with the earlier
  height-per-column blob is still read, and becomes voxels on the way in. Every sample carries a
  material as well -- Roblox's own `Enum.Material` values, Grass to Water, each drawn in its colour
  on the vertices -- so Paint changes what the ground is made of without moving it, and Add lays
  down the brush's material. **Scripts reach it too**: `Terrain:FillBlock(cframe, size, material)`,
  `FillBall(centre, radius, material)`, `FillCylinder(cframe, height, radius, material)`,
  `FillWedge(cframe, size, material)`, `FillRegion(region, resolution, material)`,
  `ReplaceMaterial(region, resolution, from, to)`, `WriteVoxels` / `WriteVoxelChannels`
  (`SolidMaterial` and `SolidOccupancy` for the ground, `LiquidOccupancy` for the water) and
  `Clear()`; `ReadVoxels` / `ReadVoxelChannels`, `CountCells`, `WorldToCell` and its `PreferEmpty` /
  `PreferSolid` twins (all three answer the cell the point is in), `CellCenterToWorld` /
  `CellCornerToWorld` and `GetMaterialColor` read the field the script side holds, and
  `SetMaterialColor` writes the `MaterialColors` blob as a property the host picks up. A write's
  field lives on the host beside its mesh, so the call is carried out to it as an op at the end of
  the frame and the host writes the new `Heights` back -- one property change, which the place keeps
  and the clients receive. A script sees the result the frame after it asks, as on a Roblox server
  where the geometry is built off the main thread too; a field that has never been made is made on
  the first call, as air, 512 studs across, for a generator script to work in. The fixed id survives
  the place file too: a model file named for Terrain -- Terrain.model.json -- applies onto the
  Terrain the runtime made rather than replacing it, since the loader's usual rule -- destroy and
  recreate -- would leave a client whose own Terrain sits at that id receiving the server's heights
  for one it never made.

  Terrain **isA BasePart and is nothing like one**: it has no Size or Position of its own,
  so taking the class literally would put an invisible 4 x 1.2 x 2 brick at the world
  origin in front of every raycast and region query -- `queryParts` skips it, and the
  scene mirror gives it no Part struct at all.
  A **stroke is one thing to undo**: the brush mutates a working copy in the world
  node for live feedback and writes nothing until you let go, at which point the field is
  encoded once and handed over as a single `Heights` write. One entry, one replication,
  one line in the file. `terrain_raycast` marches the density field itself rather than
  physics, so the brush aims exactly, a ray cast from inside a cave finds its floor and
  its roof, and a headless test needs no physics step. The ground is drawn and collided in chunks of sixteen cells a side: a stroke or a script's FillBlock redraws the handful it touched, and each chunk meshes a one-cell halo it reads but does not emit, so an edge on a boundary is drawn exactly once and the surface stays watertight.
- **Union and Negate** (Model tab; Ctrl+Shift+G and Ctrl+Shift+N, Roblox's own keys --
  which moved Ungroup to Ctrl+U, also Roblox's). A union is one part shaped like the
  parts that made it, with those parts gone from the tree, exactly as in Roblox. The
  boolean is Godot's `CSGCombiner3D`, run over the meshes already being drawn, in a
  hidden throwaway subtree that lives for two frames outside the place; the result is
  baked once and carried on the instance as `MeshData`, a base64 blob of vertices,
  normals and indices. So the runtime never runs a CSG, the place file needs no
  companion asset, and opening the place again gets the shape back without recomputing
  anything -- it is read by `op_mesh_for` in `pulseblockz_world.cpp` and fitted to `Size`
  the way a MeshPart's file is, so scaling a union scales the shape rather than
  stretching a box round it. A NegateOperation is the same geometry marked as a hole:
  translucent red, colliding with nothing, and subtracted when it is unioned with
  something. Neither class is `Instance.new`-able -- unioning is how one is made, as in
  Roblox -- so the new part goes in as a model, the way paste does. What a union was
  made of rides on it as a hidden `Operands` property, so Separate (Ctrl+Shift+U) puts
  the parts back where they were and removes the union, as one thing to undo.
- **A model in and out of a file.** `Insert from File...` reads an `.rbxmx`, `.rbxm` or
  `.model.json` in under the selection and records it as one thing to undo;
  `Save Selection to File...` writes the selection out as either.
- **Save as Roblox Place (.rbxlx)** writes the whole place as the XML Roblox Studio's File >
  Open takes: every service that is in the tree as a root item with its properties and
  everything under it, Refs resolving across the file (a Model's PrimaryPart, the
  Workspace's CurrentCamera), through the same writer whose models Studio has opened. The
  Terrain is left out (Studio makes its own, and the voxels here are not in its format);
  the Output says so when there was one. A place that saved its Camera opens with the
  Studio's view there, as in Roblox Studio.
  Checked by writing the test place and opening it back through Open
  Roblox Place, and then in Roblox Studio itself: the exported test place opened there and
  Play ran its server and client scripts and remotes (the Output showed "server up",
  "welcome", "patrol Success", "client up"). What Studio then reports as odd is the
  demo's own: a busy-loop script hits Roblox's timeout, and project-relative sound paths
  are not Roblox assets.
- **Open Roblox Place** takes what Roblox Studio's File > Save writes -- an `.rbxlx`, or the
  binary `.rbxl` -- lays down an empty Rojo project and merges the file's services into it,
  so Ctrl+S then writes the whole place out as `model.json` and `.luau` files. Each root
  item that is a service goes onto that service; a child the runtime already keeps under
  that name and class (StarterPlayerScripts, a Camera) is merged rather than doubled; Refs
  resolve across the whole file. The zstd-compressed chunks a current Studio writes are
  read too, through Godot's own zstd, installed into the reader as a hook. The place's
  Terrain's voxels come across too: Roblox's `SmoothGrid` -- worked out from a real place's
  terrain and checked against every byte of it (the format is written up over
  `terrain_decode_smoothgrid`) -- is read into the voxel field, material by material, and
  written back to the tree as this engine's own blob; the Output says how many chunks and
  studs, and whether the field's 2048 x 1028 x 2048-stud limit clipped it. Save as Roblox
  Place writes the field back out as SmoothGrid, so a place's ground makes the round trip
  to Studio (not yet opened there since). The place's `MaterialColors` (Roblox's 21 material
  colours) is the palette the ground is drawn in, crosses into Play, and goes back out; the
  terrain's vertex colours are read as sRGB; an Atmosphere's `Density` maps as
  0.06 x Density^5 per stud (its default 0.395 leaves 500 studs nearly clear and fades
  2000); and the sky below the horizon is sky-coloured, not Godot's earth brown. The Output's import
  line names the skipped services that had something under them and only counts the empty
  bookkeeping ones Studio writes (Selection, StudioData and their kind). Classes a place
  file carries that had no counterpart, and now do, so their settings survive a round trip:
  `Beam` (a ribbon of `Segments` quads on a cubic Bezier from Attachment0 to Attachment1,
  bowed `CurveSize0` / `CurveSize1` along the attachments' X axes, `Width0` to `Width1`
  across, its face along the attachments' Y or turned to the camera with `FaceCamera`,
  coloured and faded along its length by `Color` and `Transparency`, redrawn every frame,
  its `Texture` along it -- `TextureMode` Stretch fits it `TextureLength` times over the
  beam, Wrap and Static repeat it every `TextureLength` studs, `TextureSpeed` slides it
  from Attachment0 toward Attachment1),
  the legacy `Rotate` / `RotateP` / `RotateV` joints, hinges about their C0's Z axis between
  the two parts' assemblies (a door on its post swings when pushed) -- a `RotateV` driven at the speed of the Motor face it
  was made from (Part0's face along C0's Z: its `ParamB` in radians a second while that
  face's `SurfaceInput` is Constant, the per-face `ParamA` / `ParamB` / `SurfaceInput` now
  on every part), a `RotateP` serving to `DesiredAngle` at `MaxVelocity` radians a step;
  a hinged body's inertia is floored to an eighth of its largest component, since Godot's
  hinge solver tears a long thin body -- a fan blade, a propeller -- off its axis otherwise;
  `AssemblyAngularVelocity` (and the old `RotVelocity`) is read back from physics, as
  `AssemblyLinearVelocity` was,
  the six legacy BodyMovers (`BodyGyro`, `BodyVelocity`, `BodyPosition`,
  `BodyAngularVelocity`, `BodyForce`, `BodyThrust` -- and these act: each is a pull toward
  its goal on the part's own assembly every physics step, capped per world axis by its
  `MaxForce` / `MaxTorque`, `P` the rate it closes at and `D` its damping, a BodyVelocity /
  BodyPosition holding against gravity when the cap allows), `CFrameValue`, `NumberPose`, `AnimationController`,
  `UIGradient`, `SurfaceAppearance`, `WrapTarget` / `WrapLayer`, the nine `SoundEffect`s,
  TextChatService's four `*Configuration`s, `MaterialVariant`, `LocalizationTable`, and the
  services MaterialService, LocalizationService, TestService, InsertService, AssetService,
  VRService, VoiceChatService, GamePassService, TouchInputService, PermissionsService,
  PolicyService, SocialService and GroupService. A property with no home -- a CFrame on a
  class that became a Folder -- is dropped from a file quietly instead of raising an error.
  The camera follows `Camera.CameraSubject` as Roblox's does: a Humanoid means its character's
  root part, a part means the part, nothing means the local character; `CameraType` says how
  (Custom / Follow / Orbital orbit it at the user's yaw and pitch, Attach sits behind it turning
  with it, Track moves with it keeping its heading, Watch stays put facing it, Fixed stays put,
  Scriptable is the script's). A place that hands the camera to a field player gets it.
  `run_client_chunk` runs a line on the client runtime, where the camera lives.
  A `Trail` is drawn: the ribbon its two Attachments swept over the last `Lifetime` seconds,
  sampled each frame once they have moved `MinLength`, capped at `MaxLength`, its `Color` and
  `Transparency` sequences along it (time 0 at the attachments) times `Brightness`, unlit and
  double-sided, `WidthScale` narrowing it about its middle along its age, `FaceCamera` turning
  its width to the eye, its `Texture` along it (`TextureMode` Stretch fits it once from the
  attachments to the tail, Wrap repeats it every `TextureLength` studs from the attachments,
  Static every `TextureLength` studs of the path itself, staying where it was laid).
  A model that arrives for a container the runtime keeps one of at a fixed id
  (StarterPlayerScripts, StarterCharacterScripts, a Camera, the Terrain) goes onto that one
  instead of making a twin; Play copies the edit tree that way, and a twin
  StarterPlayerScripts would be one whose LocalScripts never reached PlayerScripts.
  A RemoteEvent fired before anything has connected to it is kept -- 256 a remote, then
  Roblox's own "invocation queue exhausted" warning -- and delivered the frame a handler
  arrives, in order, so a server that fires match state as the player joins, before the
  LocalScript has connected, gets it through, as on Roblox.
  `UnreliableRemoteEvent` is a RemoteEvent here (same API; Roblox's may drop or reorder its
  deliveries and caps them at 900 bytes, which a place cannot rely on either way).
  `BasePart:ApplyImpulse` / `ApplyImpulseAtPosition` / `ApplyAngularImpulse` go out with
  the frame and land on the assembly's rigid body (nothing on an anchored part, as on Roblox).
  A part's `Mass`, `AssemblyMass`, `AssemblyRootPart`, `CenterOfMass` and
  `AssemblyCenterOfMass`, and `GetConnectedParts()` / `GetRootPart()`, read the rigid
  assembly its Welds / Motor6Ds / WeldConstraints make (the same joints the host builds one
  body from): an anchored part is the root, else the heaviest; Massless parts weigh nothing.
  A `NoCollisionConstraint` is a collision exception between its two parts' bodies (their
  assemblies', since Godot excepts bodies rather than shapes), kept up as bodies are rebuilt.
  A part's six surfaces (`TopSurface`...`BackSurface`, `Enum.SurfaceType`) are kept and
  saved, drawn smooth here whatever they say, and some fifty enums a place's scripts name
  without anything here acting on them (SurfaceType, Limb, BodyPart, Technology, FontSize,
  ReverbType, Platform, the Dev*MovementMode family...) resolve at Roblox's values.
  CollectionService tags come in from Studio's `Tags` blob (NUL-separated names), go back
  out in the `.rbxmx` the same way, and sit in a `model.json` as `"tags": [...]`.
  Checked against a real, current, 45,000-instance `.rbxl` and rbx-test-files' baseplate.
- **New Place** writes a project file, the four directories a Rojo place usually has, a
  baseplate and a spawn.
- **Attributes and tags** have a panel of their own, under the declared properties
  where Studio puts them: every one the instance carries, a row to type a new name into,
  a checkbox per tag, and both recorded so Ctrl+Z takes one back. They cross the change
  log as `@Name` and `#Name`.
- **Find and replace** in the script editor: Ctrl+F, Ctrl+H for the replace half, Enter
  and Shift+Enter through the matches, a match count, and case matching. Replace All is
  one operation in the box's own undo and one entry in the Studio's history when the tab
  is committed, and it works bottom-up so an earlier replacement cannot move a later one
  out from under it. **Completion** after a dot or a colon: the expression before the caret is resolved against the live tree -- `game`, `workspace`, `script` and `script.Parent` are roots, the rest children by name -- so `workspace.Map.Crate.` lists the Crate's children with its class's properties and events, and `:` its methods, both from the runtime's own tables via `get_class_members`, so the list cannot drift from what the runtime accepts. A local that holds an instance cannot be followed without a parser, and nothing is offered rather than a guess.
- **A playtest with more than one player in it.** A count beside Play seats that many
  extra Players when you start, after your own join lands -- the first player in a Play
  Solo world is you, and one added before that would take the local seat. They are real
  Players with real characters: they fire PlayerAdded, join Teams, carry leaderstats and
  receive RemoteEvents. Nobody is behind them, so testing what a second player *sees*
  still means a second client.
- **Ctrl+S**, a Save button beside the undo arrows, middle-button pan, a wheel that zooms
  toward what is under the pointer rather than along the way the camera happens to face,
  and a Clear on the Output.
- **Play, Play Here, Run, Pause and Step.** Run is Play with nobody in it, so the place
  runs and you keep the Studio's own camera to watch it from; Stop puts that camera back
  where it was looking.
- **Debugger** (Test tab). Click left of a line in the script editor for a breakpoint;
  the Studio keeps them by the script's full name and sets them on the copy that plays, so
  a breakpoint set before Play is there from the script's first line. When a played script
  reaches one the whole place stands still -- both runtimes get no time and the physics is
  held, as under Roblox's debugger -- and the Debugger comes up under the view: the call
  stack (the stopped frame first, the chunk as "(main)"), the picked frame's locals and
  upvalues as `tostring` shows them, and Continue (F5), Step Over (F10), Step Into (F11),
  Step Out (Shift+F11). The stopped script opens in the editor with the arrow on its line;
  picking another frame opens that one. Breakpoints toggled while playing take effect at
  once; Stop puts the Debugger away. Scripts compile with full debug information so the
  locals have their names.
- **The head** is the PulseChain hexagon made solid: `MeshType.Head` draws a flat-topped
  hexagonal prism (points left and right) filling Size times Scale, faceted with hard normals,
  no curve anywhere; a character's Head carries one as Roblox's does (`Head.Mesh`), regular at
  the torso's 2 studs wide, 1.73 tall and 1 deep, lifted to sit on the torso, the gradient skin
  over it and the hat and hair attachments on its flat top.
- **Plugins** (Plugins tab), Roblox's local plugins: a `.lua` / `.luau` file in the plugins
  folder (Plugins Folder opens it; `user://plugins` of the Studio) is a Script under
  `PluginDebugService`, run in the edit world with a `plugin` global. `plugin:CreateToolbar`
  and `toolbar:CreateButton` put a group and buttons on the Plugins tab (`Click`, `SetActive`,
  `Enabled`); `plugin:Activate(exclusiveMouse)` / `Deactivate` / `IsActivated` and
  `plugin:GetMouse()` give it the pointer over the view as a PluginMouse (`Move`,
  `Button1Down`, `Hit`, `Target`, `UnitRay` along the Studio camera's ray; with the exclusive
  mouse the view's left clicks are the plugin's, not the picker's); `GetSetting` / `SetSetting`
  are kept in `user://plugin_settings.json` per plugin; `Selection` (`Get`, `Set`, `Add`,
  `Remove`, `SelectionChanged`) is the Studio's own selection both ways; what a plugin changes
  in the place -- properties, instances it adds, moves, instances it takes out -- becomes one
  entry in the Studio's undo at each `ChangeHistoryService:SetWaypoint` (or
  `FinishRecording`), named for it, Ctrl+Z / Ctrl+Y like any edit (every change carries the
  script that made it, so a plugin's are told from the Studio's own); an `.rbxmx` or
  `.model.json` in the folder is a plugin too, its scripts run named for the file; a button's
  `Icon` that is a file (absolute, or in the plugins folder) is shown; `plugin:OpenScript` opens the script in the editor; `CreateDockWidgetPluginGui`
  and `DockWidgetPluginGuiInfo.new` give a `DockWidgetPluginGui` under `CoreGui`, drawn as a
  floating titled frame over the view (its `FloatingXSize` / `FloatingYSize`, stacked down the
  right edge), and any `ScreenGui` a plugin puts under `CoreGui` is drawn full-view; both are
  live -- their buttons press, in the edit world's own runtime -- where the StarterGui preview
  is only a picture; `CreatePluginAction` gives a `PluginAction`; `RunService:IsEdit()` is true
  there. Reload Plugins fires `Unloading` and reads the folder again; the plugins are not part
  of the place, and do not go into Play.
- **Roblox's cloud assets**: an `rbxassetid://` in any of its spellings
  (`rbxassetid://123`, `http://www.roblox.com/asset/?id=123`, an assetdelivery URL) on a Decal,
  Texture, MeshPart, SpecialMesh, ImageLabel, ParticleEmitter, Trail, Beam, Sound or Animation
  is fetched once from Roblox's asset delivery (`assetdelivery.roblox.com/v1/asset/?id=`) into
  `user://roblox_cache/<id>.<ext>`, the kind told by the bytes -- an image, an Ogg or MP3, a
  Roblox `.mesh` (versions 1.00 to 5.00 are read: the first LOD, position, normal, uv, colour),
  a binary or XML model (a KeyframeSequence goes into the tree under
  `ReplicatedStorage.RobloxAssets` by its number and the Animation plays it) -- and everything
  drawn from it is rebuilt when it lands; next time it loads from the cache. One the site will
  not give (private, gone, HTTP 403 / 404) is said once in the Output. `cloud_fetch_base` on
  the world points a test at its own server.
- **EditableImage**: `AssetService:CreateEditableImage({Size = Vector2})` (or `Instance.new`) is
  a bitmap in RGBA8 up to 1024 a side that scripts draw into -- `WritePixelsBuffer` /
  `WritePixels`, `ReadPixelsBuffer` / `ReadPixels`, `DrawRectangle`, `DrawLine`, `DrawCircle`,
  `DrawImage` with an `ImageCombineType` (BlendSourceOver, Overwrite, Add, Multiply), `Resize`,
  `Copy` -- and an ImageLabel or ImageButton shows through `ImageContent =
  Content.fromObject(image)`; `Content.fromUri` / `fromAssetId` / `none` are the Image uri, and
  `ImageContent` reads back as a Content (`typeof` says so; `typeof` is the engine's own now).
  Each frame an image was drawn into reaches the engine as one texture update, so a canvas
  redrawn every tick (a Game Boy emulator's screen) stays one texture.
- **Play in its own viewport**: a played place renders in a SubViewport filling the hole the
  docks leave, with its own 3D world, so its ScreenGuis lay out to the view the way Roblox's
  do and nothing of the game draws under the ribbon or the panels; keys and the mouse over
  the view are the game's. Run (nobody playing) keeps the Studio's own view and camera.
- **Team Create** (Home tab, Collaborate): Host lets other Studios join this edit world over
  the network (port 8802); Join, given `host:port`, makes this Studio a guest of another's --
  the host's whole place appears live (every service and every property replicate to guests,
  ServerScriptService and a Script's Source included, unlike a game's clients), and every edit a
  guest makes -- a property, an attribute or tag, an insert, a move, a delete, a model dropped
  in, the command bar, a script file -- goes to the host as it is made, is applied there with
  the host's own tools and comes back replicated to everyone; guests are Players on the host
  (who is here), with no characters; the host saves. Plugins run on each Studio's own runtime,
  so a guest's plugin edits stay local for now.
- **Rig Builder** (Avatar tab): R6 Rig builds Roblox's six-limb block rig in front of the
  camera -- HumanoidRootPart, Torso, Head, arms and legs, the six Motor6Ds in the Torso (and
  the RootJoint in the root) with the C0 / C1 every R6 animation was authored against, the
  attachments accessories hang from, a Humanoid -- and R15 Rig the fifteen-limb one in this
  engine's own layout (the one players are built in), each joint in its limb as Roblox keeps
  them, meeting where the limbs touch, `Humanoid.RigType` R15. The rig is built by Luau in
  the edit world, as Roblox's own plugin builds its, and recorded as one insert for Ctrl+Z;
  it arrives selected, ready for the Animation Editor.
- **Animation Editor** (Model and Avatar tabs), on the rig the selection is in: a Model with Motor6D
  joints. Its limbs are listed by their Part1 names, as Roblox names Poses, over a
  timeline at 30 frames a second. Turn a limb with the Rotate tool and it turns about its
  joint and is keyed at the current time; move one with the Move tool and the shift is
  keyed; Add Key keys every joint where it stands, keys drag along their rows, Delete
  takes one away, Play runs the sequence looped or once, the length is a field. Posing
  writes straight to the world and not to the History -- nothing to undo -- and Close puts
  every limb back where the place has it. Save writes a `KeyframeSequence` under the rig's
  `AnimSaves` folder, where Roblox's editor keeps its own (Keyframe -> Pose named for the
  root part -> Poses nested as the joints are, each with the joint's Transform), as one
  undo entry; Load lists the ones there, so a place that came from Roblox with animations
  in it opens them here, and an `Animation` whose `AnimationId` is
  `Workspace.Rig.AnimSaves.Wave` plays the saved one -- on a character, and on any other
  rig: an `AnimationController`'s puppet takes its tracks' Poses through its Motor6Ds'
  `Transform` (Part1 = Part0 * C0 * Transform * C1^-1, Roblox's own joint), which a
  script may also write itself for a procedural animation; a limb the tracks let go of
  goes back to rest. A character is built with those joints too, as Roblox's is -- an
  R6's `RootJoint`, `Neck`, `Left Shoulder`, `Right Shoulder`, `Left Hip` and `Right Hip`
  in the Torso with the C0 / C1 every R6 animation was authored against, an R15's
  fifteen each in its limb -- so `character.Torso["Right Shoulder"].Transform` swings the
  arm about the shoulder exactly as it does there, the Animator's Poses go through the
  same joints, and the engine's own walk, jump and sit poses take over while every joint
  is at rest.
- **The verbs**, on Roblox's own keys: Cut, Copy, Paste and Duplicate (Ctrl+X / C / V /
  D), Group and Ungroup (Ctrl+G, Ctrl+Shift+G), Rename in place (F2), Select All
  (Ctrl+A), Delete, Zoom to the selection (F), and Insert Object -- a filtered list of
  every class the runtime will make, taken from the class table itself so it cannot go
  stale. Copy is the serializer again: what is on the clipboard is the `*.model.json`
  text of what was selected, so pasting is the same code that hands a tree to Play.
  One table says what each verb is, whether it can be done right now, and what key does
  it, and the toolbar, the right-click menus and the keyboard all read from it -- so a
  greyed-out item is exactly one that would do nothing.
- **Right-click** in the viewport or the Explorer for that menu. Held and moved, the
  right button turns the camera; pressed and released where it started, it is a click.
- **Three looks** -- Dark, Light and Clear -- from the View tab, remembered in
  `user://studio.cfg`. Clear is Dark seen through, derived rather than authored. Only a
  panel's fill is let down; text never is, and Clear raises its text and hardens its
  borders. The code editor opts out at any setting: fixed text over a moving 3D view
  reads as the text itself moving. The gizmo axes stay red, green and blue in every
  look -- they are world-space, not chrome.
- **Icons** are SVG source held in `Icons.gd` and rasterised at load; there is no image
  file in the repository. The art carries no colour, so one texture serves every look and
  every button state by tint, and the Explorer reads by shape first and hue second, as
  Studio's does.
- **The ribbon**, laid out the way Studio's is: a title row with the File menu and the
  undo arrows, a tab strip -- Home, Model, Test, View -- and a band of groups with each
  group's name in small text underneath. Home carries Clipboard, Tools, Insert, Edit
  (Group / Ungroup / Anchor / Lock) and Test; Model repeats the tools and holds the two
  snap grids, one in studs and one in degrees; View toggles the docks. The four tools
  are Studio's four on Studio's keys -- 1 Select, 2 Move, 3 Scale, 4 Rotate, where
  Select is the one that draws no handles. Which verb sits in which group comes from the
  same command table the menus and the keyboard read, and each button's icon from
  `Icons.gd` above (a plugin's toolbar button brings its own image file).
- **The script editor**: double-click a Script, LocalScript or ModuleScript in the
  Explorer and it opens in a tabbed `CodeEdit` over the viewport, with Luau
  highlighting, line numbers and a monospace face. The buffer is the editor's, not the
  world's -- it reads `Source` once when you open it and never again while you type, so a
  write coming back through the change log cannot move the cursor. It hands the text over
  on the moments that matter (leaving the box, switching tabs, closing, saving, playing)
  and each of those is one entry in the history: typing is the box's own undo, and Ctrl+Z
  at the Studio level takes back a whole edit. Nothing restarts -- in an edit world no
  script runs, and what Play runs is a fresh copy of the tree with the new `Source`
  serialized into it.
- **Unsaved work**: the title carries a `*`, and Reload, Open and closing the window all
  ask first.
- **Output**: the place's `print`s, warnings and errors, with a runaway script's kills
  reported once and then only now and then. A Luau error says where it happened --
  `Workspace.Map.Build:12: attempt to index nil` is the script's own full name and the
  line in it -- so the line is a link, and clicking it opens that script with the caret
  on that line. It resolves against the place, never against a copy that is playing. A
  script can be opened while one is playing for the same reason, and what you type is
  held until Stop puts the history back on the place, so nothing is written into the copy.
- **A running place is framed and named.** What you build inside one is thrown away the
  moment it stops: the view carries a border and a pill saying `Playing` or `Running`,
  and the window title says `[running]`, as Roblox frames its own.
- **Align and distribute** (Model tab): line a selection up on one axis, or spread it
  evenly between the two that are furthest apart. Both are one entry in the history.
- **A stud grid and the collision shapes** (View tab). The grid is unshaded lines a hair
  above the ground plane; the collision view outlines what the physics actually collides
  with, which for a MeshPart is a different shape from what is drawn.

```
cd gdextension/studio
godot --headless --path . -s res://tests/studio_test.gd    # prints "studio: PASS"
```
opens the demo place and works it the way a creator would: checks nothing ran and the
map is there anyway, reads and writes properties (and checks the refused ones are
refused), inserts a Part and deletes it, drags all three handles -- aimed by ray, so a
test can put the pointer exactly on an axis -- undoes and redoes each kind of edit (a property, a whole drag, an insert, a delete with children, a move in the tree), selects several and moves, edits and deletes them as one, works every verb (copy and paste, duplicate, group and ungroup, rename, insert, lock, zoom to), edits a script and checks the text reaches the tree, the undo stack and the `.luau` on disk, then plays the place, checks the tree
crossed with its unsaved edits and its Refs intact, stops, checks the place is as it was
left, and saves into a copy of it and opens that again. It also drops a brick on a wall
and on a ceiling and checks where it lands and which way up (a Ball stands off by its
radius, not half its Size; a ceiling turns a part over rather than spinning it on the
spot), pulls a Cylinder's end and checks the length grew rather than the radius, deletes
a Model and the part inside it as one batch and checks one undo brings back one Model,
puts a Part where the project has nowhere to keep it and checks Save wrote the `$path`
that gives it one, and moves something inside the demo's `.rbxmx` and checks the file
comes back as XML with the colour, the CFrame, the Ref and the Script's text intact.
Another run of checks pins the edge cases: a
ColorSequence reaches the tree in Rojo's own spelling and survives being copied through a
model (which is what Play and paste do), a selection with no drawn box takes no drag
rather than a broken one, the tool cannot change under a drag already running, deleting a
child and then its parent and undoing twice brings both back exactly once, two script
tabs left dirty by a play both reach their own file, a Save that could not write
everything sweeps nothing, a joint's offset and a renamed joint both survive the round
trip, and a script's text comes back with the characters it was typed with rather than
the ones an XML reader would decode. One
of the handle checks pins the Euler convention: `Orientation` is YXZ degrees (Godot's
`EULER_ORDER_YXZ`), and the drawn part has to sit where the panel says it does.

A new `class_name` script (or a new `.gdextension`) is only registered once Godot has
scanned the project, so a fresh checkout needs one `--editor --quit` run, or two starts.

What it is not, yet: the shoreline byte Roblox's `SmoothGrid` keeps beside a water voxel is
stepped over (the water itself lands in the water field); the UIGradients a place carries are
kept but not drawn; Water stands where it was put rather than flowing.
Nothing writes an `init.meta.json` back, so a directory instance's own properties are
read from the project file and left there, and an `.rbxm` (the binary model) is read but
never written -- Save leaves that file exactly as it found it. The `.rbxmx` writer's
output has been opened in Roblox Studio itself once: a Model holding Parts, a Cylinder,
a Ball, a PointLight, a Motor6D and a Script inserted with its tree intact, the shapes
and the plain colours right, and the Script ran there with the `<`, `>` and `&` in its
source literal. Not yet looked at there: the attributes blob, a joint's C0, and a
SpawnLocation. That is on top of round-tripping through this engine's own reader to the
byte.

### Hot reload
```
node tools/sync.js gdextension/demo/scripts                # the kit's own watcher
```
Point it at a directory or a `default.project.json`. `ScriptSync.gd` loads the project
from disk at start (nothing needs the tool), then connects to `ws://127.0.0.1:8790`;
from then on every save is pushed into the running world and the script's `print`s,
errors and kills come back to the tool's terminal. Editing `default.project.json`
remounts on the fly: scripts whose instance path went away unload, ones under a new
`$path` load; a project file that does not parse is reported and the old mounts stay.
Wire format: one JSON object per text frame, `{op:"load"|"unload", name, source}` down,
`{op:"log", level, name, text}` up. `node luau/tools/sync.test.js` (Node 22+) runs the
tool against a scratch project and checks all of that from a connected socket.

## `PulseBlockzWorld` (Node3D)

One node hosts one world. Three modes: Studio's Play, a dedicated server, a client:

- **Play Solo** (default): a server runtime and a client runtime in one process, on
  their own threads. Scripts run on the server, LocalScripts on the client; the
  replication stream and remote traffic cross every frame. `player_name` joins on the
  first frame, gets a character the default controls drive and an orbit camera that is
  also the client's `workspace.CurrentCamera`.
- **Server**: just the server runtime. `add_player` creates headless players;
  `listen_port` (or `listen(port)`) accepts Client worlds. Play Solo can listen too.
- **Client**: just a client runtime, fed by a Server over the network. `auto_join`
  connects to `server_address:server_port` in `_ready` as `player_name`; the join, the
  replication stream and the remotes are the ones a Play Solo client gets, carried by
  `rbx_wire.h` packets over ENet. Structure, remotes and the poses that start or end a
  motion go reliable and ordered; poses mid-motion go unreliable-ordered (a lost one
  is superseded by the next, a stale one is dropped), and a part that stops gets its
  resting pose resent reliably. Two packets per direction per frame at most.

Each `_process`: if the workers are idle, take their frame results (apply tree changes
to the scene, route replication/remotes to the other side, emit signals), then submit
the next frame with the physics snapshot of unanchored parts and characters, the
`Touched`/`TouchEnded` contacts, and any queued jobs. Physics writes go back with
`fromHost` so they never echo into the scene; script writes always do. On the client
only client-made instances and purely visual properties of replicated ones reach the
scene: the server owns where everything is.

Network ownership is Roblox's: a client simulates its own character (its
`CharacterBody3D`, the slide-collision touches) and reports `Position`/`Orientation`/
`AssemblyLinearVelocity` of the root and `MoveDirection`/`Jump` of the Humanoid every
frame; the server accepts exactly those, for exactly that character, and drops the rest
of the packet. Everything else a client sees — the map, other characters, bricks — is
posed from the server's tree (static bodies on the client; other players' roots ease
toward each reported pose over the interval the reports arrive at, snapping on a
teleport or a fresh body, and animate from their reported velocity). A malformed
packet disconnects the peer.

| Member | Notes |
|---|---|
| `mode` | `Play Solo` / `Server` / `Client`; read when the runtimes are created |
| `listen_port`, `bind_address` | Server / Play Solo: accept clients in `_ready` (0 = off). Bind `127.0.0.1` to stay on loopback (no firewall prompt) |
| `data_store_path` | the JSON file `DataStoreService` persists to (`user://datastores.json`); empty keeps stores in memory |
| `server_address`, `server_port` | Client: where `auto_join` connects |
| `physics_layer` (1–16) | the collision layers every body gets (one for anchored parts and characters, layer + 16 for loose parts), so two worlds in one scene (the net test) keep their physics apart |
| `player_name`, `user_id`, `auto_join` | the local player in Play Solo |
| `default_controls`, `default_camera`, `default_animations` | WASD/space, orbit camera, the R6 walk/jump/idle pose. Turn off to drive them from your own scripts/nodes |
| `default_lighting` | a `Sun` (DirectionalLight3D) and a `Lighting` (WorldEnvironment with a procedural sky) under the world, driven by the `Lighting` service. Turn off to bring your own |
| `threaded` (default true), `auto_step` | worker threads / step from `_process`; `threaded = false` runs frames inline for lockstep and tests |
| `max_millis_per_call`, `max_frame_millis`, `max_steps`, `max_memory_mb` | budgets; live-tunable. Defaults are Roblox's: 10 s a call (Studio's script timeout), no frame budget, no step or memory cap (0 = unlimited). The demo sets 4 / 8 ms to show the kill |
| `load_file(rel_path, source)`, `unload_file(rel_path)` | the Rojo path decides the class and place. Runs at the next frame; a bad path arrives as `script_error("", err)` |
| `run_chunk(name, source)` | console: run a chunk on the server with `script = nil` |
| `add_player(name, user_id)`, `remove_player(name)` | `PlayerAdded` fires, a character spawns. In Play Solo the first one is the local player; ignored in Client mode |
| `listen(port, max_clients=32)`, `connect_to_server(host, port)`, `disconnect_from_server()`, `is_server_connected()`, `get_client_count()` | the transport by hand |
| `flush()`, `is_idle()` | run every queued job now / is a frame in flight |
| `get_stats()` | last frame: `millis, step_millis, now, threads_live, scripts_started, resumes, errors, kills, skipped, instances, parts, client_millis, client_threads_live` |
| `get_part_node(id)`, `get_part_id(node)` | the scene body mirroring a BasePart, and back |
| `edit_mode` | a world to look at and change, not to play: no script ever starts (the tree is still built, Sources and all) and nothing is simulated, so an unanchored part stays where it was put. Set it before the world runs, as with `mode` |
| `track_properties` | keep every property change in the node's mirror, so a tool can read the whole tree from the main thread. Off by default: a game client wants none of the weight |
| `get_child_ids(id, client=false)`, `get_instance(id)`, `get_properties(id, hidden=false)`, `set_property(id, name, value)` | the mirrored tree, for an Explorer and a Properties panel. Id 0 is the DataModel; a server's instances have positive ids, a client's own its negative ones (`client` picks which root). `get_instance` gives `{id, name, class_name, parent, path}`, `get_properties` one dictionary per declared property `{name, type, value, read_only, is_default, enum_type, enum_items}` (`hidden` to include the ones a panel should not show but a file must carry, such as a Script's `Source`). A write is checked against the class here and applied on the runtime's thread |
| `add_model(parent_path, name, model_json)` | a `*.model.json` subtree under a container path (`"Workspace/Map"`), Refs and all. Nothing is replaced: it is added, so two children may share a name -- which a file's path could not. How a Studio hands its edit tree to the world that plays it |
| `set_parent(id, parent)` | move an instance under another, as writing `Parent` would. Parent is not a declared property -- the runtime handles it apart from the rest -- so `set_property` cannot reach it and this can |
| `get_creatable_classes()` | every class `create_instance` will make, sorted. What an Insert Object list is built from, so it cannot drift from what the runtime has |
| `create_instance(class_name, parent)`, `destroy_instance(id)` | `Instance.new` + a Parent, and `:Destroy()`, queued the same way |
| `get_tree_version()` | bumped by every create, destroy, reparent and rename, so a panel rebuilds only when the shape changed |
| `gui_preview`, `gui_preview_rect`, `gui_at(point)` | draw StarterGui as it would look in game, with no player: the same builder, laid out in the rect an editor leaves for the view, never a hit target. `gui_at` is the GuiObject under a point, innermost first, and only what the place authored |
| `get_terrain_id()`, `get_terrain_info()`, `terrain_flatten(res, cell, y)`, `terrain_sculpt(at, radius, amount, mode, level)`, `terrain_commit()`, `terrain_raycast(from, dir)`, `terrain_height_at(x, z)` | the Workspace's voxel field (mode 0 adds solid in a ball, 1 takes it away, 2 smooths, 3 flattens to `level`). Sculpting mutates the drawn and collided copy and returns nothing to the tree; `terrain_commit` encodes it and is pure, so the caller's history entry is the only write there is. `terrain_raycast` is against the field itself, not through physics |
| `to_rbxmx(model_json, name)` | the same `*.model.json` out as the XML Roblox reads. A pure conversion -- it reads the class table, not the tree -- and the one direction a tool cannot do for itself, since which shape a property takes in that file (a numeric token, a packed colour, one CFrame for two properties) is the class's business |
| `get_camera()`, `get_local_player_id()`, `get_local_character_id()`, `get_local_character_node()` | the default camera; the local Player / character Model ids on the server; the `CharacterBody3D` |
| signals | `script_print(script, text)`, `script_warn`, `script_error(script, error)`, `script_killed(script, reason)`, `frame_finished(stats)`, `player_joined(name, player_id)`, `client_joined(name, player_id, peer)`, `client_left(name, player_id, peer)`, `server_connected(player_id)`, `server_disconnected()` |

### Scene mirror
```
Instance tree (worker)                    scene (main thread)
Workspace.Map.Floor   (Part, Anchored)    StaticBody3D + CollisionShape3D + MeshInstance3D
Workspace.Ball        (Part)              RigidBody3D  + CollisionShape3D + MeshInstance3D
Workspace.Ada.HumanoidRootPart          CharacterBody3D + capsule; the other limbs are
                                          MeshInstance3Ds riding on it
```
Shapes `Block / Ball / Cylinder / Wedge`, `Color`, `Material` (Neon emits; SmoothPlastic, Glass and Ice
are glossy), `Transparency`, `Reflectance`, `CanCollide`, `CastShadow`,
`Anchored`. `Touched` / `TouchEnded` come from contact monitoring for rigid parts and
from slide collisions for characters, and name the part that was actually hit even
inside a welded assembly (one body wearing every part's shape: the contact says which
shape, and the shape says which part) (a touch about a part destroyed the same frame is
dropped rather than delivered as nil). A character shoves loose parts and carries what
lands on its head, as Roblox's does: unanchored parts live on a second collision layer,
and a character whose sweep Godot cancelled as stuck against one is swept again
against the anchored world alone, the physics step then pushing the part out; in Play Solo the client hears the local
character's touches and `MoveToFinished` too, as a Roblox client does. A LocalScript
owns the local character's Humanoid (`WalkSpeed`, `Jump`, `MoveTo`) without the
server's say, also as on Roblox. Parts below `Workspace.FallenPartsDestroyHeight`
are destroyed; `Workspace.Gravity` is in studs/s².

### The default skin
A spawned character's limbs get `Color = kDefaultSkin` (`rbx_instance.h`). A limb with
exactly that Color, `Plastic` and `Transparency < 1` is rendered with the PulseBlockz
gradient — red → magenta → purple → blue → cyan on a 45° line from the outer-bottom
corner of the left foot to the right ear, one ramp across the whole body. Set any
other Color and the limb is that colour, like any part.

## `PulseBlockzCrypto` (static)
| Method | Notes |
|---|---|
| `verify_curation_list(list: Dictionary) -> bool` | canonicalise entries, EIP-712 digest, recover, compare with `maintainer`. `Curation.gd` uses it |
| `curation_list_signer(list) -> String`, `curation_list_digest(list) -> PackedByteArray` | recovered address (`""` if malformed) / the 32-byte digest |
| `keccak256(bytes)`, `keccak256_hex(text)`, `canonical_json(text)`, `checksum_address(a)`, `recover_address(digest, sig_hex)` | building blocks |
| `sign_digest(digest, secret_key_hex) -> String`, `address_from_key(secret_key_hex) -> String` | the 65-byte recoverable signature as hex (`""` unless the digest is 32 bytes and the key parses), and the key's address; the key's heap copy is zeroed before returning. `Tx.gd` signs with the first |

## Design notes
- **One Runtime per context, one thread per Runtime.** The server has one, every client
  has one. A Runtime has no thread affinity but is not synchronized: `RuntimeThread`
  hands it a `FrameIn` (dt, physics writes, world events, jobs) and gets a `FrameOut`
  back (tree changes, remotes, logs, stats). The engine never touches a runtime while a
  frame is in flight, which is why script effects land one frame later — the latency an
  authoritative server introduces anyway.
- **The change log is the interface.** Mirroring into the scene and replicating to
  clients are the same stream filtered differently; `DataModel::snapshot` rebuilds a
  tree for a late joiner from it.
- **Steps ≠ instructions.** Luau's interrupt fires on loop back-edges and calls, not every
  opcode, and costs nothing when idle.
- **Host API is where the real security is.** The sandbox stops scripts hurting the
  process and the budgets stop them stalling it; neither validates *what* a property
  write does. The typed property table and the replication filter do that.
- **Server = same code.** A headless server runs the same node in `Server` mode with
  tighter budgets; the ENet transport carries the same `Change` / `RemoteMsg` streams
  `LocalSession` hands across in-process in Play Solo. Not yet: plausibility checks
  on a client's reported speed (Roblox does not either; a server script can).

### `PulseBlockzChain` (static)
Resolving `pblockz://` asset URIs against `AssetStore` (see `../ASSETS.md`).

| Method | Notes |
|---|---|
| `parse_asset_uri(uri) -> Dictionary`, `format_asset_uri(parts)` | the `pblockz://` scheme |
| `read_calldata(blob_id)`, `read_range_calldata(...)`, `chunks_of_calldata(...)`, `blob_calldata(...)`, `blob_of_calldata(hash)` | `data` for an `eth_call` to AssetStore |
| `decode_bytes(hex)`, `decode_addresses(hex)`, `decode_uint(hex)`, `decode_blob(hex)` | the results |
| `assemble_chunks(codes_hex)`, `verify(data, hash)`, `content_hash_of(data)` | from `eth_getCode` to verified bytes |

`gdextension/demo/ChainAssets.gd` puts them together over HTTPRequest: `await ChainAssets.fetch(uri)`.

## demo2: the town, and chain data reaching a creator's script

`gdextension/demo2` is a second Godot project: a town square with a fountain,
shops and a bank you can walk into. Unlike the demo next door it has no falling brick,
no runaway script and no NPCs on patrol.

Seven people to talk to (`scripts/src/map/*.model.json`, each a Model of five parts with a
`ProximityPrompt` in its Torso; `scripts/src/server/*.server.luau` is what each says), each with
their own corner of the chain:

- **Penn**, the teller, north in the bank: your wallet. What you are holding, what you own, your
  address, and sending some of it to somebody else -- PLS as a plain transfer, a token through its
  own `transfer(address,uint256)` -- or a gift to the town's donations address. Every one of those
  is a write your own wallet is asked to sign. - **Finch**, at the stall on the west side: the shop.
  Everything on the shelf was registered on chain by whoever made it; Finch sets no prices, takes no
  cut, and nothing is bought. What gates the shelf is the rung of the holdings ladder your address
  stands on, and taking a thing is one `Inventory.add(bytes32, uint8)` your wallet signs -- the
  item's own content hash and its tier -- costing the gas and nothing else. - **Rebh**, at the
  trading house on the east side: PulseX, as deployed on this testnet (chain 943; router, factory
  and WPLS at their own addresses in `gdextension/host/Wallet.gd`). A quote is `getAmountsOut` on
  that router; a swap and adding or removing liquidity are the router's own calls, each signed by
  your wallet; your positions are read off the pairs; a token pasted in by address joins the list,
  kept on your machine. - **Doug**, at the far end of the same counter: the pair screener. The
  server fetches the town's watched pools off DexScreener (PulseChain mainnet) every two minutes,
  and everybody at the board reads the same rows, sorted on liquidity; the search box filters those
  as you type and, after a pause, asks DexScreener for the rest of the market from your own machine
  (`gdextension/demo2/scripts/src/shared/Market.luau`, answered by your `gdextension/demo2/host/Market.gd`) -- never from the town's server. -
  **Unger**, in the Hall of Records to the south: the block explorer, read aloud. Height and gas,
  what is on record for your address, what you have done (the explorer's API, not the node), which
  contracts the town uses and how to check any of it yourself -- and a page can be taken and carried
  as an Engram, a link you hold for as long as you are in town; nothing about that touches the
  chain. - **Kara**, the tailor in the south-west: a drawing table. Draw on a 20×26 grid, try it on
  as often as you like -- that renders locally and touches no chain -- then publish: the drawing,
  the cape, its shelf picture and its description go to `AssetStore` as data, and one
  `Inventory.keep(bytes32)` records the cape as yours, with no token anywhere. - **Funmaster Mike**,
  in the square: whether you want to be hit. PvP is on for everyone until you say otherwise, and the
  flag is `Duelling.setFighting(bool)`, written by your wallet so every town that reads the contract
  remembers it. Ranked duels (1v1, 2v2), the leaderboard and space fishing are his too; a duel's
  result lives on the server, and goes on chain (`DuelRecords`) only when one side sends it carrying
  the other side's EIP-712 signature.

Press **E** at any of them, and **I** anywhere for the wardrobe -- everything you own,
with a button to put it on or take it off. A dialog closes when you walk away from
whoever opened it (16 studs, past the prompt's own 10). All of it is live PulseChain testnet v4,
and the host starts reading it at load, so by the time you have crossed the square
your holdings, the shelf and every model you own are already in.

```
godot --path gdextension/demo2                                          # play it
godot --headless --path gdextension/demo2 -s res://tests/town_test.gd   # offline check
godot --headless --path gdextension/demo2 -s res://tests/tx_test.gd     # the signer, against ethers.js vectors
godot --headless --path gdextension/demo2 -s res://tests/security_test.gd    # the holes that were closed
godot --headless --path gdextension/demo2 -s res://tests/paint_test.gd       # the drawing table's preview
godot --headless --path gdextension/demo2 -s res://tests/buy_live.gd -- 16   # takes #16 off the shelf for real (one Inventory.add); needs a funded key
godot --headless --path gdextension/demo2 -s res://tests/pulsex_live.gd     # quotes, routes, pool shares against the live testnet router; reads only
godot --headless --path gdextension/demo2 -s res://tests/tailor_live.gd      # draws a cape and publishes it for real
godot --headless --path gdextension/demo2 -s res://tests/abi_test.gd         # ABI encoding, against ethers.js
godot --headless --path gdextension/demo2 -s res://tests/live_probe.gd       # what the host publishes, and how long it took
```

The first run in a fresh checkout needs `--import` once so Godot registers the
extension. `scons` writes the DLL into `demo2/bin` along with `demo/bin` and
`studio/bin`.

**Why E is free here.** In the Luau demo next door, `ContextActionService`
binds E to drop a brick, and an action is offered a key *before* any
ProximityPrompt, so the teller never saw it. demo2 binds E only while a dialog is open
(`Dialog.client.luau`: it closes the window), and unbinds it with the window.

**The boundary.** A creator's script reaches the network only where Roblox's would:
`HttpService:GetAsync` / `PostAsync` / `RequestAsync` are server-only and off until the
host opts in (`http_enabled`; the runtime owns no socket, the host makes the request and
wakes the script). The town's own `Main.tscn` turns it on for its server -- Doug's board is
`HttpService` reading DexScreener -- and `Main.gd` turns it off again on a player's machine
running a place fetched by hash, since in Play Solo that place's server half runs there.
A script never reads the chain itself. A server script asks its host through
`ServerStorage.Chain` (`Ledger.request` / `Ledger.ask` in `Ledger.luau`: a `Request`
attribute holding the numbered asks still open, a `Result` attribute for each answer),
and `gdextension/host/Wallet.gd` does the JSON-RPC with `HTTPRequest` (`eth_call`, with
selectors from `PulseBlockzCrypto.keccak256_hex` and bytes decoded by `PulseBlockzChain`);
`Chain.server.luau` reads the catalogue and each player's holdings that way and hands every
desk `Ledger.data(player)`. A player's balances, tokens, pools and history are read by their
own machine's `Wallet.gd` and published client-side as the `Wallet` attribute on
`ReplicatedStorage.Chain`, which `Mine.client.luau` passes to the server for that player's
panels alone -- shown, never believed. A desk with nothing yet says the line is down rather
than failing.

**Buying, the same way round.** A script cannot sign a transaction and has no key
to sign one with; a hosted server's host loads none either. Pressing Browse and taking a thing
sends the shop's remote `buy` with the listing's id, and the shop asks *your* wallet
(`Ledger.askFor`): `{action = "write", to = <Inventory>, fn = "add(bytes32,uint8)", args =
{hash, tier}, note = "Take X off the shelf"}`. The ask travels over `WalletRemote` to your
client (`WalletBridge.client.luau`), which writes it into the `AskWallet` attribute on your
own `ReplicatedStorage.Chain` (`OwnWallet.luau`); your machine's `Wallet.gd` reads that
attribute, encodes the call itself, **asks you** -- showing the address, the function, the
arguments, the value and the fee as it read them, with the place's `note` labelled as the
place's words -- signs only then, writes the outcome back as `WalletResult`, and the
answer rides the remote back to the desk, which reads it out. The vocabulary the wallet
takes is its own: `read`, `write`, `store`, `fetch`, `who`, `sign_typed` and the PulseX
actions (`quote`, `swap`, `pool_quote`, `add_liquidity`, `remove_liquidity`, `positions`,
`add_token`, `forget_token`); anything else is dropped without a reply.

Any script may ask. The confirmation is drawn by the host out of what the host read,
so a hostile creator's script can put a dialog in front of you but cannot spend
anything. Turn `Wallet.confirm_purchases` off only in tests.

The signing itself is `PulseBlockzCrypto.sign_digest` -- one call into the
GDExtension for the elliptic curve, wrapped by `gdextension/host/Tx.gd`, which does RLP and
the EIP-1559 envelope in GDScript. `gdextension/demo2/tests/tx_test.gd`
compares a whole signed transaction byte for byte with what ethers.js produces for
the same fields and key.

The key is read from `PBLOCKZ_PLAYER_KEY`, then `user://player.key` (or a
`user://watch.address` to read for and sign nothing), then, in a dev build, the
`CREATOR_KEY` in the repo's gitignored `.env.testnet` -- never from anywhere inside
`res://`, so it is not part of the game's files and not somewhere a script could reach
even if the sandbox were bypassed; a hosted server (`Server` mode) loads none at all.
With no key present everything else still works and a desk asked to sign says you have
no wallet signed in that can sign.

**Owning things without tokens.** `contracts/Inventory.sol` is `address → content
hash → count` and nothing else: no ERC standard, no ids to mint, no registry, no
marketplace. An item's identity is the hash of its own data, and `AssetStore.blobOf(hash)`
turns a bare hash back into the blob, which records its own mime, so there is nothing to
look an item up in. Metadata documents name their assets by that bare hash
(`"model": "0x<keccak>"`), not by a full `pblockz://` link.

**The ladder is enforced on chain, not here.** What you may claim depends on how much
PLS you hold, and `Inventory.add` checks `msg.sender.balance` itself; a client check
would be skipped by anyone running their own. A contract cannot read an item's metadata
to learn its tier, so the tier is folded into the identity:
`item = keccak256(dataHash, tier)`. Claiming a lower tier to duck the threshold produces
a *different* id, which is not the id anything in the game knows that thing by. The
client reads the rungs from `ladder()` rather than keeping a copy; only the names
(Plankton to Humpback) live in the client.

That is a floor on who may claim what, not a ceiling on how many exist: anyone high
enough who knows a hash can record that they hold one too.

**Talking to the explorer.** Unger's pages come from `api.scan.v4.testnet.pulsechain.com`,
the same Blockscout API `scan.pulsechain.com` runs on — not the node. An account's history is
not something JSON-RPC gives you without walking every block yourself. The machine that wants
a page fetches it (`gdextension/demo2/scripts/src/shared/Scan.luau`, answered by that
machine's `gdextension/host/Scan.gd`), never the town's server: reading an explorer needs no
key, and a hosted town's operator should not be fetching every visitor's browsing on their
own bandwidth.

**Using PulseX rather than replacing it.** Rebh's desk is `gdextension/host/Pulsex.gd` over
PulseX's own testnet v4 deployment (chain 943; the router, factory and WPLS addresses in
`gdextension/host/Wallet.gd`, each starting token checked against that factory for a real
WPLS pool -- a decoy "Test Incentive" shares INC's ticker there with no pair). A quote is
`getAmountsOut` on that router, the same call their front end makes; a swap is the router's
`swapExact…SupportingFeeOnTransferTokens` entry for the pair of coin and token in hand,
liquidity `addLiquidityETH` / `addLiquidity` and `removeLiquidityETH…` / `removeLiquidity`,
so a trade settles with the same testnet PLS the shelf runs on.
`gdextension/demo2/tests/pulsex_live.gd` reads against the live deployment to prove the
shipped addresses hold code and the router answers.

**An experience is a hash.** No script source crosses a socket:
`Replicator::sendsProperty` (`src/rbx_net.cpp`) returns `inProcess_`, so a `NoReplicate`
property — `Source` among them — is sent only when the client is the same program as the
server, and `sendContentHolder` is a no-op, so no asset bytes cross either. A joining
client is told the published place's uri in the Welcome packet (`NetPacket::place`, wire
protocol 5), fetches that place from the chain, checks every file against its own hash,
and loads the code over the instances the server sent (`Experience.mount(uri,
code_only)`; `ScriptSync.load_place()` loads `.luau` only). It becomes the local player
only after that, because that is what copies StarterPlayerScripts into PlayerScripts.
The exception is in-process — Studio's play mode, Play Solo and `LocalSession` are one
program with the same files and no socket.

`scripts/publish-experience.js` resolves a Rojo project into a flat list of (instance
path, content hash), publishes every file, and prints one hash that names the whole
thing. `Experience.gd` fetches a manifest, shows what it is *before* running anything,
and mounts only after every file has arrived and matched its own hash. A curation list
then has an exact thing to vouch for or block, and "this changed since you last played"
is checkable. The join screen is drawn by the host: a screen asking whether you trust
some code cannot be drawn by the code it is asking about.

```
node scripts/publish-experience.js luau/gdextension/demo2/scripts "PulseBlockz Town"
godot --headless --path gdextension/demo2 -s res://tests/experience_live.gd
```

pBlockz Home is published this way: 114 files, 1,180,368 bytes, one hash —
`pblockz://25accbadcd14c6df55859f5c39e4eef6fc53df4a8f33421e3166e25f7bc4db77`
(`experiences.943.json`).

**Reading the chain quickly.** The first read of a full shop took **two minutes**,
and none of it was the client's arithmetic: this RPC answers a batched `eth_call`
strictly in order, so forty-six of them cost forty-six round trips. Two changes took
it to about **thirteen seconds** warm, twenty-six cold:

- `ChainAssets.prefetch(uris)` asks for every asset a page needs in one batch instead
  of one at a time, verifying each against its hash exactly as the single fetch does.
- Both batchers split across a handful of `HTTPRequest` nodes and start every request
  before awaiting any of them. Results come back through one-shot callbacks rather
  than by awaiting each lane in turn — a signal is not queued, so a lane that finished
  before its await was reached would hang that await forever.

**The wardrobe.** `Wardrobe.server.luau` decides what you are wearing, from what
the chain says you hold: the client can ask for anything and only what you own is
honoured. Models arrive as `ReplicatedStorage.OnChain.<Name>` and go on with
`Humanoid:AddAccessory`, the same call any Roblox place would use.
