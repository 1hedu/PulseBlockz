// Internals shared by the runtime's binding files. Not part of the public API.
#pragma once
#include "rbx_runtime.h"
#include "rbx_editable_mesh.h"
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pulseblockz::rbx {

// Userdata tags. Each tag has one shared metatable (lua_setuserdatametatable).
enum Tag { TAG_INSTANCE = 1, TAG_SIGNAL, TAG_CONNECTION, TAG_COLOR3, TAG_CFRAME, TAG_ENUM, TAG_ENUMITEM,
           TAG_TWEENINFO, TAG_RANDOM, TAG_VECTOR2, TAG_UDIM, TAG_UDIM2, TAG_RAY, TAG_RAYCASTPARAMS, TAG_BRICKCOLOR, TAG_OVERLAPPARAMS,
           TAG_FONT, TAG_PHYSICALPROPERTIES, TAG_NUMBERRANGE, TAG_NUMBERSEQUENCE, TAG_NUMBERSEQUENCEKEYPOINT, TAG_COLORSEQUENCE, TAG_COLORSEQUENCEKEYPOINT, TAG_CONTENT, TAG_OBJECT, TAG_OPAQUE, TAG_REGION3,
           TAG_FLOATCURVEKEY, TAG_ROTATIONCURVEKEY };

// ---- value types (rbx_types.cpp) ---------------------------------------------------
// Rotation is row-major: m[r*3+c]. Columns are the Right/Up/-Look vectors.
struct CFrameV {
    Vec3 p;
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    static CFrameV identity() { return {}; }
    static CFrameV fromPos(Vec3 p) { CFrameV c; c.p = p; return c; }
    static CFrameV anglesXYZ(float rx, float ry, float rz);   // CFrame.Angles
    static CFrameV anglesYXZ(float rx, float ry, float rz);   // CFrame.fromOrientation
    static CFrameV axisAngle(Vec3 axis, float angle);
    static CFrameV lookAt(Vec3 pos, Vec3 target, Vec3 up = {0, 1, 0});
    CFrameV operator*(const CFrameV& o) const;
    Vec3 operator*(Vec3 v) const;             // point to world
    Vec3 rotate(Vec3 v) const;                // vector to world
    CFrameV inverse() const;
    CFrameV lerp(const CFrameV& b, float t) const;
    Vec3 col(int c) const { return {m[c], m[3 + c], m[6 + c]}; }
    Vec3 look() const { Vec3 z = col(2); return {-z.x, -z.y, -z.z}; }
    void toEulerXYZ(float& x, float& y, float& z) const;
    void toEulerYXZ(float& x, float& y, float& z) const;
    bool operator==(const CFrameV& o) const;
};
CFrameV cframeFromPosOrient(Vec3 pos, Vec3 orientDeg);
// Every method the runtime answers on this class, sorted: an editor builds its completion list from it.
std::vector<std::string> methodNamesFor(const ClassDef& cls);
void cframeToPosOrient(const CFrameV& c, Vec3& pos, Vec3& orientDeg);

struct TweenInfoV {
    float time = 1; int style = 0; int direction = 1; int repeatCount = 0; bool reverses = false; float delay = 0;
};
float ease(int style, int direction, float t);

inline Vec3 toVec3(const float* v) { return {v[0], v[1], v[2]}; }
inline void pushVec3(lua_State* L, Vec3 v) { lua_pushvector(L, v.x, v.y, v.z); }
Vec3 checkVec3(lua_State* L, int idx);
bool isColor3(lua_State* L, int idx);
Col3 checkColor3(lua_State* L, int idx);
bool isVector2(lua_State* L, int idx);
Vec2 checkVector2(lua_State* L, int idx);
void pushVector2(lua_State* L, Vec2 v);
bool isUDim(lua_State* L, int idx);
UDim checkUDim(lua_State* L, int idx);
void pushUDim(lua_State* L, UDim d);
bool isUDim2(lua_State* L, int idx);
UDim2 checkUDim2(lua_State* L, int idx);
void pushUDim2(lua_State* L, UDim2 d);
void pushColor3(lua_State* L, Col3 c);
// BrickColor is a palette number; an unknown name gives Medium stone grey (194).
bool isBrickColor(lua_State* L, int idx);
int checkBrickColor(lua_State* L, int idx);
void pushBrickColor(lua_State* L, int number);
int brickNearest(Col3 c);
Col3 brickColor(int number);
const char* brickName(int number);
int brickNumber(const char* name);
bool isCFrame(lua_State* L, int idx);
const CFrameV& checkCFrame(lua_State* L, int idx);
void pushCFrame(lua_State* L, const CFrameV& c);
// The later of CFrameValue.Value's two halves, so Changed fires once with the whole CFrame.
extern thread_local const char* g_cframeValueLastHalf;
bool isEnumItem(lua_State* L, int idx);
void pushEnumItem(lua_State* L, const EnumDef* e, const EnumItem* i);
const EnumItem* checkEnumItem(lua_State* L, int idx, const EnumDef* e);   // accepts item, name or value
bool readEnumItem(lua_State* L, int idx, const EnumDef*& e, const EnumItem*& i);   // false unless an EnumItem userdata

// The Instance globals must be listed mutable: otherwise `workspace.Part.Position` compiles to
// an import the VM resolves once at load (the env is safe), freezing the values at startup.
inline lua_CompileOptions userCompileOptions() {
    static const char* const kMutable[] = {"game", "Game", "workspace", "Workspace", "script", "plugin", "shared", "_G",
                                           "typeof",   // ours, not the builtin: it names Vector3 and the datatypes made of tables (Content)
                                           nullptr};
    lua_CompileOptions o = {};
    o.optimizationLevel = 1;
    o.debugLevel = 2;             // local names too: the debugger shows them, as Studio's does
    o.mutableGlobals = kMutable;
    return o;
}
// The Font datatype (TextLabel.FontFace): a Value of type Font holding family, weight, italic.
// Lua builds one with Font.new, Font.fromEnum, Font.fromName or Font.fromId.
bool isFont(lua_State* L, int idx);
Value checkFont(lua_State* L, int idx);
void pushFont(lua_State* L, const Value& f);
// The Content datatype (ImageLabel.ImageContent, MeshPart.MeshContent), a Value of type
// Content. Lua builds one with Content.fromUri / fromAssetId / fromObject, or takes the empty
// Content.none. The Lua value holds its object strongly, as Roblox's does.
bool isContent(lua_State* L, int idx);
Value checkContent(lua_State* L, int idx);
void pushContent(lua_State* L, const Value& c);
bool pushOpaqueContent(lua_State* L, Instance::Ptr holder);   // a Content with SourceType Opaque, holding `holder` (a DataModelContent)
// EditableMesh's methods (rbx_editable_mesh.cpp), and its data for the drawing methods that read it.
std::vector<std::pair<const char*, lua_CFunction>> editableMeshMethods();
namespace em { EditableMeshData& meshOf(lua_State* L, Instance& mesh, const char* method); }
// Enum.Font <-> FontFace, the pairs Roblox couples: GothamBold is GothamSSm at Bold.
bool fontOfEnum(const std::string& fontName, Value& face);
std::string fontEnumOf(const Value& face);            // "Unknown" when no Enum.Font is that face
// NumberRange / NumberSequence / ColorSequence (a ParticleEmitter's Lifetime, Size, Color): Values of those types
bool isNumberRange(lua_State* L, int idx);
bool isNumberSequence(lua_State* L, int idx);
bool isColorSequence(lua_State* L, int idx);
Value checkSequence(lua_State* L, int idx);           // any of the three
// PhysicalProperties (a part's CustomPhysicalProperties): a Value of type PhysProps.
bool isPhysicalProperties(lua_State* L, int idx);
Value checkPhysicalProperties(lua_State* L, int idx);
void pushPhysicalProperties(lua_State* L, const Value& v);
void pushSequence(lua_State* L, const Value& v);      // any of the three
// FloatCurveKey / RotationCurveKey, a FloatCurve's / RotationCurve's keys: Time, Value, Interpolation
// (an Enum.KeyInterpolationMode value, Cubic unless given) and the tangents, NaN until set.
struct FloatKeyV { float time = 0, value = 0; int interp = 2; float left = NAN, right = NAN; };
struct RotKeyV { float time = 0; CFrameV value; int interp = 2; float left = NAN, right = NAN; };
bool isFloatCurveKey(lua_State* L, int idx);
FloatKeyV checkFloatCurveKey(lua_State* L, int idx);
void pushFloatCurveKey(lua_State* L, const FloatKeyV& k);
bool isRotationCurveKey(lua_State* L, int idx);
RotKeyV checkRotationCurveKey(lua_State* L, int idx);
void pushRotationCurveKey(lua_State* L, const RotKeyV& k);
// The Input Action System's keyboard: a key going down or up reaches the InputBindings (rbx_api.cpp).
void inputActionsKey(Runtime::Impl& rt, int keyCode, bool down);
struct RayV { Vec3 origin, direction; };
bool isRay(lua_State* L, int idx);
RayV checkRay(lua_State* L, int idx);
void pushRay(lua_State* L, RayV r);
// Region3: an axis-aligned box by its two corners (Terrain's regions, ExpandToGrid to its 4-stud cells).
struct Region3V { Vec3 lo, hi; };
bool isRegion3(lua_State* L, int idx);
Region3V checkRegion3(lua_State* L, int idx);
void pushRegion3(lua_State* L, Region3V r);
// workspace:Raycast against the tree's BaseParts (rbx_api.cpp): `direction`'s length is the reach.
struct RaycastFilter { std::vector<int64_t> ids; bool include = false, respectCanCollide = false, ignoreWater = true; std::string collisionGroup = "Default";
                       int maxParts = 20; bool bruteForce = false; };   // OverlapParams' extras
struct RayHit { Instance* part = nullptr; Vec3 position, normal; float distance = 0; };
bool raycastTree(DataModel& dm, Vec3 origin, Vec3 direction, const RaycastFilter* filter, RayHit& out);
// The limb group (Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg) a character part belongs to
// by name, R6 or R15; null for a part that is no limb. rbx_runtime.cpp.
const char* limbGroupOf(const std::string& partName);
// rbx_api.cpp: the pointer's ray landing, and the KeyframeSequence an Animation names.
bool mouseHit(Runtime::Impl& rt, Vec3& out);
Instance* animationSequenceOf(Runtime::Impl& rt, Instance* anim);
// The parts a query sees under the filter (CanQuery, RespectCanCollide, the Include / Exclude subtrees); `fn` returns false to stop. rbx_spatial.cpp.
void queryParts(DataModel& dm, const RaycastFilter* filter, const std::function<bool(Instance&)>& fn);
// Spatial queries and shape casts: a block is an oriented box, a ball a sphere.
struct ShapeV { CFrameV cf; Vec3 half; float radius = 0; bool sphere = false; };
ShapeV partShape(Instance& part);
ShapeV partBounds(Instance& part);                   // its world-aligned bounding box
bool shapesOverlap(const ShapeV& a, const ShapeV& b);
// `a` moved by `d` against `b`: t in units of d, the contact point on b and b's normal there. Starting inside misses.
bool sweepShape(const ShapeV& a, Vec3 d, const ShapeV& b, float& t, Vec3& pos, Vec3& n);
// The parts overlapping `s` (their bounding boxes when `bounds`), `skip` left out, up to the filter's MaxParts.
std::vector<Instance*> overlapTree(DataModel& dm, const ShapeV& s, const RaycastFilter* filter, Instance* skip, bool bounds);
bool shapecastTree(DataModel& dm, const ShapeV& s, Vec3 d, const RaycastFilter* filter, Instance* skip, RayHit& out);
// A unit ray from the camera through a viewport pixel (Camera:ViewportPointToRay).
RayV cameraRay(Instance& camera, float x, float y, float depth = 0);
// Path:ComputeAsync over the tree (rbx_pathfinding.cpp): returns the Enum.PathStatus value, `out` the waypoints on Success.
struct PathAgent { float radius = 2, height = 5; bool canJump = true, canClimb = false; float spacing = 4; std::unordered_map<std::string, float> costs; };
struct PathPoint { Vec3 pos; int action = 0; std::string label; };   // action: Enum.PathWaypointAction
int computePath(DataModel& dm, const PathAgent& agent, Vec3 start, Vec3 goal, std::vector<PathPoint>& out);
bool pathOccupied(DataModel& dm, const PathAgent& agent, Vec3 point);   // Path:CheckOccupancyAsync
struct PathState { Instance::Ptr inst; PathAgent agent; std::vector<PathPoint> points; int blockedAt = 0; };
bool isTweenInfo(lua_State* L, int idx);
TweenInfoV checkTweenInfo(lua_State* L, int idx);
void pushTweenInfo(lua_State* L, const TweenInfoV& t);
void openTypes(lua_State* L);   // installs Vector3, Color3, CFrame, Enum, TweenInfo, Random globals
int json_encode(lua_State* L);  // HttpService:JSONEncode(value)
int json_decode(lua_State* L);  // HttpService:JSONDecode(string)

// ---- scheduler ---------------------------------------------------------------------
#include <map>
#include <set>
// Who a thread runs as. A `host` chunk (command bar, debugger watch) has the Plugin capability, as Roblox's command bar does.
struct ScriptCtx { int64_t scriptId = 0; std::string name; bool host = false; };

struct Task {
    enum State { Running, Ready, Sleeping, Deferred, WaitSignal, WaitChild, Parked, Dead };
    lua_State* co = nullptr;
    int ref = LUA_NOREF;                      // keeps the coroutine alive
    std::shared_ptr<ScriptCtx> ctx;
    State state = Ready;
    double wakeAt = 0, sleepStart = 0;
    Instance::Ptr waitParent; std::string waitName; double waitDeadline = 0; bool waitWarned = false;
    bool doomed = false;                      // its script was stopped while it was running
    int pendingArgs = -1;                     // >= 0: refused by the frame budget with these args still on the stack; retry with them
    const std::function<void(lua_State*, int)>* sink = nullptr;   // callSync: gets the results when it returns without yielding
    // RequestAsync resumes with the whole response as a table, Get/PostAsync with the body string.
    bool httpWantsTable = false;
};

struct Conn { int id; int fnRef; bool once; bool connected; std::shared_ptr<ScriptCtx> ctx; };
struct Signal {
    Instance* owner = nullptr;
    std::string name;
    std::vector<Conn> conns;
    std::vector<Task*> waiters;
    int nextId = 1;
    bool hasConns() const { for (auto& c : conns) if (c.connected) return true; return !waiters.empty(); }
};

// Per-instance data the binding layer hangs on Instance::binding.
struct Callback { int ref; std::shared_ptr<ScriptCtx> ctx; };
struct InstBinding {
    std::unordered_map<std::string, std::unique_ptr<Signal>> signals;
    std::unordered_map<std::string, Callback> callbacks;   // OnInvoke / OnServerInvoke ...
};

struct InstanceUD { Instance::Ptr inst; };
struct SignalUD { Instance::Ptr owner; Signal* sig; };
struct ConnUD { Instance::Ptr owner; Signal* sig; int id; };

struct Tween {
    Instance::Ptr obj; Instance::Ptr tween;       // `tween` is the Tween instance scripts hold
    TweenInfoV info;
    struct Prop { std::string name, cfPos, cfOri; Value from, to; CFrameV cfFrom, cfTo; bool isCFrame = false; };
    std::vector<Prop> props;
    double elapsed = 0; int cycle = 0; bool playing = false; bool reversed = false; bool done = false;
};

// Whether an Audio API instance has a pin of that name (rbx_runtime.cpp).
bool audioHasPin(const Instance& i, const std::string& pin, bool output);
std::vector<std::string> audioPins(const Instance& i, bool output);
std::vector<std::string> audioChannelPins(const std::string& layout);
bool audioEffectClass(const std::string& className);

struct Runtime::Impl {
    Runtime* self;
    Options opts;
    Callbacks cb;
    bool tearingDown = false;     // set by ~Impl: GC destructors must not touch the queues below
    DataModel dm;                 // declared before sb: Lua dies first, releasing every Instance::Ptr it holds
    Sandbox sb;
    lua_State* L;                 // main state
    Stats stats;
    // Milliseconds each script spent in this step, by label. Only the outermost resume of a
    // task counts: a signal fired inside it, or a task.spawn, is already that script's time.
    std::unordered_map<std::string, double> stepScriptMs;
    double now = 0;
    int depth = 0;                // nested resume depth (0 = driven by step())
    // ---- the debugger ----
    // Keyed by the script's full name, set on its chunk at load and on every restart. A task
    // stopped at one is Parked: step() does nothing and event handlers wait until debugContinue
    // or debugStep resumes it.
    std::map<std::string, std::set<int>> breakpoints;
    std::unordered_map<int64_t, int> mainRefs;            // script id -> its chunk function
    std::unordered_map<std::string, int64_t> mainByName;  // full name -> script id
    Task* pausedTask = nullptr;

    // ---- the Studio's plugins: scripts under PluginDebugService, each with a `plugin` global ----
    struct PluginButtonInfo { int64_t id = 0, toolbar = 0, plugin = 0; std::string toolbarName, buttonId, tooltip, icon, text; bool active = false, enabled = true; };
    std::vector<PluginButtonInfo> pluginButtons;
    bool pluginButtonsDirty = false;
    std::vector<Instance::Ptr> pluginObjects;                     // Plugins, toolbars, buttons, mice, actions: outside the tree, kept alive here
    std::unordered_map<int64_t, Instance*> pluginOfRoot;          // the thing directly under PluginDebugService -> its Plugin
    std::unordered_set<int64_t> pluginRoots;                       // what the host put directly under PluginDebugService; only these, and what is under them, are plugins
    bool pluginFilesAllowed = false;                               // true only inside addPluginFile: the source-file loader may then place under PluginDebugService
    std::unordered_map<int64_t, Instance*> toolbarPlugin;         // toolbar -> its Plugin
    std::unordered_map<int64_t, Instance*> pluginMouse;           // Plugin -> its PluginMouse
    Instance* activePlugin = nullptr;
    bool activeExclusive = false, pluginActiveDirty = false;
    RayV pluginRay; bool pluginRayValid = false;                  // the Studio camera's ray through the pointer, for the PluginMouse
    std::map<std::string, std::map<std::string, std::string>> pluginSettings;   // plugin name -> key -> JSON
    struct PluginSettingWrite { std::string plugin, key, json; };
    std::vector<PluginSettingWrite> pluginSettingWrites;
    std::vector<int64_t> selection;                               // the Studio's selection
    bool selectionRequested = false;                              // a plugin set it: the Studio should follow
    std::vector<std::pair<int64_t, int>> openScriptRequests;
    std::vector<std::string> pluginWaypoints;                     // ChangeHistoryService:SetWaypoint / FinishRecording, in order
    bool historyRecording = false;                                // ChangeHistoryService: between TryBeginRecording and FinishRecording
    std::string ribbonTool = "None";                              // Plugin:SelectRibbonTool's last choice; the Studio here has no ribbon
    std::set<std::string> onboardingsDone;                        // UserGameSettings:SetOnboardingCompleted, for the session

    // ---- EditableImages: a bitmap scripts draw into, shown by an ImageLabel's ImageContent ----
    // destroyed: Destroy() dropped the pixels. placeholder: a stand-in for an image the other side owns.
    struct EditableImage { int w = 0, h = 0; std::vector<uint8_t> rgba; bool dirty = false, destroyed = false, placeholder = false; };
    std::unordered_map<int64_t, EditableImage> editableImages;   // by the EditableImage's id; gone when the object is released
    // Roblox's unusable stand-in of the same class for an object this side has none of: one per id, kept for the session.
    std::unordered_map<int64_t, Instance::Ptr> placeholders;
    std::unordered_map<int64_t, EditableMeshData> editableMeshes;   // by the EditableMesh's id; gone when the object is released
    void placeholderFor(int64_t id, const std::string& className);
    Instance* pluginFor(Instance& script);                        // the Plugin of a script under PluginDebugService, made on first ask; null elsewhere
    bool isPluginScript(const Instance& s) const;
    const Instance* pluginRootOf(const Instance& s) const;         // the registered root above s (or s), null when s is not a plugin's
    bool inPluginTree(const Instance* p) const;                   // p is the real PluginDebugService or sits under it
    int stepMode = 0, stepDepth = 0, stepLine = 0;        // 0 none, 1 into, 2 over, 3 out
    bool pausedAtBreak = false;                           // the stop was a BREAK instruction, not a step
    lua_State* skipBreakCo = nullptr; int skipBreakLine = 0;   // resumed on that BREAK: its next hit is the same one, let it pass
    Runtime::DebugPause pause;
    bool pauseFresh = false;
    void rememberMain(int64_t id, const std::string& name, lua_State* co);   // the chunk is on co's top
    void onDebugBreak(lua_State* L, lua_Debug* ar);
    void onDebugStep(lua_State* L, lua_Debug* ar);
    void pauseAt(lua_State* L, int line);

    std::deque<Task*> ready;      // resume with 0 args
    std::vector<Task*> sleepers;
    std::deque<Task*> deferred;
    std::vector<Task*> childWaiters;
    std::unordered_map<lua_State*, Task*> byThread;
    // A thread a script made (coroutine.create / wrap) runs as its maker: the VM's userthread
    // callback copies the parent's ctx here. A thread with no ctx fails every capability check.
    std::unordered_map<lua_State*, std::shared_ptr<ScriptCtx>> threadCtx;
    std::unordered_set<int64_t> started;
    std::unordered_set<Instance*> bound;                  // instances with an InstBinding (signals live here)
    std::vector<int64_t> pendingStart;
    std::vector<int> pendingUnref;                       // refs released from GC destructors
    std::unordered_map<int64_t, int> moduleResults;      // ModuleScript id -> ref of its return value
    std::unordered_set<int64_t> moduleLoading;
    std::vector<std::unique_ptr<Tween>> tweens;
    std::vector<std::unique_ptr<PathState>> paths;        // every Path a script still holds
    double pathCheckIn = 0;                               // until the next Blocked / Unblocked look
    void stepPaths(double dt);
    std::vector<std::pair<std::string, int>> renderSteps; // BindToRenderStep name -> fn ref (client)
    std::vector<int> bindToClose;
    Instance* localPlayer = nullptr;
    int64_t guiCharacter = 0;         // client: the character the PlayerGui's StarterGui copies were made for
    void resetGuis();                 // client: a respawn replaces the LayerCollectors with ResetOnSpawn
    int64_t focusedBox = 0;           // client: the TextBox with keyboard focus (UserInputService:GetFocusedTextBox())
    Instance* focusedTextBox();
    void focusBox(Instance& box);                 // Focused, UserInputService.TextBoxFocused; the one before loses focus
    void blurBox(Instance& box, bool enter);      // FocusLost(enterPressed), UserInputService.TextBoxFocusReleased

    // DataStoreService, name -> scope -> key -> JSON: written to opts.dataStorePath after a step that changed it. Server only, as on Roblox.
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> dataStores;
    std::map<std::string, Instance::Ptr> dataStoreObjects;                          // "name\nscope" -> GlobalDataStore
    std::unordered_map<int64_t, std::pair<std::string, std::string>> dataStoreKeys;  // its id -> name, scope
    bool dataStoresDirty = false;
    // Every write a key has had, newest last: ListVersionsAsync lists them, GetVersionAsync reads
    // one back. Roblox drops them after 30 days; these last as long as the file.
    // users and meta: the JSON of the user ids and metadata the write named, "" for none (DataStoreKeyInfo:GetUserIds / GetMetadata).
    struct DsVersion { std::string version, json; double time = 0; bool deleted = false; std::string users, meta; };
    std::map<std::string, std::map<std::string, std::map<std::string, std::vector<DsVersion>>>> dataStoreVersions;
    // Adds a version for a write, or for a removal -- deleted, keeping the value it had.
    std::string recordVersion(const std::string& name, const std::string& scope, const std::string& key,
                              const std::string& json, bool deleted);
    struct Pages {
        std::vector<std::pair<std::string, double>> items;   // key -> value (a sorted store)
        std::vector<Instance::Ptr> objects;                  // or the instances a listing hands back
        std::vector<std::pair<std::string, std::string>> jsonItems;   // or key -> JSON value (a MemoryStoreHashMap listing)
        size_t pos = 0; size_t pageSize = 50;
        size_t count() const { return !objects.empty() ? objects.size() : !jsonItems.empty() ? jsonItems.size() : items.size(); }
    };
    std::unordered_map<int64_t, Pages> dataStorePages;                               // DataStorePages id -> its snapshot
    void loadDataStores();
    void saveDataStores();

    // ---- the services family ----
    // LogService: the last lines out, oldest first, and MessageOut for each; type is Enum.MessageType's value.
    struct LogLine { std::string message; int type = 0; double timestamp = 0; };
    std::deque<LogLine> logHistory;
    void logLine(const std::string& text, int type);
    std::map<int64_t, std::set<double>> badgeAwards;             // BadgeService: user id -> the badges awarded this session
    std::map<std::string, std::string> teleportSettings;         // TeleportService:SetTeleportSetting, key -> JSON, for the session
    int64_t teleportGui = 0;                                     // TeleportService:SetTeleportGui
    std::string vrTouchpadMode[2] = {"Touch", "Touch"};          // VRService's Left and Right pads
    std::map<std::string, int> fetchStatus;                      // ContentProvider:GetAssetFetchStatus: uri -> Enum.AssetFetchStatus value, once a preload settled
    double lastDt = 1.0 / 60;                                    // the last step's dt (TweenService:SmoothDamp's default)
    // MemoryStoreService: one store per kind and name, in this process for the session. A map's
    // items expire; a queue's items carry a priority and, once read, an invisibility window ending
    // when RemoveAsync takes them or it runs out.
    struct MemItem { std::string json; double expires = 0; bool hasSort = false, sortIsNumber = false; double sortNum = 0; std::string sortStr; };
    struct MemQueueItem { std::string json; double expires = 0; double priority = 0; uint64_t seq = 0; double invisibleUntil = 0; std::string readId; };
    struct MemStore { Instance::Ptr inst; char kind = 0; std::map<std::string, MemItem> items; std::deque<MemQueueItem> queue; double invisibility = 30; uint64_t nextSeq = 1, nextRead = 1; };
    std::map<std::string, MemStore> memoryStores;                // kind, a newline, the name -> the store
    std::unordered_map<int64_t, std::string> memoryStoreKeys;    // its instance's id -> that key
    // MemoryStoreQueue:ReadAsync callers waiting for items or their timeout, checked each step.
    struct QueueWait { Task* task = nullptr; std::string store; int count = 1; bool allOrNothing = false; double deadline = 0; };
    std::vector<QueueWait> queueWaiters;
    void checkQueueWaiters();

    // input (client): what the engine reported through Runtime::input
    std::map<std::string, Instance::Ptr> inputObjects;   // "UserInputType\nKeyCode" -> its InputObject
    std::set<std::string> keysDown, mouseDown;           // KeyCode / UserInputType item names
    Vec3 mousePos, mouseDelta;
    Instance::Ptr mouse;                                            // Player:GetMouse(), made on first ask
    Instance::Ptr userSettings;                                     // UserSettings(), made on first ask
    int64_t hoverDetector = 0;                                      // ClickDetector under the pointer (MouseHoverEnter/Leave)
    // The Mouse.Icon from before the pointer entered a ClickDetector, restored on the way out:
    // the place may have set its own, so the default cannot be assumed.
    std::string iconBeforeHover;
    bool cursorTaken = false;
    std::vector<int64_t> hotbar;                                    // the Backpack's tools in the order they were first seen
    void refreshHotbar();                                           // client: recompute it, write the tools' HotbarSlot
    void hotbarSelect(int slot);                                    // client: key / click on slot 0-8: Equip or Unequip
    std::string lastInputType = "None";
    struct ActionBinding {
        std::string name; int fnRef = LUA_NOREF; std::shared_ptr<ScriptCtx> ctx;
        int priority = 2000; uint64_t order = 0;         // later bindings win among equal priorities
        std::vector<std::string> inputs;                 // "KeyCode.E", "UserInputType.MouseButton1"
        std::string title, description, image;           // SetTitle / SetDescription / SetImage: the touch button's, which nothing here draws
        Value buttonPosition;                            // SetPosition: a UDim2, or nil until set
    };
    std::vector<ActionBinding> actionBindings;
    uint64_t nextActionOrder = 1;
    int actionWrapperRef = LUA_NOREF;                    // function(sink, fn, ...) sink(fn(...)) end
    bool actionPassed = false;                           // the handler returned Enum.ContextActionResult.Pass
    bool lastInputSunk = false;                          // the last input: taken by the GUI or sunk by an action
    void unbindAction(const std::string& name);

    // replication + remotes (rbx_net.h)
    Replicator replicator;
    bool replicating = false;
    size_t repCursor = 0;                                // dm.changes() entries already filtered
    std::vector<Change> pendingRep;
    std::vector<RemoteMsg> outRemotes;
    // Fired with nothing connected: held -- Roblox holds 256 per remote -- until a handler appears.
    std::unordered_map<int64_t, std::vector<RemoteMsg>> remoteQueue;
    std::unordered_set<int64_t> remoteQueueWarned;
    std::vector<Runtime::ChatLine> chatLines;           // client: for the engine's chat window and bubbles
    std::vector<Runtime::Notification> notifications;   // client: SendNotification's cards
    std::vector<Runtime::TerrainOp> terrainOps;   // Terrain:Fill* calls, for the host
    std::vector<Runtime::Impulse> impulses;       // BasePart:ApplyImpulse* calls, for the host
    int64_t resetCallback = 0;                           // SetCore("ResetButtonCallback", bindable): fired instead of a reset
    bool resetEnabled = true;                            // SetCore("ResetButtonCallback", false): the menu's button does nothing
    void menu(bool open);                                // the engine's escape menu opened / closed
    void savedQuality(int level);                        // the menu's Graphics Quality: 0 Automatic, 1..10
    int savedQualityLevel = 0;
    void resetCharacter();                               // its Reset Character button
    // chat (rbx_chat.cpp): the legacy Chat and TextChatService over the Players remote
    const Callback* callback(Instance& i, const char* name);
    void callSync(const Callback& cb, const std::function<int(lua_State*)>& pushArgs, const std::function<void(lua_State*, int)>& take);
    int nextMessageId = 1;
    std::unordered_map<std::string, Instance::Ptr> sentMessages;   // client: SendAsync's, until the server's copy returns
    Instance* textChannels();
    Instance* textSourceOf(Instance& channel, int64_t userId);
    Instance* playerOfSource(Instance* source);
    Instance* addTextSource(Instance& channel, Instance& player);
    void makeDefaultChannels();
    void playerChannels(Instance& player, bool joining);
    Instance::Ptr newMessage(Instance& channel, const std::string& text, Instance* source, const std::string& meta, int status);
    Instance::Ptr sendAsync(Instance& channel, const std::string& text, const std::string& meta);
    Instance::Ptr systemMessage(Instance& channel, const std::string& text, const std::string& meta);
    void incoming(Instance& msg);
    void chatSend(const std::string& text);
    void chatRemote(const RemoteMsg& m, Instance* sender);
    uint64_t nextCall = 1;
    std::unordered_map<uint64_t, Task*> pendingInvokes;  // InvokeServer/InvokeClient callers, parked
    int invokeWrapperRef = LUA_NOREF;                    // function(reply, fn, ...) reply(pcall(fn, ...)) end
    uint64_t nextHttp = 1;
    std::unordered_map<uint64_t, Task*> pendingHttp;     // HttpService callers, parked on the host
    // Solid modelling callers, parked on the host.
    struct PendingSolid {
        Task* task = nullptr;
        // The calling part's look at the time of the call: the result wears it, and the part may be gone by then.
        std::vector<std::pair<std::string, Value>> look;
        bool wantsTable = false;                         // GeometryService returns {PartOperation}
        std::string resultClass = "UnionOperation";      // UnionOperation, IntersectOperation, or MeshPart
        std::string resultName;                          // "" = the class's own default name
        std::string collision = "Default", render = "Automatic", fluid = "Automatic";
    };
    uint64_t nextSolid = 1;
    std::unordered_map<uint64_t, PendingSolid> pendingSolid;
    // AssetService:CreateMeshPartAsync callers, parked while the host loads the mesh.
    struct PendingMeshPart {
        Task* task = nullptr;
        std::string uri;
        bool editable = false, fixedSize = true;           // CreateEditableMeshAsync, not CreateMeshPartAsync
        std::string collision = "Default", render = "Automatic", fluid = "Automatic";
    };
    // ContentProvider:PreloadAsync callers, parked until every content in the list settles; the callback hears each one.
    struct PreloadGroup { Task* task = nullptr; int left = 0; int cbRef = LUA_NOREF; std::shared_ptr<ScriptCtx> ctx; };
    struct PendingPreload { uint64_t group = 0; std::string uri; };
    uint64_t nextPreload = 1, nextPreloadGroup = 1;
    std::unordered_map<uint64_t, PreloadGroup> preloadGroups;
    std::unordered_map<uint64_t, PendingPreload> pendingPreload;
    int64_t contentProviderId = 0;                         // the service, once a script has asked it something
    uint64_t nextMesh = 1;
    std::unordered_map<uint64_t, PendingMeshPart> pendingMeshPart;
    std::unordered_map<uint64_t, Task*> pendingImages;             // CreateEditableImageAsync callers
    size_t dataModelContentBytes = 0;                              // what CreateDataModelContentAsync holds, against its budget
    void filterReplication();

    Impl(Runtime* s, Options o, Callbacks c);
    ~Impl();

    // scheduler
    Task* newTask(lua_State* from, std::shared_ptr<ScriptCtx> ctx);
    Task* taskFor(lua_State* L);                         // adopt a foreign coroutine (coroutine.wrap ...)
    void resume(Task* t, int nargs, bool asError = false);
    void kill(Task* t);
    void unqueue(Task* t);                               // pull from every scheduler queue
    void stopScript(Instance& script);                   // Disabled / destroyed: threads + connections die
    void runReady();
    void runDeferred();
    void wakeSleepers();
    void checkChildWaiters();
    bool applyingBatch = false;                                     // inside applyReplication: waiters wake after the batch
    // Players whose Character property arrived before the model it names -- on a client the two
    // can land in different batches, and CharacterAdded has to hand the character over.
    std::vector<std::pair<int64_t, int64_t>> pendingCharacterAdded;
    void stepTweens(double dt);
    std::shared_ptr<ScriptCtx> ctxOf(lua_State* L);
    // PluginSecurity: a plugin's script or the host's own chunk; anything else is refused.
    bool hasPluginCapability(lua_State* L);
    void report(const RunResult& r, const std::shared_ptr<ScriptCtx>& ctx);

    // scripts
    bool shouldRun(Instance& script) const;
    void startScript(Instance& script);
    void queueStart(Instance& i);

    // signals
    InstBinding& binding(Instance& i);
    Signal* findSignal(Instance& i, const std::string& name);   // null if never created
    Signal& signal(Instance& i, const std::string& name);
    void fire(Signal& s, const std::function<int(lua_State*)>& pushArgs);
    void fireValues(Instance& i, const std::string& event, const std::vector<Value>& args);
    void disconnectAll(Instance& i);

    // values
    void pushInstance(lua_State* L, Instance* i);
    Instance* toInstance(lua_State* L, int idx);           // null if not an instance
    Instance* toObject(lua_State* L, int idx);             // an Instance or any other Object (an EditableImage); null otherwise
    Instance& checkObject(lua_State* L, int idx);
    Instance& checkInstance(lua_State* L, int idx);
    void pushValue(lua_State* L, const Value& v, const EnumDef* e = nullptr);
    bool toValue(lua_State* L, int idx, Value::Type want, const EnumDef* e, Value& out, std::string& err);
    void pushSignal(lua_State* L, Instance& owner, Signal& s);
    void pushNetValue(lua_State* L, const NetValue& v);
    NetValue toNetValue(lua_State* L, int idx, int depth = 0);

    // players
    Instance* spawnCharacter(Instance& player);
    // A character's dressing: the six body colours of a BodyColors, or of a HumanoidDescription
    // being applied, painted onto the limbs. The last description applied to each Humanoid is
    // kept here, parentless, for GetAppliedDescription.
    void paintLimbs(Instance& character, const Instance& colours);
    void bodyColorsChanged(Instance& bodyColors);
    void applyDescription(Instance& humanoid, Instance& description);
    std::unordered_map<int64_t, Instance::Ptr> appliedDescriptions;
    std::unordered_map<int64_t, std::string> humanoidState;         // each Humanoid's last StateName, for the state events' edges
    std::unordered_map<int64_t, std::unordered_map<std::string, Value>> trackParams;   // AnimationTrack:SetParameter's record
    std::unordered_map<int64_t, std::vector<Vec3>> wireSegments;                        // WireframeHandleAdornment's lines, a point pair each; nothing draws them
    // tools: a Tool in a character (a Model with a Humanoid) is held
    std::unordered_set<int64_t> heldTools;
    void toolMoved(Instance& tool, Instance* oldP, Instance* newP);
    void equipTool(Instance& character, Instance& tool);
    void unequipTools(Instance& character, Instance* except = nullptr);   // to its player's Backpack
    void dropTool(Instance& character, Instance& tool);
    Instance* mouseObject();

    // Every ProximityPrompt in the tree; on a client shownPrompts is nearest first, holdingPrompt
    // is the one whose key is down for its HoldDuration, and a triggered prompt ends on key up.
    std::unordered_set<int64_t> prompts;
    std::vector<int64_t> shownPrompts;
    int64_t holdingPrompt = 0; double holdElapsed = 0;
    // The prompt the mouse triggered, so letting the button go releases it: otherwise it stays
    // in triggeredPrompts, never fires TriggerEnded, and refuses every later trigger.
    int64_t clickedPrompt = 0;
    std::unordered_set<int64_t> triggeredPrompts;
    Instance* promptPart(Instance& prompt);              // the BasePart it stands on
    void updatePrompts(double dt);                        // client, each step
    void promptKey(const std::string& key, bool down);    // Runtime::input
    void promptPress(Instance& prompt);                   // its key went down / InputHoldBegin
    void promptRelease(Instance& prompt);                 // came up / InputHoldEnd
    void triggerPrompt(Instance& prompt);
    void promptGone(int64_t id);

    // Every Sound in the tree, PlayLocalSound's too. TimePosition ticks here while Playing and the
    // engine's TimeLength is known; Ended / DidLoop fire from it. soundVerb picks which event a
    // Playing write fires -- Paused / Resumed / Ended, or a plain Stopped / Played when SoundNone.
    std::unordered_set<int64_t> sounds;
    std::unordered_set<int64_t> motors;         // Motor6Ds in the tree: CurrentAngle chases DesiredAngle
    void stepMotors(double dt);
    std::unordered_map<int64_t, int> soundLoops;
    enum SoundVerb { SoundNone, SoundPausing, SoundResuming, SoundEnding } soundVerb = SoundNone;
    void updateSounds(double dt);
    void soundPlay(Instance& s, bool resume, bool anywhere);   // Play / Resume / PlayLocalSound
    CFrameV listenerCFrame{};                                  // SetListener(CFrame, cf), for GetListener
    void soundStop(Instance& s, bool pause);                   // Stop / Pause

    // AudioPlayers keep a clock here as Sounds do. Every step re-checks each wire for whether it
    // connects, and both its ends hear WiringChanged when that flips. A Play or Stop given a
    // SoundService:GetMixerTime() waits in audioActions.
    std::unordered_set<int64_t> audioPlayers, wires;
    // The Input Action System: every InputBinding in the tree, and the keys each holds down.
    std::unordered_set<int64_t> inputBindings;
    std::unordered_map<int64_t, std::set<int>> bindingKeysDown;
    // A StyleRule's properties (SetProperty / GetProperties); nothing here applies them.
    std::unordered_map<int64_t, std::map<std::string, Value>> styleProps;
    struct AudioAction { int64_t id, player; double at; bool play; };
    std::vector<AudioAction> audioActions;
    int64_t nextAudioAction = 1;
    void updateAudio(double dt);
    void audioPlay(Instance& player, bool play);
    void refreshWires();

    // Rojo *.meta.json that arrived before the instance it describes (path -> text)
    std::map<std::string, std::string> pendingMeta;
    std::vector<std::pair<double, int64_t>> respawns;   // (when, player id): Players.RespawnTime after a Died
    // (when, ForceField id): a spawn's Duration running out. Not a timer on the instance, which a
    // character destroyed under it would leave dangling.
    std::vector<std::pair<double, int64_t>> forceFields;
    std::unordered_map<int64_t, int64_t> playerTeam;    // player id -> the Team it was last on, for Team.PlayerRemoved
    int spawnTurn = 0;                                  // spawns take turns among the SpawnLocations open to the player
    Instance* teamByColor(const Value& color);
    void teamTouch(Instance& spawn, Instance* hit);
    void checkRespawns();
    void detonate(Instance& e);                           // an Explosion parented into the Workspace
    // Seats: a character's part touching one sits it; Jump / Sit = false / dying stands it up
    void seatTouch(Instance& seat, Instance* part);
    void sitOn(Instance& seat, Instance& hum);
    void unseat(Instance& hum);
    void steerVehicle(Instance& hum);
    std::unordered_map<int64_t, double> seatCooldown;    // humanoid id -> when it may sit again after standing
    std::vector<std::pair<double, int64_t>> explosions;   // (when, explosion id): removed once played
};

void openInstanceApi(Runtime::Impl& rt);   // rbx_api.cpp: instance/signal metatables + globals
void openTaskApi(Runtime::Impl& rt);       // rbx_runtime.cpp: task.*, wait, require, print...

// Helpers
inline Runtime::Impl& rtOf(lua_State* L) { return Runtime::from(L)->impl(); }
std::string valueToString(const Value& v);

} // namespace pulseblockz::rbx
