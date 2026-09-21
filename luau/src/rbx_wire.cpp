#include "rbx_wire.h"
#include <cstring>

namespace pulseblockz::rbx {
namespace {

constexpr size_t kMaxString = 16u << 20;   // one Source, generously
constexpr size_t kMaxCount = 1u << 24;
constexpr int kMaxTableDepth = 32;

struct Writer {
    std::string out;
    void u8(uint8_t v) { out.push_back((char)v); }
    void varint(uint64_t v) {
        while (v >= 0x80) { out.push_back((char)(uint8_t)(v | 0x80)); v >>= 7; }
        out.push_back((char)(uint8_t)v);
    }
    void svarint(int64_t v) { varint(((uint64_t)v << 1) ^ (uint64_t)(v >> 63)); }   // zigzag
    void f32(float v) { char b[4]; std::memcpy(b, &v, 4); out.append(b, 4); }
    void f64(double v) { char b[8]; std::memcpy(b, &v, 8); out.append(b, 8); }
    void str(const std::string& s) { varint(s.size()); out.append(s); }
    void vec3(Vec3 v) { f32(v.x); f32(v.y); f32(v.z); }
    void col3(Col3 c) { f32(c.r); f32(c.g); f32(c.b); }
    void value(const Value& v) {
        u8((uint8_t)v.type);
        switch (v.type) {
        case Value::Nil: break;
        case Value::Bool: u8(v.b); break;
        case Value::Number: f64(v.n); break;
        case Value::String: str(v.s); break;
        case Value::Vector3: vec3(v.v); break;
        case Value::Vector2: f32(v.v.x); f32(v.v.y); break;
        case Value::UDim: f32(v.u[0]); f32(v.u[1]); break;
        case Value::UDim2: for (float f : v.u) f32(f); break;
        case Value::Color3: col3(v.c); break;
        case Value::Ref: svarint(v.ref); break;
        case Value::Enum: str(v.s); f64(v.n); break;
        case Value::Font: str(v.s); f64(v.n); u8(v.b); break;
        case Value::NumberRange: f32(v.u[0]); f32(v.u[1]); break;
        case Value::PhysProps: for (float f : v.u) f32(f); f64(v.n); break;
        case Value::NumberSequence: case Value::ColorSequence: varint(v.kp.size()); for (float f : v.kp) f32(f); break;
        case Value::Content: u8((uint8_t)v.n); str(v.s); svarint(v.ref); break;
        }
    }
    void netValue(const NetValue& v) {
        u8((uint8_t)v.type);
        switch (v.type) {
        case NetValue::Nil: break;
        case NetValue::Bool: u8(v.b); break;
        case NetValue::Number: f64(v.n); break;
        case NetValue::String: str(v.s); break;
        case NetValue::Vector3: vec3(v.v); break;
        case NetValue::Vector2: f32(v.v.x); f32(v.v.y); break;
        case NetValue::UDim: f32(v.u[0]); f32(v.u[1]); break;
        case NetValue::UDim2: for (float f : v.u) f32(f); break;
        case NetValue::Color3: col3(v.c); break;
        case NetValue::CFrame: vec3(v.v); for (float m : v.m) f32(m); break;
        case NetValue::Ref: svarint(v.ref); break;
        case NetValue::Enum: str(v.s); f64(v.n); break;
        case NetValue::Font: str(v.s); f64(v.n); u8(v.b); break;
        case NetValue::NumberRange: f32(v.u[0]); f32(v.u[1]); break;
        case NetValue::NumberSequence: case NetValue::ColorSequence: varint(v.kp.size()); for (float f : v.kp) f32(f); break;
        case NetValue::Content: u8((uint8_t)v.n); str(v.s); svarint(v.ref); break;
        case NetValue::Table:
            varint(v.t ? v.t->size() : 0);
            if (v.t) for (auto& [k, val] : *v.t) { netValue(k); netValue(val); }
            break;
        }
    }
    void change(const Change& c) {
        u8((uint8_t)c.kind);
        svarint(c.id);
        u8(c.fromHost);
        switch (c.kind) {
        case Change::Create: svarint(c.parent); str(c.className); str(c.name); break;
        case Change::Destroy: break;
        case Change::Parent: svarint(c.parent); break;
        case Change::Property: str(c.name); value(c.value); break;
        }
    }
    void remote(const RemoteMsg& m) {
        u8((uint8_t)m.kind); svarint(m.remote); svarint(m.player); varint(m.call); u8(m.ok);
        varint(m.args.size());
        for (auto& a : m.args) netValue(a);
    }
};

struct Reader {
    const std::string& in;
    size_t pos = 0;
    bool ok = true;
    explicit Reader(const std::string& s) : in(s) {}

    bool need(size_t n) { if (!ok || in.size() - pos < n) ok = false; return ok; }
    uint8_t u8() { if (!need(1)) return 0; return (uint8_t)in[pos++]; }
    uint64_t varint() {
        uint64_t v = 0; int shift = 0;
        for (;;) {
            if (!need(1)) return 0;
            uint8_t b = (uint8_t)in[pos++];
            if (shift > 63) { ok = false; return 0; }
            v |= (uint64_t)(b & 0x7f) << shift;
            if (!(b & 0x80)) return v;
            shift += 7;
        }
    }
    int64_t svarint() { uint64_t u = varint(); return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }
    float f32() { if (!need(4)) return 0; float v; std::memcpy(&v, in.data() + pos, 4); pos += 4; return v; }
    double f64() { if (!need(8)) return 0; double v; std::memcpy(&v, in.data() + pos, 8); pos += 8; return v; }
    std::string str() {
        uint64_t n = varint();
        if (!ok || n > kMaxString || !need((size_t)n)) { ok = false; return {}; }
        std::string s = in.substr(pos, (size_t)n);
        pos += (size_t)n;
        return s;
    }
    size_t count() {
        uint64_t n = varint();
        if (!ok || n > kMaxCount || !need((size_t)n)) { ok = false; return 0; }   // every element is >= 1 byte
        return (size_t)n;
    }
    Vec3 vec3() { Vec3 v; v.x = f32(); v.y = f32(); v.z = f32(); return v; }
    Col3 col3() { Col3 c; c.r = f32(); c.g = f32(); c.b = f32(); return c; }
    Value value() {
        Value v;
        uint8_t t = u8();
        if (!ok || t > Value::Content) { ok = false; return v; }
        v.type = (Value::Type)t;
        switch (v.type) {
        case Value::Nil: break;
        case Value::Bool: v.b = u8() != 0; break;
        case Value::Number: v.n = f64(); break;
        case Value::String: v.s = str(); break;
        case Value::Vector3: v.v = vec3(); break;
        case Value::Vector2: v.v.x = f32(); v.v.y = f32(); break;
        case Value::UDim: v.u[0] = f32(); v.u[1] = f32(); break;
        case Value::UDim2: for (float& f : v.u) f = f32(); break;
        case Value::Color3: v.c = col3(); break;
        case Value::Ref: v.ref = svarint(); break;
        case Value::Enum: v.s = str(); v.n = f64(); break;
        case Value::Font: v.s = str(); v.n = f64(); v.b = u8() != 0; break;
        case Value::NumberRange: v.u[0] = f32(); v.u[1] = f32(); break;
        case Value::PhysProps: for (float& f : v.u) f = f32(); v.n = f64(); break;
        case Value::Content: v.n = u8(); v.s = str(); v.ref = svarint(); if (v.n > Value::ContentOpaque) ok = false; break;
        case Value::NumberSequence: case Value::ColorSequence: {
            size_t n = count();
            if (n > 80) { ok = false; return v; }
            v.kp.resize(n);
            for (float& f : v.kp) f = f32();
            break;
        }
        }
        return v;
    }
    NetValue netValue(int depth) {
        NetValue v;
        uint8_t t = u8();
        if (!ok || t > NetValue::Content) { ok = false; return v; }
        v.type = (NetValue::Type)t;
        switch (v.type) {
        case NetValue::Nil: break;
        case NetValue::Bool: v.b = u8() != 0; break;
        case NetValue::Number: v.n = f64(); break;
        case NetValue::String: v.s = str(); break;
        case NetValue::Vector3: v.v = vec3(); break;
        case NetValue::Vector2: v.v.x = f32(); v.v.y = f32(); break;
        case NetValue::UDim: v.u[0] = f32(); v.u[1] = f32(); break;
        case NetValue::UDim2: for (float& f : v.u) f = f32(); break;
        case NetValue::Color3: v.c = col3(); break;
        case NetValue::CFrame: v.v = vec3(); for (float& m : v.m) m = f32(); break;
        case NetValue::Ref: v.ref = svarint(); break;
        case NetValue::Enum: v.s = str(); v.n = f64(); break;
        case NetValue::Font: v.s = str(); v.n = f64(); v.b = u8() != 0; break;
        case NetValue::Content: v.n = u8(); v.s = str(); v.ref = svarint(); if (v.n > Value::ContentOpaque) ok = false; break;
        case NetValue::NumberRange: v.u[0] = f32(); v.u[1] = f32(); break;
        case NetValue::NumberSequence: case NetValue::ColorSequence: {
            size_t n = count();
            if (n > 80) { ok = false; return v; }
            v.kp.resize(n);
            for (float& f : v.kp) f = f32();
            break;
        }
        case NetValue::Table: {
            if (depth >= kMaxTableDepth) { ok = false; return v; }
            size_t n = count();
            v.t = std::make_shared<std::vector<std::pair<NetValue, NetValue>>>();
            for (size_t i = 0; i < n && ok; i++) {
                NetValue k = netValue(depth + 1);
                NetValue val = netValue(depth + 1);
                v.t->emplace_back(std::move(k), std::move(val));
            }
            break;
        }
        }
        return v;
    }
    Change change() {
        Change c;
        uint8_t k = u8();
        if (!ok || k > Change::Property) { ok = false; return c; }
        c.kind = (Change::Kind)k;
        c.id = svarint();
        c.fromHost = u8() != 0;
        switch (c.kind) {
        case Change::Create: c.parent = svarint(); c.className = str(); c.name = str(); break;
        case Change::Destroy: break;
        case Change::Parent: c.parent = svarint(); break;
        case Change::Property: c.name = str(); c.value = value(); break;
        }
        return c;
    }
    RemoteMsg remote() {
        RemoteMsg m;
        uint8_t k = u8();
        if (!ok || k > RemoteMsg::Result) { ok = false; return m; }
        m.kind = (RemoteMsg::Kind)k;
        m.remote = svarint(); m.player = svarint(); m.call = varint(); m.ok = u8() != 0;
        size_t n = count();
        for (size_t i = 0; i < n && ok; i++) m.args.push_back(netValue(0));
        return m;
    }
};

} // namespace

std::string encodePacket(const NetPacket& p) {
    Writer w;
    w.u8('P'); w.u8('B');
    w.u8((uint8_t)p.type);
    switch (p.type) {
    case NetPacket::Hello:
        w.varint(p.protocol); w.str(p.name); w.svarint(p.userId);
        break;
    case NetPacket::Welcome:
        w.svarint(p.playerId);
        w.varint(p.changes.size()); for (auto& c : p.changes) w.change(c);
        w.str(p.nonce);
        w.str(p.place);
        break;
    case NetPacket::Proof:
        w.str(p.address); w.str(p.signature); w.str(p.server);
        break;
    case NetPacket::ServerFrame:
        w.varint(p.changes.size()); for (auto& c : p.changes) w.change(c);
        w.varint(p.remotes.size()); for (auto& m : p.remotes) w.remote(m);
        break;
    case NetPacket::ClientFrame:
        w.varint(p.writes.size());
        for (auto& x : p.writes) { w.svarint(x.id); w.str(x.prop); w.value(x.value); }
        w.varint(p.events.size());
        for (auto& e : p.events) {
            w.svarint(e.id); w.str(e.event);
            w.varint(e.args.size()); for (auto& a : e.args) w.value(a);
        }
        w.varint(p.remotes.size()); for (auto& m : p.remotes) w.remote(m);
        w.varint(p.edits.size());
        for (auto& e : p.edits) { w.str(e.op); w.svarint(e.id); w.svarint(e.parent); w.str(e.name); w.str(e.text); w.str(e.path); w.u8(e.flag ? 1 : 0); w.value(e.value); }
        break;
    }
    return std::move(w.out);
}

bool decodePacket(const std::string& bytes, NetPacket& out) {
    Reader r(bytes);
    out = NetPacket();
    if (r.u8() != 'P' || r.u8() != 'B') return false;
    uint8_t t = r.u8();
    if (!r.ok || t < NetPacket::Hello || t > NetPacket::Proof) return false;
    out.type = (NetPacket::Type)t;
    switch (out.type) {
    case NetPacket::Hello:
        out.protocol = (uint32_t)r.varint(); out.name = r.str(); out.userId = r.svarint();
        break;
    case NetPacket::Welcome: {
        out.playerId = r.svarint();
        size_t n = r.count();
        for (size_t i = 0; i < n && r.ok; i++) out.changes.push_back(r.change());
        out.nonce = r.str();
        out.place = r.str();
        break;
    }
    case NetPacket::Proof:
        out.address = r.str(); out.signature = r.str(); out.server = r.str();
        break;
    case NetPacket::ServerFrame: {
        size_t n = r.count();
        for (size_t i = 0; i < n && r.ok; i++) out.changes.push_back(r.change());
        n = r.count();
        for (size_t i = 0; i < n && r.ok; i++) out.remotes.push_back(r.remote());
        break;
    }
    case NetPacket::ClientFrame: {
        size_t n = r.count();
        for (size_t i = 0; i < n && r.ok; i++) {
            HostWrite x; x.id = r.svarint(); x.prop = r.str(); x.value = r.value();
            out.writes.push_back(std::move(x));
        }
        n = r.count();
        for (size_t i = 0; i < n && r.ok; i++) {
            HostEvent e; e.id = r.svarint(); e.event = r.str();
            size_t na = r.count();
            for (size_t j = 0; j < na && r.ok; j++) e.args.push_back(r.value());
            out.events.push_back(std::move(e));
        }
        n = r.count();
        for (size_t i = 0; i < n && r.ok; i++) out.remotes.push_back(r.remote());
        n = r.count();
        for (size_t i = 0; i < n && r.ok; i++) {
            NetEdit e; e.op = r.str(); e.id = r.svarint(); e.parent = r.svarint(); e.name = r.str(); e.text = r.str(); e.path = r.str(); e.flag = r.u8() != 0; e.value = r.value();
            out.edits.push_back(std::move(e));
        }
        break;
    }
    }
    return r.ok && r.pos == bytes.size();
}

} // namespace pulseblockz::rbx
