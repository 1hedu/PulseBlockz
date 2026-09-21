// Spike harness: try every way a creator script could hurt the host, and
// report whether the sandbox caught it and how long the host was stalled.
#include "luau_sandbox.h"
#include "script_worker.h"
#include "eip712.h"
#include "chain_assets.h"
#include "vectors_assetstore.h"
#include "vectors_txasset.h"
#include "lua.h"
#include "lualib.h"
#include <chrono>
#include <cstdio>
#include <cmath>
#include <string>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

using namespace pulseblockz;

static int g_partsCreated = 0;
static int partCreate(lua_State* L) {
    double x = luaL_checknumber(L, 1), y = luaL_checknumber(L, 2), z = luaL_checknumber(L, 3);
    (void)x; (void)y; (void)z;
    g_partsCreated++;
    lua_pushinteger(L, g_partsCreated); // fake part id
    return 1;
}

// Worker-style host function: never touches host state, just marshals a command.
static int wpartCreate(lua_State* L) {
    double x = luaL_checknumber(L, 1), y = luaL_checknumber(L, 2), z = luaL_checknumber(L, 3);
    auto* w = ScriptWorker::from(L);
    HostEvent e; e.text = "part.create";
    e.script = w->currentScript(); e.id = w->nextObjectId(); e.x = x; e.y = y; e.z = z;
    int64_t id = e.id;
    if (!w->command(std::move(e))) luaL_error(L, "Part.create: command budget exceeded (%d per call)", w->maxCommandsPerCall());
    lua_pushinteger(L, (int)id);
    return 1;
}

static int passed = 0, failed = 0;
static void check(bool c, const char* what) { (c ? passed : failed)++; std::printf("  %s %s\n", c ? "PASS" : "FAIL", what); std::fflush(stdout); }
static void section(const char* s) { std::printf("\n== %s\n", s); std::fflush(stdout); }
void runRbxTests(void (*check)(bool, const char*), void (*section)(const char*)); // test/rbx_harness.cpp

static double ms(std::chrono::steady_clock::time_point a) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
}

int main() {
    Budget b; b.maxMillis = 4.0; b.maxSteps = 2'000'000; b.maxMemory = 8u << 20;
    Sandbox sb(b);
    sb.expose("Part", {{"create", &partCreate}});
    sb.setPrintHandler([](const std::string& s) { std::printf("    [script] %s\n", s.c_str()); });

    section("1. well-behaved script with Tick(dt)");
    {
        std::string err;
        int h = sb.loadScript("good.luau", R"(
            local t = 0
            local id = Part.create(0, 5, 0)
            function Tick(dt) t += dt; if t > 0.5 then print("half a second, part id", id); t = 0 end end
        )", &err);
        check(h != 0, ("loads: " + err).c_str());
        auto r = sb.runMain(h);
        check(r.ok, "main runs");
        double worst = 0;
        for (int i = 0; i < 60; i++) { auto t = sb.call(h, "Tick", 1.0 / 60); if (!t.ok) { check(false, t.error.c_str()); } worst = std::max(worst, t.millisUsed); }
        check(worst < 0.5, "60 ticks, worst frame < 0.5 ms");
        check(g_partsCreated == 1, "host API called once");
        sb.unloadScript(h);
    }

    section("2. infinite loop in main");
    {
        int h = sb.loadScript("loop.luau", "while true do end");
        auto t0 = std::chrono::steady_clock::now();
        auto r = sb.runMain(h);
        double wall = ms(t0);
        check(!r.ok, r.error.c_str());
        check(wall < b.maxMillis * 3, ("host stalled only " + std::to_string(wall) + " ms").c_str());
        sb.unloadScript(h);
    }

    section("3. infinite loop inside Tick — frame budget across 100 frames");
    {
        int h = sb.loadScript("looptick.luau", "function Tick(dt) local x = 0 while true do x = x + 1 end end");
        sb.runMain(h);
        double worst = 0; int kills = 0;
        for (int i = 0; i < 100; i++) { auto r = sb.call(h, "Tick", 0.016); if (!r.ok) kills++; worst = std::max(worst, r.millisUsed); }
        check(kills == 100, "killed every frame");
        check(worst < b.maxMillis * 3, ("worst frame " + std::to_string(worst) + " ms").c_str());
        sb.unloadScript(h);
    }

    section("3b. sustained kills — past Luau's MAXTAGLOOP (100)");
    {
        // Regression: the kill path used to re-sandbox the thread on every error,
        // layering a new globals table whose __index pointed at the previous one.
        // At 100 links the next global lookup raised "'__index' chain too long"
        // from lua_getglobal, outside any protected call, taking the process down.
        // Section 3 stops at exactly 100 kills, one short of the limit.
        int h = sb.loadScript("longkill.luau", "function Tick(dt) while true do end end");
        sb.runMain(h);
        int kills = 0;
        for (int i = 0; i < 250; i++)
            if (!sb.call(h, "Tick", 0.016).ok) kills++;
        check(kills == 250, ("still killed after 250 kills (" + std::to_string(kills) + ")").c_str());
        auto after = sb.call(h, "OtherFn", 0.0);
        check(after.ok, "global lookup on the same thread still works after 250 kills");
        sb.unloadScript(h);
    }

    section("4. memory bomb");
    {
        Budget slow = b; slow.maxMillis = 1000; sb.setBudget(slow); // make sure it's memory, not time, that fires
        int h = sb.loadScript("mem.luau", R"(
            local t = {}
            for i = 1, 1e9 do t[i] = string.rep("x", 1048576) .. tostring(i) end
        )");
        auto r = sb.runMain(h);
        sb.setBudget(b);
        check(!r.ok, r.error.c_str());
        check(r.memoryPeak <= b.maxMemory, ("peak " + std::to_string(r.memoryPeak / 1024) + " KB <= limit").c_str());
        sb.unloadScript(h);
        // reclaim
        lua_gc(sb.rawState(), LUA_GCCOLLECT, 0);
        check(sb.memoryNow() < (1u << 20), ("memory reclaimed after unload: " + std::to_string(sb.memoryNow() / 1024) + " KB").c_str());
    }

    section("5. unbounded recursion");
    {
        int h = sb.loadScript("rec.luau", "local function f(n) return 1 + f(n + 1) end f(0)");
        auto r = sb.runMain(h);
        check(!r.ok, r.error.c_str());
        sb.unloadScript(h);
        int h2 = sb.loadScript("tailrec.luau", "local function f(n) return f(n + 1) end f(0)");
        auto r2 = sb.runMain(h2);
        check(!r2.ok, r2.error.c_str());
        sb.unloadScript(h2);
    }

    section("6. capability stripping");
    {
        int h = sb.loadScript("caps.luau", R"(
            assert(os == nil, "os leaked")
            assert(debug == nil, "debug leaked")
            assert(io == nil, "io leaked")
            assert(loadstring == nil, "loadstring leaked")
            assert(require == nil, "require leaked")
            assert(getfenv == nil and setfenv == nil, "fenv leaked")
            assert(collectgarbage == nil, "collectgarbage leaked")
        )");
        auto r = sb.runMain(h);
        check(r.ok, r.ok ? "no process capabilities visible" : r.error.c_str());
        sb.unloadScript(h);
    }

    section("7. isolation between scripts + frozen stdlib");
    {
        int a = sb.loadScript("a.luau", "SECRET = 42  string.evil = function() end");
        auto ra = sb.runMain(a);
        check(!ra.ok, ("A cannot patch stdlib: " + ra.error).c_str());
        int a2 = sb.loadScript("a2.luau", "SECRET = 42");
        check(sb.runMain(a2).ok, "A2 sets its own global");
        int bb = sb.loadScript("b.luau", "assert(SECRET == nil, 'B can see A2 global')  assert(Part ~= nil, 'B lost host API')");
        auto rb = sb.runMain(bb);
        check(rb.ok, rb.ok ? "B cannot see A2's globals, still has host API" : rb.error.c_str());
        int c = sb.loadScript("c.luau", "Part.create = nil");
        auto rc = sb.runMain(c);
        check(!rc.ok, ("cannot mutate host API table: " + rc.error).c_str());
        sb.unloadScript(a); sb.unloadScript(a2); sb.unloadScript(bb); sb.unloadScript(c);
    }

    section("8. recovery: sandbox still healthy after all that");
    {
        int h = sb.loadScript("after.luau", "function Tick(dt) return Part.create(1,2,3) end");
        sb.runMain(h);
        auto r = sb.call(h, "Tick", 0.016);
        check(r.ok && g_partsCreated == 2, "new script runs and calls host after prior kills");
        sb.unloadScript(h);
    }

    section("9. throughput: how much work fits in the 4 ms budget?");
    {
        int h = sb.loadScript("work.luau", R"(
            function Tick(dt)
                local acc = 0
                for i = 1, 200000 do acc = acc + math.sin(i) end
                return acc
            end
        )");
        sb.runMain(h);
        auto r = sb.call(h, "Tick", 0.016);
        std::printf("    200k sin() iterations: ok=%d in %.2f ms, %llu steps\n", r.ok, r.millisUsed, (unsigned long long)r.stepsUsed);
        check(true, "informational");
        sb.unloadScript(h);
    }


    section("10. frame budget: 5 runaway scripts, 4 ms each, 6 ms per frame");
    {
        Budget fb = b; fb.maxFrameMillis = 6.0; sb.setBudget(fb);
        std::vector<int> hs;
        for (int i = 0; i < 5; i++) { hs.push_back(sb.loadScript("rt" + std::to_string(i) + ".luau", "function Tick(dt) while true do end end")); sb.beginFrame(); check(sb.runMain(hs.back()).ok, "main runs under a fresh frame"); }
        double worstFrame = 0; int ran = 0, skipped = 0;
        for (int f = 0; f < 20; f++) {
            auto t0 = std::chrono::steady_clock::now();
            sb.beginFrame();
            for (int h : hs) { auto r = sb.call(h, "Tick", 0.016); if (r.skipped) skipped++; else ran++; }
            worstFrame = std::max(worstFrame, ms(t0));
        }
        check(worstFrame < fb.maxFrameMillis * 1.5, ("worst frame " + std::to_string(worstFrame) + " ms (5 x 4 ms would be 20)").c_str());
        check(ran > 0 && skipped > 0, ("ran " + std::to_string(ran) + ", skipped " + std::to_string(skipped)).c_str());
        // a script that runs late in the frame gets only the remainder, not a full 4 ms
        sb.beginFrame();
        auto r0 = sb.call(hs[0], "Tick", 0.016);
        auto r1 = sb.call(hs[1], "Tick", 0.016);
        check(!r1.skipped && r1.millisUsed < r0.millisUsed, ("second script got the remainder: " + std::to_string(r1.millisUsed) + " ms").c_str());
        check(sb.frameMillisUsed() <= fb.maxFrameMillis * 1.5, "frameMillisUsed tracks the frame");
        for (int h : hs) sb.unloadScript(h);
        sb.setBudget(b);
        // unlimited frame budget behaves exactly as before
        int h = sb.loadScript("nofb.luau", "function Tick(dt) end");
        sb.runMain(h); sb.beginFrame();
        check(sb.call(h, "Tick", 0.016).ok, "maxFrameMillis=0 never skips");
        sb.unloadScript(h);
    }

    section("11. worker thread: host never blocks, events marshal back in order");
    {
        ScriptWorker::Options o; o.budget = b; o.budget.maxFrameMillis = 8.0; o.threaded = true;
        std::atomic<int> wakes{0};
        ScriptWorker w(o, [](Sandbox& s) { s.expose("Part", {{"create", &wpartCreate}}); });
        w.setWakeHandler([&] { wakes++; });
        int good = w.load("good.luau", R"(
            local ids = {}
            for i = 1, 3 do ids[i] = Part.create(i, 0, 0) end
            local t = 0
            function Tick(dt) t += dt; if t >= 0.1 then print("tick", ids[1]); t = 0 end end
        )");
        int evil = w.load("evil.luau", "function Tick(dt) while true do end end");
        int bad  = w.load("bad.luau", "function Tick( oops");
        w.runMain(good); w.runMain(evil); w.runMain(bad);
        double worstCall = 0;
        for (int i = 0; i < 30; i++) {
            auto t0 = std::chrono::steady_clock::now();
            w.tick(1.0 / 60);
            worstCall = std::max(worstCall, ms(t0));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        check(worstCall < 0.5, ("host-side tick() never blocked: worst " + std::to_string(worstCall) + " ms").c_str());
        w.waitIdle();
        auto ev = w.drain();
        int cmds = 0, kills = 0, loadErr = 0, prints = 0, results = 0, ticks = 0; int64_t lastId = 0; bool ordered = true;
        double worstBatch = 0; int totalRan = 0;
        for (auto& e : ev) switch (e.type) {
            case HostEvent::Command:   cmds++; if (e.id <= lastId) ordered = false; lastId = e.id; break;
            case HostEvent::Killed:    kills++; if (e.name != "evil.luau") ordered = false; break;
            case HostEvent::LoadError: loadErr++; if (e.script != bad) ordered = false; break;
            case HostEvent::Print:     prints++; if (e.name != "good.luau") ordered = false; break;
            case HostEvent::Result:    results++; break;
            case HostEvent::TickStats: ticks++; worstBatch = std::max(worstBatch, e.result.millisUsed); totalRan += (int)e.id; break;
        }
        check(cmds == 3 && ordered, "3 part.create commands, ids in order, attributed to the right scripts");
        check(loadErr == 1, "bad.luau reported as LoadError, its handle is a harmless no-op");
        check(kills > 0 && kills == ticks, ("evil.luau killed on every tick (" + std::to_string(kills) + ")").c_str());
        check(prints >= 1, "print() attributed to good.luau");
        check(results == 2, "runMain results for the two scripts that loaded");
        check(worstBatch < o.budget.maxFrameMillis * 1.5, ("worst tick batch " + std::to_string(worstBatch) + " ms").c_str());
        check(wakes >= 1 && wakes <= (int)ev.size(), ("wake fired " + std::to_string(wakes.load()) + "x for " + std::to_string(ev.size()) + " events (edge-triggered)").c_str());
        check(w.drain().empty(), "drain() is destructive");
        w.unload(good); w.unload(evil); w.waitIdle();
        check(w.memoryUsed() < (1u << 20), "memory stat published to host thread");
    }

    section("12. tick coalescing: a slow batch folds deltas, never a backlog");
    {
        ScriptWorker::Options o; o.budget = b; o.threaded = true;
        ScriptWorker w(o, nullptr);
        int h = w.load("sum.luau", R"(
            total = 0; calls = 0
            function Tick(dt) total += dt; calls += 1; local x = 0; for i = 1, 100000 do x = x + i end end
            function Report() print(string.format("%.4f %d", total, calls)) end
        )");
        w.runMain(h);
        w.waitIdle();
        for (int i = 0; i < 200; i++) w.tick(0.01);   // 2.0 s of game time, faster than the script can run
        w.call(h, "Report");
        w.waitIdle();
        std::string report; int ticks = 0;
        for (auto& e : w.drain()) { if (e.type == HostEvent::Print) report = e.text; if (e.type == HostEvent::TickStats) ticks++; }
        double total = 0; int calls = 0; std::sscanf(report.c_str(), "%lf %d", &total, &calls);
        check(std::abs(total - 2.0) < 1e-6, ("script saw all 2.0 s of delta (got " + std::to_string(total) + ")").c_str());
        check(calls < 200 && calls == ticks, ("in " + std::to_string(calls) + " coalesced ticks, not 200").c_str());
    }

    section("13. threaded=false: same API, runs inline, events available immediately");
    {
        ScriptWorker::Options o; o.budget = b; o.threaded = false;
        ScriptWorker w(o, [](Sandbox& s) { s.expose("Part", {{"create", &wpartCreate}}); });
        int h = w.load("inline.luau", "Part.create(1,2,3) function Tick(dt) print('t') end");
        w.runMain(h);
        auto ev = w.drain();
        check(ev.size() == 2 && ev[0].type == HostEvent::Command && ev[1].type == HostEvent::Result, "runMain: command + result, synchronously");
        w.tick(0.016);
        ev = w.drain();
        check(ev.size() == 2 && ev[0].type == HostEvent::Print && ev[1].type == HostEvent::TickStats && ev[1].id == 1, "tick: print + stats, synchronously");
        check(w.idle(), "always idle");
    }

    section("14. command flood: CPU budget can't stall the host, so cap what reaches it");
    {
        ScriptWorker::Options o; o.budget = b; o.threaded = true; o.maxCommandsPerCall = 100;
        ScriptWorker w(o, [](Sandbox& s) { s.expose("Part", {{"create", &wpartCreate}}); });
        int h = w.load("flood.luau", "function Tick(dt) for i = 1, 1000000 do Part.create(i, 0, 0) end end");
        w.runMain(h); w.tick(0.016); w.waitIdle(); w.tick(0.016); w.waitIdle();   // waitIdle: otherwise the ticks coalesce
        int cmds = 0, kills = 0; std::string reason;
        for (auto& e : w.drain()) { if (e.type == HostEvent::Command) cmds++; if (e.type == HostEvent::Killed) { kills++; reason = e.text; } }
        check(cmds == 200 && kills == 2, ("2 ticks -> " + std::to_string(cmds) + " commands reached the host, " + std::to_string(kills) + " kills: " + reason).c_str());
        w.setBudget(b, 0);
        int h2 = w.load("many.luau", "function Tick(dt) for i = 1, 500 do Part.create(i, 0, 0) end end");
        w.runMain(h2); w.unload(h); w.tick(0.016); w.waitIdle();
        cmds = 0; for (auto& e : w.drain()) if (e.type == HostEvent::Command) cmds++;
        check(cmds == 500, "cap 0 = unlimited");
    }

    section("15. curation list signatures: keccak, canonical JSON, EIP-712 recovery (vector from curation.js)");
    {
        namespace cr = pulseblockz::crypto;
        check(cr::toHex(cr::keccak256(std::string(""))) == "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470", "keccak256(\"\")");
        check(cr::toHex(cr::keccak256(std::string("abc"))) == "0x4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45", "keccak256(\"abc\")");
        check(cr::toHex(cr::keccak256(std::string(1000, 'x'))).size() == 66, "keccak over multiple blocks runs");
        check(cr::checksumAddress("0x70997970c51812dc3a010c7d01b50e0d17dc79c8") == "0x70997970C51812dc3A010C7d01b50e0d17dc79C8", "EIP-55 checksum");

        // The entries as another JSON library might re-serialise them: other key order, whitespace,
        // \/ escapes. Must canonicalise to exactly what curation.js produced.
        const char* messy = R"json([ {"note":"ripped \"model\"\n", "reason":"copyright", "kind":"asset", "id":"7"},
            {"reason":"scam","kind":"creator","address":"0x90F79bf6EB2c4f870365E785982E1f101E93b906"},
            {"prefix":"ipfs:\/\/bafyabuse","kind":"uri","reason":"abuse"} ])json";
        const char* expected = R"json([{"id":"7","kind":"asset","note":"ripped \"model\"\n","reason":"copyright"},{"address":"0x90F79bf6EB2c4f870365E785982E1f101E93b906","kind":"creator","reason":"scam"},{"kind":"uri","prefix":"ipfs://bafyabuse","reason":"abuse"}])json";
        std::string canon, err;
        check(cr::canonicalJson(messy, canon, &err) && canon == expected, ("canonical JSON matches JS reference " + err).c_str());
        // Godot's JSON parses integers as floats and writes them back as "1.0"; JS writes "1".
        // Escapes: \u00e9 -> raw UTF-8, \u0001 stays escaped, as JSON.stringify does.
        check(cr::canonicalJson(R"({"b":1.0,"a":[true,null,"\u00e9\u0001",2.5e-3],"c":{"z":-0,"y":12345678901234}})", canon) &&
              canon == std::string("{\"a\":[true,null,\"\xc3\xa9\\u0001\",0.0025],\"b\":1,\"c\":{\"y\":12345678901234,\"z\":0}}"),
              ("numbers/escapes/unicode canonicalise like JSON.stringify: " + canon).c_str());
        check(!cr::canonicalJson("[1,2", canon, &err) && !cr::canonicalJson("{\"a\":1} x", canon, &err), "malformed JSON rejected");
        cr::Hash eh;
        check(cr::entriesHash(messy, eh) && cr::toHex(eh) == "0xe7081dd9885125f986557b7fd00db551e3345b6a5a579af94241ce4fe4a7398b", "entriesHash matches JS");

        cr::CurationFields f;
        f.mode = "block"; f.chainId = 369; f.registry = "0xd8058efe0198ae9dD7D563e1b4938Dcbc86A1F81";
        f.name = "vector list \xc3\xa9\"\\"; f.updated = 1757000000; f.ttl = 86400; f.entriesJson = messy;
        const char* sig = "0x0239884ff910949da837855135c849010f6e66cd0fee13cb071736ba30c0efc15c940c85eaeef3a3c9cb8f8af0abf28baf5aef3af56399c9dbfb81e17f9af5e61b";   // ethers v6 signTypedData, domain PulseBlockzCuration/1, anvil #1
        const char* maintainer = "0x70997970C51812dc3A010C7d01b50e0d17dc79C8";
        std::string signer; err.clear();
        check(cr::verifyCurationList(f, maintainer, sig, &signer, &err) && signer == maintainer, ("recovers the maintainer from an ethers signTypedData signature: " + signer + " " + err).c_str());
        check(cr::verifyCurationList(f, "0x70997970c51812dc3a010c7d01b50e0d17dc79c8", sig), "maintainer comparison is case-insensitive");
        cr::CurationFields t = f; t.name = "renamed";
        check(!cr::verifyCurationList(t, maintainer, sig, &signer, &err) && !signer.empty() && signer != maintainer, ("tampered name -> different signer: " + signer).c_str());
        t = f; t.entriesJson = "[]";
        check(!cr::verifyCurationList(t, maintainer, sig), "tampered entries -> mismatch");
        t = f; t.mode = "allow";
        check(!cr::verifyCurationList(t, maintainer, sig), "tampered mode -> mismatch");
        t = f; t.chainId = 943;
        check(!cr::verifyCurationList(t, maintainer, sig), "tampered chainId -> mismatch");
        check(!cr::verifyCurationList(f, maintainer, "0x1234", &signer, &err) && signer.empty(), ("bad signature length -> recovery fails cleanly: " + err).c_str());
        std::string sigV0 = std::string(sig).substr(0, 130) + "00"; // v=0 instead of 27
        check(cr::verifyCurationList(f, maintainer, sigV0), "v=0/1 form accepted");

        // Signing, the other half of the pair: the host uses this to sign transactions
        // so a creator's script never has to hold a key. anvil's first default account,
        // which is public knowledge and funded on nobody's real network.
        std::vector<uint8_t> key;
        check(cr::fromHex("0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80", key) && key.size() == 32, "a secret key parses");
        check(cr::addressFromKey(key) == "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266", "the address derived from it");
        cr::Hash digest = cr::keccak256(std::string("PulseBlockz"));
        std::vector<uint8_t> made = cr::signDigest(digest, key);
        check(made.size() == 65 && (made[64] == 0 || made[64] == 1), "signDigest returns r||s||v with v as the recovery id");
        check(cr::recoverAddress(digest, made) == cr::addressFromKey(key), "and the signature recovers to the signer");
        // RFC6979: no randomness, so the same digest and key always give the same bytes.
        check(cr::signDigest(digest, key) == made, "signing is deterministic");
        // EIP-2 requires low-S; libsecp256k1 only ever produces it.
        check(made[32] < 0x80, "and canonical (low-S)");
        cr::Hash other = cr::keccak256(std::string("PulseBlockz "));
        check(cr::signDigest(other, key) != made, "a different digest signs differently");
        check(cr::signDigest(digest, std::vector<uint8_t>(31, 1)).empty(), "a key of the wrong length is refused");
        check(cr::signDigest(digest, std::vector<uint8_t>(32, 0)).empty(), "and so is a key outside the curve order");
        check(cr::addressFromKey(std::vector<uint8_t>(32, 0)).empty(), "which has no address either");
    }

    runRbxTests(check, section);

    section("16. pblockz:// assets: URI, AssetStore calldata + decoding, chunk assembly, hash (vectors from a chain run)");
    {
        namespace ch = pulseblockz::chain;
        namespace V = assetstore_vectors;
        auto eqi = [](std::string a, std::string b) { std::transform(a.begin(), a.end(), a.begin(), ::tolower); std::transform(b.begin(), b.end(), b.begin(), ::tolower); return a == b; };
        std::string err;

        ch::AssetUri u;
        check(ch::parseAssetUri(V::small_uri, u, &err) && u.contentHash == V::small_contentHash && u.hasChain && u.chainId == V::chainId
              && eqi(u.store, V::store) && u.blobId == std::stoull(V::small_blobId) && u.mime == "application/json", ("parses the URI the CLI produced: " + err).c_str());
        check(ch::formatAssetUri(u) == V::small_uri, "format(parse(uri)) round-trips byte for byte");
        ch::AssetUri m;
        check(ch::parseAssetUri("pblockz://" + std::string(V::small_contentHash).substr(2) + "?src=https%3A%2F%2Fcdn.example%2Fa.glb&src=ipfs%3A%2F%2Fbafy1&x=ignored", m) && !m.hasChain && m.srcs.size() == 2 && m.srcs[1] == "ipfs://bafy1", "mirror-only URI: srcs decoded, unknown keys ignored");
        check(!ch::parseAssetUri("pblockz://1234", u, &err) && !ch::parseAssetUri("https://x", u, &err) && !ch::parseAssetUri(std::string("pblockz://") + std::string(V::small_contentHash).substr(2) + "?chain=31337:0x1234:1", u, &err), "bad hash / scheme / store address rejected");

        uint64_t id = std::stoull(V::small_blobId);
        check(eqi(ch::readCalldata(id), V::small_readCalldata), "read(uint256) calldata matches ethers");
        check(eqi(ch::chunksOfCalldata(id), V::small_chunksOfCalldata), "chunksOf calldata matches ethers");
        check(eqi(ch::blobCalldata(id), V::small_blobCalldata), "blob calldata matches ethers");
        check(eqi(ch::blobOfCalldata(V::small_contentHash), V::small_blobOfCalldata), "blobOf(bytes32) calldata matches ethers");
        check(eqi(ch::readRangeCalldata(std::stoull(V::two_blobId), 24570, 10), V::two_readRangeCalldata), "readRange calldata matches ethers");

        ch::Bytes want; ch::hexToBytes(V::small_bytesHex, want);
        ch::Bytes got;
        check(ch::decodeBytes(V::small_readResult, got, &err) && got == want, ("decodes read() result to the exact bytes: " + err).c_str());
        check(ch::verify(got, V::small_contentHash) && ch::contentHashOf(got) == V::small_contentHash, "content hash verifies");
        std::vector<std::string> chunks;
        check(ch::decodeAddressArray(V::small_chunksOfResult, chunks, &err) && chunks.size() == 1 && eqi(chunks[0], V::small_chunks[0]), ("decodes chunksOf() to the chunk address: " + err).c_str());
        ch::BlobInfo bi;
        check(ch::decodeBlob(V::small_blobResult, bi, &err) && bi.contentHash == V::small_contentHash && bi.size == V::small_size && bi.mime == "application/json" && bi.chunks.size() == 1 && eqi(bi.chunks[0], V::small_chunks[0]) && bi.publisher.size() == 42, ("decodes blob() tuple: " + err).c_str());
        uint64_t bid;
        check(ch::decodeUint(V::small_blobOfResult, bid, &err) && bid == id, "decodes blobOf() -> blobId");
        ch::Bytes code0; ch::hexToBytes(V::small_code0, code0);
        ch::Bytes asm1;
        check(code0[0] == 0x00 && ch::assembleChunks({code0}, asm1, &err) && asm1 == want, "eth_getCode chunk is STOP || data; assembles to the bytes");

        // Two-chunk blob: regenerate the bytes the generator stored (same LCG), so the
        // chunking and the on-chain copy can be checked independently of each other.
        ch::Bytes two(V::two_size); { uint32_t x = (uint32_t)V::two_seedSalt; for (size_t i = 0; i < two.size(); i++) { x = x * 1103515245u + 12345u; two[i] = (uint8_t)(x >> 24); } }
        check(ch::contentHashOf(two) == V::two_contentHash, "regenerated 30,000-byte blob hashes to what the chain recorded");
        auto parts = ch::chunk(two);
        check(parts.size() == 2 && parts[0].size() == 24575 && parts[1].size() == 5425, "chunk() splits at 24,575");
        ch::Bytes c0, c1; ch::hexToBytes(V::two_code0, c0); ch::hexToBytes(V::two_code1, c1);
        ch::Bytes asm2;
        check(ch::assembleChunks({c0, c1}, asm2, &err) && asm2 == two, "two chunks from eth_getCode assemble to the blob");
        ch::Bytes rd;
        check(ch::decodeBytes(V::two_readResult, rd, &err) && rd == two, "read() of the two-chunk blob decodes to the same bytes");
        ch::Bytes range, rangeWant; ch::hexToBytes(V::two_readRangeExpected, rangeWant);
        check(ch::decodeBytes(V::two_readRangeResult, range, &err) && range == rangeWant && range == ch::Bytes(two.begin() + 24570, two.begin() + 24580), "readRange across the chunk boundary decodes correctly");
        ch::Bytes tampered = two; tampered[100] ^= 1;
        check(!ch::verify(tampered, V::two_contentHash), "a flipped bit fails verification");
        ch::Bytes bad = c0; bad[0] = 0x60;
        check(!ch::assembleChunks({bad}, asm2, &err), ("a chunk without the STOP prefix is refused: " + err).c_str());
        check(!ch::decodeBytes("0x0000", got, &err) && !ch::decodeBytes("zz", got, &err), "truncated / non-hex results fail cleanly");
    }


    section("17. assets published as calldata: the manifest/chunk scheme (vectors from PulseChain mainnet)");
    {
        namespace ch = pulseblockz::chain;
        namespace V = txasset_vectors;
        std::string err;

        // The real manifest transaction behind a PDF someone published on mainnet.
        ch::TxManifest man;
        check(ch::parseTxManifest(V::manifestJson, man, &err) && man.mime == "application/pdf" && man.chunkTxs.size() == 2,
              ("reads a manifest written by another tool: " + err).c_str());
        check(man.chunkTxs[0] == V::chunkTx0 && man.chunkTxs[1] == V::chunkTx1, "chunk transaction hashes come back in order");
        check(ch::txManifestJson(man.mime, man.chunkTxs) == V::manifestJson, "and we write the same manifest byte for byte");

        // base64 round trip, including every padding case.
        for (const char* sample : {"", "a", "ab", "abc", "abcd", "%PDF-1.3\n\x01\x02\xff"}) {
            ch::Bytes in(sample, sample + std::char_traits<char>::length(sample)), back;
            check(ch::base64Decode(ch::base64Encode(in), back) && back == in, "base64 round trip");
        }
        ch::Bytes prefix;
        check(ch::base64Decode(V::filePrefixB64, prefix) && prefix.size() >= 8 && std::string(prefix.begin(), prefix.begin() + 8) == "%PDF-1.3",
              "the published bytes really are the PDF they claim to be");

        // A chunk we write is a chunk we can read.
        ch::Bytes payload;
        for (int i = 0; i < 5000; i++) payload.push_back((uint8_t)(i * 7 + (i >> 3)));
        std::string chunkJson = ch::txChunkJson(payload, "application/octet-stream", "");
        ch::Bytes readBack;
        check(ch::parseTxChunk(chunkJson, readBack, &err) && readBack == payload, ("chunk JSON round trip: " + err).c_str());
        check(chunkJson.find("\"parentHash\":0") != std::string::npos, "the first chunk has no parent");
        check(ch::txChunkJson(payload, "x", V::chunkTx0).find(V::chunkTx0) != std::string::npos, "a later chunk names the one before it");

        // Malformed input is refused, never guessed at.
        ch::TxManifest bad;
        check(!ch::parseTxManifest("{\"mimeType\":\"x\"}", bad, &err), "a manifest with no chunk list is refused");
        check(!ch::parseTxManifest("{\"chunkHashes\":[\"0x1234\"]}", bad, &err), "a short chunk hash is refused");
        check(!ch::parseTxChunk("{\"parentHash\":0}", readBack, &err), "a chunk with no data is refused");
        check(!ch::parseTxChunk("{\"chunkData\":\"not base64!!\"}", readBack, &err), "chunkData that is not base64 is refused");

        // The URI locator: a hash, plus where a calldata copy of it lives.
        std::string uri = std::string("pblockz://") + std::string(V::fileHash).substr(2) + "?tx=369:" + V::chunkTx0 + "&mime=application%2Fpdf";
        ch::AssetUri u;
        check(ch::parseAssetUri(uri, u, &err) && u.hasTx && u.txChainId == 369 && u.manifestTx == V::chunkTx0 && !u.hasChain && u.mime == "application/pdf",
              ("a tx= locator parses: " + err).c_str());
        check(ch::formatAssetUri(u) == uri, "and round-trips");
        ch::AssetUri both;
        check(ch::parseAssetUri("pblockz://" + std::string(V::fileHash).substr(2) + "?chain=369:0x0f9D08e13BE2345856026615d05F7251F07efAfA:4&tx=369:" + V::chunkTx1, both, &err)
              && both.hasChain && both.hasTx, "an asset can name both copies: contract code and calldata");
        check(!ch::parseAssetUri("pblockz://" + std::string(V::fileHash).substr(2) + "?tx=369", u, &err) &&
              !ch::parseAssetUri("pblockz://" + std::string(V::fileHash).substr(2) + "?tx=369:0xzz", u, &err),
              "a malformed tx= locator is refused");
    }

    std::printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
