# Roblox parity: what is left, and what each one needs

`scripts/parity-audit.py` finds properties a creator can set that nothing anywhere reads.
A missing API errors the first time you call it; a present one that silently does nothing
leaves you unable to tell a misuse from a gap.

The count was **132**. It is now **32**, and this file is what those 32 are.

Each one is either a feature this engine does not have yet, a case where matching Roblox
means doing nothing, or a case where two readings of the semantics differ materially --
the three sections below, in that order.

Run the audit any time:

```
cd luau && python ../scripts/parity-audit.py
```

---

## 1. The property is fine; the feature does not exist here

The property is one line of a system that has not been built; building the system is the work,
not wiring the property.

| Properties | The feature | What it needs |
|---|---|---|
| `WrapLayer.Order`, `.Puffiness`, `.ReferenceMeshId`, `BaseWrap.CageMeshId` | **Layered clothing.** A garment deforms to fit whatever body it is put on, and several garments stack in order without passing through each other. | Cage meshes: an inner and outer cage per garment and per body, and a solver that maps one onto the other. The largest item here, and the only one that would change how the wardrobe works. |
| `LocalizationTable.SourceLocaleId`, `TextChatMessage.Translation`, `TextChatMessageProperties.Translation` | **Localisation.** A place ships tables of translated strings and the engine swaps GUI text and chat for the player's locale. | `LocalizationService:GetTranslatorForPlayer`, a translation lookup, and the automatic text substitution. Without the lookup, `SourceLocaleId` names the language of strings nothing is translating. |
| `TextChatCommand.AutocompleteVisible` | **Chat command autocomplete.** Typing `/` offers the commands a place registered. | The suggestion list itself. The commands work; nothing offers them. |
| `PathfindingLink.IsBidirectional` | **Pathfinding links.** A jump, a ladder or a teleport as an edge in the navmesh, so a path may cross a gap. | Links in the graph the pathfinder walks. It has none, so there is no edge to be one-way. |
| `SpringConstraint.Coils`, `.Radius`, `BallSocketConstraint.Radius`, `HingeConstraint.Radius`, `CylindricalConstraint.RotationAxisVisible` | **Constraint gizmos.** How a constraint is DRAWN when Studio shows constraint details. | A gizmo layer in the Studio viewport. A constraint here is a physics joint and has no visual at all; these five say what shape it would be if it did. |
| `Plugin.CollisionEnabled`, `.GridSize`, `PluginButton.ClickableWhenViewportHidden` | **Studio chrome.** Drag-to-grid, collision while dragging, and a toolbar button that stays live with the viewport hidden. | The Studio's own drag and toolbar behaviour. |
| `PartOperation.Operands` | **Separate.** Taking a union back apart into the parts it was made from. | The Studio tool. The property already holds what it needs to; nothing asks. |
| `PartOperation.UsePartColor` | **Per-operand colour in a union.** A union drawn in the colours of the parts it was made from rather than one colour. | Vertex colours in the baked union format. `PBOP` v1 carries position and normal per vertex and nothing else, so there is no per-operand colour to use; it needs a v2 and a publisher that writes one. |
| `Player.CharacterAppearanceId` | **Appearance loading.** Whose look a character wears, by account. | An appearance service. Characters here are built from a rig and what the chain says you own; there is no account whose look could be fetched. |
| `Pose.MaskWeight`, `KeyframeSequence.AuthoredHipHeight` | **Pose blending.** Weighted blending of poses across tracks, and scaling root motion for a body of a different height. | Animation runs off tracks and keyframes; `Pose` instances are kept and never blended. |
| `SurfaceGui.ToolPunchThroughDistance` | **Tool rays through a surface GUI.** A tool used close to a SurfaceGui hits what is behind it rather than the GUI. | The tool's own ray against the GUI. That hit-test path does not exist: a SurfaceGui here is either hit or not. |

## 2. Matching Roblox means doing nothing

| Properties | Why |
|---|---|
| `BevelMesh.Bulge`, `.Roundness` | Deprecated and non-functional in Roblox too: a `BevelMesh` with `Bulge = 1` looks exactly like one without, there and here. Implementing an effect would be a *divergence*, not parity. |

## 3. Two readings, and a guess would be worse than nothing

| Properties | The question |
|---|---|
| `BasePart.TopParamA` and its five siblings | They feed the legacy surface-input system. `SurfaceInput = Constant` uses `ParamB` as the value, which is implemented and drives the legacy `RotateV` motors. `Sin` uses both, and the two plausible readings -- amplitude and rate, or rate and amplitude -- give a motor running at different speeds. **What would settle it:** one legacy place with a `Sin` surface, run in Roblox, with the resulting speed measured. |
| `Atmosphere.Decay` | Roblox's `Color` is the haze near you and `Decay` the colour it tends to with distance. Godot's built-in fog has one colour, which saturates at distance -- so either choice silently changes the look of every place that set the other. **What it needs:** a two-colour distance fog, which is a screen-space pass rather than a property. |

---

## What PulseBlockz adds

One thing a place can say that Roblox has no setting for, as an attribute so the same place still
runs on Roblox, where the attribute is ignored:

- **`PixelFont`** (true) on a TextLabel, TextButton or TextBox, or on anything it sits under:
  its font file is drawn hard-edged -- no antialiasing, hinting or subpixel positioning. Roblox
  draws every font smooth, and so does PulseBlockz without it.

## What was done

100 properties, in these groups. Each has a test that measures an effect rather than reading a
value back, except where the platform makes that impossible -- and where it does,
`gdextension/demo2/tests/WINDOWED.md` says what to look at instead.

| | |
|---|---|
| **Characters** | `StarterPlayer.StarterCharacter` -- a place's own body, cloned for every spawn in place of the default rig; `CharacterWalkSpeed`, `CharacterJumpPower`, `CharacterJumpHeight` and `CharacterUseJumpPower` put on each Humanoid as it spawns; `Humanoid.UseJumpPower` false, as on a new Roblox Humanoid |
| **Sky** | `SunTextureId`, `MoonTextureId`, `MoonAngularSize`, `StarCount` -- a sun, a moon and stars over the cubemap |
| **DataModel** | `JobId`, `PlaceVersion`, `PrivateServerId`, `PrivateServerOwnerId` -- which server this is, replicated so a client is told rather than guessing |
| **BasePart** | the six `*Surface` -- studs and inlets as real geometry |
| **ParticleEmitter** | `ShapeInOut`, `ShapeStyle`, `VelocityInheritance` -- and Cylinder and Disc, which drew a box |
| **GUI** | a ScrollingFrame's three grabber images and two insets, `Selectable`, `Modal`, `Selected` |
| **Surfaces** | `SurfaceAppearance` and `MaterialVariant`'s eight maps, plus `BasePart.MaterialVariant`, which was missing |
| **Legacy wear** | `Accoutrement.AttachmentPos` and its three axes -- a hat from before Attachments |
| **Terrain** | `WaterReflectance`, `WaterWaveSize`, `WaterWaveSpeed` -- water that moves |
| **Constraints** | `MaxAxesForce`, `ForceRelativeTo`, the three reaction flags, the three motor accelerations, `AngularRestitution`, `MaxFrictionTorque`, the winch's force and responsiveness, a rod's two limit angles, `DynamicRotate.BaseAngle` |
| **Sound** | all nine effect classes, including a Tremolo Godot does not ship |
| **Audio API** | `AudioPlayer`, `AudioFilter` (all twelve `AudioFilterType`s), `AudioReverb`, `AudioEmitter`, `AudioListener`, `AudioDeviceOutput` and `Wire` -- wired chains heard from an emitter or straight out, `Connected` refusing loops, `WiringChanged`, `GetGainAt`, the distance and angle curves and `GetAudibilityFor`; the listener is the camera's, as `DefaultListenerLocation` puts it |
| **Light and colour** | `SunRaysEffect.Spread`, `DataModelMesh.VertexColor` |
| **Spawning** | `SpawnLocation.Duration`, and the `ForceField` class it needed, which did not exist |
| **Input** | `ClickDetector.CursorIcon`, `ProximityPrompt.ClickablePrompt` |
| **Driving** | `VehicleSeat.HeadsUpDisplay` -- the speed readout, reading the assembly rather than the seat |

Six of those were **missing** rather than unread -- `BasePart.MaterialVariant`,
`AlignPosition.ForceRelativeTo`, `GuiButton.Selectable`'s override, the `ForceField` class,
and two halves of `UserInputService` being client-owned. A property that is not declared at
all does not show up in an audit of properties nobody reads, so this list is the floor of
what is missing, not the ceiling.
