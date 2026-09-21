#include "rbx_net.h"
#include "rbx_runtime.h"

namespace pulseblockz::rbx {

NetValue NetValue::fromValue(const Value& v) {
    NetValue r;
    switch (v.type) {
    case Value::Nil: break;
    case Value::Bool: r.type = Bool; r.b = v.b; break;
    case Value::Number: r.type = Number; r.n = v.n; break;
    case Value::String: r.type = String; r.s = v.s; break;
    case Value::Vector3: r.type = Vector3; r.v = v.v; break;
    case Value::Vector2: r.type = Vector2; r.v = v.v; break;
    case Value::UDim: r.type = UDim; for (int i = 0; i < 4; i++) r.u[i] = v.u[i]; break;
    case Value::UDim2: r.type = UDim2; for (int i = 0; i < 4; i++) r.u[i] = v.u[i]; break;
    case Value::Color3: r.type = Color3; r.c = v.c; break;
    case Value::Ref: r.type = Ref; r.ref = v.ref; break;
    case Value::Enum: r.type = Enum; r.s = v.s; r.n = v.n; break;
    case Value::Font: r.type = Font; r.s = v.s; r.n = v.n; r.b = v.b; break;
    case Value::NumberRange: r.type = NumberRange; r.u[0] = v.u[0]; r.u[1] = v.u[1]; break;
    case Value::NumberSequence: r.type = NumberSequence; r.kp = v.kp; break;
    case Value::ColorSequence: r.type = ColorSequence; r.kp = v.kp; break;
    case Value::PhysProps: break;   // not a remote argument
    case Value::Content: r.type = Content; r.n = v.n; r.s = v.s; r.ref = v.ref; break;
    }
    return r;
}

// ---- Replicator ----------------------------------------------------------------------
// Between frames known_ is exactly the set of instances under a replicating service; filter()
// is what keeps it so, and mutates it mid-pass, so it does not hold at every instant. filter()
// reads where an instance is now, not the log's intermediate parents, so a Create can go out
// for an id the clients already have; apply() treats that as a no-op.

bool Replicator::visible(const Instance* i) const {
    if (!i || i->destroyed()) return false;
    const Instance* root = dm_.root();
    if (i == root) return false;
    const Instance* top = i;
    while (top->parent() && top->parent() != root) top = top->parent();
    return top->parent() == root && (all_ || replicates(*top));
}

bool Replicator::sendsProperty(const Instance& i, const std::string& prop) const {
    if (prop.empty() || prop[0] == '@' || prop[0] == '#') return true;   // attributes, tags
    // all_ is Team Create: an edit session replicates every service and every property
    if (all_) return true;
    const PropDef* d = i.cls().findProp(prop);
    if (!d || !(d->flags & NoReplicate)) return true;
    // Source never crosses a socket: the client is told the place (NetPacket::place) and fetches
    // it from the chain, checking each file against its hash. In process there is no socket.
    return inProcess_;
}

// Opaque Content -- a DataModelContent outside the tree -- never crosses: bytes a script made
// reach a client through the chain, named by their hash. An EditableImage is not an Instance
// and has no pixel property, so only the client that drew it sees it -- worked example in
// luau/gdextension/demo2/scripts/src/client/Paint.client.luau. The empty body drops nothing
// only because no .luau calls AssetService:CreateDataModelContentAsync (rbx_api.cpp), the one
// API that makes an Opaque Content; the first caller of it needs this to send something.
void Replicator::sendContentHolder(const Value&, std::vector<Change>&) {}

void Replicator::snapshot(const Instance& from, std::vector<Change>& out) {
    for (Change& c : dm_.snapshot(&from)) {
        if (c.kind == Change::Property) {
            const Instance* i = dm_.find(c.id);
            if (i && !sendsProperty(*i, c.name)) continue;
            sendContentHolder(c.value, out);
        }
        if (c.kind == Change::Create) known_.insert(c.id);
        out.push_back(std::move(c));
    }
}

void Replicator::forgetSubtree(const Instance& i) {
    known_.erase(i.id());
    for (auto& k : i.children()) forgetSubtree(*k);
}

std::vector<Change> Replicator::join() {
    known_.clear();
    std::vector<Change> out;
    // Identity first: a script running on the join frame reads the real JobId, not an empty
    // string it has to poll.
    for (const char* prop : {"JobId", "PlaceVersion", "PrivateServerId", "PrivateServerOwnerId"}) {
        Change c;
        c.kind = Change::Property;
        c.id = dm_.root()->id();
        c.name = prop;
        c.value = dm_.root()->get(prop);
        out.push_back(std::move(c));
    }
    for (auto& svc : dm_.root()->children())
        if (all_ || replicates(*svc)) snapshot(*svc, out);
    return out;
}

std::vector<Change> Replicator::filter(const std::vector<Change>& log) {
    std::vector<Change> out;
    const Instance* root = dm_.root();
    // The topmost instance the clients lack, i itself included; its parent is one they have
    auto topUnknown = [&](const Instance* i) {
        while (i->parent() && i->parent() != root && !known_.count(i->parent()->id())) i = i->parent();
        return i;
    };
    for (const Change& c : log) {
        switch (c.kind) {
        case Change::Create:
            break;   // the Parent change that follows decides whether clients hear of it
        case Change::Destroy:
            if (known_.erase(c.id)) out.push_back(c);
            break;
        case Change::Property: {
            // The root is the one instance outside known_ whose properties still go out: it is
            // never created or destroyed and holds the same id on both sides, and JobId,
            // PlaceVersion and the private-server pair are the server's to state.
            if (c.id != root->id() && !known_.count(c.id)) break;
            const Instance* i = dm_.find(c.id);
            if (i && !sendsProperty(*i, c.name)) break;
            sendContentHolder(c.value, out);
            out.push_back(c);
            break;
        }
        case Change::Parent: {
            const Instance* i = dm_.find(c.id);
            bool vis = visible(i), known = known_.count(c.id) > 0;
            if (vis && !known) {
                snapshot(*topUnknown(i), out);                 // entered the replicated tree
            } else if (vis && known) {
                const Instance* p = i->parent();
                if (p != root && !known_.count(p->id())) {
                    snapshot(*topUnknown(p), out);             // moved under something clients lack (yet)
                } else {
                    Change pc; pc.kind = Change::Parent; pc.id = c.id; pc.parent = p->id();
                    out.push_back(pc);
                }
            } else if (!vis && known) {
                Change d; d.kind = Change::Destroy; d.id = c.id;   // left it: clients drop their copy
                out.push_back(d);
                known_.erase(c.id);
                if (i) forgetSubtree(*i);
            }
            break;
        }
        }
    }
    return out;
}

// ---- LocalSession --------------------------------------------------------------------
LocalSession::LocalSession(Runtime& server) : server_(server) {
    server_.setInProcess(true);     // same program, same files: see Replicator::setInProcess
    server_.setReplicating(true);
}
LocalSession::~LocalSession() { server_.setReplicating(false); }

Instance* LocalSession::join(Runtime& client, const std::string& name, int64_t userId) {
    pump();                                                   // older traffic is not for the newcomer
    Instance* p = server_.addPlayer(name, userId);            // server: Player, character, PlayerAdded
    client.applyReplication(server_.replicationJoin());       // client: the world, that Player included
    Instance* lp = client.addPlayer(name, userId);            // client: becomes it (LocalPlayer, starter scripts)
    clients_.push_back({&client, p->id()});
    return lp;
}

void LocalSession::leave(Runtime& client) {
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if (it->rt != &client) continue;
        server_.removePlayer(server_.dataModel().find(it->playerId));
        clients_.erase(it);
        return;
    }
}

void LocalSession::pump() {
    std::vector<Change> rep = server_.takeReplication();
    std::vector<RemoteMsg> down = server_.takeRemotes();
    for (Client& c : clients_) {
        if (!rep.empty()) c.rt->applyReplication(rep);
        for (const RemoteMsg& m : down)
            if (m.player == 0 || m.player == c.playerId) c.rt->deliverRemote(m);
    }
    for (Client& c : clients_)
        for (RemoteMsg m : c.rt->takeRemotes()) { m.player = c.playerId; server_.deliverRemote(m); }
}

void LocalSession::stepAll(double dt) {
    server_.startScripts();
    server_.step(dt);
    for (Client& c : clients_) { c.rt->startScripts(); c.rt->step(dt); }
    pump();
}

int64_t LocalSession::playerOf(const Runtime& client) const {
    for (const Client& c : clients_) if (c.rt == &client) return c.playerId;
    return 0;
}

} // namespace pulseblockz::rbx
