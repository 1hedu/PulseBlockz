// What crosses the server/client boundary, engine-agnostic: remote-call values, remote messages,
// the replication filter, and a server plus its clients in one process. Roblox's model exactly --
// the server's tree flows down to every client, and the only way up is a remote call.
#pragma once
#include "rbx_instance.h"
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pulseblockz::rbx {

class Runtime;

struct NetValue {
    enum Type { Nil, Bool, Number, String, Vector3, Color3, CFrame, Ref, Enum, Table, Vector2, UDim, UDim2, Font,
                NumberRange, NumberSequence, ColorSequence, Content };
    Type type = Nil;
    bool b = false;                       // Bool; Font: italic
    double n = 0;                         // Number; Font: weight
    std::string s;                        // String; Enum: "Type.Item"; Font: family; Content: uri (n its ContentSourceType, ref its object)
    Vec3 v;                               // Vector3; CFrame position; Vector2 (z = 0)
    float u[4] = {0, 0, 0, 0};            // UDim / UDim2
    Col3 c;
    int64_t ref = 0;                      // Ref: instance id (resolves to nil on the other side if unknown)
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};   // CFrame rotation, row-major
    std::vector<float> kp;                // the sequences' keypoints (NumberRange: lo, hi in u)
    std::shared_ptr<std::vector<std::pair<NetValue, NetValue>>> t;   // Table: pairs in iteration order

    static NetValue nil() { return {}; }
    static NetValue boolean(bool x) { NetValue r; r.type = Bool; r.b = x; return r; }
    static NetValue number(double x) { NetValue r; r.type = Number; r.n = x; return r; }
    static NetValue string(std::string x) { NetValue r; r.type = String; r.s = std::move(x); return r; }
    static NetValue vector3(Vec3 x) { NetValue r; r.type = Vector3; r.v = x; return r; }
    static NetValue instance(int64_t id) { NetValue r; r.type = Ref; r.ref = id; return r; }
    static NetValue fromValue(const Value& v);
};

struct RemoteMsg {
    enum Kind { Event, Invoke, Result };
    Kind kind = Event;
    int64_t remote = 0;       // the RemoteEvent / RemoteFunction
    int64_t player = 0;       // server -> client: the target Player (0 = every client)
                              // client -> server: the sender's Player, filled in by the link
    uint64_t call = 0;        // Invoke <-> Result pairing, unique per sender
    bool ok = true;           // Result: false = the callback raised; args[0] is the message
    bool queued = false;      // Event: already kept once for a handler that was not there (not on the wire)
    std::vector<NetValue> args;
};

class Replicator {
public:
    explicit Replicator(DataModel& dm) : dm_(dm) {}
    /// Everything a joining client needs, top-down; resets the known set, so call it between frames, never mid-log.
    std::vector<Change> join();
    /// Filter one frame's change log down to what every client receives.
    std::vector<Change> filter(const std::vector<Change>& log);
    size_t knownCount() const { return known_.size(); }
    /// Team Create: send every service and property (ServerScriptService, Script.Source), not a game's cut.
    void setAll(bool b) { all_ = b; }
    /// The clients are in this process, sharing its files (Play Solo, a test running both
    /// halves), so client source may be handed over directly. Over a socket it may not: the
    /// client fetches the published place from the chain and checks it (NetPacket::place).
    /// Turned on by LocalSession and by Play Solo in pulseblockz_world.cpp, never elsewhere.
    void setInProcess(bool b) { inProcess_ = b; }

private:
    bool visible(const Instance* i) const;
    bool sendsProperty(const Instance& i, const std::string& prop) const;
    void snapshot(const Instance& from, std::vector<Change>& out);
    void forgetSubtree(const Instance& i);
    void sendContentHolder(const Value& v, std::vector<Change>& out);

    DataModel& dm_;
    bool all_ = false;
    bool inProcess_ = false;
    std::unordered_set<int64_t> known_;   // ids every client has, == the visible set between frames
};

/// In-process link. Owners step their runtimes as usual; pump() moves replication and remotes, both ways.
class LocalSession {
public:
    explicit LocalSession(Runtime& server);
    ~LocalSession();

    /// Adds a Player on the server and hands the client the world. Returns its LocalPlayer.
    Instance* join(Runtime& client, const std::string& name, int64_t userId);
    void leave(Runtime& client);

    /// Move the server's pending replication and everyone's pending remotes.
    void pump();
    /// Step every runtime, then pump. For tests and lockstep hosts.
    void stepAll(double dt);

    Runtime& server() { return server_; }
    size_t clientCount() const { return clients_.size(); }
    int64_t playerOf(const Runtime& client) const;

private:
    struct Client { Runtime* rt; int64_t playerId; };
    Runtime& server_;
    std::vector<Client> clients_;
};

} // namespace pulseblockz::rbx
