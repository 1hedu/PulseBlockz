// Engine-agnostic host layer over Runtime: RuntimeThread drives it one frame at a time
// (FrameIn in, FrameOut back), and the loaders turn a Rojo file tree into instances.
#pragma once
#include "rbx_runtime.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace pulseblockz::rbx {

// quiet: applied to the tree but not logged, so neither the mirror nor replication sees it --
// for values every side derives alike
struct HostWrite { int64_t id; std::string prop; Value value; bool quiet = false; };
struct HostEvent { int64_t id; std::string event; std::vector<Value> args; };
// HttpService. The Runtime has no socket: a request leaves a frame as an HttpAsk, the host
// fetches it off the Runtime thread, and the answer rides back in on a later FrameIn.
struct HttpAsk {
    uint64_t id = 0;
    std::string method, url, body, contentType;
    std::vector<std::pair<std::string, std::string>> headers;
};
struct HttpAnswer {
    uint64_t id = 0;
    bool ok = false;                                     // false: never reached the other end
    int status = 0;
    std::string statusText, body;
    std::vector<std::pair<std::string, std::string>> headers;
};
// Solid modelling, the same round trip; the host runs it on the main thread, where the
// geometry lives.
struct SolidAsk {
    uint64_t id = 0;
    Runtime::SolidRequest request;
};
struct SolidAnswer {
    uint64_t id = 0;
    Runtime::SolidResult result;
};
// AssetService:CreateMeshPartAsync
struct MeshAsk {
    uint64_t id = 0;
    std::string uri;
    bool geometry = false;                               // CreateEditableMeshAsync needs the triangles, not just the bounds
};
struct MeshAnswer {
    uint64_t id = 0;
    Runtime::MeshResult result;
};
struct ImageAsk { uint64_t id = 0; std::string uri; };
struct ImageAnswer { uint64_t id = 0; Runtime::ImageResult result; };
// ContentProvider:PreloadAsync, one content per ask
struct PreloadAsk { uint64_t id = 0; std::string uri; };
struct PreloadAnswer { uint64_t id = 0; bool ok = false; };
struct LogLine {
    enum Level { Print, Warn, Error, Killed } level;
    std::string script, text;
};

struct FrameIn {
    double dt = 0;
    // Applied in declaration order, then startScripts() + step(dt).
    std::vector<Change> replication;                     // client: from the server
    std::vector<RemoteMsg> remotes;                      // from the other side
    std::vector<HttpAnswer> httpAnswers;
    std::vector<SolidAnswer> solidAnswers;
    std::vector<MeshAnswer> meshAnswers;
    std::vector<ImageAnswer> imageAnswers;
    std::vector<PreloadAnswer> preloadAnswers;
    std::vector<HostWrite> writes;                       // fromHost changes
    std::vector<HostEvent> events;
    std::vector<std::function<void(Runtime&)>> jobs;     // add scripts, players
};
struct FrameOut {
    std::vector<Change> changes;
    std::vector<Change> replication;                     // server: for every client
    std::vector<RemoteMsg> remotes;                      // for the other side
    std::vector<Runtime::ChatLine> chat;                 // client: chat window and bubbles
    std::vector<Runtime::Notification> notifications;    // client: SetCore SendNotification cards
    std::vector<Runtime::TerrainOp> terrain;             // server
    std::vector<Runtime::Impulse> impulses;              // server
    std::vector<HttpAsk> httpAsks;
    std::vector<SolidAsk> solidAsks;
    std::vector<MeshAsk> meshAsks;
    std::vector<ImageAsk> imageAsks;
    std::vector<PreloadAsk> preloadAsks;
    std::vector<LogLine> logs;
    Runtime::Stats stats;
    double now = 0;
    double millis = 0;                                   // milliseconds of wall time the frame took on the worker
    bool debugPaused = false;                            // stopped in the debugger; the runtime did not advance this frame
    bool debugFresh = false;                             // `debug` is where it stopped this frame
    Runtime::DebugPause debug;
    std::vector<Runtime::PluginButton> pluginButtons;    // the whole set, only when it changed
    bool pluginButtonsChanged = false;
    std::vector<int64_t> selection;                      // what a plugin set
    bool selectionRequested = false;
    std::vector<Runtime::PluginSetting> pluginSettings;  // SetSetting calls, for the Studio to persist
    bool pluginActiveChanged = false;
    std::string activePlugin;
    bool pluginActive = false, pluginExclusive = false;
    std::vector<std::pair<int64_t, int>> openScripts;    // plugin:OpenScript(script, line)
    std::vector<std::string> pluginWaypoints;            // ChangeHistoryService waypoints
    std::vector<Runtime::ImageFrame> editableImages;     // only those drawn into this frame
    std::vector<Runtime::MeshFrame> editableMeshes;      // only those changed this frame
};

class RuntimeThread {
public:
    /// threaded = false runs each frame inline inside submit(), same API — for tests
    /// and lockstep hosts.
    RuntimeThread(Runtime::Options opts, bool threaded);
    ~RuntimeThread();

    /// Main thread, and requires idle(). The frame runs on the worker; idle() turns true
    /// again when it is done, and only then is the wake handler called -- so the handler
    /// may itself call take().
    void submit(FrameIn in);
    bool idle() const;
    void waitIdle();
    FrameOut take();                                     // requires idle()
    void setWakeHandler(std::function<void()> fn);       // called on the WORKER thread

    /// Only while idle().
    Runtime& runtime() { return *rt_; }

private:
    void loop();
    void runFrame();

    std::unique_ptr<Runtime> rt_;
    bool threaded_;
    std::thread thread_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    bool hasJob_ = false, busy_ = false, quit_ = false;
    FrameIn in_;
    FrameOut out_;
    std::vector<LogLine> logs_;                          // logs_ through preloadAsks_ are filled by the
    std::vector<HttpAsk> httpAsks_;                      // Runtime callbacks while a frame runs
    std::vector<SolidAsk> solidAsks_;
    std::vector<MeshAsk> meshAsks_;
    std::vector<ImageAsk> imageAsks_;
    std::vector<PreloadAsk> preloadAsks_;
    std::function<void()> wake_;
};

/// A Rojo-style relative path as (container path, class, name):
///   ServerScriptService/Main.server.luau              -> Script
///   StarterPlayer/StarterPlayerScripts/UI.client.luau -> LocalScript
///   ReplicatedStorage/Util.luau                       -> ModuleScript
///   ReplicatedStorage/Shared/init.luau                -> ModuleScript named Shared
///   Workspace/Map/                                    -> Folder
struct SourceFileInfo {
    std::vector<std::string> containers;
    std::string className, name;          // className empty for a *.model.json: the file names the class
    enum Kind { Source, Meta, Model } kind = Source;
    bool initMeta = false;                // init.meta.json: the directory's own instance
    bool rbxmx = false;                   // *.rbxmx, Roblox's XML model, rather than *.model.json
    bool rbxm = false;                    // *.rbxm, the binary model; its source is the file's base64
    std::string path() const;             // containers/name
};
bool classifySourceFile(const std::string& relPath, SourceFileInfo& out);
/// Newer Studios compress .rbxm / .rbxl chunks with zstd rather than LZ4. No zstd is built
/// in: a host that has one installs it here -- the Godot build does, in
/// gdextension/src/pulseblockz_world.cpp -- and without it such a chunk is refused.
using ZstdDecoder = bool (*)(const std::string& in, size_t outLen, std::string& out);
void setZstdDecoder(ZstdDecoder fn);

/// Create or replace the instance a source file stands for, making any missing container
/// (a service at the top level, a Folder below). An existing instance is destroyed and
/// recreated, so the script restarts. Null with `err` set on failure.
Instance* loadSourceFile(Runtime& rt, const std::string& relPath, const std::string& source, std::string* err = nullptr);
/// A *.model.json subtree added under a container path ({"Workspace", "Map"}), Refs and all.
/// Nothing is replaced, so two children may end up sharing a name.
Instance* addModelJson(Runtime& rt, const std::vector<std::string>& containers, const std::string& name,
                       const std::string& json, std::string* err = nullptr);
/// A *.model.json subtree out as *.rbxmx, the XML Roblox reads. The inverse of the reader,
/// so it emits Studio's spelling and not this runtime's: an enum as a numeric <token>, a
/// BasePart's Color packed into one Color3uint8, its Size lower-cased to `size`, and a
/// Position/Orientation pair fused into a single <CoordinateFrame>. `name` is the root
/// instance's, which a model file takes from its own file name.
bool modelJsonToRbxmx(const std::string& json, const std::string& name, std::string& out, std::string* err = nullptr);
/// A whole place as .rbxlx: `json` is a DataModel node whose children are the services,
/// each written as a root item, with Refs resolved across the file. Terrain is left out --
/// the voxels here are not in Studio's format, and Studio makes its own.
bool placeJsonToRbxlx(const std::string& json, std::string& out, std::string* err = nullptr);
/// A whole Roblox place -- .rbxlx (XML) or .rbxl (binary) -- merged into the DataModel: a
/// root item that is a service goes onto that service, a child already there by name and
/// class (StarterPlayerScripts, Camera) merges rather than doubling, and Refs resolve
/// place-wide. Terrain is not imported (Roblox's voxel format is its own) and is named in
/// `note`. Returns the number of instances taken in, or -1 with `err` set.
int importPlace(Runtime& rt, const std::string& bytes, std::string* err, std::string* note);
/// Destroy the instance a source file stands for; false if there is none.
bool unloadSourceFile(Runtime& rt, const std::string& relPath);

} // namespace pulseblockz::rbx
