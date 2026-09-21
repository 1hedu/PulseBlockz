// The Roblox object model, free of Luau and Godot: a ClassName, a Name, a Parent and typed
// properties per Instance, `game` as the root. One DataModel per context, one thread each.
// Luau reaches it through rbx_api.cpp; the mirror is the engine's copy of the renderable subset (parts), fed the change log.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace pulseblockz::rbx {

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec2 { float x = 0, y = 0; };
struct Col3 { float r = 0, g = 0, b = 0; };
/// Limb Color of a spawned character; the renderer keys on this exact Color (see spawnCharacter)
inline constexpr Col3 kDefaultSkin{0.5f, 0.0f, 1.0f};
inline bool operator==(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
struct UDim { float scale = 0, offset = 0; };
struct UDim2 { UDim x, y; };
inline bool operator==(UDim a, UDim b) { return a.scale == b.scale && a.offset == b.offset; }
inline bool operator==(UDim2 a, UDim2 b) { return a.x == b.x && a.y == b.y; }
inline bool operator==(Col3 a, Col3 b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

// A property value; every type shares these fields. `Enum` carries the item name in `s` and its
// number in `n`, the enum type coming from the PropDef; `Font` is FontFace, weight 100..900.
struct Value {
    enum Type { Nil, Bool, Number, String, Vector3, Color3, Ref, Enum, Vector2, UDim, UDim2, Font, NumberRange, NumberSequence, ColorSequence, PhysProps, Content };
    Type type = Nil;
    bool b = false;
    double n = 0;
    std::string s;
    Vec3 v;
    Col3 c;
    float u[4] = {0, 0, 0, 0};
    int64_t ref = 0;
    std::vector<float> kp;   // NumberSequence: (time, value, envelope) per keypoint; ColorSequence: (time, r, g, b)

    static Value nil() { return {}; }
    static Value boolean(bool x) { Value r; r.type = Bool; r.b = x; return r; }
    static Value number(double x) { Value r; r.type = Number; r.n = x; return r; }
    static Value string(std::string x) { Value r; r.type = String; r.s = std::move(x); return r; }
    static Value vector3(float x, float y, float z) { Value r; r.type = Vector3; r.v = {x, y, z}; return r; }
    static Value vector3(Vec3 v) { Value r; r.type = Vector3; r.v = v; return r; }
    static Value vector2(float x, float y) { Value r; r.type = Vector2; r.v = {x, y, 0}; return r; }
    Vec2 v2() const { return {v.x, v.y}; }
    static Value udim(float scale, float offset) { Value r; r.type = UDim; r.u[0] = scale; r.u[1] = offset; return r; }
    static Value udim2(float xs, float xo, float ys, float yo) { Value r; r.type = UDim2; r.u[0] = xs; r.u[1] = xo; r.u[2] = ys; r.u[3] = yo; return r; }
    static Value udim2(rbx::UDim2 d) { return udim2(d.x.scale, d.x.offset, d.y.scale, d.y.offset); }
    rbx::UDim udim() const { return {u[0], u[1]}; }
    rbx::UDim2 udim2() const { return {{u[0], u[1]}, {u[2], u[3]}}; }
    static Value color3(float x, float y, float z) { Value r; r.type = Color3; r.c = {x, y, z}; return r; }
    static Value instance(int64_t id) { Value r; r.type = Ref; r.ref = id; return r; }
    static Value enumItem(std::string name, double value) { Value r; r.type = Enum; r.s = std::move(name); r.n = value; return r; }
    static Value numberRange(float lo, float hi) { Value r; r.type = NumberRange; r.u[0] = lo; r.u[1] = hi; return r; }
    static Value physProps(float density, float friction, float elasticity, float frictionWeight = 1, float elasticityWeight = 1) {
        Value r; r.type = PhysProps; r.u[0] = density; r.u[1] = friction; r.u[2] = elasticity; r.u[3] = frictionWeight; r.n = elasticityWeight; return r;
    }
    static Value numberSequence(std::vector<float> keypoints) { Value r; r.type = NumberSequence; r.kp = std::move(keypoints); return r; }
    static Value numberSequence(float a, float b) { return numberSequence({0, a, 0, 1, b, 0}); }
    static Value colorSequence(std::vector<float> keypoints) { Value r; r.type = ColorSequence; r.kp = std::move(keypoints); return r; }
    static Value colorSequence(Col3 a, Col3 b) { return colorSequence({0, a.r, a.g, a.b, 1, b.r, b.g, b.b}); }
    static Value colorSequence(Col3 c) { return colorSequence(c, c); }
    // Value at t in [0, 1], envelopes ignored; a NumberRange samples as its low end
    float sampleNumber(float t) const;
    Col3 sampleColor(float t) const;
    static Value font(std::string family, int weight, bool italic) { Value r; r.type = Font; r.s = std::move(family); r.n = weight; r.b = italic; return r; }
    enum ContentSource { ContentNone = 0, ContentUri = 1, ContentObject = 2, ContentOpaque = 3 };
    static Value content(int source, std::string uri = "", int64_t object = 0) { Value r; r.type = Content; r.n = source; r.s = std::move(uri); r.ref = object; return r; }
    static Value contentUri(std::string uri) { return uri.empty() ? content(ContentNone) : content(ContentUri, std::move(uri)); }
    bool operator==(const Value& o) const;
    bool operator!=(const Value& o) const { return !(*this == o); }
    static const char* typeName(Type t);
};

// ---- enums -------------------------------------------------------------------------
struct EnumItem { const char* name; int value; };
struct EnumDef {
    const char* name;
    std::vector<EnumItem> items;
    const EnumItem* find(const std::string& itemName) const;
    const EnumItem* findValue(int v) const;
};
const EnumDef* findEnum(const std::string& name);
const std::vector<EnumDef>& allEnums();

// ---- classes -----------------------------------------------------------------------
enum PropFlag : unsigned {
    ReadOnly    = 1,   // scripts cannot assign it (Parent is handled specially)
    NoReplicate = 2,   // never crosses the server -> client channel (e.g. Script.Source)
    Hidden      = 4,   // not listed, not shown in tools
    Brick       = 8,   // a Color3 scripts see as a BrickColor (TeamColor)
    PluginWrite = 16,  // PluginSecurity on write: a plugin or the command bar may set it, a game script may not
    PluginRead  = 32,  // PluginSecurity on read
    NotBrowsable = 64, // scripts use it, the Properties panel does not list it (Roblox's "Hidden" tag)
    NoScriptWrite = 128, // NotAccessibleSecurity on write: no script sets it, plugin or not; the engine and Studio do
    NoScriptRead = 256,  // RobloxScriptSecurity / RobloxEngineSecurity on read: no place script reads it
    ServerWrite = 512,   // a server Script may set it, a LocalScript may not (Player.DevComputerMovementMode)
};

struct PropDef {
    std::string name;
    Value::Type type;
    Value def;
    unsigned flags = 0;
    const EnumDef* enumType = nullptr;   // when type == Enum
    // The other half of a Content / string pair (ImageContent and Image), each naming the
    // other. Writing either sets both; a Content holding an object reads as "" through it.
    std::string twin;
    const char* contentObjects = nullptr;   // a Content property: the class of object it may hold; null = asset URIs only
    // An alias: nothing is stored, replicated or saved under this name, only under `aliasOf`
    std::string aliasOf;
};

struct ClassDef {
    std::string name;
    const ClassDef* base = nullptr;
    std::vector<PropDef> props;
    std::vector<std::string> events;     // Touched, Heartbeat, ...
    std::vector<std::string> callbacks;  // function-valued members, not signals (OnInvoke, ...)
    bool creatable = true;               // Instance.new allowed
    bool service = false;                // game:GetService creates it on demand
    const PropDef* findProp(const std::string& n) const;
    bool hasEvent(const std::string& n) const;
    bool hasCallback(const std::string& n) const;
    bool isA(const std::string& className) const;
    void collectProps(std::vector<const PropDef*>& out) const;
};
const ClassDef* findClass(const std::string& name);
const std::vector<const ClassDef*>& allClasses();

// ---- materials ---------------------------------------------------------------------
// Roblox's per-material figures: a part's mass is its volume times the density (Plastic 0.7,
// Metal 7.85). Unknown and terrain materials read as Plastic.
struct MaterialPhysics { double density, friction, elasticity; };
const MaterialPhysics& materialPhysics(const std::string& material);
// A part's CustomPhysicalProperties if it has one, else its material's
MaterialPhysics physicsOf(const std::string& material, const Value& custom);

// ---- change log --------------------------------------------------------------------
// `game` has id 0, so "no parent" cannot be 0
constexpr int64_t kNoParent = INT64_MIN;

struct Change {
    enum Kind { Create, Destroy, Parent, Property };
    Kind kind;
    int64_t id = 0;
    int64_t parent = kNoParent; // Create / Parent: new parent id (kNoParent = nil)
    std::string className;      // Create
    std::string name;           // Create: Name at creation. Property: property name
    Value value;                // Property
    bool fromHost = false;      // written by the engine (physics snapshot) or replicated in, not by a local script:
                                // the mirror must not echo it back, replication still sends it
    bool remote = false;        // of those, the ones off the wire (applyReplication): a client must not take its own
                                // pose back as the server's, however late it arrives
    int64_t script = 0;         // the script whose code made it (0: the engine, the host, a loose chunk); one under PluginDebugService marks it a plugin edit for the Studio
};

class DataModel;

class Instance : public std::enable_shared_from_this<Instance> {
public:
    using Ptr = std::shared_ptr<Instance>;

    int64_t id() const { return id_; }
    const ClassDef& cls() const { return *cls_; }
    const std::string& className() const { return cls_->name; }
    const std::string& name() const { return name_; }
    Instance* parent() const { return parent_; }
    DataModel& dataModel() const { return *dm_; }
    bool destroyed() const { return destroyed_; }
    const std::vector<Ptr>& children() const { return children_; }
    bool isA(const std::string& className) const { return cls_->isA(className); }

    // get() returns nil for an unknown name. set() type-checks and enforces ReadOnly; on
    // failure it fills `err` and changes nothing.
    Value get(const std::string& prop) const;
    bool set(const std::string& prop, const Value& v, std::string* err = nullptr);
    // Host-only set() that ignores ReadOnly (UserId, LocalPlayer, PlaybackState).
    bool setInternal(const std::string& prop, const Value& v, std::string* err = nullptr) { return setImpl(prop, v, err, true); }
    bool setName(const std::string& n) { return set("Name", Value::string(n)); }
    // Host-only write recording no change: per-frame state nobody mirrors (Sound.TimePosition)
    void setSilent(const std::string& prop, const Value& v) {
        const PropDef* d = cls_->findProp(prop);
        if (!d) return;
        if (v == d->def) props_.erase(prop); else props_[prop] = v;
    }
    // Re-record the current value: a write that is a command, not a new value (Humanoid:MoveTo)
    void touch(const std::string& prop);

    // setParent refuses cycles and destroyed instances; nullptr detaches
    bool setParent(Instance* p, std::string* err = nullptr);
    Instance* findFirstChild(const std::string& n, bool recursive = false) const;
    Instance* findFirstChildOfClass(const std::string& className) const;
    Instance* findFirstChildWhichIsA(const std::string& className) const;
    Instance* findFirstAncestorOfClass(const std::string& className) const;
    std::vector<Instance*> getDescendants() const;
    bool isDescendantOf(const Instance* a) const;
    bool isAncestorOf(const Instance* d) const { return d && d->isDescendantOf(this); }
    std::string fullName() const;

    // Detaches, locks Parent, destroys descendants; stays valid while a script still holds it
    void destroy();
    // Deep copy, skipping Archivable = false, parented to nothing
    Ptr clone() const;
private:
    /// clone()'s first pass; `made` maps original id -> copy id for the second pass's Refs
    Ptr cloneTree(std::unordered_map<int64_t, int64_t>& made) const;
public:

    // SetAttribute / GetAttribute: free-form typed values, undeclared by the class
    Value getAttribute(const std::string& n) const;
    bool setAttribute(const std::string& n, const Value& v);
    const std::unordered_map<std::string, Value>& attributes() const { return attrs_; }

    // Tags (CollectionService)
    bool hasTag(const std::string& t) const;
    void addTag(const std::string& t);
    void removeTag(const std::string& t);
    const std::vector<std::string>& tags() const { return tags_; }

    // Opaque slot for the binding layer (cached userdata, signals); ~Instance calls bindingFree
    void* binding = nullptr;
    std::function<void(void*)> bindingFree;
    ~Instance();

private:
    friend class DataModel;
    Instance(DataModel* dm, int64_t id, const ClassDef* cls);
    void detachFromParent();
    bool setImpl(const std::string& prop, const Value& v, std::string* err, bool host);

    DataModel* dm_;
    int64_t id_;
    const ClassDef* cls_;
    std::string name_;
    Instance* parent_ = nullptr;
    std::vector<Ptr> children_;
    std::unordered_map<std::string, Value> props_;   // only values that differ from the default
    // Content.Object is a strong reference on Roblox: the held object, by property name
    std::unordered_map<std::string, Ptr> heldObjects_;
    void holdContent(const std::string& prop, const Value& v);
    std::unordered_map<std::string, Value> attrs_;
    std::vector<std::string> tags_;
    bool destroyed_ = false;
    bool parentLocked_ = false;
};

class DataModel {
public:
    // Service ids are fixed by class, server ids count up from here, client-local ids count
    // down from -1: the sides never collide, so a replicated instance keeps its id on both.
    static constexpr int64_t kFirstServerId = 1000;
    static int64_t serviceId(const std::string& className);   // 0 if not a service
    explicit DataModel(bool isServer);
    ~DataModel();
    DataModel(const DataModel&) = delete;
    DataModel& operator=(const DataModel&) = delete;

    bool isServer() const { return isServer_; }
    Instance* root() const { return root_.get(); }          // `game`
    Instance* getService(const std::string& className);      // creates on first use
    Instance* workspace() { return getService("Workspace"); }

    // Fresh id (Instance.new); parent may be null
    Instance::Ptr create(const std::string& className, Instance* parent = nullptr, std::string* err = nullptr);
    // Host-only: ignores `creatable` (Player, PlayerScripts, Tween)
    Instance::Ptr createInternal(const std::string& className, Instance* parent = nullptr);
    // Given id: replication or sync applying a remote Create
    Instance::Ptr createWithId(int64_t id, const std::string& className);
    Instance* find(int64_t id) const;
    // Not find(): a nil Ref is 0 and id 0 is `game`, so find(0) hands back the whole DataModel
    Instance* findRef(int64_t ref) const { return ref ? find(ref) : nullptr; }

    // Every mutation appends; the owner drains this each step for the mirror and replication
    std::vector<Change> takeChanges() { std::vector<Change> out; out.swap(changes_); return out; }
    const std::vector<Change>& changes() const { return changes_; }
    // Remote changes are recorded for the mirror but tagged, so the replicator cannot echo them
    void setApplyingRemote(bool b) { applyingRemote_ = b; }
    bool applyingRemote() const { return applyingRemote_; }
    void setHostWriting(bool b) { hostWriting_ = b; }
    // A quiet write records nothing: no mirror update, no replication
    void setQuiet(bool b) { quiet_ = b; }
    void setCurrentScript(int64_t id) { currentScript_ = id; }   // stamped on every change it makes
    int64_t currentScript() const { return currentScript_; }
    // Apply a change from the other side; false (with err) for an unknown instance or class
    bool apply(const Change& c, std::string* err = nullptr);
    // Create + Parent + every non-default property, top-down, rebuilding `from` (default: root)
    std::vector<Change> snapshot(const Instance* from = nullptr) const;

    // Hooks the binding layer uses to fire ChildAdded / Changed / Destroying
    std::function<void(Instance&, const std::string& prop)> onPropertyChanged;
    std::function<void(Instance&, Instance* oldParent, Instance* newParent)> onParentChanged;
    std::function<void(Instance&)> onDestroying;
    // The last reference to an instance went: the id is about to be forgotten
    std::function<void(int64_t id)> onReleased;

    size_t instanceCount() const { return byId_.size(); }

private:
    friend class Instance;
    void record(Change c);
    void forget(int64_t id) { byId_.erase(id); }

    bool isServer_;
    int64_t nextId_;
    std::unordered_map<int64_t, Instance*> byId_;   // declared before root_: instances forget() themselves on death
    std::vector<Change> changes_;
    std::unordered_map<int64_t, Instance::Ptr> orphans_;   // apply(Create)d, not yet parented: kept alive until then
    bool applyingRemote_ = false;
    bool hostWriting_ = false;
    bool quiet_ = false;
    int64_t currentScript_ = 0;
    bool dying_ = false;
    Instance::Ptr root_;
};

// Roblox's placement rules for the containers a Script or a LocalScript runs under
bool serverScriptRunsUnder(const Instance& container);
bool clientScriptRunsUnder(const Instance& container, const Instance* localPlayer);
// Whether this service's contents replicate server -> client
bool replicates(const Instance& topLevelService);

} // namespace pulseblockz::rbx
