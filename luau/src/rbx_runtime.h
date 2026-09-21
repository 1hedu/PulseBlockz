// The scripting runtime: one DataModel, one Luau sandbox and a scheduler, presenting the
// Roblox API to scripts. One Runtime per context -- the server, and each client.
// No thread affinity, no locking: drive one Runtime from one thread.
#pragma once
#include "luau_sandbox.h"
#include "rbx_instance.h"
#include "rbx_net.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

namespace pulseblockz::rbx {

class Runtime {
public:
    struct Options {
        Budget budget;
        bool isServer = true;
        bool isStudio = true;         // RunService:IsStudio()
        // Off: the tree is built, Sources and all, but no script starts (an edit world)
        bool runScripts = true;
        std::string dataStorePath;    // the file DataStoreService persists to; "" keeps it in memory
        // As Roblox gates HttpService: off unless the host enables it, never on a client
        bool httpEnabled = false;
    };
    /// One part going into a boolean, world space and self-contained: operands need not be
    /// in the Workspace, so the host never looks the part up.
    struct SolidOperand {
        std::string className;                          // Part, WedgePart, CornerWedgePart, MeshPart, UnionOperation, NegateOperation
        std::string shape;                              // a Part's Shape: Block, Ball, Cylinder, Wedge, CornerWedge
        Vec3 size, pos, orient;                         // orient: Orientation, degrees
        std::string meshData;                           // a PartOperation or a generated MeshPart: its geometry
        std::string meshId;                             // a MeshPart: its file
        Col3 color{1, 1, 1};                            // Color: the faces it contributes keep it
        bool subtract = false;                          // cut rather than added, per operand: a Subtract, or a negated part in a Union
    };
    /// A boolean a script asked for. operands[0] is the part the method was called on: the
    /// result is built in its frame and takes its look.
    struct SolidRequest {
        enum Op { Union, Subtract, Intersect } op = Union;
        std::vector<SolidOperand> operands;
        bool splitApart = false;                        // GeometryService: one part per disconnected piece
        // GeometryService keeps the main part's space (the result's PVInstance.Origin);
        // BasePart's methods recentre on the result's own bounds
        bool keepOrigin = false;
    };
    /// What the host sends back: geometry in the result's own space (recentred on its
    /// bounds) and where that space sits in the world.
    struct SolidResult {
        struct Piece { std::string meshData; Vec3 size, pos, orient; int triangles = 0; };
        bool ok = false;
        std::string error;
        std::vector<Piece> pieces;                      // one, unless splitApart
    };
    /// AssetService:CreateMeshPartAsync: the mesh, as the host found it.
    struct MeshResult {
        bool ok = false;
        std::string error;
        Vec3 size;                                      // the mesh's own bounds: the part's Size and MeshSize
        std::string geometry;                           // when asked for: EditableMeshData bytes
    };
    /// AssetService:CreateEditableImageAsync: the image, decoded by the host.
    struct ImageResult {
        bool ok = false;
        std::string error;
        int w = 0, h = 0;
        std::string rgba;                               // w * h * 4 bytes
    };
    struct Callbacks {
        // `script`: the full name of the script whose thread produced it
        // ("ServerScriptService.Main"), or "" for the host
        std::function<void(const std::string& script, const std::string& text)> print;
        std::function<void(const std::string& script, const std::string& text)> warn;
        std::function<void(const std::string& script, const std::string& error)> error;   // uncaught
        std::function<void(const std::string& script, const std::string& reason)> killed; // budget
        // HttpService: the host owns the socket, and answers through deliverHttp with this `id`
        std::function<void(uint64_t id, const std::string& method, const std::string& url,
                           const std::string& body, const std::string& contentType,
                           const std::vector<std::pair<std::string, std::string>>& headers)> httpRequest;
        // BasePart:UnionAsync / SubtractAsync / IntersectAsync and GeometryService's three:
        // the host meshes the boolean and answers through deliverSolid with this `id`
        std::function<void(uint64_t id, const SolidRequest& request)> solidRequest;
        // AssetService:CreateMeshPartAsync; answered through deliverMesh
        std::function<void(uint64_t id, const std::string& uri, bool geometry)> meshRequest;
        std::function<void(uint64_t id, const std::string& uri)> imageRequest;
        // ContentProvider:PreloadAsync; answered through deliverPreload
        std::function<void(uint64_t id, const std::string& uri)> preloadRequest;
    };
    /// A line of chat for the engine to draw (client): a player's message, a system message
    /// in `color`, or Chat:Chat's bubble over `part`.
    struct ChatLine {
        int64_t speaker = 0;      // the Player, 0 for a system message or a bubble
        int64_t part = 0;         // bubble only: the part it floats over
        std::string name, text;   // name: the speaker's DisplayName (or Name)
        std::string rich;         // the window's line as Roblox rich text (PrefixText .. Text); empty: draw name/text
        Col3 color{1, 1, 1};      // a player's line: the name's colour; a bubble: its colour
        bool system = false;
        // Also floats over the speaker's head; a whisper does not
        bool bubble = true;
    };
    /// StarterGui:SetCore("SendNotification", ...): a card in the corner (client).
    struct Notification {
        std::string title, text, icon, button1, button2;
        double duration = 5;          // seconds, Roblox's default
        int64_t callback = 0;         // a BindableFunction, invoked with the pressed button's text
    };
    struct Stats {
        int scriptsStarted = 0;
        int threadsLive = 0;      // coroutines the scheduler is tracking
        int resumes = 0;          // total resumptions
        int errors = 0;
        int kills = 0;
        int skipped = 0;          // resumptions refused by the frame budget
        double lastStepMillis = 0;
        // Bytes the sandbox allocator holds now and at its peak (luau_sandbox.cpp: alloc)
        size_t memoryNow = 0;
        size_t memoryPeak = 0;
    };

    Runtime(Options opts, Callbacks cb);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    DataModel& dataModel();
    Sandbox& sandbox();
    const Options& options() const;
    const Stats& stats() const;

    /// Start every Script/LocalScript that belongs in this context and has not started yet
    /// (placement rules in rbx_instance.h); returns how many. One parented into a running
    /// container later starts itself at the next step.
    int startScripts();

    /// Advance simulated time by dt seconds: Stepped, sleepers, deferred work, tweens,
    /// Heartbeat -- all under the sandbox's frame budget.
    void step(double dt);
    double now() const;

    /// The tree mutations since the last call, in order.
    std::vector<Change> takeChanges();

    /// Replace a script's Source. A running Script/LocalScript restarts from the top; a
    /// ModuleScript drops the whole require cache, so the next require sees the new code --
    /// a script already holding the old table keeps it.
    void reloadSource(Instance& script, const std::string& source);

    /// Route a host-side error (a bad file path...) through the error callback, so it lands
    /// in the console with script output.
    void reportError(const std::string& script, const std::string& error);
    void reportWarn(const std::string& script, const std::string& text);

    /// ---- the debugger -----------------------------------------------------------
    /// Set or clear a breakpoint at `line` of the script with that full name
    /// ("ServerScriptService.Main"), now and on every restart of it. Returns the line it landed
    /// on (the next with code), 0 if the script is not loaded; a stop halts the whole runtime.
    int setBreakpoint(const std::string& script, int line, bool on);
    void clearBreakpoints();
    struct DebugFrame {
        std::string function, script;                            // "(main)" / a function's name; the script's full name
        int line = 0;
        std::vector<std::pair<std::string, std::string>> locals; // locals then upvalues, each as tostring shows it
    };
    struct DebugPause { std::string script; int line = 0; std::vector<DebugFrame> frames; };
    bool debugPaused() const;
    bool takeDebugPause(DebugPause& out);                        // true once per stop
    void debugContinue();
    void debugStep(int kind);                                    // 1 into, 2 over, 3 out

    /// Write a property from the engine (physics moved a part, a player typed). Ignores
    /// ReadOnly; the change is logged with fromHost = true.
    bool hostWrite(int64_t id, const std::string& prop, const Value& v);
    /// The same write, unlogged: scripts read the new value, but no Change is recorded, so
    /// nothing is mirrored or replicated. For values each side derives itself, like a limb's pose.
    bool hostWriteQuiet(int64_t id, const std::string& prop, const Value& v);

    /// Fire `event` on `inst` from the host (Touched, PlayerAdded, InputBegan...). Handlers
    /// run immediately, under budget, on their own coroutines.
    void fireEvent(Instance& inst, const std::string& event, const std::vector<Value>& args);

    /// One input from the engine, for a client (a server ignores it). ContextActionService
    /// bindings see it first -- highest priority wins, the newest binding on a tie; one that
    /// does not return Enum.ContextActionResult.Pass sinks it. UserInputService.InputBegan /
    /// InputChanged / InputEnded then fire with (InputObject, gameProcessedEvent); Space also
    /// fires JumpRequest.
    struct UserInput {
        std::string type;             // Enum.UserInputType item: "Keyboard", "MouseButton1", "MouseMovement", "MouseWheel"...
        std::string key = "Unknown";  // Enum.KeyCode item for Keyboard
        std::string state = "Begin";  // Enum.UserInputState item: "Begin", "Change", "End"
        Vec3 position, delta;         // mouse position in pixels (z: wheel direction), motion since the last event
        bool processed = false;       // the engine's GUI took it first: InputBegan's gameProcessedEvent
    };
    void input(const UserInput& in);
    /// Whether the input just given was sunk -- the engine's GUI, or a ContextActionService
    /// action that did not Pass. This is gameProcessedEvent; the default camera skips a
    /// claimed drag, as Roblox's does.
    bool lastInputSunk() const;
    /// Client: UserInputService.WindowFocused / WindowFocusReleased.
    void windowFocus(bool focused);

    /// ---- the Studio's plugins -------------------------------------------------
    /// Scripts under PluginDebugService run even in an edit world, each with a `plugin`
    /// global (Roblox's local plugins); what they build comes out through the takes below.
    struct PluginButton { int64_t id = 0; std::string plugin, toolbar, buttonId, tooltip, icon, text; bool active = false, enabled = true; };
    struct PluginSetting { std::string plugin, key, json; };
    void addPlugin(const std::string& name, const std::string& source);   // a Script of that name and source under PluginDebugService
    void addPluginFile(const std::string& file, const std::string& source);   // a plugin as a file (a .rbxmx / .model.json of scripts) placed under PluginDebugService
    void unloadPlugins();                                                 // fires Unloading, destroys the scripts, drops the toolbars
    void setPluginSetting(const std::string& plugin, const std::string& key, const std::string& json);   // what GetSetting will find
    void pluginButtonClick(int64_t buttonId);
    void setSelection(const std::vector<int64_t>& ids);                   // the Studio's: Selection:Get(), SelectionChanged
    void pluginInput(const UserInput& in, Vec3 rayOrigin, Vec3 rayDirection);   // the pointer over the view while a plugin is active: its PluginMouse
    bool takePluginButtons(std::vector<PluginButton>& out);               // true when they changed
    bool takeSelectionRequest(std::vector<int64_t>& out);                 // true when a plugin set the selection
    std::vector<PluginSetting> takePluginSettings();
    bool takePluginActive(std::string& plugin, bool& active, bool& exclusive);
    std::vector<std::pair<int64_t, int>> takeOpenScripts();
    std::vector<std::string> takePluginWaypoints();                       // ChangeHistoryService waypoints: the Studio closes an undo entry at each
    /// Team Create: replicate every service and property to the clients (other Studios),
    /// not the cut a game's clients get. Set it before setReplicating.
    void setReplicateAll(bool b);
    /// The clients share this process (Studio play mode, a test): Replicator::setInProcess.
    void setInProcess(bool b);
    /// EditableImages a script drew into this frame: the whole bitmap of each, once.
    struct ImageFrame { int64_t id = 0; int w = 0, h = 0; std::string rgba; };
    std::vector<ImageFrame> takeEditableImages();
    /// EditableMeshes changed since the last take, as triangles corner by corner (a destroyed one: none).
    struct MeshFrame { int64_t id = 0; std::vector<float> pos, nrm, uv, rgba; };
    std::vector<MeshFrame> takeEditableMeshes();

    /// The engine's GUI reporting the pointer on a GuiObject (client). `phase` is "Down",
    /// "Up", "Click", "Enter" or "Leave"; `button` is 1 or 2; `pos` is pixels. Any GuiObject
    /// gets MouseEnter(x, y) / MouseLeave(x, y) and InputBegan / InputEnded; only a GuiButton
    /// gets MouseButton1Down(x, y) / Up / Click() and Activated(inputObject, clickCount).
    void guiInput(int64_t id, const std::string& phase, int button, Vec2 pos);

    /// A TextBox's engine-side box (client). `phase` is "Focus", "Change" (`text` and
    /// `caret`, the CursorPosition, are what it holds now, written as the engine's so the
    /// mirror does not echo them) or "Blur" (`enter` when Enter did it). Fires Focused /
    /// FocusLost(enterPressed) and UserInputService.TextBoxFocused / TextBoxFocusReleased.
    void guiText(int64_t id, const std::string& phase, const std::string& text = "", int caret = -1, bool enter = false);

    /// The engine's escape menu (client): GuiService.MenuIsOpen, MenuOpened / MenuClosed.
    void menu(bool open);
    void savedQuality(int level);   // Graphics Quality from the menu: 0 Automatic, 1..10 (UserGameSettings.SavedQualityLevel)
    /// Its Reset Character button (client): the server zeroes the character's Humanoid.Health,
    /// unless SetCore("ResetButtonCallback", ...) gave a BindableEvent to fire instead, or false.
    void resetCharacter();

    /// Hotbar slot 0-8 clicked (client; the slots come from the Tools' HotbarSlot): equips
    /// that tool, or unequips it if it is already in hand.
    void hotbarSelect(int slot);

    /// The local player sent a chat message (client): it goes to the server, which fires
    /// Player.Chatted and sends it on to every client. Trimmed, empty ignored, 200 characters.
    void chat(const std::string& text);
    /// Chat lines that arrived since the last call, for the engine to draw.
    std::vector<ChatLine> takeChat();
    /// Notifications sent since the last call, for the engine to draw.
    std::vector<Notification> takeNotifications();
    /// A notification's button was pressed: its Callback is invoked with the button's text.
    void notificationButton(int64_t callback, const std::string& text);
    /// Terrain:FillBlock / FillBall / FillCylinder / FillWedge / FillRegion / ReplaceMaterial /
    /// WriteVoxels / Clear. The voxel field lives on the host with its mesh and collision, so a
    /// script's call goes out as an op and the host writes Heights back.
    struct TerrainOp {
        int kind = 0;                                   // 0 block, 1 ball, 2 clear, 3 cylinder, 4 wedge, 5 replace material, 6 write voxels
        Vec3 pos;                                       // the shape's centre
        float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};     // the shape's rotation, row-major
        Vec3 size;                                      // a block's or wedge's; a cylinder's height in y
        float radius = 0;
        int material = 1280;                            // the Enum.Material value
        int fromMaterial = 1792;                        // replace material: what gives way
        int nx = 0, ny = 0, nz = 0;                     // write voxels: the region in 4-stud cells from pos (its low corner)
        std::vector<uint8_t> occupancy;                 // write voxels: 0..255, x fastest, then y, then z
        std::vector<uint16_t> materials;                // write voxels: Enum.Material values, the same order
    };
    void queueTerrainOp(const TerrainOp& op);
    std::vector<TerrainOp> takeTerrainOps();
    /// BasePart:ApplyImpulse / ApplyImpulseAtPosition / ApplyAngularImpulse: the bodies are
    /// the host's, so these go out with the frame and land on the part's assembly there.
    struct Impulse {
        int64_t part = 0;
        int kind = 0;                                   // 0 at the centre of mass, 1 at `at`, 2 angular
        Vec3 v;                                         // the impulse (mass x velocity), or the angular one
        Vec3 at;                                        // kind 1: the world point it is applied at
    };
    std::vector<Impulse> takeImpulses();

    /// Run a chunk as a throwaway script with `script` = nil; returns "" or the error.
    std::string runChunk(const std::string& chunkname, const std::string& source);

    /// tostring() of an expression's first result; `error` is set on failure.
    std::string eval(const std::string& expr, std::string* error = nullptr);

    /// Server: create Players.<name>, fire PlayerAdded, spawn a character. Client: the Player
    /// has usually arrived by replication already; it becomes Players.LocalPlayer and gets
    /// its PlayerScripts/PlayerGui from StarterPlayerScripts/StarterGui. Returns the Player.
    // playerId: on a client, which replicated Player this is (the Welcome says which) -- names are not unique
    Instance* addPlayer(const std::string& name, int64_t userId, int64_t playerId = 0);
    void removePlayer(Instance* player);
    Instance* localPlayer() const;

    // ---- replication (rbx_net.h) ----
    /// Server: keep the outbound stream. Off by default, since unread it only piles up.
    void setReplicating(bool on);
    /// Server: everything a joining client needs; call between frames.
    std::vector<Change> replicationJoin();
    /// Server: what every connected client must apply since the last call.
    std::vector<Change> takeReplication();
    /// Client: apply what the server sent. Fires PlayerAdded / CharacterAdded for replicated
    /// players, and restarts a LocalScript whose Source changed.
    void applyReplication(const std::vector<Change>& changes);

    // ---- remotes ----
    /// FireServer / FireClient / Invoke* calls made by scripts since the last call.
    std::vector<RemoteMsg> takeRemotes();
    /// Deliver one from the other side. Server: m.player is the sender's Player.
    void deliverRemote(const RemoteMsg& m);
    /// The host finishing a boolean a script is parked on.
    void deliverSolid(uint64_t id, const SolidResult& result);
    /// The host answering CreateMeshPartAsync.
    void deliverMesh(uint64_t id, const MeshResult& result);
    /// The host answering CreateEditableImageAsync.
    void deliverImage(uint64_t id, const ImageResult& result);
    /// The host answering ContentProvider:PreloadAsync, one content at a time.
    void deliverPreload(uint64_t id, bool ok);
    /// The host answering an HttpService request under the `id` it was given. `ok` false resumes
    /// the parked script with an error, as on Roblox; a failing HTTP status is still a success.
    void deliverHttp(uint64_t id, bool ok, int status, const std::string& statusText,
                     const std::string& body,
                     const std::vector<std::pair<std::string, std::string>>& headers);

    static Runtime* from(lua_State* L);

    struct Impl;
    Impl& impl() { return *impl_; }

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace pulseblockz::rbx
