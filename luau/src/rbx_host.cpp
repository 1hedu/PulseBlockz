#include <cmath>
#include "rbx_host.h"
#include "rbx_internal.h"
#include <cctype>
#include <chrono>
#include <array>
#include <cstring>
#include <unordered_map>
#include <cstdlib>

namespace pulseblockz::rbx {

// ---- RuntimeThread -----------------------------------------------------------------
RuntimeThread::RuntimeThread(Runtime::Options opts, bool threaded) : threaded_(threaded) {
    Runtime::Callbacks cb;
    cb.print = [this](const std::string& s, const std::string& t) { logs_.push_back({LogLine::Print, s, t}); };
    cb.warn = [this](const std::string& s, const std::string& t) { logs_.push_back({LogLine::Warn, s, t}); };
    cb.error = [this](const std::string& s, const std::string& t) { logs_.push_back({LogLine::Error, s, t}); };
    cb.killed = [this](const std::string& s, const std::string& t) { logs_.push_back({LogLine::Killed, s, t}); };
    // Carried out on the FrameOut: this runs on the worker thread, which touches no socket or node.
    cb.httpRequest = [this](uint64_t id, const std::string& method, const std::string& url,
                            const std::string& body, const std::string& contentType,
                            const std::vector<std::pair<std::string, std::string>>& headers) {
        httpAsks_.push_back({id, method, url, body, contentType, headers});
    };
    cb.solidRequest = [this](uint64_t id, const Runtime::SolidRequest& request) {
        solidAsks_.push_back({id, request});
    };
    cb.meshRequest = [this](uint64_t id, const std::string& uri, bool geometry) { meshAsks_.push_back({id, uri, geometry}); };
    cb.imageRequest = [this](uint64_t id, const std::string& uri) { imageAsks_.push_back({id, uri}); };
    cb.preloadRequest = [this](uint64_t id, const std::string& uri) { preloadAsks_.push_back({id, uri}); };
    rt_ = std::make_unique<Runtime>(std::move(opts), std::move(cb));
    if (threaded_) thread_ = std::thread([this] { loop(); });
}

RuntimeThread::~RuntimeThread() {
    if (threaded_) {
        { std::lock_guard<std::mutex> lk(mu_); quit_ = true; }
        cv_.notify_all();
        thread_.join();
    }
    // rt_ is destroyed after this body, on this thread, with the worker already joined.
}

void RuntimeThread::setWakeHandler(std::function<void()> fn) { std::lock_guard<std::mutex> lk(mu_); wake_ = std::move(fn); }

bool RuntimeThread::idle() const { std::lock_guard<std::mutex> lk(mu_); return !hasJob_ && !busy_; }

void RuntimeThread::waitIdle() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return !hasJob_ && !busy_; });
}

void RuntimeThread::submit(FrameIn in) {
    if (!threaded_) { in_ = std::move(in); runFrame(); return; }
    {
        std::lock_guard<std::mutex> lk(mu_);
        in_ = std::move(in);
        hasJob_ = true;
    }
    cv_.notify_all();
}

FrameOut RuntimeThread::take() {
    std::lock_guard<std::mutex> lk(mu_);
    FrameOut o = std::move(out_);
    out_ = FrameOut();
    return o;
}

void RuntimeThread::loop() {
    for (;;) {
        std::function<void()> wake;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this] { return hasJob_ || quit_; });
            if (quit_) return;
            hasJob_ = false;
            busy_ = true;
        }
        runFrame();
        {
            std::lock_guard<std::mutex> lk(mu_);
            busy_ = false;
            wake = wake_;
        }
        cv_.notify_all();
        if (wake) wake();
    }
}

// Worker thread, or the caller inline. For the length of a frame in_, logs_ and the ask queues
// belong to this function -- submit() and the locked tail are the only other writers -- so the
// moves below take no lock. The main thread reads out_ only through take(), once idle().
void RuntimeThread::runFrame() {
    auto t0 = std::chrono::steady_clock::now();
    Runtime& rt = *rt_;
    FrameIn in = std::move(in_);
    in_ = FrameIn();
    if (!in.replication.empty()) rt.applyReplication(in.replication);
    for (auto& m : in.remotes) rt.deliverRemote(m);
    for (auto& h : in.httpAnswers) rt.deliverHttp(h.id, h.ok, h.status, h.statusText, h.body, h.headers);
    for (auto& a : in.solidAnswers) rt.deliverSolid(a.id, a.result);
    for (auto& a : in.meshAnswers) rt.deliverMesh(a.id, a.result);
    for (auto& a : in.imageAnswers) rt.deliverImage(a.id, a.result);
    for (auto& a : in.preloadAnswers) rt.deliverPreload(a.id, a.ok);
    for (auto& w : in.writes) { if (w.quiet) rt.hostWriteQuiet(w.id, w.prop, w.value); else rt.hostWrite(w.id, w.prop, w.value); }
    for (auto& e : in.events) {
        Instance* i = rt.dataModel().find(e.id);
        if (i && !i->destroyed()) rt.fireEvent(*i, e.event, e.args);
    }
    for (auto& j : in.jobs) j(rt);
    rt.startScripts();
    rt.step(in.dt);
    FrameOut o;
    o.changes = rt.takeChanges();
    o.replication = rt.takeReplication();
    o.remotes = rt.takeRemotes();
    o.chat = rt.takeChat();
    o.notifications = rt.takeNotifications();
    o.terrain = rt.takeTerrainOps();
    o.impulses = rt.takeImpulses();
    o.logs = std::move(logs_);
    logs_.clear();
    o.httpAsks = std::move(httpAsks_);
    httpAsks_.clear();
    o.solidAsks = std::move(solidAsks_);
    solidAsks_.clear();
    o.meshAsks = std::move(meshAsks_);
    meshAsks_.clear();
    o.imageAsks = std::move(imageAsks_);
    imageAsks_.clear();
    o.preloadAsks = std::move(preloadAsks_);
    preloadAsks_.clear();
    o.stats = rt.stats();
    o.debugPaused = rt.debugPaused();
    o.debugFresh = rt.takeDebugPause(o.debug);
    o.pluginButtonsChanged = rt.takePluginButtons(o.pluginButtons);
    o.selectionRequested = rt.takeSelectionRequest(o.selection);
    o.pluginSettings = rt.takePluginSettings();
    o.pluginActiveChanged = rt.takePluginActive(o.activePlugin, o.pluginActive, o.pluginExclusive);
    o.openScripts = rt.takeOpenScripts();
    o.pluginWaypoints = rt.takePluginWaypoints();
    o.editableImages = rt.takeEditableImages();
    o.editableMeshes = rt.takeEditableMeshes();
    o.now = rt.now();
    o.millis = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (threaded_) { std::lock_guard<std::mutex> lk(mu_); out_ = std::move(o); }
    else out_ = std::move(o);
}

// ---- Rojo file conventions -----------------------------------------------------------
static bool endsWith(const std::string& s, const char* suf) {
    size_t n = std::char_traits<char>::length(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

std::string SourceFileInfo::path() const {
    std::string p;
    for (auto& c : containers) p += c + "/";
    return p + name;
}

// ---- JSON, for *.model.json and *.meta.json ----------------------------------------
namespace {
struct Json {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false; double n = 0; std::string s;
    std::vector<Json> a; std::vector<std::pair<std::string, Json>> o;
    const Json* get(const char* k) const { for (auto& kv : o) if (kv.first == k) return &kv.second; return nullptr; }
    const Json* get(const char* k, const char* alt) const { const Json* j = get(k); return j ? j : get(alt); }   // Rojo takes both spellings
    bool isNums(size_t count) const { if (t != Arr || a.size() != count) return false; for (auto& x : a) if (x.t != Num) return false; return true; }
    float f(size_t i) const { return (float)a[i].n; }
};

struct JsonParser {
    const std::string& s; size_t i = 0; std::string err;
    explicit JsonParser(const std::string& text) : s(text) {}
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at offset " + std::to_string(i); return false; }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool str(std::string& out) {
        i++;   // the opening quote
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) break;
            char e = s[i++];
            switch (e) {
            case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
            case 'b': out += '\b'; break; case 'f': out += '\f'; break;
            case 'u': { if (i + 4 > s.size()) return fail("bad \\u escape"); utf8(out, (unsigned)std::strtoul(s.substr(i, 4).c_str(), nullptr, 16)); i += 4; break; }
            default: out += e;
            }
        }
        if (i >= s.size()) return fail("unterminated string");
        i++;
        return true;
    }
    bool value(Json& out) {
        ws();
        if (i >= s.size()) return fail("unexpected end");
        char c = s[i];
        if (c == '{') {
            out.t = Json::Obj; i++; ws();
            if (i < s.size() && s[i] == '}') { i++; return true; }
            for (;;) {
                ws();
                std::string key;
                if (i >= s.size() || s[i] != '"' || !str(key)) return fail("expected a key");
                ws();
                if (i >= s.size() || s[i] != ':') return fail("expected ':'");
                i++;
                Json v;
                if (!value(v)) return false;
                out.o.emplace_back(std::move(key), std::move(v));
                ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == '}') { i++; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            out.t = Json::Arr; i++; ws();
            if (i < s.size() && s[i] == ']') { i++; return true; }
            for (;;) {
                Json v;
                if (!value(v)) return false;
                out.a.push_back(std::move(v));
                ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == ']') { i++; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { out.t = Json::Str; return str(out.s); }
        if (s.compare(i, 4, "true") == 0) { out.t = Json::Bool; out.b = true; i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { out.t = Json::Bool; i += 5; return true; }
        if (s.compare(i, 4, "null") == 0) { i += 4; return true; }
        // Not JSON, but what some writers (Godot's) put for a non-finite number.
        {
            static const struct { const char* word; double value; } odd[] = {
                {"-Infinity", -INFINITY}, {"Infinity", INFINITY}, {"-inf", -INFINITY}, {"inf", INFINITY}, {"NaN", NAN}, {"nan", NAN}};
            for (auto& o : odd) {
                size_t n = std::strlen(o.word);
                if (s.compare(i, n, o.word) == 0 && (i + n >= s.size() || !std::isalnum((unsigned char)s[i + n]))) { out.t = Json::Num; out.n = o.value; i += n; return true; }
            }
        }
        size_t start = i;
        if (s[i] == '-') i++;
        while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) i++;
        if (i == start || (i == start + 1 && s[start] == '-')) return fail("unexpected character");
        out.t = Json::Num; out.n = std::strtod(s.c_str() + start, nullptr);
        return true;
    }
};

bool parseJson(const std::string& text, Json& out, std::string* err) {
    JsonParser p(text);
    bool ok = p.value(out);
    if (ok) { p.ws(); if (p.i != text.size()) ok = p.fail("trailing characters"); }
    if (!ok && err) *err = p.err;
    return ok;
}

// A Rojo property value by its declared type: bare ([x, y, z], "Wood") or tagged ({"Enum": 1280}).
bool jsonToValue(const Json& j, Value::Type type, const EnumDef* en, Value& out, std::string* err, bool brick = false) {
    const Json* v = &j;
    std::string tag;
    if (j.t == Json::Obj && j.o.size() == 1) {
        tag = j.o[0].first; v = &j.o[0].second;
        if (tag == "BrickColor") { type = Value::Color3; brick = true; }
        else if (tag == "Vector3") type = Value::Vector3;
        else if (tag == "Vector2") type = Value::Vector2;
        else if (tag == "Color3" || tag == "Color3uint8") type = Value::Color3;
        else if (tag == "Enum") type = Value::Enum;
        else if (tag == "Content" && type == Value::Content) {}   // already Content: the tag is not a retype
        else if (tag == "String" || tag == "Content" || tag == "ContentId") type = Value::String;
        else if (tag == "Bool") type = Value::Bool;
        else if (tag == "Float32" || tag == "Float64" || tag == "Int32" || tag == "Int64") type = Value::Number;
        else if (tag == "UDim") type = Value::UDim;
        else if (tag == "UDim2") type = Value::UDim2;
        else if (tag == "Font") type = Value::Font;
        else if (tag == "NumberRange") type = Value::NumberRange;
        else if (tag == "PhysicalProperties") type = Value::PhysProps;
        else if (tag == "NumberSequence") type = Value::NumberSequence;
        else if (tag == "ColorSequence") type = Value::ColorSequence;
        else if ((tag == "keypoints" || tag == "Keypoints")
                 && (type == Value::NumberSequence || type == Value::ColorSequence)) {
            // Not a type tag: a sequence is {"keypoints": [...]}, a one-key object that is itself the value.
            tag.clear();
            v = &j;
        }
        else { if (err) *err = "unsupported value type " + tag; return false; }
    }
    auto bad = [&]() { if (err) *err = std::string("not a ") + (type == Value::Enum && en ? en->name : Value::typeName(type)); return false; };
    switch (type) {
    case Value::Number: {
        if (v->t == Json::Str) {   // {"Float64": "inf"} / "-inf" / "nan": JSON has no other spelling for these
            if (v->s == "inf" || v->s == "Infinity") { out = Value::number(INFINITY); return true; }
            if (v->s == "-inf" || v->s == "-Infinity") { out = Value::number(-INFINITY); return true; }
            if (v->s == "nan" || v->s == "NaN") { out = Value::number(NAN); return true; }
        }
        if (v->t != Json::Num) return bad();
        out = Value::number(v->n);
        return true;
    }
    case Value::String: if (v->t != Json::Str) return bad(); out = Value::string(v->s); return true;
    case Value::Bool: if (v->t != Json::Bool) return bad(); out = Value::boolean(v->b); return true;
    case Value::Vector3: if (!v->isNums(3)) return bad(); out = Value::vector3(v->f(0), v->f(1), v->f(2)); return true;
    case Value::Vector2: if (!v->isNums(2)) return bad(); out = Value::vector2(v->f(0), v->f(1)); return true;
    case Value::Color3: {
        if (brick && v->t == Json::Num) { Col3 c = brickColor((int)v->n); out = Value::color3(c.r, c.g, c.b); return true; }
        if (brick && v->t == Json::Str) { Col3 c = brickColor(brickNumber(v->s.c_str())); out = Value::color3(c.r, c.g, c.b); return true; }
        if (!v->isNums(3)) return bad();
        float r = v->f(0), g = v->f(1), b = v->f(2);
        if (tag == "Color3uint8" || r > 1 || g > 1 || b > 1) { r /= 255; g /= 255; b /= 255; }   // 0-255 given: bytes
        out = Value::color3(r, g, b);
        return true;
    }
    case Value::UDim: if (!v->isNums(2)) return bad(); out = Value::udim(v->f(0), v->f(1)); return true;
    case Value::UDim2:
        if (v->t == Json::Arr && v->a.size() == 2 && v->a[0].isNums(2) && v->a[1].isNums(2)) { out = Value::udim2(v->a[0].f(0), v->a[0].f(1), v->a[1].f(0), v->a[1].f(1)); return true; }
        if (v->isNums(4)) { out = Value::udim2(v->f(0), v->f(1), v->f(2), v->f(3)); return true; }
        return bad();
    case Value::Font: {
        if (v->t == Json::Str) { if (fontOfEnum(v->s, out)) return true; out = Value::font(v->s, 400, false); return true; }
        if (v->t != Json::Obj) return bad();
        const Json* fam = v->get("family", "Family"); const Json* w = v->get("weight", "Weight"); const Json* st = v->get("style", "Style");
        if (!fam || fam->t != Json::Str) return bad();
        int weight = 400; bool italic = false;
        if (w && w->t == Json::Num) weight = (int)w->n;
        else if (w && w->t == Json::Str) { const EnumItem* it = findEnum("FontWeight")->find(w->s); if (!it) return bad(); weight = it->value; }
        if (st && st->t == Json::Str) italic = st->s == "Italic";
        out = Value::font(fam->s, weight, italic);
        return true;
    }
    case Value::Content: {
        // A uri or {"Uri": ...}; JSON null is Content.none, and so is "" -- contentUri() folds an
        // empty uri into it (rbx_instance.h). A source that is a live Object has no file spelling.
        if (v->t == Json::Str) { out = Value::contentUri(v->s); return true; }
        if (v->t == Json::Null) { out = Value::content(Value::ContentNone); return true; }
        if (v->t != Json::Obj) return bad();
        const Json* uri = v->get("Uri", "uri");
        if (!uri || uri->t != Json::Str) return bad();
        out = Value::contentUri(uri->s);
        return true;
    }
    case Value::PhysProps: {
        // null: the material's own physics; otherwise a Material name, or 3 numbers, 5 with the weights.
        if (v->t == Json::Null) { out = Value::nil(); return true; }
        if (v->t == Json::Str) { const MaterialPhysics& m = materialPhysics(v->s); out = Value::physProps(m.density, m.friction, m.elasticity); return true; }
        if (!v->isNums(3) && !v->isNums(5)) return bad();
        out = v->isNums(5) ? Value::physProps(v->f(0), v->f(1), v->f(2), v->f(3), v->f(4)) : Value::physProps(v->f(0), v->f(1), v->f(2));
        return true;
    }
    case Value::NumberRange:
        if (v->t == Json::Num) { out = Value::numberRange((float)v->n, (float)v->n); return true; }
        if (!v->isNums(2)) return bad();
        out = Value::numberRange(v->f(0), v->f(1));
        return true;
    case Value::NumberSequence: case Value::ColorSequence: {
        // A bare number or [r, g, b] is a flat sequence, [a, b] a two-point one.
        bool color = type == Value::ColorSequence;
        int w = color ? 4 : 3;
        if (!color && v->t == Json::Num) { out = Value::numberSequence((float)v->n, (float)v->n); return true; }
        if (!color && v->isNums(2)) { out = Value::numberSequence(v->f(0), v->f(1)); return true; }
        if (color && v->isNums(3)) { out = Value::colorSequence(Col3{v->f(0), v->f(1), v->f(2)}); return true; }
        const Json* kps = v->t == Json::Obj ? v->get("keypoints", "Keypoints") : v->t == Json::Arr ? v : nullptr;
        if (!kps || kps->t != Json::Arr) return bad();
        std::vector<float> kp;
        for (const Json& k : kps->a) {
            if (k.t != Json::Obj) return bad();
            const Json* t = k.get("time", "Time"); if (!t || t->t != Json::Num) return bad();
            kp.push_back((float)t->n);
            if (color) {
                const Json* c = k.get("color", "Color"); if (!c) c = k.get("value", "Value");
                if (!c) return bad();
                if (c->t == Json::Obj && c->o.size() == 1 && (c->o[0].first == "Color3uint8" || c->o[0].first == "Color3"))
                    c = &c->o[0].second;
                if (!c->isNums(3)) return bad();
                float r = c->f(0), g = c->f(1), b = c->f(2);
                if (r > 1 || g > 1 || b > 1) { r /= 255; g /= 255; b /= 255; }
                kp.insert(kp.end(), {r, g, b});
            } else {
                const Json* val = k.get("value", "Value"); const Json* env = k.get("envelope", "Envelope");
                if (!val || val->t != Json::Num) return bad();
                kp.insert(kp.end(), {(float)val->n, env && env->t == Json::Num ? (float)env->n : 0.f});
            }
        }
        if (kp.size() < (size_t)2 * w) return bad();
        out = color ? Value::colorSequence(std::move(kp)) : Value::numberSequence(std::move(kp));
        return true;
    }
    case Value::Enum: {
        if (!en) return bad();
        const EnumItem* item = nullptr;
        if (v->t == Json::Num) item = en->findValue((int)v->n);
        else if (v->t == Json::Str) {
            std::string n = v->s, pre = std::string("Enum.") + en->name + ".";
            if (n.rfind(pre, 0) == 0) n = n.substr(pre.size());
            item = en->find(n);
        }
        if (!item) return bad();
        out = Value::enumItem(item->name, item->value);
        return true;
    }
    default: if (err) *err = "unsupported property type"; return false;
    }
}

// Set while an rbxmx builds: unknown properties are skipped and Refs wait here for their referents.
struct RefFixup { Instance* inst; std::string prop, referent; };
struct RbxmxBuild {
    std::vector<RefFixup> refs;
    std::unordered_map<std::string, Instance*> referents;
};
static thread_local RbxmxBuild* g_rbxmx = nullptr;

// A *.model.json Ref is a path, from the model's own root ("Map/Chassis") or from the DataModel
// ("game.Workspace.Map"). Resolved once the model is in place, so a weld may name a later part.
struct ModelBuild { std::vector<RefFixup> refs; };
static thread_local ModelBuild* g_model = nullptr;

// One step of a Ref path: the child of that name, or -- when none is literally called that and
// the segment ends "[k]" -- the k-th child of that name, from 1. Roblox lets siblings share one.
static Instance* refStep(Instance* at, const std::string& seg) {
    if (Instance* c = at->findFirstChild(seg)) return c;
    size_t open = seg.rfind('[');
    if (open == std::string::npos || open == 0 || seg.back() != ']') return nullptr;
    std::string digits = seg.substr(open + 1, seg.size() - open - 2);
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos) return nullptr;
    int k = std::atoi(digits.c_str());
    std::string name = seg.substr(0, open);
    for (auto& c : at->children()) if (c->name() == name && --k == 0) return c.get();
    return nullptr;
}

static Instance* resolveRefPath(Runtime& rt, Instance* root, const std::string& path) {
    Instance* at = root;
    std::string rest = path;
    char sep = '/';
    if (path == "game") return rt.dataModel().root();
    if (path.rfind("game.", 0) == 0) { at = rt.dataModel().root(); rest = path.substr(5); sep = '.'; }
    for (size_t start = 0; at && start <= rest.size();) {
        size_t cut = rest.find(sep, start);
        std::string seg = rest.substr(start, cut == std::string::npos ? std::string::npos : cut - start);
        if (!seg.empty()) at = refStep(at, seg);
        if (cut == std::string::npos) break;
        start = cut + 1;
    }
    return at;
}

// Each "properties" entry by the class's definition. A CFrame lands as a position/orientation pair.
void applyJsonProperties(Runtime& rt, Instance& inst, const Json* props, const std::string& relPath) {
    if (!props || props->t != Json::Obj) return;
    for (auto& [name, jv] : props->o) {
        std::string err;
        bool jointCf = (name == "C0" || name == "C1") && inst.isA("JointInstance");
        bool poseCf = name == "CFrame" && (inst.isA("Pose") || inst.isA("BodyGyro"));   // a Keyframe's Pose, a BodyGyro's goal: their own pair
        bool valueCf = name == "Value" && inst.isA("CFrameValue");
        if (name == "CFrame" || jointCf || valueCf) {
            std::string posP = jointCf ? name + "Position" : valueCf ? "ValuePosition" : poseCf ? "CFramePosition" : "Position";
            std::string oriP = jointCf ? name + "Orientation" : valueCf ? "ValueOrientation" : poseCf ? "CFrameOrientation" : "Orientation";
            // No pair on this class: an rbxmx's CFrame is dropped in silence, a model.json's reported.
            if (!inst.cls().findProp(posP)) { if (!g_rbxmx) rt.reportError(relPath, name + " is not a property of " + inst.className()); continue; }
            const Json* cf = &jv;
            if (jv.t == Json::Obj && jv.o.size() == 1 && jv.o[0].first == "CFrame") cf = &jv.o[0].second;
            const Json* pos = cf->get("position", "Position");
            const Json* ori = cf->get("orientation", "Orientation");
            if (pos && pos->isNums(3)) inst.set(posP, Value::vector3(pos->f(0), pos->f(1), pos->f(2)), &err);
            else err = "expected {position = [x, y, z], orientation = [[...], [...], [...]]}";
            if (ori && ori->t == Json::Arr && ori->a.size() == 3 && ori->a[0].isNums(3) && ori->a[1].isNums(3) && ori->a[2].isNums(3)) {
                CFrameV c;
                for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) c.m[r * 3 + k] = ori->a[r].f(k);
                float x, y, z;
                c.toEulerYXZ(x, y, z);
                const float deg = 180.f / 3.14159265f;
                auto d = [deg](float a) { float r = a * deg; return r == 0 ? 0.f : r; };   // no "-0"
                inst.set(oriP, Value::vector3(d(x), d(y), d(z)), &err);
            }
            if (!err.empty()) rt.reportError(relPath, name + ": " + err);
            continue;
        }
        std::string prop = name;
        bool brick = false;
        if (name == "BrickColor" && inst.isA("BasePart")) { prop = "Color"; brick = true; }   // Part.BrickColor is Color on the palette
        const PropDef* def = inst.cls().findProp(prop);
        if (!def) { if (!g_rbxmx) rt.reportError(relPath, name + " is not a property of " + inst.className()); continue; }
        if (g_rbxmx && jv.t == Json::Obj && jv.o.size() == 1 && jv.o[0].first == "Ref") {
            if (def->type == Value::Ref && jv.o[0].second.t == Json::Str) g_rbxmx->refs.push_back({&inst, prop, jv.o[0].second.s});
            continue;
        }
        if (g_rbxmx && (def->flags & ReadOnly)) continue;   // serialized, not settable here
        if (g_model && def->type == Value::Ref) {
            if (jv.t == Json::Str && !jv.s.empty()) g_model->refs.push_back({&inst, prop, jv.s});
            else if (jv.t != Json::Null) rt.reportError(relPath, name + ": expected the path of an instance");
            continue;
        }
        Value v;
        if (!jsonToValue(jv, def->type, def->enumType, v, &err, brick || (def->flags & Brick)) || !inst.set(prop, v, &err)) rt.reportError(relPath, name + ": " + err);
    }
}

// CollectionService tags. A Roblox file carries them as one NUL-separated blob.
void applyJsonTags(Instance& inst, const Json* tags) {
    if (!tags || tags->t != Json::Arr) return;
    for (const Json& t : tags->a) if (t.t == Json::Str && !t.s.empty()) inst.addTag(t.s);
}
static Json jstr(std::string s);   // defined with the rbxmx reader below
static Json tagsFromBlob(const std::string& blob) {
    Json arr; arr.t = Json::Arr;
    size_t start = 0;
    while (start <= blob.size()) {
        size_t nul = blob.find('\0', start);
        std::string one = blob.substr(start, nul == std::string::npos ? std::string::npos : nul - start);
        if (!one.empty()) arr.a.push_back(jstr(one));
        if (nul == std::string::npos) break;
        start = nul + 1;
    }
    return arr;
}

void applyJsonAttributes(Runtime& rt, Instance& inst, const Json* attrs, const std::string& relPath) {
    if (!attrs || attrs->t != Json::Obj) return;
    for (auto& [name, jv] : attrs->o) {
        Value::Type t = jv.t == Json::Num ? Value::Number : jv.t == Json::Str ? Value::String : jv.t == Json::Bool ? Value::Bool : jv.isNums(3) ? Value::Vector3 : Value::Nil;
        Value v; std::string err;
        if (!jsonToValue(jv, t, nullptr, v, &err)) { rt.reportError(relPath, "attribute " + name + ": " + err); continue; }
        inst.setAttribute(name, v);
    }
}

Instance::Ptr buildModel(Runtime& rt, const Json& j, const std::string& name, const std::string& relPath, std::string* err) {
    const Json* cls = j.get("className", "ClassName");
    if (!cls || cls->t != Json::Str) { if (err) *err = name + ": no className"; return nullptr; }
    if (!findClass(cls->s)) { if (err) *err = name + ": " + cls->s + " is not a valid class"; return nullptr; }
    Instance::Ptr inst = rt.dataModel().createInternal(cls->s);
    inst->setName(name);
    if (g_rbxmx) if (const Json* ref = j.get("referent")) if (ref->t == Json::Str) g_rbxmx->referents[ref->s] = inst.get();
    applyJsonProperties(rt, *inst, j.get("properties", "Properties"), relPath);
    applyJsonAttributes(rt, *inst, j.get("attributes", "Attributes"), relPath);
    applyJsonTags(*inst, j.get("tags", "Tags"));
    if (const Json* kids = j.get("children", "Children")) if (kids->t == Json::Arr)
        for (const Json& k : kids->a) {
            const Json* kn = k.get("name", "Name");
            Instance::Ptr child = buildModel(rt, k, kn && kn->t == Json::Str ? kn->s : cls->s, relPath, err);
            if (!child) return nullptr;
            child->setParent(inst.get());
        }
    return inst;
}

// ---- *.rbxmx: Roblox's XML model files (Studio's "Save to File" as .rbxmx) -----------
// Read into the *.model.json shape and built by buildModel. Properties arrive under their
// serialized names and types; Refs are resolved by referent once the whole file is built.
struct Xml {
    std::string name, text;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<Xml> children;
    const Xml* child(const char* n) const { for (auto& c : children) if (c.name == n) return &c; return nullptr; }
    const std::string* attr(const char* n) const { for (auto& a : attrs) if (a.first == n) return &a.second; return nullptr; }
    double num(const char* n, double dflt = 0) const { const Xml* c = child(n); return c ? std::atof(c->text.c_str()) : dflt; }
};

struct XmlParser {
    const std::string& s; size_t i = 0; std::string err;
    explicit XmlParser(const std::string& text) : s(text) {}
    bool fail(const std::string& m) { if (err.empty()) err = m + " at offset " + std::to_string(i); return false; }
    void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) i++; }
    bool starts(const char* p) const { return s.compare(i, std::strlen(p), p) == 0; }
    static void unescape(std::string& t) {
        if (t.find('&') == std::string::npos) return;
        std::string out; out.reserve(t.size());
        for (size_t k = 0; k < t.size(); k++) {
            if (t[k] != '&') { out += t[k]; continue; }
            size_t semi = t.find(';', k);
            if (semi == std::string::npos) { out += t[k]; continue; }
            std::string e = t.substr(k + 1, semi - k - 1);
            if (e == "lt") out += '<'; else if (e == "gt") out += '>'; else if (e == "amp") out += '&';
            else if (e == "quot") out += (char)34; else if (e == "apos") out += (char)39;
            else if (!e.empty() && e[0] == '#') {
                bool hex = e.size() > 1 && e[1] == 'x';
                unsigned cp = (unsigned)std::strtoul(e.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10);
                if (cp < 0x80) out += (char)cp;
                else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
            } else { out += t.substr(k, semi - k + 1); }
            k = semi;
        }
        t = std::move(out);
    }
    bool skipMisc() {
        for (;;) {
            ws();
            if (starts("<!--")) { size_t e = s.find("-->", i); if (e == std::string::npos) return fail("unterminated comment"); i = e + 3; }
            else if (starts("<?")) { size_t e = s.find("?>", i); if (e == std::string::npos) return fail("unterminated <?"); i = e + 2; }
            else if (starts("<!")) { size_t e = s.find('>', i); if (e == std::string::npos) return fail("unterminated <!"); i = e + 1; }
            else return true;
        }
    }
    bool element(Xml& out) {
        if (!skipMisc()) return false;
        if (i >= s.size() || s[i] != '<') return fail("expected an element");
        i++;
        size_t st = i;
        while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == ':' || s[i] == '-' || s[i] == '.')) i++;
        out.name = s.substr(st, i - st);
        if (out.name.empty()) return fail("expected a tag name");
        for (;;) {
            ws();
            if (i >= s.size()) return fail("unterminated tag");
            if (s[i] == '>') { i++; break; }
            if (starts("/>")) { i += 2; return true; }
            size_t as = i;
            while (i < s.size() && !std::isspace((unsigned char)s[i]) && s[i] != '=' && s[i] != '>' && s[i] != '/') i++;
            std::string an = s.substr(as, i - as);
            ws();
            if (i >= s.size() || s[i] != '=') return fail("expected = after attribute " + an);
            i++; ws();
            if (i >= s.size() || (s[i] != (char)34 && s[i] != (char)39)) return fail("expected a quoted attribute value");
            char q = s[i++];
            size_t e = s.find(q, i);
            if (e == std::string::npos) return fail("unterminated attribute value");
            std::string av = s.substr(i, e - i);
            unescape(av);
            out.attrs.emplace_back(an, av);
            i = e + 1;
        }
        for (;;) {
            if (i >= s.size()) return fail("unterminated <" + out.name + ">");
            if (starts("</")) {
                i += 2;
                size_t e = s.find('>', i);
                if (e == std::string::npos) return fail("unterminated close tag");
                std::string cn = s.substr(i, e - i);
                while (!cn.empty() && std::isspace((unsigned char)cn.back())) cn.pop_back();
                if (cn != out.name) return fail("</" + cn + "> closes <" + out.name + ">");
                i = e + 1;
                return true;
            }
            if (starts("<![CDATA[")) { size_t e = s.find("]]>", i); if (e == std::string::npos) return fail("unterminated CDATA"); out.text += s.substr(i + 9, e - i - 9); i = e + 3; continue; }
            if (starts("<!--")) { size_t e = s.find("-->", i); if (e == std::string::npos) return fail("unterminated comment"); i = e + 3; continue; }
            if (s[i] == '<') { Xml c; if (!element(c)) return false; out.children.push_back(std::move(c)); continue; }
            // Per run, never the whole buffer: CDATA is raw by definition and a script's Source
            // lives in one. A run ends at '<', which no entity contains, so none straddles two.
            size_t e = s.find('<', i);
            if (e == std::string::npos) e = s.size();
            std::string run = s.substr(i, e - i);
            unescape(run);
            out.text += run;
            i = e;
        }
    }
};

static std::string rbxmxPropName(const std::string& n) {
    if (n == "size") return "Size";
    if (n == "shape") return "Shape";
    if (n == "Color3uint8") return "Color";
    if (n == "Health_XML") return "Health";
    return n;
}

static Json jnum(double n) { Json j; j.t = Json::Num; j.n = n; return j; }
static Json jstr(std::string s) { Json j; j.t = Json::Str; j.s = std::move(s); return j; }
static Json jarr(std::initializer_list<double> xs) { Json j; j.t = Json::Arr; for (double x : xs) j.a.push_back(jnum(x)); return j; }
static Json jtag(const char* tag, Json v) { Json j; j.t = Json::Obj; j.o.emplace_back(tag, std::move(v)); return j; }

// One <Properties> child -> the model.json value, or Null for a type this runtime does not carry.
static Json rbxmxValue(const Xml& p) {
    const std::string& t = p.name;
    if (t == "string" || t == "ProtectedString") return jstr(p.text);
    if (t == "Content" || t == "ContentId") {
        // <uri> is a Content, <url> a ContentId and a Content written before release 645,
        // <null> an empty one -- which is what the writer emits whenever the uri is empty
        if (const Xml* u = p.child("uri")) return jstr(u->text);
        if (const Xml* u = p.child("url")) return jstr(u->text);
        if (p.child("null")) return jstr("");
        return Json();
    }
    if (t == "bool") { Json j; j.t = Json::Bool; j.b = p.text == "true"; return j; }
    if (t == "int" || t == "int64" || t == "float" || t == "double") {
        if (p.text == "INF" || p.text == "-INF" || p.text == "NAN") return Json();
        return jnum(std::atof(p.text.c_str()));
    }
    if (t == "token") return jtag("Enum", jnum(std::atof(p.text.c_str())));
    if (t == "Vector3") return jtag("Vector3", jarr({p.num("X"), p.num("Y"), p.num("Z")}));
    if (t == "Vector2") return jtag("Vector2", jarr({p.num("X"), p.num("Y")}));
    if (t == "Color3") return jtag("Color3", jarr({p.num("R"), p.num("G"), p.num("B")}));
    if (t == "Color3uint8") { unsigned v = (unsigned)std::strtoul(p.text.c_str(), nullptr, 10); return jtag("Color3uint8", jarr({(double)((v >> 16) & 255), (double)((v >> 8) & 255), (double)(v & 255)})); }
    if (t == "UDim") return jtag("UDim", jarr({p.num("S"), p.num("O")}));
    if (t == "UDim2") return jtag("UDim2", jarr({p.num("XS"), p.num("XO"), p.num("YS"), p.num("YO")}));
    if (t == "CoordinateFrame") {
        Json cf; cf.t = Json::Obj;
        cf.o.emplace_back("position", jarr({p.num("X"), p.num("Y"), p.num("Z")}));
        Json rows; rows.t = Json::Arr;
        rows.a.push_back(jarr({p.num("R00", 1), p.num("R01"), p.num("R02")}));
        rows.a.push_back(jarr({p.num("R10"), p.num("R11", 1), p.num("R12")}));
        rows.a.push_back(jarr({p.num("R20"), p.num("R21"), p.num("R22", 1)}));
        cf.o.emplace_back("orientation", std::move(rows));
        return jtag("CFrame", std::move(cf));
    }
    if (t == "Font") {
        const Xml* fam = p.child("Family"); const Xml* url = fam ? fam->child("url") : nullptr;
        Json f; f.t = Json::Obj;
        f.o.emplace_back("family", jstr(url ? url->text : fam ? fam->text : ""));
        f.o.emplace_back("weight", jnum(p.num("Weight", 400)));
        const Xml* st = p.child("Style");
        f.o.emplace_back("style", jstr(st ? st->text : "Normal"));
        return jtag("Font", std::move(f));
    }
    if (t == "PhysicalProperties") {
        const Xml* custom = p.child("CustomPhysics");
        if (!custom || custom->text != "true") return Json();   // the material's own
        return jtag("PhysicalProperties", jarr({p.num("Density", 0.7), p.num("Friction", 0.3), p.num("Elasticity", 0.5),
                                                p.num("FrictionWeight", 1), p.num("ElasticityWeight", 1)}));
    }
    if (t == "Ref") return p.text == "null" ? Json() : jtag("Ref", jstr(p.text));
    if (t == "NumberRange" || t == "NumberSequence" || t == "ColorSequence") {
        // whitespace-separated floats: "lo hi", "time value envelope"..., "time r g b envelope"...
        std::vector<double> f;
        for (const char* s = p.text.c_str(); *s;) { char* e; double x = std::strtod(s, &e); if (e == s) break; f.push_back(x); s = e; }
        if (t == "NumberRange") return f.size() >= 2 ? jtag("NumberRange", jarr({f[0], f[1]})) : Json();
        Json kps; kps.t = Json::Arr;
        size_t w = t == "NumberSequence" ? 3 : 5;
        for (size_t k = 0; k + w <= f.size(); k += w) {
            Json kp; kp.t = Json::Obj;
            kp.o.emplace_back("time", jnum(f[k]));
            if (w == 3) { kp.o.emplace_back("value", jnum(f[k + 1])); kp.o.emplace_back("envelope", jnum(f[k + 2])); }
            else kp.o.emplace_back("color", jarr({f[k + 1], f[k + 2], f[k + 3]}));
            kps.a.push_back(std::move(kp));
        }
        Json o; o.t = Json::Obj; o.o.emplace_back("keypoints", std::move(kps));
        return jtag(t.c_str(), std::move(o));
    }
    return Json();   // BinaryString, SharedString, Faces, Axes, Rect, UniqueId, ...: not carried
}


// AttributesSerialize: a little-endian blob, base64 in an rbxmx and raw in an rbxm. u32 count,
// then per attribute a u32-length name, a type byte and the value. An unknown type ends the read:
// only the type gives the value its width, so there is no way to find the next name.
static std::string base64Decode(const std::string& in) {
    std::string out;
    unsigned buf = 0; int bits = 0;
    for (char ch : in) {
        int v;
        if (ch >= 'A' && ch <= 'Z') v = ch - 'A'; else if (ch >= 'a' && ch <= 'z') v = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') v = ch - '0' + 52; else if (ch == '+') v = 62; else if (ch == '/') v = 63;
        else continue;   // '=', whitespace
        buf = (buf << 6) | (unsigned)v; bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)((buf >> bits) & 0xFF); }
    }
    return out;
}

struct AttrReader {
    const std::string& b; size_t i = 0; bool ok = true;
    explicit AttrReader(const std::string& bytes) : b(bytes) {}
    bool need(size_t n) { if (i + n > b.size()) { ok = false; return false; } return true; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v = (uint8_t)b[i] | ((uint8_t)b[i + 1] << 8) | ((uint8_t)b[i + 2] << 16) | ((uint32_t)(uint8_t)b[i + 3] << 24); i += 4; return v; }
    uint8_t u8() { if (!need(1)) return 0; return (uint8_t)b[i++]; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v = (uint16_t)((uint8_t)b[i] | ((uint8_t)b[i + 1] << 8)); i += 2; return v; }
    float f32() { uint32_t v = u32(); float f; std::memcpy(&f, &v, 4); return f; }
    double f64() { if (!need(8)) return 0; uint64_t v = 0; for (int k = 7; k >= 0; k--) v = (v << 8) | (uint8_t)b[i + k]; i += 8; double d; std::memcpy(&d, &v, 8); return d; }
    std::string str() { uint32_t n = u32(); if (!need(n)) return ""; std::string s = b.substr(i, n); i += n; return s; }
};

static Json decodeAttributes(const std::string& bytes) {
    Json out; out.t = Json::Obj;
    AttrReader r(bytes);
    uint32_t count = r.u32();
    for (uint32_t k = 0; k < count && r.ok; k++) {
        std::string name = r.str();
        uint8_t type = r.u8();
        if (!r.ok) break;
        Json v;
        switch (type) {
        case 0x02: v = jstr(r.str()); break;
        case 0x03: v.t = Json::Bool; v.b = r.u8() != 0; break;
        case 0x04: v = jnum((int32_t)r.u32()); break;
        case 0x05: v = jnum(r.f32()); break;
        case 0x06: v = jnum(r.f64()); break;
        case 0x09: { float s = r.f32(); double o = (int32_t)r.u32(); v = jtag("UDim", jarr({s, o})); break; }
        case 0x0A: { float xs = r.f32(); double xo = (int32_t)r.u32(); float ys = r.f32(); double yo = (int32_t)r.u32(); v = jtag("UDim2", jarr({xs, xo, ys, yo})); break; }
        case 0x0E: v = jtag("BrickColor", jnum(r.u32())); break;
        case 0x0F: { float cr = r.f32(), cg = r.f32(), cb = r.f32(); v = jtag("Color3", jarr({cr, cg, cb})); break; }
        case 0x10: { float x = r.f32(), y = r.f32(); v = jtag("Vector2", jarr({x, y})); break; }
        case 0x11: { float x = r.f32(), y = r.f32(), z = r.f32(); v = jarr({x, y, z}); break; }
        case 0x14: { for (int j = 0; j < 3; j++) r.f32(); if (r.u8() == 0) for (int j = 0; j < 9; j++) r.f32(); break; }   // CFrame
        case 0x15: r.str(); r.u32(); break;                                   // EnumItem: type name, value
        case 0x17: { uint32_t n = r.u32(); for (uint32_t j = 0; j < n && r.ok; j++) { r.f32(); r.f32(); r.f32(); } break; }   // NumberSequence
        case 0x19: { uint32_t n = r.u32(); for (uint32_t j = 0; j < n && r.ok; j++) { r.f32(); r.f32(); r.f32(); r.f32(); r.f32(); } break; }   // ColorSequence
        case 0x1B: r.f32(); r.f32(); break;                                   // NumberRange
        case 0x1C: for (int j = 0; j < 4; j++) r.f32(); break;                // Rect
        case 0x21: {                                                          // Font: weight, style, family, cached face
            int weight = r.u16(); bool italic = r.u8() == 1; std::string family = r.str(); r.str();
            Json f; f.t = Json::Obj;
            f.o.emplace_back("family", jstr(family)); f.o.emplace_back("weight", jnum(weight)); f.o.emplace_back("style", jstr(italic ? "Italic" : "Normal"));
            v = jtag("Font", std::move(f));
            break;
        }
        default: r.ok = false; break;
        }
        if (r.ok && v.t != Json::Null) out.o.emplace_back(name, std::move(v));
    }
    return out;
}

// An <Item> -> the model.json shape. An unknown class becomes a Folder, so the rest still loads.
static Json rbxmxItem(const Xml& item, const std::string& nameOverride) {
    Json j; j.t = Json::Obj;
    const std::string* cls = item.attr("class");
    std::string className = cls ? *cls : "Folder";
    if (!findClass(className)) className = "Folder";
    j.o.emplace_back("className", jstr(className));
    if (const std::string* ref = item.attr("referent")) j.o.emplace_back("referent", jstr(*ref));
    Json props; props.t = Json::Obj;
    std::string name = nameOverride;
    if (const Xml* ps = item.child("Properties"))
        for (const Xml& p : ps->children) {
            const std::string* pn = p.attr("name");
            if (!pn) continue;
            if (*pn == "Name") { if (name.empty()) name = p.text; continue; }
            if (*pn == "AttributesSerialize" && p.name == "BinaryString") { j.o.emplace_back("attributes", decodeAttributes(base64Decode(p.text))); continue; }
            if (*pn == "SmoothGrid" && p.name == "BinaryString") { props.o.emplace_back("SmoothGrid", jstr(base64Decode(p.text))); continue; }   // raw: the importer looks at its size
            if (*pn == "MaterialColors" && p.name == "BinaryString") { props.o.emplace_back("MaterialColors", jstr(base64Decode(p.text))); continue; }
            if (*pn == "Tags" && p.name == "BinaryString") { Json t = tagsFromBlob(base64Decode(p.text)); if (!t.a.empty()) j.o.emplace_back("tags", std::move(t)); continue; }
            Json v = rbxmxValue(p);
            if (v.t == Json::Null) continue;
            props.o.emplace_back(rbxmxPropName(*pn), std::move(v));
        }
    j.o.emplace_back("name", jstr(name.empty() ? className : name));
    j.o.emplace_back("properties", std::move(props));
    Json kids; kids.t = Json::Arr;
    for (const Xml& c : item.children) if (c.name == "Item") kids.a.push_back(rbxmxItem(c, ""));
    j.o.emplace_back("children", std::move(kids));
    return j;
}

// A single root <Item> is the instance, named after the file; several go into a Folder of that name.
static bool rbxmxToJson(const std::string& text, const std::string& name, Json& out, std::string* err) {
    Xml doc;
    XmlParser p(text);
    if (!p.element(doc)) { if (err) *err = p.err; return false; }
    if (doc.name != "roblox") { if (err) *err = "not a Roblox model: the root is <" + doc.name + ">"; return false; }
    std::vector<const Xml*> roots;
    for (const Xml& c : doc.children) if (c.name == "Item") roots.push_back(&c);
    if (roots.empty()) { if (err) *err = "no <Item> in the model"; return false; }
    if (roots.size() == 1) { out = rbxmxItem(*roots[0], name); return true; }
    out.t = Json::Obj;
    out.o.emplace_back("className", jstr("Folder"));
    out.o.emplace_back("name", jstr(name));
    Json kids; kids.t = Json::Arr;
    for (const Xml* r : roots) kids.a.push_back(rbxmxItem(*r, ""));
    out.o.emplace_back("children", std::move(kids));
    return true;
}

static Instance::Ptr buildFromRbx(Runtime& rt, const Json& j, const std::string& name, const std::string& relPath, std::string* err) {
    RbxmxBuild build;
    g_rbxmx = &build;
    Instance::Ptr inst = buildModel(rt, j, name, relPath, err);
    g_rbxmx = nullptr;
    if (!inst) return nullptr;
    for (const RefFixup& f : build.refs) {
        auto it = build.referents.find(f.referent);
        if (it != build.referents.end()) f.inst->set(f.prop, Value::instance(it->second->id()));
    }
    return inst;
}

static Instance::Ptr buildRbxmx(Runtime& rt, const std::string& text, const std::string& name, const std::string& relPath, std::string* err) {
    Json j;
    if (!rbxmxToJson(text, name, j, err)) return nullptr;
    return buildFromRbx(rt, j, name, relPath, err);
}

// ---- *.rbxm: the binary model format ---------------------------------------
// "<roblox!", a 6-byte signature, u16 version 0, i32 class and instance counts, 8 reserved bytes;
// then chunks: a 4-byte name, u32 compressed length (0: stored as is), u32 length, 4 reserved
// bytes, the data as an LZ4 block. INST names one class's instances by referent, PROP one property
// of all of them, PRNT the tree. Arrays are interleaved big-endian: zigzag ints, cumulative
// referents, floats with the sign bit at the bottom. Each PROP chunk stands alone, so a type this
// runtime cannot use is simply not read.
static bool lz4Block(const std::string& in, size_t outLen, std::string& out) {
    out.clear(); out.reserve(outLen);
    size_t i = 0, n = in.size();
    auto byte = [&](size_t k) { return (uint8_t)in[k]; };
    while (i < n) {
        uint8_t token = byte(i++);
        size_t lit = token >> 4;
        if (lit == 15) { uint8_t x; do { if (i >= n) return false; x = byte(i++); lit += x; } while (x == 255); }
        if (i + lit > n) return false;
        out.append(in, i, lit); i += lit;
        if (i >= n) break;   // the last sequence is literals only
        if (i + 2 > n) return false;
        size_t off = byte(i) | (byte(i + 1) << 8); i += 2;
        if (off == 0 || off > out.size()) return false;
        size_t len = token & 15;
        if (len == 15) { uint8_t x; do { if (i >= n) return false; x = byte(i++); len += x; } while (x == 255); }
        len += 4;
        if (out.size() + len > outLen) return false;
        for (size_t k = 0; k < len; k++) out += out[out.size() - off];   // may overlap itself
    }
    return out.size() == outLen;
}

struct BinReader {
    const std::string& b; size_t i = 0; bool ok = true;
    explicit BinReader(const std::string& bytes) : b(bytes) {}
    bool need(size_t n) { if (i + n > b.size()) { ok = false; return false; } return true; }
    uint8_t u8() { if (!need(1)) return 0; return (uint8_t)b[i++]; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v = (uint16_t)((uint8_t)b[i] | ((uint8_t)b[i + 1] << 8)); i += 2; return v; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v = (uint8_t)b[i] | ((uint8_t)b[i + 1] << 8) | ((uint8_t)b[i + 2] << 16) | ((uint32_t)(uint8_t)b[i + 3] << 24); i += 4; return v; }
    float f32() { uint32_t v = u32(); float f; std::memcpy(&f, &v, 4); return f; }
    double f64() { if (!need(8)) return 0; uint64_t v = 0; for (int k = 7; k >= 0; k--) v = (v << 8) | (uint8_t)b[i + k]; i += 8; double d; std::memcpy(&d, &v, 8); return d; }
    std::string str() { uint32_t n = u32(); if (!need(n)) return ""; std::string s = b.substr(i, n); i += n; return s; }
    // n values of w bytes each, byte j of value k at [j * n + k], big-endian
    std::vector<uint64_t> interleaved(size_t n, size_t w) {
        std::vector<uint64_t> out(n, 0);
        if (!need(n * w)) return out;
        for (size_t j = 0; j < w; j++) for (size_t k = 0; k < n; k++) out[k] = (out[k] << 8) | (uint8_t)b[i + j * n + k];
        i += n * w;
        return out;
    }
    std::vector<int64_t> ints(size_t n, size_t w = 4) {
        std::vector<uint64_t> u = interleaved(n, w);
        std::vector<int64_t> out(n);
        for (size_t k = 0; k < n; k++) out[k] = (int64_t)(u[k] >> 1) ^ -(int64_t)(u[k] & 1);
        return out;
    }
    std::vector<float> floats(size_t n) {
        std::vector<uint64_t> u = interleaved(n, 4);
        std::vector<float> out(n);
        for (size_t k = 0; k < n; k++) { uint32_t bits = ((uint32_t)u[k] >> 1) | ((uint32_t)u[k] << 31); std::memcpy(&out[k], &bits, 4); }
        return out;
    }
    std::vector<int64_t> refs(size_t n) {
        std::vector<int64_t> out = ints(n);
        for (size_t k = 1; k < n; k++) out[k] += out[k - 1];
        return out;
    }
};

struct BinInst { std::string className, name; Json props, attrs, tags; bool hasAttrs = false; std::vector<int64_t> children; };
struct BinClass { std::string name; std::vector<int64_t> refs; };

static ZstdDecoder g_zstd = nullptr;

static bool rbxmChunk(const std::string& bytes, size_t& at, std::string& name, std::string& data, std::string* err) {
    if (at + 16 > bytes.size()) { if (err) *err = "truncated file"; return false; }
    name = bytes.substr(at, 4);
    while (!name.empty() && name.back() == '\0') name.pop_back();
    BinReader h(bytes); h.i = at + 4;
    uint32_t compressed = h.u32(), length = h.u32();
    at += 16;
    size_t stored = compressed ? compressed : length;
    if (at + stored > bytes.size()) { if (err) *err = "truncated " + name + " chunk"; return false; }
    if (!compressed) data = bytes.substr(at, stored);
    else {
        std::string raw = bytes.substr(at, stored);
        if (raw.size() >= 4 && (uint8_t)raw[0] == 0x28 && (uint8_t)raw[1] == 0xB5 && (uint8_t)raw[2] == 0x2F && (uint8_t)raw[3] == 0xFD) {
            // zstd, which is what current Studio writes: decoded by whatever the host installed.
            if (!g_zstd) { if (err) *err = "a zstd-compressed " + name + " chunk and no zstd here: save the model as .rbxmx instead"; return false; }
            if (!g_zstd(raw, length, data) || data.size() != length) { if (err) *err = "corrupt zstd data in the " + name + " chunk"; return false; }
            at += stored;
            return true;
        }
        if (!lz4Block(raw, length, data)) { if (err) *err = "corrupt LZ4 data in the " + name + " chunk"; return false; }
    }
    at += stored;
    return true;
}

// One PROP chunk's values -> the model.json spelling of them, or nothing.
static void rbxmProperty(BinReader& r, uint8_t type, const std::string& pname, const BinClass& cls, std::unordered_map<int64_t, BinInst>& insts) {
    size_t n = cls.refs.size();
    std::vector<Json> vals(n);
    auto each = [&](auto&& f) { for (size_t k = 0; k < n; k++) f(k); };
    switch (type) {
    case 0x01: {   // String, ProtectedString, Content, BinaryString
        if (pname == "AttributesSerialize") { each([&](size_t k) { BinInst& in = insts[cls.refs[k]]; in.attrs = decodeAttributes(r.str()); in.hasAttrs = true; }); return; }
        if (pname == "Tags") { each([&](size_t k) { insts[cls.refs[k]].tags = tagsFromBlob(r.str()); }); return; }
        // Roblox's own binary blobs (a union's mesh, a MeshPart's physics): not text.
        static const char* const blobs[] = {"MeshData", "PhysicsData", "ChildData", "LODData", "MeshData2", "ChildData2", "SolidMeshHolder",
                                            "PhysicsGrid", "MaterialVariantSerialized", "InitialSize", "FormFactor"};
        for (const char* b : blobs) if (pname == b) return;
        each([&](size_t k) { vals[k] = jstr(r.str()); });
        break;
    }
    case 0x02: each([&](size_t k) { vals[k].t = Json::Bool; vals[k].b = r.u8() != 0; }); break;
    case 0x03: { auto v = r.ints(n); each([&](size_t k) { vals[k] = jnum((double)v[k]); }); break; }
    case 0x04: { auto v = r.floats(n); each([&](size_t k) { vals[k] = jnum(v[k]); }); break; }
    case 0x05: each([&](size_t k) { vals[k] = jnum(r.f64()); }); break;
    case 0x06: { auto s = r.floats(n); auto o = r.ints(n); each([&](size_t k) { vals[k] = jtag("UDim", jarr({s[k], (double)o[k]})); }); break; }
    case 0x07: { auto xs = r.floats(n); auto ys = r.floats(n); auto xo = r.ints(n); auto yo = r.ints(n); each([&](size_t k) { vals[k] = jtag("UDim2", jarr({xs[k], (double)xo[k], ys[k], (double)yo[k]})); }); break; }
    case 0x0B: { auto v = r.interleaved(n, 4); each([&](size_t k) { vals[k] = jtag("BrickColor", jnum((double)v[k])); }); break; }
    case 0x0C: { auto cr = r.floats(n); auto cg = r.floats(n); auto cb = r.floats(n); each([&](size_t k) { vals[k] = jtag("Color3", jarr({cr[k], cg[k], cb[k]})); }); break; }
    case 0x0D: { auto x = r.floats(n); auto y = r.floats(n); each([&](size_t k) { vals[k] = jtag("Vector2", jarr({x[k], y[k]})); }); break; }
    case 0x0E: { auto x = r.floats(n); auto y = r.floats(n); auto z = r.floats(n); each([&](size_t k) { vals[k] = jtag("Vector3", jarr({x[k], y[k], z[k]})); }); break; }
    case 0x10: {   // CFrame: per value a rotation id (0: nine floats follow), then all the positions
        std::vector<std::array<float, 9>> rot(n);
        for (size_t k = 0; k < n && r.ok; k++) {
            uint8_t id = r.u8();
            if (id == 0) { for (int m = 0; m < 9; m++) rot[k][m] = r.f32(); continue; }
            // id - 1 = 6 * (X axis normal) + (Y axis normal); Z is their cross product; columns are the axes
            static const float N[6][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
            const float* X = N[((id - 1) / 6) % 6]; const float* Y = N[(id - 1) % 6];
            float Z[3] = {X[1] * Y[2] - X[2] * Y[1], X[2] * Y[0] - X[0] * Y[2], X[0] * Y[1] - X[1] * Y[0]};
            for (int row = 0; row < 3; row++) { rot[k][row * 3] = X[row]; rot[k][row * 3 + 1] = Y[row]; rot[k][row * 3 + 2] = Z[row]; }
        }
        auto x = r.floats(n); auto y = r.floats(n); auto z = r.floats(n);
        each([&](size_t k) {
            Json cf; cf.t = Json::Obj;
            cf.o.emplace_back("position", jarr({x[k], y[k], z[k]}));
            Json rows; rows.t = Json::Arr;
            for (int row = 0; row < 3; row++) rows.a.push_back(jarr({rot[k][row * 3], rot[k][row * 3 + 1], rot[k][row * 3 + 2]}));
            cf.o.emplace_back("orientation", std::move(rows));
            vals[k] = jtag("CFrame", std::move(cf));
        });
        break;
    }
    case 0x15: case 0x16: {   // NumberSequence: (time, value, envelope) per keypoint; ColorSequence: (time, r, g, b, envelope)
        bool color = type == 0x16;
        each([&](size_t k) {
            uint32_t cnt = r.u32();
            Json kps; kps.t = Json::Arr;
            for (uint32_t i = 0; i < cnt && r.ok; i++) {
                Json kp; kp.t = Json::Obj;
                kp.o.emplace_back("time", jnum(r.f32()));
                if (color) { double cr = r.f32(), cg = r.f32(), cb = r.f32(); r.f32(); kp.o.emplace_back("color", jarr({cr, cg, cb})); }
                else { double val = r.f32(), env = r.f32(); kp.o.emplace_back("value", jnum(val)); kp.o.emplace_back("envelope", jnum(env)); }
                kps.a.push_back(std::move(kp));
            }
            Json o; o.t = Json::Obj; o.o.emplace_back("keypoints", std::move(kps));
            vals[k] = jtag(color ? "ColorSequence" : "NumberSequence", std::move(o));
        });
        break;
    }
    case 0x17: each([&](size_t k) { double lo = r.f32(), hi = r.f32(); vals[k] = jtag("NumberRange", jarr({lo, hi})); }); break;
    case 0x12: { auto v = r.interleaved(n, 4); each([&](size_t k) { vals[k] = jtag("Enum", jnum((double)v[k])); }); break; }
    case 0x13: { auto v = r.refs(n); each([&](size_t k) { if (v[k] >= 0) vals[k] = jtag("Ref", jstr(std::to_string(v[k]))); }); break; }
    case 0x1A: {   // Color3uint8: the red bytes, the green bytes, the blue bytes
        if (!r.need(3 * n)) return;
        each([&](size_t k) { vals[k] = jtag("Color3uint8", jarr({(double)(uint8_t)r.b[r.i + k], (double)(uint8_t)r.b[r.i + n + k], (double)(uint8_t)r.b[r.i + 2 * n + k]})); });
        r.i += 3 * n;
        break;
    }
    case 0x1B: { auto v = r.ints(n, 8); each([&](size_t k) { vals[k] = jnum((double)v[k]); }); break; }
    case 0x20: {   // Font: family, weight, style, cached face id
        each([&](size_t k) {
            std::string family = r.str(); int weight = r.u16(); bool italic = r.u8() == 1; r.str();
            Json f; f.t = Json::Obj;
            f.o.emplace_back("family", jstr(family)); f.o.emplace_back("weight", jnum(weight)); f.o.emplace_back("style", jstr(italic ? "Italic" : "Normal"));
            vals[k] = jtag("Font", std::move(f));
        });
        break;
    }
    case 0x19: {   // PhysicalProperties: a flag byte a part; 1 = five custom floats, 3 = six (AcousticAbsorption last), 0 / 2 = the material's own
        each([&](size_t k) {
            uint8_t flag = r.u8();
            if (flag != 1 && flag != 3) return;
            double d = r.f32(), f = r.f32(), e = r.f32(), fw = r.f32(), ew = r.f32();
            if (flag == 3) r.f32();               // AcousticAbsorption, not kept here
            vals[k] = jtag("PhysicalProperties", jarr({d, f, e, fw, ew}));
        });
        break;
    }
    case 0x22: {   // Content: every value's source type, the uris in order, object referents, external referents
        auto kinds = r.ints(n);
        uint32_t uriCount = r.u32();
        std::vector<std::string> uris;
        for (uint32_t j = 0; j < uriCount && r.ok; j++) uris.push_back(r.str());
        uint32_t objects = r.u32();
        r.refs(objects);                  // an object in a file: nothing this loader can hand a script
        uint32_t external = r.u32();      // Roblox-internal, ignored
        if (r.need((size_t)external * 4)) r.i += (size_t)external * 4;
        size_t next = 0;
        each([&](size_t k) {
            if (kinds[k] == 0) vals[k] = jstr("");
            else if (kinds[k] == 1 && next < uris.size()) vals[k] = jstr(uris[next++]);
        });
        break;
    }
    default: return;   // Ray, Faces, Axes, sequences, ranges, Rect, SharedString, UniqueId, ...: not carried
    }
    if (!r.ok) return;
    for (size_t k = 0; k < n; k++) {
        if (vals[k].t == Json::Null) continue;
        BinInst& in = insts[cls.refs[k]];
        if (pname == "Name") { if (vals[k].t == Json::Str) in.name = vals[k].s; continue; }
        in.props.t = Json::Obj;
        in.props.o.emplace_back(rbxmxPropName(pname), std::move(vals[k]));
    }
}

static Json rbxmItem(std::unordered_map<int64_t, BinInst>& insts, int64_t ref, const std::string& nameOverride) {
    BinInst& in = insts[ref];
    Json j; j.t = Json::Obj;
    std::string className = findClass(in.className) ? in.className : "Folder";
    j.o.emplace_back("className", jstr(className));
    in.props.t = Json::Obj;
    j.o.emplace_back("referent", jstr(std::to_string(ref)));
    std::string name = nameOverride.empty() ? in.name : nameOverride;
    j.o.emplace_back("name", jstr(name.empty() ? className : name));
    j.o.emplace_back("properties", std::move(in.props));
    if (in.hasAttrs) j.o.emplace_back("attributes", std::move(in.attrs));
    if (in.tags.t == Json::Arr && !in.tags.a.empty()) j.o.emplace_back("tags", std::move(in.tags));
    Json kids; kids.t = Json::Arr;
    for (int64_t c : in.children) kids.a.push_back(rbxmItem(insts, c, ""));
    j.o.emplace_back("children", std::move(kids));
    return j;
}

static bool rbxmToJson(const std::string& bytes, const std::string& name, Json& out, std::string* err) {
    if (bytes.size() < 32 || bytes.compare(0, 8, "<roblox!") != 0) { if (err) *err = "not a Roblox binary model (no <roblox! header)"; return false; }
    std::unordered_map<uint32_t, BinClass> classes;
    std::unordered_map<int64_t, BinInst> insts;
    std::vector<int64_t> roots;
    size_t at = 32;
    std::string chunk, data;
    while (at < bytes.size()) {
        if (!rbxmChunk(bytes, at, chunk, data, err)) return false;
        if (chunk == "END") break;
        BinReader r(data);
        if (chunk == "INST") {
            uint32_t id = r.u32();
            BinClass cls; cls.name = r.str(); r.u8();   // object format byte: service markers, unused here
            cls.refs = r.refs(r.u32());
            if (!r.ok) { if (err) *err = "corrupt INST chunk"; return false; }
            for (int64_t ref : cls.refs) { insts[ref].className = cls.name; roots.push_back(ref); }
            classes[id] = std::move(cls);
        } else if (chunk == "PROP") {
            uint32_t id = r.u32();
            std::string pname = r.str();
            uint8_t type = r.u8();
            auto it = classes.find(id);
            if (!r.ok || it == classes.end()) { if (err) *err = "corrupt PROP chunk"; return false; }
            rbxmProperty(r, type, pname, it->second, insts);
        } else if (chunk == "PRNT") {
            r.u8();
            uint32_t n = r.u32();
            std::vector<int64_t> kids = r.refs(n), parents = r.refs(n);
            if (!r.ok) { if (err) *err = "corrupt PRNT chunk"; return false; }
            roots.clear();
            for (uint32_t k = 0; k < n; k++) {
                if (!insts.count(kids[k])) continue;
                if (parents[k] < 0 || !insts.count(parents[k])) roots.push_back(kids[k]);
                else insts[parents[k]].children.push_back(kids[k]);
            }
        }   // META, SSTR and the rest: nothing this reader needs
    }
    if (roots.empty()) { if (err) *err = "no instances in the model"; return false; }
    if (roots.size() == 1) { out = rbxmItem(insts, roots[0], name); return true; }
    out.t = Json::Obj;
    out.o.emplace_back("className", jstr("Folder"));
    out.o.emplace_back("name", jstr(name));
    Json kids; kids.t = Json::Arr;
    for (int64_t ref : roots) kids.a.push_back(rbxmItem(insts, ref, ""));
    out.o.emplace_back("children", std::move(kids));
    return true;
}

// A *.rbxm arrives base64-encoded: the sync channel carries text. An rbxmx renamed loads as XML.
static Instance::Ptr buildRbxm(Runtime& rt, const std::string& source, const std::string& name, const std::string& relPath, std::string* err) {
    std::string bytes = base64Decode(source);
    Json j;
    size_t first = bytes.find_first_not_of(" \t\r\n");
    bool xml = first != std::string::npos && bytes[first] == '<' && bytes.compare(0, 8, "<roblox!") != 0;
    if (xml ? !rbxmxToJson(bytes, name, j, err) : !rbxmToJson(bytes, name, j, err)) return nullptr;
    return buildFromRbx(rt, j, name, relPath, err);
}
// ---- writing an *.rbxmx ------------------------------------------------------------
// The inverse of the reader above: a *.model.json subtree out as the XML Studio reads, each
// property in the serialized form the class table gives it. Only what the model.json holds:
// Studio serializes changed properties only, so a partial set is a whole rbxmx, not a lossy one.
struct RbxmxWriter {
    bool place = false;                       // writing a whole place: services are roots, Terrain stays home
    std::string out;
    std::string err;
    const Json* root = nullptr;
    std::unordered_map<const Json*, std::string> referents;   // node -> RBX...
    int nextRef = 0;

    static void escape(const std::string& in, std::string& to) {
        for (char c : in) switch (c) {
        case '<': to += "&lt;"; break;
        case '>': to += "&gt;"; break;
        case '&': to += "&amp;"; break;
        case '"': to += "&quot;"; break;
        default: to += c;
        }
    }

    // Studio's referents are "RBX" and 32 hex digits. Counting up makes the same tree the same
    // bytes, which is what lets Place.gd's _put() skip a Save whose text already matches; random
    // referents, as Studio mints them, would rewrite every file on every Save.
    std::string mint() {
        char buf[40];
        std::snprintf(buf, sizeof buf, "RBX%032X", nextRef++);
        return buf;
    }

    static std::string num(double v) {
        if (v != v || v > 1e308 || v < -1e308) return "0";
        char buf[40];
        std::snprintf(buf, sizeof buf, "%.9g", v);
        return buf;
    }

    void open(int depth, const char* tag, const std::string& name) {
        out.append((size_t)depth * 2, ' ');
        out += '<'; out += tag; out += " name=\""; escape(name, out); out += "\">";
    }
    void close(const char* tag) { out += "</"; out += tag; out += ">\n"; }

    void simple(int depth, const char* tag, const std::string& name, const std::string& text) {
        open(depth, tag, name);
        escape(text, out);
        close(tag);
    }
    // <Vector3 name="size"><X>..</X>..</Vector3>: the parts have no name attribute.
    void parts(int depth, const char* tag, const std::string& name,
               std::initializer_list<const char*> keys, std::initializer_list<double> values) {
        open(depth, tag, name);
        auto k = keys.begin();
        for (double v : values) { out += '<'; out += *k; out += '>'; out += num(v); out += "</"; out += *k; out += '>'; ++k; }
        close(tag);
    }
};

// The name a property is serialized under. rbxmxPropName maps these back: the pair moves together.
static std::string rbxmxSerializedName(const ClassDef& cls, const std::string& prop) {
    if (cls.isA("BasePart")) {
        if (prop == "Size") return "size";
        if (prop == "Shape") return "shape";
        if (prop == "Color") return "Color3uint8";
    }
    if (cls.isA("Humanoid") && prop == "Health") return "Health_XML";
    return prop;
}

// Asset URLs are their own type in the file; this runtime keeps them as strings. The list mirrors
// the properties the class table declares as such, and grows with them.
static bool isContentProp(const std::string& prop) {
    static const char* const names[] = {"MeshId", "TextureId", "TextureID", "Texture", "Image",
                                        "SoundId", "HoverImage", "PressedImage", "SkyboxBk", "SkyboxDn",
                                        "SkyboxFt", "SkyboxLf", "SkyboxRt", "SkyboxUp", "Graphic",
                                        "ClickIcon", "TextureTop", "TextureBottom"};
    for (const char* n : names) if (prop == n) return true;
    return false;
}

// Numbers Studio serializes as <int>. Every other one goes out as <float>, whole or not, as
// Studio's own files do: <float name="Brightness">2.
static bool isIntProp(const std::string& prop) {
    static const char* const names[] = {"ZIndex", "LayoutOrder", "DisplayOrder", "MaxPlayers",
                                        "TeamSize", "UserId", "CursorPosition", "SelectionStart",
                                        "MaxVisibleGraphemes", "PlaceId", "GameId"};
    for (const char* n : names) if (prop == n) return true;
    return false;
}

static std::string base64Encode(const std::string& in) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < in.size(); i += 3) {
        unsigned v = (unsigned)(uint8_t)in[i] << 16;
        if (i + 1 < in.size()) v |= (unsigned)(uint8_t)in[i + 1] << 8;
        if (i + 2 < in.size()) v |= (unsigned)(uint8_t)in[i + 2];
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += i + 1 < in.size() ? T[(v >> 6) & 63] : '=';
        out += i + 2 < in.size() ? T[v & 63] : '=';
    }
    return out;
}

// The blob decodeAttributes reads, from the shapes a model.json holds an attribute in.
static void putU32(std::string& b, uint32_t v) { for (int i = 0; i < 4; i++) b += (char)((v >> (8 * i)) & 0xFF); }
static void putF32(std::string& b, float f) { uint32_t v; std::memcpy(&v, &f, 4); putU32(b, v); }
static void putF64(std::string& b, double d) { uint64_t v; std::memcpy(&v, &d, 8); for (int i = 0; i < 8; i++) b += (char)((v >> (8 * i)) & 0xFF); }
static void putStr(std::string& b, const std::string& s) { putU32(b, (uint32_t)s.size()); b += s; }

static std::string encodeAttributes(const Json& attrs) {
    std::string body;
    uint32_t count = 0;
    for (auto& [name, v] : attrs.o) {
        const Json* c = v.t == Json::Obj && v.o.size() == 1 ? &v.o[0].second : nullptr;
        bool colour = c && (v.o[0].first == "Color3" || v.o[0].first == "Color3uint8");
        if (v.t == Json::Str) { putStr(body, name); body += (char)0x02; putStr(body, v.s); }
        else if (v.t == Json::Bool) { putStr(body, name); body += (char)0x03; body += (char)(v.b ? 1 : 0); }
        else if (v.t == Json::Num) { putStr(body, name); body += (char)0x06; putF64(body, v.n); }
        else if (colour && c->isNums(3)) {
            bool bytes = v.o[0].first == "Color3uint8";
            putStr(body, name); body += (char)0x0F;
            for (int i = 0; i < 3; i++) putF32(body, bytes ? c->f(i) / 255.f : c->f(i));
        }
        else if (v.isNums(3)) { putStr(body, name); body += (char)0x11; for (int i = 0; i < 3; i++) putF32(body, v.f(i)); }
        else continue;
        count++;
    }
    if (!count) return "";
    std::string b;
    putU32(b, count);
    b += body;
    return b;
}

// One property out, typed by the class. False: no spelling for that value, so it is left out.
static bool writeProp(RbxmxWriter& w, int depth, const ClassDef& cls, const PropDef& def, const Json& v) {
    const std::string xmlName = rbxmxSerializedName(cls, def.name);
    switch (def.type) {
    case Value::Bool:
        if (v.t != Json::Bool) return false;
        w.simple(depth, "bool", xmlName, v.b ? "true" : "false");
        return true;
    case Value::Content: {
        std::string uri;
        if (v.t == Json::Str) uri = v.s;
        else if (const Json* u = v.t == Json::Obj ? v.get("Uri", "uri") : nullptr; u && u->t == Json::Str) uri = u->s;
        else if (v.t != Json::Null) return false;
        w.out.append((size_t)depth * 2, ' ');
        w.out += "<Content name=\""; RbxmxWriter::escape(xmlName, w.out); w.out += "\">";
        if (uri.empty()) w.out += "<null></null>";
        else { w.out += "<uri>"; RbxmxWriter::escape(uri, w.out); w.out += "</uri>"; }
        w.out += "</Content>\n";
        return true;
    }
    case Value::String: {
        if (v.t != Json::Str) return false;
        if (def.name == "Source") {
            w.out.append((size_t)depth * 2, ' ');
            w.out += "<ProtectedString name=\"Source\"><![CDATA[";
            std::string text = v.s;
            for (size_t at = text.find("]]>"); at != std::string::npos; at = text.find("]]>", at + 5))
                text.replace(at, 3, "]]]]><![CDATA[>");   // the one sequence a CDATA cannot hold
            w.out += text;
            w.out += "]]></ProtectedString>\n";
            return true;
        }
        if (isContentProp(def.name)) {
            w.out.append((size_t)depth * 2, ' ');
            w.out += "<Content name=\""; RbxmxWriter::escape(xmlName, w.out); w.out += "\"><url>";
            RbxmxWriter::escape(v.s, w.out);
            w.out += "</url></Content>\n";
            return true;
        }
        w.simple(depth, "string", xmlName, v.s);
        return true;
    }
    case Value::Number:
        if (v.t != Json::Num) return false;
        w.simple(depth, isIntProp(def.name) ? "int" : "float", xmlName, RbxmxWriter::num(v.n));
        return true;
    case Value::Enum: {
        if (v.t != Json::Str || !def.enumType) return false;
        const EnumItem* item = def.enumType->find(v.s);
        if (!item) return false;
        w.simple(depth, "token", xmlName, std::to_string(item->value));
        return true;
    }
    case Value::Vector3:
        if (!v.isNums(3)) return false;
        w.parts(depth, "Vector3", xmlName, {"X", "Y", "Z"}, {v.a[0].n, v.a[1].n, v.a[2].n});
        return true;
    case Value::Vector2:
        if (!v.isNums(2)) return false;
        w.parts(depth, "Vector2", xmlName, {"X", "Y"}, {v.a[0].n, v.a[1].n});
        return true;
    case Value::UDim:
        if (!v.isNums(2)) return false;
        w.parts(depth, "UDim", xmlName, {"S", "O"}, {v.a[0].n, v.a[1].n});
        return true;
    case Value::UDim2: {
        double f[4];
        if (v.t == Json::Arr && v.a.size() == 2 && v.a[0].isNums(2) && v.a[1].isNums(2)) {
            f[0] = v.a[0].a[0].n; f[1] = v.a[0].a[1].n; f[2] = v.a[1].a[0].n; f[3] = v.a[1].a[1].n;
        } else if (v.isNums(4)) {
            for (int i = 0; i < 4; i++) f[i] = v.a[i].n;
        } else return false;
        w.parts(depth, "UDim2", xmlName, {"XS", "XO", "YS", "YO"}, {f[0], f[1], f[2], f[3]});
        return true;
    }
    case Value::Color3: {
        // A BasePart's colour packs into one integer, top byte an unused alpha; the rest are <R><G><B>.
        const Json* rgb = &v;
        bool bytes = false;
        if (v.t == Json::Obj && v.o.size() == 1 && (v.o[0].first == "Color3uint8" || v.o[0].first == "Color3")) {
            bytes = v.o[0].first == "Color3uint8";
            rgb = &v.o[0].second;
        }
        if (!rgb->isNums(3)) return false;
        double r = bytes ? rgb->a[0].n / 255.0 : rgb->a[0].n;
        double g = bytes ? rgb->a[1].n / 255.0 : rgb->a[1].n;
        double b = bytes ? rgb->a[2].n / 255.0 : rgb->a[2].n;
        if (xmlName == "Color3uint8") {
            auto clamp255 = [](double c) { long v255 = std::lround(c * 255.0); return (unsigned)(v255 < 0 ? 0 : v255 > 255 ? 255 : v255); };
            unsigned packed = 0xFF000000u | (clamp255(r) << 16) | (clamp255(g) << 8) | clamp255(b);
            w.simple(depth, "Color3uint8", xmlName, std::to_string(packed));
            return true;
        }
        w.parts(depth, "Color3", xmlName, {"R", "G", "B"}, {r, g, b});
        return true;
    }
    case Value::NumberRange: {
        if (!v.isNums(2)) return false;
        w.simple(depth, "NumberRange", xmlName, RbxmxWriter::num(v.a[0].n) + " " + RbxmxWriter::num(v.a[1].n) + " ");
        return true;
    }
    case Value::NumberSequence:
    case Value::ColorSequence: {
        const Json* kps = v.t == Json::Obj ? v.get("keypoints") : &v;
        if (!kps || kps->t != Json::Arr) return false;
        bool colour = def.type == Value::ColorSequence;
        std::string text;
        for (const Json& kp : kps->a) {
            const Json* t = kp.get("time");
            if (!t || t->t != Json::Num) continue;
            // Whole keypoints only: one stream read back in fixed groups, so half a one shifts the rest.
            std::string one = RbxmxWriter::num(t->n) + " ";
            if (colour) {
                const Json* c = kp.get("color");
                if (!c) continue;
                bool bytes = false;
                if (c->t == Json::Obj && c->o.size() == 1 && (c->o[0].first == "Color3uint8" || c->o[0].first == "Color3")) {
                    bytes = c->o[0].first == "Color3uint8";
                    c = &c->o[0].second;
                }
                if (!c->isNums(3)) continue;
                for (int i = 0; i < 3; i++) { one += RbxmxWriter::num(bytes ? c->a[i].n / 255.0 : c->a[i].n); one += ' '; }
                one += "0 ";                                    // the envelope Roblox writes and ignores
            } else {
                const Json* val = kp.get("value");
                const Json* env = kp.get("envelope");
                one += RbxmxWriter::num(val && val->t == Json::Num ? val->n : 0); one += ' ';
                one += RbxmxWriter::num(env && env->t == Json::Num ? env->n : 0); one += ' ';
            }
            text += one;
        }
        w.simple(depth, colour ? "ColorSequence" : "NumberSequence", xmlName, text);
        return true;
    }
    case Value::PhysProps: {
        if (!v.isNums(5)) return false;
        w.open(depth, "PhysicalProperties", xmlName);
        w.out += "<CustomPhysics>true</CustomPhysics>";
        static const char* const keys[5] = {"Density", "Friction", "Elasticity", "FrictionWeight", "ElasticityWeight"};
        for (int i = 0; i < 5; i++) { w.out += '<'; w.out += keys[i]; w.out += '>'; w.out += RbxmxWriter::num(v.a[i].n); w.out += "</"; w.out += keys[i]; w.out += '>'; }
        w.close("PhysicalProperties");
        return true;
    }
    case Value::Font: {
        if (v.t != Json::Obj) return false;
        const Json* fam = v.get("family");
        const Json* weight = v.get("weight");
        const Json* style = v.get("style");
        w.open(depth, "Font", xmlName);
        w.out += "<Family><url>"; RbxmxWriter::escape(fam && fam->t == Json::Str ? fam->s : "", w.out); w.out += "</url></Family>";
        w.out += "<Weight>"; w.out += RbxmxWriter::num(weight && weight->t == Json::Num ? weight->n : 400); w.out += "</Weight>";
        w.out += "<Style>"; w.out += style && style->t == Json::Str ? style->s : "Normal"; w.out += "</Style>";
        w.close("Font");
        return true;
    }
    default:
        return false;   // Ref is written by the caller, which knows the tree
    }
}

// Every node holds its referent before anything is written: a Ref may point forwards.
static void mintReferents(RbxmxWriter& w, const Json& node) {
    w.referents[&node] = w.mint();
    if (const Json* kids = node.get("children"))
        if (kids->t == Json::Arr)
            for (const Json& k : kids->a) mintReferents(w, k);
}

// A model.json Ref is a path down from the root ("Kart/Seat"); the file wants that node's referent.
static const Json* nodeAtPath(const Json& root, const std::string& path) {
    const Json* at = &root;
    size_t start = 0;
    while (start <= path.size()) {
        size_t slash = path.find('/', start);
        std::string seg = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!seg.empty()) {
            const Json* kids = at->get("children");
            const Json* found = nullptr;
            if (kids && kids->t == Json::Arr)
                for (const Json& k : kids->a) {
                    const Json* n = k.get("name");
                    if (n && n->t == Json::Str && n->s == seg) { found = &k; break; }
                }
            if (!found) return nullptr;
            at = found;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return at == &root ? nullptr : at;
}

static void writeItem(RbxmxWriter& w, const Json& node, const std::string& name, int depth) {
    const Json* clsName = node.get("className");
    std::string className = clsName && clsName->t == Json::Str ? clsName->s : "Folder";
    const ClassDef* cls = findClass(className);
    if (!cls) { className = "Folder"; cls = findClass(className); }
    const std::string pad((size_t)depth * 2, ' ');
    w.out += pad + "<Item class=\"";
    RbxmxWriter::escape(className, w.out);
    w.out += "\" referent=\"" + w.referents[&node] + "\">\n";
    w.out += pad + "  <Properties>\n";
    w.simple(depth + 2, "string", "Name", name);

    const Json* props = node.get("properties");
    // The pairs a CFrame splits into; must match the reader's list (applyJsonProperties) exactly.
    // AlignOrientation is absent from both on purpose, though it does declare the pair.
    struct CfPair { const char* pos; const char* ori; const char* xml; };
    std::vector<CfPair> pairs;
    if (cls && (cls->isA("BasePart") || cls->isA("Camera") || cls->isA("Attachment")))
        pairs.push_back({"Position", "Orientation", "CFrame"});
    if (cls && cls->isA("JointInstance")) {
        pairs.push_back({"C0Position", "C0Orientation", "C0"});
        pairs.push_back({"C1Position", "C1Orientation", "C1"});
    }
    if (cls && (cls->isA("Pose") || cls->isA("BodyGyro")))
        pairs.push_back({"CFramePosition", "CFrameOrientation", "CFrame"});
    if (cls && cls->isA("CFrameValue"))
        pairs.push_back({"ValuePosition", "ValueOrientation", "Value"});
    if (props && props->t == Json::Obj) {
        for (const CfPair& pr : pairs) {
            const Json* pos = props->get(pr.pos);
            const Json* ori = props->get(pr.ori);
            if (!(pos && pos->isNums(3)) && !(ori && ori->isNums(3))) continue;
            Vec3 p{0, 0, 0}, o{0, 0, 0};
            if (pos && pos->isNums(3)) p = {pos->f(0), pos->f(1), pos->f(2)};
            if (ori && ori->isNums(3)) o = {ori->f(0), ori->f(1), ori->f(2)};
            CFrameV cf = cframeFromPosOrient(p, o);
            w.open(depth + 2, "CoordinateFrame", pr.xml);
            static const char* const axis[3] = {"X", "Y", "Z"};
            const double xyz[3] = {cf.p.x, cf.p.y, cf.p.z};
            for (int i = 0; i < 3; i++) {
                w.out += '<'; w.out += axis[i]; w.out += '>';
                w.out += RbxmxWriter::num(xyz[i]);
                w.out += "</"; w.out += axis[i]; w.out += '>';
            }
            for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) {
                const char tag[4] = {'R', (char)('0' + r), (char)('0' + c), 0};
                w.out += '<'; w.out += tag; w.out += '>';
                w.out += RbxmxWriter::num(cf.m[r * 3 + c]);
                w.out += "</"; w.out += tag; w.out += '>';
            }
            w.close("CoordinateFrame");
        }
        for (auto& [pname, pv] : props->o) {
            if (pname == "Name" || pname == "Parent") continue;
            if (pname == "Heights") continue;                                   // this engine's voxels: Studio has no use for them
            if (pname == "SmoothGrid" || pname == "MaterialColors") { if (pv.t == Json::Str && !pv.s.empty()) w.simple(depth + 2, "BinaryString", pname, pv.s); continue; }
            bool half = false;
            for (const CfPair& pr : pairs) if (pname == pr.pos || pname == pr.ori) half = true;
            if (half) continue;
            const PropDef* def = cls ? cls->findProp(pname) : nullptr;
            if (!def) continue;
            if (def->type == Value::Ref) {
                // A Ref out of the subtree never reached the model.json; one inside names a node.
                if (pv.t != Json::Str) continue;
                const Json* target = nodeAtPath(*w.root, pv.s);
                w.simple(depth + 2, "Ref", pname, target ? w.referents[target] : "null");
                continue;
            }
            if (def->type == Value::String && !def->twin.empty()) {
                // Image and ImageContent are one value; the file carries the Content, so the string goes out as one.
                if (props->get(def->twin.c_str())) continue;
                if (const PropDef* cd = cls->findProp(def->twin)) writeProp(w, depth + 2, *cls, *cd, pv);
                continue;
            }
            writeProp(w, depth + 2, *cls, *def, pv);
        }
    }
    const Json* attrs = node.get("attributes");
    if (attrs && attrs->t == Json::Obj) {
        std::string blob = encodeAttributes(*attrs);
        if (!blob.empty()) w.simple(depth + 2, "BinaryString", "AttributesSerialize", base64Encode(blob));
    }
    const Json* tags = node.get("tags");
    if (tags && tags->t == Json::Arr && !tags->a.empty()) {
        std::string blob;
        for (const Json& t : tags->a) if (t.t == Json::Str && !t.s.empty()) { if (!blob.empty()) blob += '\0'; blob += t.s; }
        if (!blob.empty()) w.simple(depth + 2, "BinaryString", "Tags", base64Encode(blob));
    }
    w.out += pad + "  </Properties>\n";
    if (const Json* kids = node.get("children"))
        if (kids->t == Json::Arr)
            for (const Json& k : kids->a) {
                const Json* kn = k.get("name");
                const Json* kc = k.get("className");
                if (w.place && kc && kc->t == Json::Str && kc->s == "Terrain") {
                    // Studio makes its own Terrain; ours goes only when it carries voxels for it
                    const Json* tp = k.get("properties");
                    const Json* sg = tp ? tp->get("SmoothGrid") : nullptr;
                    if (!(sg && sg->t == Json::Str && !sg->s.empty())) continue;
                }
                writeItem(w, k, kn && kn->t == Json::Str ? kn->s : "Instance", depth + 1);
            }
    w.out += pad + "</Item>\n";
}


// ---- a whole place ------------------------------------------------------------------
static int countItems(const Json& j) {
    int n = 1;
    if (const Json* kids = j.get("children")) if (kids->t == Json::Arr) for (const Json& k : kids->a) n += countItems(k);
    return n;
}

static int importPlaceItems(Runtime& rt, const Json& doc, std::string* note) {
    DataModel& dm = rt.dataModel();
    // One root is that instance; several came back wrapped in a Folder.
    std::vector<const Json*> roots;
    const Json* cls = doc.get("className");
    const Json* kids = doc.get("children");
    if (cls && cls->t == Json::Str && cls->s == "Folder" && kids && kids->t == Json::Arr) for (const Json& k : kids->a) roots.push_back(&k);
    else roots.push_back(&doc);
    RbxmxBuild build;
    g_rbxmx = &build;
    int count = 0;
    std::vector<std::pair<std::string, int>> skipped;   // name, instances under it
    size_t terrainBytes = 0;
    for (const Json* r : roots) {
        const Json* rc = r->get("className");
        const Json* rn = r->get("name");
        std::string className = rc && rc->t == Json::Str ? rc->s : "";
        std::string rname = rn && rn->t == Json::Str ? rn->s : className;
        const ClassDef* def = findClass(className);
        // Plugins come from plugin_add and nowhere else: shouldRun() (rbx_runtime.cpp) lets a
        // plugin Script run in an edit world, so an imported one would run with Play never pressed.
        if (className == "PluginDebugService") { skipped.push_back({rname, countItems(*r) - 1}); continue; }
        Instance* svc = def && def->service ? dm.getService(className) : nullptr;
        if (!svc) { skipped.push_back({rname, countItems(*r) - 1}); continue; }
        if (const Json* ref = r->get("referent")) if (ref->t == Json::Str) build.referents[ref->s] = svc;
        applyJsonProperties(rt, *svc, r->get("properties"), rname);
        applyJsonAttributes(rt, *svc, r->get("attributes"), rname);
        applyJsonTags(*svc, r->get("tags"));
        const Json* rk = r->get("children");
        if (!rk || rk->t != Json::Arr) continue;
        for (const Json& k : rk->a) {
            const Json* kc = k.get("className");
            const Json* kn = k.get("name");
            std::string kclass = kc && kc->t == Json::Str ? kc->s : "";
            std::string kname = kn && kn->t == Json::Str ? kn->s : kclass;
            if (kclass == "Terrain") {
                // Onto the Terrain this world keeps. A two-byte SmoothGrid is a version and a
                // chunk size alone: an empty Terrain.
                const Json* tp = k.get("properties");
                const Json* sg = tp ? tp->get("SmoothGrid") : nullptr;
                if (Instance* t = svc->findFirstChildOfClass("Terrain")) {
                    if (sg && sg->t == Json::Str && sg->s.size() > 2) { t->set("SmoothGrid", Value::string(base64Encode(sg->s))); terrainBytes = sg->s.size(); }
                    const Json* mc = tp ? tp->get("MaterialColors") : nullptr;
                    if (mc && mc->t == Json::Str && mc->s.size() == 69) t->set("MaterialColors", Value::string(base64Encode(mc->s)));
                }
                continue;
            }
            // Something already here under that name and class takes the file's properties and children.
            Instance* have = svc->findFirstChild(kname);
            if (have && have->className() == kclass) {
                if (const Json* ref = k.get("referent")) if (ref->t == Json::Str) build.referents[ref->s] = have;
                applyJsonProperties(rt, *have, k.get("properties"), kname);
                applyJsonAttributes(rt, *have, k.get("attributes"), kname);
                applyJsonTags(*have, k.get("tags"));
                if (const Json* gk = k.get("children")) if (gk->t == Json::Arr)
                    for (const Json& g : gk->a) {
                        const Json* gn = g.get("name");
                        std::string err;
                        Instance::Ptr made = buildModel(rt, g, gn && gn->t == Json::Str ? gn->s : "Instance", kname, &err);
                        if (!made) { rt.reportError(kname, err); continue; }
                        made->setParent(have);
                        count += countItems(g);
                    }
                count++;
                continue;
            }
            std::string err;
            Instance::Ptr made = buildModel(rt, k, kname, rname, &err);
            if (!made) { rt.reportError(rname, err); continue; }
            made->setParent(svc);
            count += countItems(k);
        }
    }
    g_rbxmx = nullptr;
    for (const RefFixup& f : build.refs) {
        auto it = build.referents.find(f.referent);
        if (it != build.referents.end()) f.inst->set(f.prop, Value::instance(it->second->id()));
    }
    if (note) {
        std::string n;
        if (terrainBytes) n += "the Terrain's voxels came too (" + std::to_string((terrainBytes + 1023) / 1024) + " KB of SmoothGrid); ";
        if (!skipped.empty()) {
            // A service with something under it is named; empty ones are only counted.
            std::string full;
            int empty = 0;
            for (auto& [sname, inside] : skipped) {
                if (!inside) { empty++; continue; }
                full += (full.empty() ? "" : ", ") + sname + " (" + std::to_string(inside) + " inside)";
            }
            n += "not a service here, skipped: " + full;
            if (empty) n += std::string(full.empty() ? "" : ", and ") + std::to_string(empty) + " empty ones Studio keeps for itself";
        }
        *note = n;
    }
    return count;
}
} // namespace

int importPlace(Runtime& rt, const std::string& bytes, std::string* err, std::string* note) {
    Json j;
    size_t first = bytes.find_first_not_of(" \t\r\n");
    bool xml = first != std::string::npos && bytes[first] == '<' && bytes.compare(0, 8, "<roblox!") != 0;
    if (xml ? !rbxmxToJson(bytes, "Place", j, err) : !rbxmToJson(bytes, "Place", j, err)) return -1;
    return importPlaceItems(rt, j, note);
}

bool modelJsonToRbxmx(const std::string& json, const std::string& name, std::string& out, std::string* err) {
    Json root;
    if (!parseJson(json, root, err) || root.t != Json::Obj) {
        if (err && err->empty()) *err = "not a model";
        return false;
    }
    RbxmxWriter w;
    w.root = &root;
    mintReferents(w, root);
    w.out = "<roblox xmlns:xmime=\"http://www.w3.org/2005/05/xmlmime\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
            " xsi:noNamespaceSchemaLocation=\"http://www.roblox.com/roblox.xsd\" version=\"4\">\n"
            "  <Meta name=\"ExplicitAutoJoints\">true</Meta>\n"
            "  <External>null</External>\n"
            "  <External>nil</External>\n";
    writeItem(w, root, name, 1);
    w.out += "</roblox>\n";
    out = std::move(w.out);
    return true;
}

bool placeJsonToRbxlx(const std::string& json, std::string& out, std::string* err) {
    Json root;
    if (!parseJson(json, root, err) || root.t != Json::Obj) {
        if (err && err->empty()) *err = "not a place";
        return false;
    }
    const Json* kids = root.get("children");
    if (!kids || kids->t != Json::Arr || kids->a.empty()) { if (err) *err = "a place with no services in it"; return false; }
    RbxmxWriter w;
    w.place = true;
    w.root = &root;
    mintReferents(w, root);
    w.out = "<roblox xmlns:xmime=\"http://www.w3.org/2005/05/xmlmime\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
            " xsi:noNamespaceSchemaLocation=\"http://www.roblox.com/roblox.xsd\" version=\"4\">\n"
            "  <Meta name=\"ExplicitAutoJoints\">true</Meta>\n"
            "  <External>null</External>\n"
            "  <External>nil</External>\n";
    for (const Json& k : kids->a) {
        const Json* kn = k.get("name");
        const Json* kc = k.get("className");
        std::string name = kn && kn->t == Json::Str ? kn->s : kc && kc->t == Json::Str ? kc->s : "Instance";
        writeItem(w, k, name, 1);
    }
    w.out += "</roblox>\n";
    out = std::move(w.out);
    return true;
}

void setZstdDecoder(ZstdDecoder fn) { g_zstd = fn; }

bool classifySourceFile(const std::string& relPath, SourceFileInfo& out) {
    std::string p = relPath;
    for (char& c : p) if (c == '\\') c = '/';
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= p.size()) {
        size_t slash = p.find('/', start);
        std::string seg = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!seg.empty() && seg != ".") parts.push_back(seg);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    if (parts.empty()) return false;
    std::string file = parts.back();
    parts.pop_back();
    std::string stem, cls;
    out.kind = SourceFileInfo::Source;
    out.initMeta = false;
    out.rbxmx = false; out.rbxm = false;
    if (endsWith(file, ".server.luau") || endsWith(file, ".server.lua")) { cls = "Script"; stem = file.substr(0, file.rfind(".server.")); }
    else if (endsWith(file, ".client.luau") || endsWith(file, ".client.lua")) { cls = "LocalScript"; stem = file.substr(0, file.rfind(".client.")); }
    else if (endsWith(file, ".luau") || endsWith(file, ".lua")) { cls = "ModuleScript"; stem = file.substr(0, file.rfind('.')); }
    else if (endsWith(file, ".model.json")) { out.kind = SourceFileInfo::Model; stem = file.substr(0, file.size() - 11); }
    else if (endsWith(file, ".rbxmx")) { out.kind = SourceFileInfo::Model; out.rbxmx = true; stem = file.substr(0, file.size() - 6); }
    else if (endsWith(file, ".rbxm")) { out.kind = SourceFileInfo::Model; out.rbxm = true; stem = file.substr(0, file.size() - 5); }
    else if (endsWith(file, ".meta.json")) { out.kind = SourceFileInfo::Meta; stem = file.substr(0, file.size() - 10); }
    else return false;
    if (stem.empty()) return false;
    if (stem == "init") {
        // The directory itself is the instance; its other files are that instance's children.
        if (parts.empty() || out.kind == SourceFileInfo::Model) return false;
        stem = parts.back();
        parts.pop_back();
        out.initMeta = out.kind == SourceFileInfo::Meta;
    }
    out.containers = std::move(parts);
    out.className = cls;
    out.name = stem;
    return true;
}

// A top-level name is a service, created on demand; a deeper one a Folder, unless a script stands there.
static Instance* ensureContainers(Runtime& rt, const std::vector<std::string>& containers, std::string* err) {
    DataModel& dm = rt.dataModel();
    Instance* cur = dm.root();
    for (size_t i = 0; i < containers.size(); i++) {
        const std::string& name = containers[i];
        if (i == 0 && name == "PluginDebugService" && !rt.impl().pluginFilesAllowed) { if (err) *err = "PluginDebugService is the Studio's: a place file cannot go there"; return nullptr; }
        Instance* next = cur->findFirstChild(name);
        if (!next) {
            if (i == 0) {
                next = dm.getService(name);
                if (!next) { if (err) *err = "'" + name + "' is not a valid Service name"; return nullptr; }
            } else {
                next = dm.create("Folder", cur).get();
                next->setName(name);
            }
        }
        cur = next;
    }
    return cur;
}

// *.meta.json: the instance's class (a Folder is replaced by it, children kept) and properties.
// At the top level it is a service's, as Lighting/init.meta.json is. Early arrivals wait in pendingMeta.
static Instance* applyMeta(Runtime& rt, const SourceFileInfo& info, const std::string& relPath, const std::string& text, std::string* err) {
    Json j;
    if (!parseJson(text, j, err)) return nullptr;
    Instance* parent = ensureContainers(rt, info.containers, err);
    if (!parent) return nullptr;
    DataModel& dm = rt.dataModel();
    if (info.containers.empty() && info.name == "PluginDebugService" && !rt.impl().pluginFilesAllowed) { if (err) *err = "PluginDebugService is the Studio's: a place file cannot go there"; return nullptr; }
    Instance* target = parent->findFirstChild(info.name);
    if (!target && info.containers.empty()) {
        target = dm.getService(info.name);
        if (!target) { if (err) *err = "'" + info.name + "' is not a valid Service name"; return nullptr; }
    }
    if (info.containers.empty()) {
        const Json* cls = j.get("className", "ClassName");
        if (cls && cls->t == Json::Str && cls->s != target->className()) { if (err) *err = info.name + " is a " + target->className() + ", not a " + cls->s; return nullptr; }
        applyJsonProperties(rt, *target, j.get("properties", "Properties"), relPath);
        applyJsonAttributes(rt, *target, j.get("attributes", "Attributes"), relPath);
        applyJsonTags(*target, j.get("tags", "Tags"));
        return target;
    }
    const Json* cls = j.get("className", "ClassName");
    if (cls && cls->t == Json::Str && (!target || target->className() != cls->s)) {
        if (!findClass(cls->s)) { if (err) *err = cls->s + " is not a valid class"; return nullptr; }
        Instance::Ptr inst = dm.createInternal(cls->s);
        inst->setName(info.name);
        if (target) {
            std::vector<Instance::Ptr> kids = target->children();
            for (auto& k : kids) k->setParent(inst.get());
            target->destroy();
        }
        inst->setParent(parent);
        target = inst.get();
    }
    if (!target) { rt.impl().pendingMeta[info.path()] = text; return parent; }
    applyJsonProperties(rt, *target, j.get("properties", "Properties"), relPath);
    applyJsonAttributes(rt, *target, j.get("attributes", "Attributes"), relPath);
    applyJsonTags(*target, j.get("tags", "Tags"));
    return target;
}

static void applyPendingMeta(Runtime& rt, const SourceFileInfo& info) {
    auto it = rt.impl().pendingMeta.find(info.path());
    if (it == rt.impl().pendingMeta.end()) return;
    std::string text = it->second;
    rt.impl().pendingMeta.erase(it);
    std::string err;
    if (!applyMeta(rt, info, info.path() + ".meta.json", text, &err)) rt.reportError(info.path() + ".meta.json", err);
}

// A *.model.json subtree added under a container path, with no file behind it and nothing replaced.
Instance* addModelJson(Runtime& rt, const std::vector<std::string>& containers, const std::string& name,
                       const std::string& json, std::string* err) {
    Instance* parent = ensureContainers(rt, containers, err);
    if (!parent) return nullptr;
    Json j;
    if (!parseJson(json, j, err)) return nullptr;
    std::string where = name;
    for (auto it = containers.rbegin(); it != containers.rend(); ++it) where = *it + "/" + where;
    ModelBuild model;
    // A container the runtime keeps one of at a fixed id is not doubled; the model applies onto
    // the one there. The copy into PlayerScripts takes the first, so a twin's LocalScripts never run.
    const Json* jc = j.get("className", "ClassName");
    std::string cls = jc && jc->t == Json::Str ? jc->s : "";
    Instance* have = nullptr;
    for (const char* one : {"StarterPlayerScripts", "StarterCharacterScripts", "Camera", "Terrain"})
        if (cls == one) { have = parent->findFirstChildOfClass(cls); break; }
    if (have) {
        g_model = &model;
        applyJsonProperties(rt, *have, j.get("properties", "Properties"), where);
        applyJsonAttributes(rt, *have, j.get("attributes", "Attributes"), where);
        applyJsonTags(*have, j.get("tags", "Tags"));
        if (const Json* kids = j.get("children", "Children")) if (kids->t == Json::Arr)
            for (const Json& k : kids->a) {
                const Json* kn = k.get("name", "Name");
                std::string kname = kn && kn->t == Json::Str ? kn->s : "Instance";
                std::string kerr;
                Instance::Ptr made = buildModel(rt, k, kname, where + "/" + kname, &kerr);
                if (!made) { rt.reportError(where, kerr); continue; }
                made->setParent(have);
            }
        g_model = nullptr;
        for (const RefFixup& f : model.refs) {
            Instance* to = resolveRefPath(rt, have, f.referent);
            std::string bad;
            if (!to) rt.reportError(where, f.prop + ": there is no instance at " + f.referent);
            else if (!f.inst->set(f.prop, Value::instance(to->id()), &bad)) rt.reportError(where, f.prop + ": " + bad);
        }
        return have;
    }
    g_model = &model;
    Instance::Ptr inst = buildModel(rt, j, name, where, err);
    g_model = nullptr;
    if (!inst) return nullptr;
    inst->setParent(parent);
    for (const RefFixup& f : model.refs) {
        Instance* to = resolveRefPath(rt, inst.get(), f.referent);
        std::string bad;
        if (!to) rt.reportError(where, f.prop + ": there is no instance at " + f.referent);
        else if (!f.inst->set(f.prop, Value::instance(to->id()), &bad)) rt.reportError(where, f.prop + ": " + bad);
    }
    return inst.get();
}

Instance* loadSourceFile(Runtime& rt, const std::string& relPath, const std::string& source, std::string* err) {
    SourceFileInfo info;
    if (!classifySourceFile(relPath, info)) { if (err) *err = "not a Luau source file: " + relPath; return nullptr; }
    if (info.kind == SourceFileInfo::Meta) return applyMeta(rt, info, relPath, source, err);
    Instance* parent = ensureContainers(rt, info.containers, err);
    if (!parent) return nullptr;
    DataModel& dm = rt.dataModel();
    Instance* existing = parent->findFirstChild(info.name);
    if (info.kind == SourceFileInfo::Model) {
        // Built before the old subtree is destroyed: a file that fails to parse leaves what is there.
        Instance::Ptr inst;
        if (info.rbxmx) inst = buildRbxmx(rt, source, info.name, relPath, err);
        else if (info.rbxm) inst = buildRbxm(rt, source, info.name, relPath, err);
        ModelBuild model;
        if (!info.rbxmx && !info.rbxm) {
            Json j;
            if (!parseJson(source, j, err)) return nullptr;
            // One Terrain per Workspace, at a fixed id the client makes too and expects the
            // server's Heights to land on. A second at a fresh id arrives nowhere.
            if (const Json* cn = j.get("className"); cn && cn->t == Json::Str && cn->s == "Terrain")
                if (Instance* ter = parent->findFirstChildOfClass("Terrain")) {
                    applyJsonProperties(rt, *ter, j.get("properties"), relPath);
                    applyPendingMeta(rt, info);
                    return ter;
                }
            g_model = &model;
            inst = buildModel(rt, j, info.name, relPath, err);
            g_model = nullptr;
        }
        if (!inst) return nullptr;
        if (existing) existing->destroy();
        inst->setParent(parent);
        for (const RefFixup& f : model.refs) {
            Instance* to = resolveRefPath(rt, inst.get(), f.referent);
            std::string bad;
            if (!to) rt.reportError(relPath, f.prop + ": there is no instance at " + f.referent);
            else if (!f.inst->set(f.prop, Value::instance(to->id()), &bad)) rt.reportError(relPath, f.prop + ": " + bad);
        }
        applyPendingMeta(rt, info);
        return inst.get();
    }
    if (existing && existing->className() == info.className) {
        // Same file saved again: the instance and its children (init.luau's siblings) stay.
        rt.reloadSource(*existing, source);
        return existing;
    }
    Instance::Ptr inst = dm.create(info.className);
    inst->setName(info.name);
    inst->set("Source", Value::string(source));
    if (existing && !existing->isA("BaseScript")) {
        // init.luau arrived after its siblings: what stood in for the directory becomes the script.
        std::vector<Instance::Ptr> kids = existing->children();
        for (auto& k : kids) k->setParent(inst.get());
        existing->destroy();
    } else if (existing) {
        existing->destroy();
    }
    inst->setParent(parent);
    applyPendingMeta(rt, info);
    return inst.get();
}

bool unloadSourceFile(Runtime& rt, const std::string& relPath) {
    SourceFileInfo info;
    if (!classifySourceFile(relPath, info)) return false;
    Instance* cur = rt.dataModel().root();
    for (auto& c : info.containers) { cur = cur->findFirstChild(c); if (!cur) return false; }
    Instance* inst = cur->findFirstChild(info.name);
    if (info.kind == SourceFileInfo::Meta) {
        rt.impl().pendingMeta.erase(info.path());
        // init.meta.json gone: the directory is a plain Folder again.
        if (info.initMeta && inst && !info.containers.empty() && !inst->isA("BaseScript") && inst->className() != "Folder") {
            Instance::Ptr folder = rt.dataModel().create("Folder");
            folder->setName(info.name);
            std::vector<Instance::Ptr> kids = inst->children();
            for (auto& k : kids) k->setParent(folder.get());
            folder->setParent(cur);
            inst->destroy();
        }
        return true;
    }
    if (info.kind == SourceFileInfo::Model) {
        if (!inst) return false;
        inst->destroy();
        return true;
    }
    if (!inst || inst->className() != info.className) return false;
    if (!inst->children().empty()) {
        // init.luau removed but its siblings remain: degrade to a Folder.
        Instance::Ptr folder = rt.dataModel().create("Folder");
        folder->setName(info.name);
        std::vector<Instance::Ptr> kids = inst->children();
        for (auto& k : kids) k->setParent(folder.get());
        folder->setParent(cur);
    }
    inst->destroy();
    return true;
}

} // namespace pulseblockz::rbx
