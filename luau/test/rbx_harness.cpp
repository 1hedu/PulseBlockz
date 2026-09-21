// Runtime tests: the Roblox-shaped API (game/workspace/Instance/task/signals/
// CFrame/require/TweenService/Players) driven the way the engine will drive it.
#include "rbx_runtime.h"
#include "rbx_host.h"
#include "rbx_internal.h"
#include "rbx_wire.h"
#include <cstdio>
#include <string>
#include <vector>
#include <cmath>
#include "lua.h"

using namespace pulseblockz;
using namespace pulseblockz::rbx;

namespace {

struct Log {
    std::vector<std::string> lines;   // print output, in order
    std::vector<std::string> errors, kills, warns;
    bool has(const std::string& s) const { for (auto& l : lines) if (l == s) return true; return false; }
    // Error messages carry a "chunk:line: " prefix like Roblox's; match the tail.
    bool ends(const std::string& s) const { for (auto& l : lines) if (l.size() >= s.size() && l.compare(l.size() - s.size(), s.size(), s) == 0) return true; return false; }
    int count(const std::string& s) const { int n = 0; for (auto& l : lines) if (l == s) n++; return n; }
    int starts(const std::string& s) const { int n = 0; for (auto& l : lines) if (l.compare(0, s.size(), s) == 0) n++; return n; }
    int indexOf(const std::string& s) const { for (size_t i = 0; i < lines.size(); i++) if (lines[i] == s) return (int)i; return -1; }
    std::string joined() const { std::string o; for (auto& l : lines) { o += l; o += " | "; } return o; }
};

Runtime::Callbacks callbacks(Log& log, bool echo = true) {
    Runtime::Callbacks cb;
    cb.print = [&log, echo](const std::string& who, const std::string& t) { log.lines.push_back(t); if (echo) std::printf("    [%s] %s\n", who.c_str(), t.c_str()); };
    cb.warn = [&log](const std::string& who, const std::string& t) { log.warns.push_back(t); std::printf("    [%s] warn: %s\n", who.c_str(), t.c_str()); };
    cb.error = [&log](const std::string& who, const std::string& t) { log.errors.push_back(t); std::printf("    [%s] error: %s\n", who.c_str(), t.c_str()); };
    cb.killed = [&log](const std::string& who, const std::string& t) { log.kills.push_back(t); std::printf("    [%s] killed: %s\n", who.c_str(), t.c_str()); };
    return cb;
}

Runtime::Options serverOpts() { Runtime::Options o; o.budget.maxMillis = 4; o.budget.maxSteps = 2'000'000; o.isServer = true; return o; }

Instance* addScript(Runtime& rt, const char* cls, const char* name, Instance* parent, const std::string& src) {
    Instance::Ptr s = rt.dataModel().create(cls);
    s->setName(name);
    s->set("Source", Value::string(src));
    s->setParent(parent);
    return s.get();
}

} // namespace

void runRbxTests(void (*check)(bool, const char*), void (*section)(const char*)) {
    section("R1. instance tree: Instance.new, properties, services, errors read like Roblox");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        std::string err = rt.runChunk("Console", R"(
            local part = Instance.new("Part")
            part.Name = "Floor"
            part.Size = Vector3.new(10, 1, 10)
            part.Position = Vector3.new(0, -0.5, 0)
            part.Anchored = true
            part.Color = Color3.fromRGB(255, 0, 0)
            part.Material = Enum.Material.Neon
            part.Parent = workspace
            print(part:GetFullName(), part.Parent == workspace, typeof(part.Position), part.Material == Enum.Material.Neon, tostring(part.Material))
            print(part.Size.X, part.Position.Y, part.Color.R, part.Anchored, part.CanCollide, part.ClassName, part:IsA("BasePart"), part:IsA("Instance"), part:IsA("Model"))
            local rs = game:GetService("ReplicatedStorage")
            local f = Instance.new("Folder", rs); f.Name = "Data"
            print(rs.Data == f, game.ReplicatedStorage.Data:IsA("Folder"), workspace:FindFirstChild("Floor") == part, #workspace:GetChildren(), f:GetFullName())
            local ok, e = pcall(function() part.Bogus = 1 end); print(e)
            ok, e = pcall(function() part.Position = 5 end); print(e)
            ok, e = pcall(function() return part.Nope end); print(e)
            ok, e = pcall(function() part.ClassName = "x" end); print(e)
            ok, e = pcall(function() return Instance.new("Player") end); print(e)
            ok, e = pcall(function() return game:GetService("Nope") end); print(e)
            f:Destroy(); print(f.Parent, rs:FindFirstChild("Data"))
            ok, e = pcall(function() f.Parent = rs end); print(e)
            part.Material = "Wood"; print(part.Material.Value, part.Material.Name)
            part:SetAttribute("Health", 50); part:SetAttribute("Tag", "x")
            print(part:GetAttribute("Health"), part:GetAttribute("Tag"), part:GetAttribute("Missing"))
            part:AddTag("Lava"); print(part:HasTag("Lava"), #game:GetService("CollectionService"):GetTagged("Lava"))
            local c = part:Clone(); c.Parent = workspace; print(c.Name, c.Position == part.Position, c ~= part, #workspace:GetChildren())
            print(tostring(part), game.Name, game.ClassName, workspace.Name, game.Workspace == workspace, game.Players.ClassName)
        )");
        check(err.empty(), ("chunk ran: " + err).c_str());
        check(log.has("Workspace.Floor\ttrue\tVector3\ttrue\tEnum.Material.Neon"), ("full name / identity / typeof / enum identity: " + (log.lines.empty() ? "" : log.lines[0])).c_str());
        check(log.has("10\t-0.5\t1\ttrue\ttrue\tPart\ttrue\ttrue\tfalse"), "property reads, defaults and IsA");
        check(log.has("true\ttrue\ttrue\t2\tReplicatedStorage.Data"), "services are children; child lookup by name; GetChildren (Workspace holds Floor and the default Terrain)");
        check(log.ends("Bogus is not a valid member of Part \"Workspace.Floor\""), "unknown property write error");
        check(log.ends("invalid argument #3 to 'Position' (Vector3 expected, got number)"), "type error on write");
        check(log.ends("Nope is not a valid member of Part \"Workspace.Floor\""), "unknown member read error");
        check(log.ends("Unable to assign property ClassName. Property is read only"), "read-only error");
        check(log.ends("Unable to create an Instance of type \"Player\""), "non-creatable class");
        check(log.ends("'Nope' is not a valid Service name"), "bad service name");
        check(log.has("nil\tnil"), "Destroy detaches");
        check(log.ends("The Parent property of Data is locked, current parent: NULL, new parent ReplicatedStorage"), "destroyed instance cannot be re-parented");
        check(log.has("512\tWood"), "enum assignment from string; .Value/.Name");
        check(log.has("50\tx\tnil"), "attributes");
        check(log.has("true\t1"), "tags + CollectionService:GetTagged");
        check(log.has("Floor\ttrue\ttrue\t3"), "Clone copies properties, distinct identity (Floor, its clone, and Terrain)");
        check(log.has("Floor\tGame\tDataModel\tWorkspace\ttrue\tPlayers"), "tostring(instance) = Name; game/workspace globals");
        // The change log carries what the engine mirror needs.
        auto ch = rt.takeChanges();
        int creates = 0, props = 0, parents = 0, destroys = 0;
        for (auto& c : ch) { creates += c.kind == Change::Create; props += c.kind == Change::Property; parents += c.kind == Change::Parent; destroys += c.kind == Change::Destroy; }
        check(creates >= 3 && parents >= 4 && props >= 8 && destroys == 1, ("change log: creates/parents/props/destroys = " + std::to_string(creates) + "/" + std::to_string(parents) + "/" + std::to_string(props) + "/" + std::to_string(destroys)).c_str());
        check(rt.takeChanges().empty(), "takeChanges drains");
    }

    section("R2. scheduler: task.spawn/defer/delay/wait across step()");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        addScript(rt, "Script", "Main", rt.dataModel().getService("ServerScriptService"), R"(
            print("start")
            task.spawn(function() print("spawned"); local dt = task.wait(0.5); print("spawned woke", time(), dt) end)
            task.defer(function() print("deferred") end)
            task.delay(1, function(a) print("delayed", a) end, "arg")
            spawn(function() print("legacy spawn") end)
            print("after spawn")
            local t = task.wait(1)
            print("waited", t)
            local co = coroutine.wrap(function() task.wait(0.25); print("wrapped coroutine woke") end)
            co()
            game:GetService("RunService").Heartbeat:Connect(function(dt) print("heartbeat", dt) end)
        )");
        int started = rt.startScripts();
        check(started == 1, "one script started");
        check(log.indexOf("start") == 0 && log.indexOf("spawned") == 1 && log.indexOf("after spawn") == 2 && log.indexOf("deferred") == 3 && log.indexOf("legacy spawn") == 4,
              ("spawn runs immediately, defer/legacy spawn run after the slice: " + log.joined()).c_str());
        for (int i = 0; i < 4; i++) rt.step(0.25);   // t = 1.0
        check(log.has("spawned woke\t0.5\t0.5"), "task.wait(0.5) woke at t=0.5 with elapsed");
        check(log.has("delayed\targ"), "task.delay passes args");
        check(log.has("waited\t1"), "main thread resumed after 1s");
        rt.step(0.25);
        check(log.has("wrapped coroutine woke"), "a coroutine.wrap coroutine can task.wait");
        check(log.count("heartbeat\t0.25") == 2, "Heartbeat fires once per step from the step it was connected in");
        check(log.errors.empty(), "no errors");
        check(rt.stats().threadsLive <= 2, ("finished threads are released: live=" + std::to_string(rt.stats().threadsLive)).c_str());
    }

    section("R3. signals: Connect/Once/Wait/Disconnect, Changed, ChildAdded, host-fired Touched");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        addScript(rt, "Script", "Sig", rt.dataModel().getService("ServerScriptService"), R"(
            local part = Instance.new("Part"); part.Name = "Pad"; part.Parent = workspace
            local n = 0
            conn = part.Touched:Connect(function(other) n += 1; print("touched by", other.Name, n) end)
            part.Touched:Once(function() print("once") end)
            part:GetPropertyChangedSignal("Transparency"):Connect(function() print("transparency now", part.Transparency) end)
            part.Changed:Connect(function(p) print("changed", p) end)
            workspace.ChildAdded:Connect(function(c) print("child added", c.Name) end)
            part.Destroying:Connect(function() print("destroying") end)
            task.spawn(function() local who = part.Touched:Wait(); print("wait got", who.Name) end)
            part.Transparency = 0.5
            local x = Instance.new("Part"); x.Name = "Toucher"; x.Parent = workspace
            print("conn", conn.Connected, typeof(conn), typeof(part.Touched))
            task.delay(1, function() conn:Disconnect(); print("disconnected", conn.Connected) end)
            task.delay(2, function() part:Destroy(); print("after destroy", conn.Connected) end)
        )");
        rt.startScripts();
        check(log.has("transparency now\t0.5") && log.has("changed\tTransparency"), "GetPropertyChangedSignal + Changed");
        check(log.has("child added\tToucher"), "ChildAdded");
        check(log.has("conn\ttrue\tRBXScriptConnection\tRBXScriptSignal"), "connection/signal types");
        Instance* pad = rt.dataModel().workspace()->findFirstChild("Pad");
        Instance* toucher = rt.dataModel().workspace()->findFirstChild("Toucher");
        rt.fireEvent(*pad, "Touched", {Value::instance(toucher->id())});
        rt.fireEvent(*pad, "Touched", {Value::instance(toucher->id())});
        check(log.count("once") == 1, "Once fires once");
        check(log.has("touched by\tToucher\t2"), "Connect fires every time with args");
        check(log.count("wait got\tToucher") == 1, ":Wait() resumes with the args, once");
        rt.step(1);
        rt.fireEvent(*pad, "Touched", {Value::instance(toucher->id())});
        check(log.has("disconnected\tfalse") && !log.has("touched by\tToucher\t3"), "Disconnect stops delivery");
        rt.step(1);
        check(log.has("destroying") && log.has("after destroy\tfalse"), "Destroying fires; Destroy disconnects everything");
        check(log.errors.empty(), "no errors");
    }
    {
        // A ValueBase's Changed carries the new value, not "Value"; a CFrameValue's fires once, whole.
        Log log; Runtime rt(serverOpts(), callbacks(log));
        addScript(rt, "Script", "Vals", rt.dataModel().getService("ServerScriptService"), R"(
            local s = Instance.new("StringValue")
            s.Changed:Connect(function(v) print("string changed", v) end)
            s.Value = "hello"
            local n = Instance.new("NumberValue")
            n.Changed:Connect(function(v) print("number changed", v) end)
            n.Value = 7
            local c = Instance.new("CFrameValue")
            local fired = 0
            c.Changed:Connect(function(cf) fired += 1; print("cframe changed", typeof(cf), cf.Position.X, fired) end)
            c.Value = CFrame.new(3, 4, 5) * CFrame.Angles(0, math.rad(90), 0)
            c.Value = CFrame.new(9, 4, 5) * CFrame.Angles(0, math.rad(90), 0)
            local p = Instance.new("Part")
            p.Changed:Connect(function(name) print("part changed", name) end)
            p.Name = "Named"
        )");
        rt.startScripts();
        check(log.has("string changed\thello") && log.has("number changed\t7"), "ValueBase.Changed passes the new value");
        check(log.has("cframe changed\tCFrame\t3\t1") && log.has("cframe changed\tCFrame\t9\t2") && !log.has("cframe changed\tCFrame\t3\t2") && !log.has("cframe changed\tCFrame\t9\t3"),
              "CFrameValue.Changed fires once per set, with the whole CFrame");
        check(log.has("part changed\tName"), "an ordinary Instance's Changed still passes the property name");
        check(log.errors.empty(), "no errors");
    }

    section("R4. require, WaitForChild, module cache");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        Instance* rs = rt.dataModel().getService("ReplicatedStorage");
        addScript(rt, "ModuleScript", "Util", rs, R"(
            local M = {}
            M.loads = (M.loads or 0) + 1
            function M.add(a, b) return a + b end
            print("module loaded", script:GetFullName())
            return M
        )");
        addScript(rt, "ModuleScript", "Bad", rs, "return 1, 2");
        addScript(rt, "Script", "Main", rt.dataModel().getService("ServerScriptService"), R"(
            local Util = require(game.ReplicatedStorage:WaitForChild("Util"))
            print("add", Util.add(2, 3), require(game.ReplicatedStorage.Util) == Util)
            local ok, e = pcall(require, game.ReplicatedStorage.Bad); print(e)
            ok, e = pcall(require, workspace); print(e)
            local late = workspace:WaitForChild("Late")
            print("got", late.Name, late.ClassName)
            local none = workspace:WaitForChild("Never", 0.5)
            print("timeout", none)
        )");
        rt.startScripts();
        check(log.count("module loaded\tReplicatedStorage.Util") == 1 && log.has("add\t5\ttrue"), "module runs once; result cached");
        check(log.has("Module code did not return exactly one value (ReplicatedStorage.Bad)"), "bad module error");
        check(log.has("Attempted to call require with invalid argument(s). (Workspace is not a ModuleScript)"), "require non-module error");
        rt.step(0.1);
        check(!log.has("got\tLate\tFolder"), "WaitForChild is still waiting");
        Instance::Ptr late = rt.dataModel().create("Folder"); late->setName("Late"); late->setParent(rt.dataModel().workspace());
        check(log.has("got\tLate\tFolder"), "WaitForChild resumes when the child appears");
        rt.step(0.3); rt.step(0.3);
        check(log.has("timeout\tnil"), "WaitForChild with timeout returns nil");
        check(log.errors.empty(), "no errors");
    }

    section("R5. placement rules: where a script lives decides whether it runs");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        addScript(rt, "Script", "A", dm.getService("ServerScriptService"), "print('sss')");
        addScript(rt, "Script", "B", dm.workspace(), "print('workspace')");
        addScript(rt, "Script", "C", dm.getService("ServerStorage"), "print('serverstorage')");
        addScript(rt, "Script", "D", dm.getService("ReplicatedStorage"), "print('replicatedstorage')");
        addScript(rt, "LocalScript", "E", dm.getService("StarterPlayer")->findFirstChildOfClass("StarterPlayerScripts"), "print('localscript on server')");
        Instance* f = addScript(rt, "Script", "F", dm.getService("ServerScriptService"), "print('disabled')");
        f->set("Enabled", Value::boolean(false));
        Instance* g = addScript(rt, "Script", "G", dm.getService("ReplicatedStorage"), "print('client runcontext')");
        g->set("RunContext", Value::string("Client"));
        Instance* h = addScript(rt, "Script", "H", dm.getService("ReplicatedStorage"), "print('server runcontext')");
        h->set("RunContext", Value::string("Server"));
        Instance* folder = dm.create("Folder", dm.workspace()).get();
        addScript(rt, "Script", "I", folder, "print('nested in workspace')");
        int n = rt.startScripts();
        check(n == 4, ("server started exactly the 4 that should run: " + std::to_string(n)).c_str());
        check(log.has("sss") && log.has("workspace") && log.has("nested in workspace") && log.has("server runcontext"), "SSS, Workspace (nested), RunContext=Server run");
        check(!log.has("serverstorage") && !log.has("replicatedstorage") && !log.has("localscript on server") && !log.has("disabled") && !log.has("client runcontext"),
              "ServerStorage/ReplicatedStorage/LocalScript/Disabled/RunContext=Client do not");
        // Late placement: a script parented into SSS after start runs at the next step.
        Instance::Ptr late = dm.create("Script"); late->setName("Late"); late->set("Source", Value::string("print('late start')"));
        late->setParent(dm.getService("ServerScriptService"));
        rt.step(0.016);
        check(log.has("late start"), "script parented into a running container starts");

        // Client side: LocalScripts run once there is a LocalPlayer.
        Log clog; Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime crt(co, callbacks(clog));
        DataModel& cdm = crt.dataModel();
        addScript(crt, "LocalScript", "Boot", cdm.getService("StarterPlayer")->findFirstChildOfClass("StarterPlayerScripts"),
                  "local p = game:GetService('Players').LocalPlayer; print('client', p.Name, p.UserId, script:GetFullName(), game:GetService('RunService'):IsClient())");
        addScript(crt, "LocalScript", "First", cdm.getService("ReplicatedFirst"), "print('replicated first')");
        addScript(crt, "Script", "S", cdm.getService("ServerScriptService"), "print('server script on client')");
        int cn = crt.startScripts();
        check(cn == 1 && clog.has("replicated first"), "ReplicatedFirst runs before a player exists; server scripts never on the client");
        crt.addPlayer("ada", 42);
        check(clog.has("client\tada\t42\tPlayers.ada.PlayerScripts.Boot\ttrue"), ("StarterPlayerScripts copied to PlayerScripts and run: " + clog.joined()).c_str());
        check(!clog.has("server script on client"), "Script in SSS never runs on client");
    }

    section("R6. Vector3 / CFrame / Color3 math");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        std::string err = rt.runChunk("Math", R"(
            local function r(x) return math.floor(x * 1000 + 0.5) / 1000 end
            local v = Vector3.new(1, 2, 3)
            print(r((v * 2).Y), r(v.Magnitude), r(v.Unit.Z), r(v:Dot(Vector3.new(1,0,0))), tostring(v:Cross(Vector3.yAxis)), v == Vector3.new(1,2,3), tostring(Vector3.zero))
            local cf = CFrame.new(1, 2, 3) * CFrame.Angles(0, math.rad(90), 0)
            local p = cf:PointToWorldSpace(Vector3.new(1, 0, 0))
            print(r(p.X), r(p.Y), r(p.Z), r(cf.LookVector.X), r(cf.LookVector.Z))
            local back = cf:ToObjectSpace(cf)
            print(back == CFrame.identity or back:FuzzyEq(CFrame.identity), tostring(cf.Position), typeof(cf))
            local look = CFrame.lookAt(Vector3.new(0,0,0), Vector3.new(0,0,-10))
            print(r(look.LookVector.Z), r((look * Vector3.new(0,0,-1)).Z))
            local x, y, z = CFrame.fromOrientation(math.rad(10), math.rad(20), math.rad(30)):ToOrientation()
            print(r(math.deg(x)), r(math.deg(y)), r(math.deg(z)))
            local part = Instance.new("Part"); part.Parent = workspace
            part.CFrame = CFrame.new(5, 6, 7) * CFrame.Angles(0, math.rad(45), 0)
            print(tostring(part.Position), r(part.Orientation.Y), r(part.CFrame.LookVector.X))
            local c = Color3.fromRGB(255, 128, 0)
            print(r(c.R), r(c.G), c:ToHex(), Color3.fromHex("#ff8000") == c, r(c:Lerp(Color3.new(0,0,0), 0.5).R))
            print(tostring(Enum.EasingStyle.Linear), Enum.EasingStyle.Linear.Value, Enum.Material.Neon.EnumType == Enum.Material, #Enum.Material:GetEnumItems() > 5)
            local rng = Random.new(7); local a = rng:NextInteger(1, 6); print(a >= 1 and a <= 6, Random.new(7):NextInteger(1, 6) == a, typeof(Random.new()) == "Random")
            print(r(TweenInfo.new(2, Enum.EasingStyle.Sine).Time), tostring(TweenInfo.new(1).EasingStyle))
        )");
        check(err.empty(), ("chunk ran: " + err).c_str());
        check(log.has("4\t3.742\t0.802\t1\t-3, 0, 1\ttrue\t0, 0, 0"), ("Vector3 ops: " + (log.lines.size() > 0 ? log.lines[0] : "")).c_str());
        check(log.has("1\t2\t2\t-1\t0"), ("CFrame * Angles, PointToWorldSpace, LookVector: " + (log.lines.size() > 1 ? log.lines[1] : "")).c_str());
        check(log.has("true\t1, 2, 3\tCFrame"), "ToObjectSpace(self) = identity; typeof");
        check(log.has("-1\t-1"), "lookAt");
        check(log.has("10\t20\t30"), ("fromOrientation/ToOrientation round trip: " + (log.lines.size() > 4 ? log.lines[4] : "")).c_str());
        check(log.has("5, 6, 7\t45\t-0.707"), ("part.CFrame decomposes to Position/Orientation: " + (log.lines.size() > 5 ? log.lines[5] : "")).c_str());
        check(log.has("1\t0.502\tff8000\ttrue\t0.5"), ("Color3: " + (log.lines.size() > 6 ? log.lines[6] : "")).c_str());
        check(log.has("Enum.EasingStyle.Linear\t0\ttrue\ttrue"), "Enum items");
        check(log.has("true\ttrue\ttrue"), "Random is seeded; Random.new() takes no seed too");
        check(log.has("2\tEnum.EasingStyle.Quad"), "TweenInfo (default style is Quad)");
    }

    section("R7. HttpService JSON, GenerateGUID; os/tick/time; typeof");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        std::string err = rt.runChunk("Json", R"(
            local H = game:GetService("HttpService")
            local s = H:JSONEncode({a = 1, b = {1, 2, 3}, c = "x\ny", d = true, e = {}})
            local t = H:JSONDecode(s)
            print(t.a, #t.b, t.b[3], t.c == "x\ny", t.d, type(t.e))
            local u = H:JSONDecode('{"a":[1,2,{"b":null}],"s":"hé","n":-1.5e2}')
            print(#u.a, u.a[3].b, u.s, u.n, H:JSONEncode({1, 2, 3}), H:JSONEncode("q\"\\"))
            print("empty", H:JSONEncode({}), H:JSONEncode({args = {}}))
            local ok, e = pcall(H.JSONDecode, H, "{bad"); print(e)
            local g = H:GenerateGUID(); local g2 = H:GenerateGUID(false)
            print(#g, g:sub(1,1), #g2, g ~= H:GenerateGUID())
            print(typeof(os.time()), os.time() > 1700000000, type(os.clock()), typeof(os.date("*t").year), tick() > 1700000000, time())
            print(typeof(Vector3.new()), typeof(workspace), typeof(Enum.Material.Neon), typeof(Enum), typeof(nil), typeof(5), typeof({}))
            print(os.getenv, os.exit, os.remove, debug, getfenv, loadstring, require ~= nil, shared ~= nil, _G ~= nil)
        )");
        check(err.empty(), ("chunk ran: " + err).c_str());
        check(log.has("1\t3\t3\ttrue\ttrue\ttable"), "JSON round trip");
        check(log.has("3\tnil\th\xc3\xa9\t-150\t[1,2,3]\t\"q\\\"\\\\\""), ("JSON decode escapes/arrays/numbers: " + (log.lines.size() > 1 ? log.lines[1] : "")).c_str());
        check(log.has("empty\t[]\t{\"args\":[]}"), "JSONEncode: an empty table is [], as on Roblox");
        check(log.lines.size() > 3 && log.lines[3].rfind("Can't parse JSON", 0) == 0, "JSON parse error message");
        check(log.has("38\t{\t36\ttrue"), "GenerateGUID");
        check(log.has("number\ttrue\tnumber\tnumber\ttrue\t0"), "os.time/clock/date, tick, time");
        check(log.has("Vector3\tInstance\tEnumItem\tEnums\tnil\tnumber\ttable"), "typeof");
        check(log.has("nil\tnil\tnil\tnil\tnil\tnil\ttrue\ttrue\ttrue"), "dangerous globals stay stripped");
    }

    section("R8. budget: a runaway script is killed, the rest keep running");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        addScript(rt, "Script", "Loop", dm.getService("ServerScriptService"), "print('loop start') while true do end");
        addScript(rt, "Script", "Good", dm.getService("ServerScriptService"), R"(
            local n = 0
            game:GetService("RunService").Heartbeat:Connect(function() n += 1 end)
            task.spawn(function() while true do end end)
            task.wait(0.1)
            print("good alive", n)
        )");
        addScript(rt, "Script", "Err", dm.getService("ServerScriptService"), "local x = nil; print(x.y)");
        rt.startScripts();
        check(log.kills.size() == 2, ("two runaway threads killed (main + spawned): " + std::to_string(log.kills.size())).c_str());
        check(log.errors.size() == 1 && log.errors[0].find("ServerScriptService.Err:1: attempt to index nil") != std::string::npos, ("runtime error names the script: " + (log.errors.empty() ? "" : log.errors[0])).c_str());
        rt.step(0.05); rt.step(0.05); rt.step(0.05);
        check(log.has("good alive\t1") || log.has("good alive\t2"), ("healthy script kept its Heartbeat: " + log.joined()).c_str());
        check(rt.stats().kills == 2 && rt.stats().errors == 1, "stats");
    }

    section("R9. TweenService, Debris, Players / Humanoid");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        addScript(rt, "Script", "Tw", dm.getService("ServerScriptService"), R"(
            local part = Instance.new("Part"); part.Name = "Mover"; part.Parent = workspace
            local TS = game:GetService("TweenService")
            local tw = TS:Create(part, TweenInfo.new(1, Enum.EasingStyle.Linear), {Position = Vector3.new(10, 0, 0), Transparency = 1})
            tw.Completed:Connect(function(state) print("completed", tostring(state), part.Position.X) end)
            print("state", tostring(tw.PlaybackState))
            tw:Play()
            print("playing", tostring(tw.PlaybackState), typeof(tw), tw.ClassName)
            local junk = Instance.new("Part"); junk.Name = "Junk"; junk.Parent = workspace
            game:GetService("Debris"):AddItem(junk, 0.75)
            local Players = game:GetService("Players")
            Players.PlayerAdded:Connect(function(p)
                print("player added", p.Name, p.UserId, p:IsA("Player"), #Players:GetPlayers())
                p.CharacterAdded:Connect(function(ch)
                    local hum = ch:WaitForChild("Humanoid")
                    print("character", ch.Name, ch.Parent == workspace, hum.Health, ch.PrimaryPart.Name)
                    hum.Died:Connect(function() print("died", Players:GetPlayerFromCharacter(ch).Name) end)
                    hum:TakeDamage(150)
                end)
            end)
            Players.PlayerRemoving:Connect(function(p) print("removing", p.Name) end)
        )");
        rt.startScripts();
        check(log.has("state\tEnum.PlaybackState.Begin") && log.has("playing\tEnum.PlaybackState.Playing\tInstance\tTween"), "Tween object");
        rt.step(0.5);
        Instance* mover = dm.workspace()->findFirstChild("Mover");
        check(mover && std::fabs(mover->get("Position").v.x - 5.f) < 1e-4 && std::fabs(mover->get("Transparency").n - 0.5) < 1e-6, "linear tween halfway");
        rt.step(0.5);
        check(log.has("completed\tEnum.PlaybackState.Completed\t10"), ("Completed fires at the end: " + log.joined()).c_str());
        check(dm.workspace()->findFirstChild("Junk") == nullptr, "Debris:AddItem destroyed it after 0.75s");
        dm.getService("Players")->set("RespawnTime", Value::number(1));
        Instance* p = rt.addPlayer("ada", 7);
        check(p && p->parent() == dm.getService("Players") && log.has("player added\tada\t7\ttrue\t1"), "PlayerAdded");
        check(log.has("character\tada\ttrue\t100\tHumanoidRootPart"), ("CharacterAdded + WaitForChild + PrimaryPart: " + log.joined()).c_str());
        check(log.has("died\tada"), "Humanoid:TakeDamage -> Died");
        Instance* firstChar = dm.find(p->get("Character").ref);
        check(firstChar && firstChar->findFirstChild("Torso") && firstChar->findFirstChild("Left Leg") && firstChar->findFirstChild("Head"), "an R6 rig");
        rt.step(0.5);
        check(dm.find(p->get("Character").ref) == firstChar, "not respawned before RespawnTime");
        rt.step(0.6);
        Instance* secondChar = dm.find(p->get("Character").ref);
        check(secondChar && secondChar != firstChar && firstChar->destroyed() && log.count("died\tada") == 2,
              "respawned after RespawnTime: a new character, CharacterAdded ran again (and TakeDamage killed it again)");
        {
            Instance* root = secondChar->findFirstChild("HumanoidRootPart");
            Instance* hum = secondChar->findFirstChildOfClass("Humanoid");
            Vec3 before = root->get("Position").v;
            rt.takeChanges();
            rt.runChunk("mv", R"(
                local hum = workspace.ada.Humanoid
                hum:MoveTo(Vector3.new(10, 5, -3))
                hum:MoveTo(Vector3.new(10, 5, -3))
                print("walkto", hum.WalkToPoint.X, hum.WalkToPoint.Z, hum.WalkToPart == nil)
                hum:Move(Vector3.new(3, 0, 4))
                print("move", math.round(hum.MoveDirection.X * 100), hum.MoveDirection.Y, math.round(hum.MoveDirection.Z * 100))
                hum:MoveTo(Vector3.new(1, 0, 0), workspace.ada.Head)
                print("rel", hum.WalkToPart.Name, hum.WalkToPoint.X - workspace.ada.Head.Position.X)
            )");
            int walkChanges = 0;
            for (const Change& c : rt.takeChanges()) if (c.kind == Change::Property && c.id == hum->id() && c.name == "WalkToPoint") walkChanges++;
            check(root->get("Position").v == before && log.has("walkto\t10\t-3\ttrue") && walkChanges == 3,
                  "Humanoid:MoveTo sets WalkToPoint for the engine (no teleport); a repeat still records a change");
            check(log.has("move\t60\t0\t80") && log.has("rel\tHead\t1"), "Humanoid:Move sets MoveDirection (normalized); MoveTo(offset, part) is relative to the part");
        }
        {
            rt.runChunk("lt", R"(
                local L = game:GetService("Lighting")
                L.ClockTime = 6.5
                local a = L.TimeOfDay
                L.TimeOfDay = "18:15:00"
                print("time", a, L.ClockTime, L.Brightness, L.FogEnd)
            )");
            check(log.has("time\t06:30:00\t18.25\t3\t100000"), "Lighting.ClockTime and TimeOfDay are one value in two forms");
        }
        {
            Instance* bob = rt.addPlayer("bob", 8);
            Instance* bobChar = bob ? dm.find(bob->get("Character").ref) : nullptr;
            Vec3 a = secondChar->findFirstChild("HumanoidRootPart")->get("Position").v;
            Vec3 b = bobChar ? bobChar->findFirstChild("HumanoidRootPart")->get("Position").v : a;
            float gap = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
            check(bobChar && gap > 2.5f && gap < 3.5f && std::fabs(a.y - b.y) < 1e-4, "a second player spawns beside, not inside, the first");
            rt.removePlayer(bob);
        }
        rt.removePlayer(p);
        check(log.has("removing\tada") && dm.getService("Players")->children().empty() && dm.workspace()->findFirstChild("ada") == nullptr, "PlayerRemoving; character and player gone");
        check(log.errors.empty(), ("no errors: " + (log.errors.empty() ? "" : log.errors[0])).c_str());
    }

    section("R10. Bindables, callbacks, ChildAdded-driven scripts, Model pivots, memory of userdata");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        addScript(rt, "Script", "B", dm.getService("ServerScriptService"), R"(
            local ev = Instance.new("BindableEvent"); ev.Parent = game.ReplicatedStorage
            local fn = Instance.new("BindableFunction"); fn.Parent = game.ReplicatedStorage
            ev.Event:Connect(function(a, b) print("event", a, b) end)
            fn.OnInvoke = function(x) return x * 2, "two" end
            ev:Fire("hello", 42)
            print("invoke", fn:Invoke(21))
            local model = Instance.new("Model"); model.Name = "Car"; model.Parent = workspace
            local a = Instance.new("Part"); a.Position = Vector3.new(0, 0, 0); a.Parent = model
            local b = Instance.new("Part"); b.Position = Vector3.new(2, 0, 0); b.Parent = model
            model.PrimaryPart = a
            model:PivotTo(CFrame.new(10, 0, 0))
            print("pivot", tostring(a.Position), tostring(b.Position), tostring(model:GetPivot().Position))
            model:MoveTo(Vector3.new(0, 5, 0)); print("moveto", tostring(b.Position))
            local fish = Instance.new("Model")
            local body = Instance.new("Part"); body.Size = Vector3.new(2, 2, 2); body.Position = Vector3.new(1, 0, 0); body.Parent = fish
            local tail = Instance.new("Part"); tail.Size = Vector3.new(1, 1, 1); tail.Position = Vector3.new(3, 0, 0); tail.Parent = fish
            local eye = Instance.new("Attachment"); eye.Position = Vector3.new(0, 1, 0); eye.Parent = tail
            fish.PrimaryPart = body
            fish:ScaleTo(3)
            print("scaled", fish:GetScale(), tostring(body.Size), tostring(tail.Size), tostring(tail.Position), tostring(eye.Position))
            fish:ScaleTo(1)
            print("unscaled", fish:GetScale(), tostring(tail.Size), tostring(tail.Position))
            for i = 1, 1000 do local p = Instance.new("Part"); p.Name = "tmp" .. i end
            print("descendants", #workspace:GetDescendants(), #game:GetDescendants() > 10)
            local sv = Instance.new("IntValue"); sv.Name = "Score"; sv.Value = 5; sv.Parent = game.ReplicatedStorage
            sv:GetPropertyChangedSignal("Value"):Connect(function() print("score", sv.Value) end)
            sv.Value += 1
        )");
        rt.startScripts();
        check(log.has("event\thello\t42") && log.has("invoke\t42\ttwo"), "BindableEvent/BindableFunction");
        check(log.has("pivot\t10, 0, 0\t12, 0, 0\t10, 0, 0"), ("PivotTo moves the whole model: " + log.joined()).c_str());
        check(log.has("moveto\t2, 5, 0"), "MoveTo");
        check(log.has("scaled\t3\t6, 6, 6\t3, 3, 3\t7, 0, 0\t0, 3, 0"), ("Model:ScaleTo sizes and spreads the parts about the pivot, attachments too: " + log.joined()).c_str());
        check(log.has("unscaled\t1\t1, 1, 1\t3, 0, 0"), "and ScaleTo(1) puts it back");
        check(log.has("descendants\t4\ttrue"), "unparented instances are not in the tree (the model, its two parts, and Terrain)");
        check(log.has("score\t6"), "IntValue + GetPropertyChangedSignal");
        // Unparented parts only lived in Lua; after GC they are gone from the DataModel too.
        size_t before = dm.instanceCount();
        lua_State* L = rt.sandbox().rawState();
        lua_gc(L, LUA_GCCOLLECT, 0); lua_gc(L, LUA_GCCOLLECT, 0);
        rt.step(0.016);
        check(dm.instanceCount() < before && dm.instanceCount() < 100, ("GC releases unreferenced instances: " + std::to_string(before) + " -> " + std::to_string(dm.instanceCount())).c_str());
        check(log.errors.empty(), "no errors");
    }

    section("R10b. DataModel::snapshot rebuilds a tree");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        rt.runChunk("Snap", R"(
            local m = Instance.new("Model"); m.Name = "Car"; m.Parent = workspace
            local p = Instance.new("Part"); p.Name = "Wheel"; p.Size = Vector3.new(1,2,3); p.Material = Enum.Material.Neon; p.Anchored = true; p.Parent = m
            local v = Instance.new("StringValue"); v.Value = "hi"; v.Parent = game.ReplicatedStorage
        )");
        std::vector<Change> snap = dm.snapshot();
        DataModel copy(false);
        std::string err; bool ok = true;
        for (auto& c : snap) if (!copy.apply(c, &err)) { ok = false; err += " at kind=" + std::to_string(c.kind) + " id=" + std::to_string(c.id) + " " + c.className + " " + c.name + " parent=" + std::to_string(c.parent); break; }
        Instance* wheel = copy.root()->findFirstChild("Wheel", true);
        Instance* sv = copy.getService("ReplicatedStorage")->findFirstChildOfClass("StringValue");
        std::string diag = err + " ok=" + std::to_string(ok) + " wheel=" + (wheel ? wheel->fullName() : "null")
            + " sv=" + (sv ? sv->get("Value").s : "null") + " counts=" + std::to_string(copy.instanceCount()) + "/" + std::to_string(dm.instanceCount());
        if (wheel) diag += " size=" + std::to_string(wheel->get("Size").v.y) + " mat=" + wheel->get("Material").s + " anch=" + std::to_string(wheel->get("Anchored").b);
        check(ok && wheel && wheel->parent()->name() == "Car" && wheel->parent()->parent() == copy.workspace()
              && wheel->get("Size").v == Vec3{1, 2, 3} && wheel->get("Material").s == "Neon" && wheel->get("Anchored").b
              && sv && sv->get("Value").s == "hi"
              && copy.instanceCount() == dm.instanceCount(),
              ("snapshot applied to a fresh DataModel reproduces it: " + diag).c_str());
        bool noNoise = true;
        for (auto& c : snap) if (c.kind == Change::Property && c.name == "Name") noNoise = false;
        check(noNoise && snap.size() < 80, ("snapshot is Create/Parent/non-default props only: " + std::to_string(snap.size())).c_str());
    }

    section("R11. stopping scripts: Disabled / Destroy end threads and connections; hostWrite");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        Instance* part = dm.create("Part", dm.workspace()).get(); part->setName("P");
        Instance* s = addScript(rt, "Script", "Stoppable", dm.getService("ServerScriptService"), R"(
            print("started", script.Name)
            workspace.P.Touched:Connect(function() print("touched handler") end)
            task.spawn(function() while true do task.wait(0.1); print("tick") end end)
            task.delay(5, function() print("never") end)
            workspace.P.Touched:Wait(); print("never either")
        )");
        Instance* self = addScript(rt, "Script", "SelfStop", dm.getService("ServerScriptService"), R"(
            task.spawn(function() task.wait(0.05); print("self spawn alive") end)
            script.Disabled = true
            print("after self disable")
            task.wait(0.05)
            print("never resumes")
        )");
        rt.startScripts();
        rt.step(0.1);
        check(log.has("started\tStoppable") && log.count("tick") == 1 && log.has("after self disable"), "scripts ran");
        check(!log.has("self spawn alive") && !log.has("never resumes"), "a script disabling itself: its spawned threads die, it stops after the current slice");
        s->set("Enabled", Value::boolean(false));
        rt.step(0.1); rt.step(0.1);
        rt.fireEvent(*part, "Touched", {Value::instance(part->id())});
        check(log.count("tick") == 1 && !log.has("touched handler") && !log.has("never either"), "Enabled=false killed the loop, the connection and the waiter");
        check(rt.stats().threadsLive == 0, ("no live threads: " + std::to_string(rt.stats().threadsLive)).c_str());
        s->set("Enabled", Value::boolean(true));
        rt.step(0.1); rt.step(0.1);
        check(log.count("started\tStoppable") == 2 && log.count("tick") == 2, "Enabled=true restarts it from the top");
        s->destroy();
        rt.step(0.1); rt.step(0.1);
        check(log.count("tick") == 2 && rt.stats().threadsLive == 0, "Destroy stops it for good");
        // hostWrite: the physics snapshot writes Position without the mirror echoing it.
        rt.takeChanges();
        check(rt.hostWrite(part->id(), "Position", Value::vector3(1, 2, 3)), "hostWrite ok");
        part->set("Transparency", Value::number(0.5));
        auto ch = rt.takeChanges();
        check(ch.size() == 2 && ch[0].fromHost && ch[0].name == "Position" && !ch[1].fromHost, "fromHost marks engine writes only");
        check(rt.eval("workspace.P.Position.Z") == "3", "scripts see the engine's write");
        check(!rt.hostWrite(99999, "Position", Value::vector3(0, 0, 0)), "hostWrite to a missing id fails quietly");
        check(log.errors.empty(), "no errors");
    }

    section("R12. Rojo file layout -> instances; reload restarts scripts");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        SourceFileInfo fi;
        check(classifySourceFile("ServerScriptService/Main.server.luau", fi) && fi.className == "Script" && fi.name == "Main" && fi.containers.size() == 1 && fi.containers[0] == "ServerScriptService", "x.server.luau -> Script");
        check(classifySourceFile("StarterPlayer\\StarterPlayerScripts\\UI.client.luau", fi) && fi.className == "LocalScript" && fi.name == "UI" && fi.containers.size() == 2, "x.client.luau -> LocalScript (backslashes ok)");
        check(classifySourceFile("ReplicatedStorage/Shared/init.luau", fi) && fi.className == "ModuleScript" && fi.name == "Shared" && fi.containers.size() == 1, "init.luau names its directory");
        check(!classifySourceFile("README.md", fi) && !classifySourceFile("init.luau", fi), "non-sources rejected");
        std::string err;
        Instance* util = loadSourceFile(rt, "ReplicatedStorage/Shared/Util.luau", "return { v = 1 }", &err);
        Instance* shared = loadSourceFile(rt, "ReplicatedStorage/Shared/init.luau", "return { util = require(script.Util) }", &err);
        check(util && shared && util->parent() == shared && shared->className() == "ModuleScript" && shared->fullName() == "ReplicatedStorage.Shared", ("Folder upgraded to ModuleScript when init.luau arrives: " + err).c_str());
        Instance* main = loadSourceFile(rt, "ServerScriptService/Main.server.luau", "print('v', require(game.ReplicatedStorage.Shared).util.v)", &err);
        check(main && main->className() == "Script", "Script created");
        check(!loadSourceFile(rt, "Nope/x.server.luau", "", &err) && err == "'Nope' is not a valid Service name", "unknown top-level container");
        loadSourceFile(rt, "Workspace/Map/Lava/Killer.server.luau", "print('lava', script:GetFullName())", &err);
        rt.startScripts();
        check(log.has("v\t1") && log.has("lava\tWorkspace.Map.Lava.Killer"), ("scripts run in their containers: " + log.joined()).c_str());
        check(dm.workspace()->findFirstChild("Map")->className() == "Folder", "nested dirs are Folders");
        // Save again: the module's cache drops, the script restarts.
        loadSourceFile(rt, "ReplicatedStorage/Shared/Util.luau", "return { v = 2 }", &err);
        Instance* main2 = loadSourceFile(rt, "ServerScriptService/Main.server.luau", "print('v', require(game.ReplicatedStorage.Shared).util.v)", &err);
        rt.step(0.016);
        check(main2 == main && log.has("v\t2"), "reload keeps the instance, restarts the script, refreshes the module");
        check(unloadSourceFile(rt, "ReplicatedStorage/Shared/init.luau") && dm.getService("ReplicatedStorage")->findFirstChild("Shared")->className() == "Folder"
              && dm.getService("ReplicatedStorage")->findFirstChild("Shared")->findFirstChild("Util") != nullptr, "removing init.luau degrades to a Folder, siblings survive");
        check(unloadSourceFile(rt, "ServerScriptService/Main.server.luau") && !dm.getService("ServerScriptService")->findFirstChild("Main") && !unloadSourceFile(rt, "ServerScriptService/Main.server.luau"), "unload destroys; twice is false");
        check(log.errors.empty(), "no errors");
    }

    section("R14. replication + remotes: a server and two clients in one process");
    {
        Log slog, alog, blog;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime::Options copts = serverOpts(); copts.isServer = false;
        Runtime alice(copts, callbacks(alog)), bob(copts, callbacks(blog));
        DataModel& sdm = server.dataModel();
        addScript(server, "Script", "Main", sdm.getService("ServerScriptService"), R"(
            local RS = game.ReplicatedStorage
            local ping = Instance.new("RemoteEvent"); ping.Name = "Ping"; ping.Parent = RS
            local ask = Instance.new("RemoteFunction"); ask.Name = "Ask"; ask.Parent = RS
            local part = Instance.new("Part"); part.Name = "Shared"; part.Color = Color3.new(1, 0, 0); part.Parent = workspace
            local secret = Instance.new("Part"); secret.Name = "Secret"; secret.Parent = game.ServerStorage
            ping.OnServerEvent:Connect(function(player, msg, data)
                print("server got", player.Name, msg, data.n, data.list[2], data.who == player)
                print("server seq", data.range.Min, data.range.Max, #data.fade.Keypoints, data.fade.Keypoints[2].Value)
                ping:FireClient(player, "pong", {ok = true})
                ping:FireAllClients("all")
            end)
            ask.OnServerInvoke = function(player, a, b)
                task.wait(0.1)
                if a == "boom" then error("kaboom") end
                return a + b, CFrame.new(1, 2, 3), Enum.Material.Neon
            end
            game.Players.PlayerAdded:Connect(function(p) print("server sees", p.Name, p.UserId) end)
        )");
        addScript(server, "LocalScript", "Client", sdm.getService("StarterPlayer")->findFirstChildOfClass("StarterPlayerScripts"), R"(
            local Players = game:GetService("Players")
            local lp = Players.LocalPlayer
            print("client", lp.Name, script:GetFullName())
            local RS = game.ReplicatedStorage
            local ping = RS:WaitForChild("Ping")
            local ask = RS:WaitForChild("Ask")
            print("sees shared", workspace:FindFirstChild("Shared") ~= nil, "sees secret", game.ServerStorage:FindFirstChild("Secret") ~= nil)
            ping.OnClientEvent:Connect(function(msg, data) print("client got", msg, type(data) == "table" and data.ok or tostring(data)) end)
            Players.PlayerAdded:Connect(function(p) print("client sees", p.Name) end)
            local local_only = Instance.new("Part"); local_only.Name = "LocalOnly"; local_only.Parent = workspace
            if lp.Name == "Alice" then
                ping:FireServer("hello", {n = 3, list = {"a", "b"}, who = lp,
                                          range = NumberRange.new(2, 5), fade = NumberSequence.new(1, 0)})
                local sum, cf, mat = ask:InvokeServer(2, 3)
                print("invoke", sum, cf.Position, mat == Enum.Material.Neon)
                local ok, err = pcall(function() return ask:InvokeServer("boom") end)
                print("invoke error", ok, string.find(err, "kaboom") ~= nil)
            end
            local ch = lp.Character or lp.CharacterAdded:Wait()
            print("character", ch.Name, ch:FindFirstChild("Humanoid") ~= nil)
            workspace.Shared:GetPropertyChangedSignal("Position"):Connect(function() print("shared moved", workspace.Shared.Position.y) end)
            workspace.ChildRemoved:Connect(function(c) print("gone", c.Name) end)
        )");
        addScript(server, "Script", "Imports", sdm.getService("ServerScriptService"), R"(
            -- `workspace.Shared.Position` must not be resolved once at load time (Luau GETIMPORT)
            task.wait(0.01)
            workspace.Shared.Position = Vector3.new(0, 1, 0)
            print("import", workspace.Shared.Position.y, game.Workspace.Shared.Position.y)
            workspace.Shared.Position = Vector3.new(0, 2, 0)
            print("import", workspace.Shared.Position.y, game.Workspace.Shared.Position.y)
            workspace.Shared:Destroy()
            local again = Instance.new("Part"); again.Name = "Shared"; again.Color = Color3.new(1, 0, 0); again.Parent = workspace
            print("import", workspace.Shared == again, game.Workspace.Shared == again)
        )");
        server.startScripts();
        server.step(0.05);   // remotes exist before anyone joins
        check(slog.has("import	1	1") && slog.has("import	2	2") && slog.has("import	true	true"), "global Instance chains are looked up every time, never cached at load");

        LocalSession session(server);
        Instance* aliceLP = session.join(alice, "Alice", 11);
        Instance* aliceServer = sdm.getService("Players")->findFirstChild("Alice");
        check(aliceLP && aliceServer && aliceLP->id() == aliceServer->id() && alice.localPlayer() == aliceLP
              && alice.dataModel().getService("Players")->get("LocalPlayer").ref == aliceLP->id(),
              "the client's LocalPlayer is the server's Player, same id");
        check(slog.has("server sees\tAlice\t11"), "PlayerAdded on the server");
        check(alog.has("client\tAlice\tPlayers.Alice.PlayerScripts.Client"), "StarterPlayerScripts copied into PlayerScripts and running on the client");
        check(alog.has("sees shared\ttrue\tsees secret\tfalse"), "Workspace replicates, ServerStorage does not");
        Instance* mainOnClient = alice.dataModel().find(sdm.getService("ServerScriptService")->findFirstChild("Main")->id());
        Instance* clientScriptSrc = alice.dataModel().find(sdm.getService("StarterPlayer")->findFirstChildOfClass("StarterPlayerScripts")->findFirstChild("Client")->id());
        check(!mainOnClient && clientScriptSrc && !clientScriptSrc->get("Source").s.empty(), "server Script stays on the server; LocalScript source reaches the client");

        for (int i = 0; i < 12; i++) session.stepAll(0.05);
        check(slog.has("server got\tAlice\thello\t3\tb\ttrue"), "FireServer: table with nested list + instance ref arrives, sender prepended");
        check(slog.has("server seq\t2\t5\t2\t0"), "a NumberRange and a NumberSequence cross a RemoteEvent whole");
        check(alog.has("client got\tpong\ttrue") && alog.has("client got\tall\tnil"), "FireClient / FireAllClients reach the client");
        check(alog.has("invoke\t5\t1, 2, 3\ttrue"), "InvokeServer waits for a yielding OnServerInvoke; CFrame and EnumItem cross");
        check(alog.has("invoke error\tfalse\ttrue") && !slog.errors.empty(), "an erroring OnServerInvoke raises in the caller and is logged on the server");
        check(alog.has("character\tAlice\ttrue"), "the character spawned by the server is the client's lp.Character");
        check(sdm.workspace()->findFirstChild("LocalOnly") == nullptr && alice.dataModel().workspace()->findFirstChild("LocalOnly") != nullptr,
              "client-made instances never reach the server");

        // Second client: sees Alice, Alice sees Bob.
        session.join(bob, "Bob", 12);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        check(blog.has("sees shared\ttrue\tsees secret\tfalse") && blog.has("character\tBob\ttrue"), "second client gets the world and its own character");
        check(alog.count("client sees\tBob") == 1 && !alog.has("client sees\tAlice") && !blog.has("client sees\tBob") && slog.count("server sees\tBob\t12") == 1,
              "PlayerAdded for the newcomer fires exactly once, on the others");
        check(bob.dataModel().getService("Players")->findFirstChild("Alice") != nullptr && bob.localPlayer()->name() == "Bob", "Bob sees Alice; his LocalPlayer is Bob");

        // Physics writes replicate; leaving the replicated tree destroys the client copy.
        Instance* shared = sdm.workspace()->findFirstChild("Shared");
        server.hostWrite(shared->id(), "Position", Value::vector3(0, 7, 0));
        session.stepAll(0.05);
        Instance* sharedA = alice.dataModel().find(shared->id());
        check(sharedA && sharedA->get("Position").v.y == 7 && sharedA->get("Color").c.r == 1 && alog.has("shared moved\t7"), "host writes on the server land on the client, Changed fires there");
        shared->setParent(sdm.getService("ServerStorage"));
        session.stepAll(0.05);
        check(alice.dataModel().find(shared->id()) == nullptr && alog.has("gone\tShared") && bob.dataModel().find(shared->id()) == nullptr,
              "moving into ServerStorage removes it from every client");
        shared->setParent(sdm.workspace());
        session.stepAll(0.05);
        check(alice.dataModel().find(shared->id()) != nullptr && alice.dataModel().find(shared->id())->get("Position").v.y == 7, "and back again brings it back with its properties");

        // A model built off-tree and parented whole arrives whole, once.
        server.runChunk("Build", R"(
            local m = Instance.new("Model"); m.Name = "House"
            for i = 1, 3 do local p = Instance.new("Part"); p.Name = "Wall" .. i; p.Parent = m end
            m.Parent = workspace
            local w = m.Wall2; w.Parent = nil; w.Parent = m   -- moved out and back in the same frame
        )");
        session.stepAll(0.05);
        Instance* houseA = alice.dataModel().workspace()->findFirstChild("House");
        check(houseA && houseA->children().size() == 3 && houseA->findFirstChild("Wall2") && alog.warns.empty() && blog.warns.empty(),
              ("a model parented whole replicates whole: " + std::to_string(houseA ? houseA->children().size() : 0) + " walls, warns=" + std::to_string(alog.warns.size())).c_str());

        session.leave(alice);
        session.stepAll(0.05);
        check(sdm.getService("Players")->findFirstChild("Alice") == nullptr && bob.dataModel().getService("Players")->findFirstChild("Alice") == nullptr
              && session.clientCount() == 1, "leaving removes the Player everywhere");
    }

    section("R13. RuntimeThread: frames on a worker, results on the caller");
    for (bool threaded : {false, true}) {
        Runtime::Options o = serverOpts();
        RuntimeThread th(o, threaded);
        int wakes = 0;
        th.setWakeHandler([&wakes] { wakes++; });
        FrameIn f0;
        f0.jobs.push_back([](Runtime& rt) {
            loadSourceFile(rt, "ServerScriptService/Main.server.luau", R"(
                local p = Instance.new("Part"); p.Name = "Mover"; p.Anchored = true; p.Parent = workspace
                p.Touched:Connect(function(o) print("touched", o.Name) end)
                game:GetService("RunService").Heartbeat:Connect(function(dt) p.Position += Vector3.new(dt, 0, 0) end)
                print("hello from", game:GetService("RunService"):IsServer())
            )");
        });
        th.submit(std::move(f0));
        th.waitIdle();
        FrameOut o0 = th.take();
        int64_t mover = 0;
        for (auto& c : o0.changes) if (c.kind == Change::Create && c.className == "Part") mover = c.id;
        check(mover != 0 && o0.logs.size() == 1 && o0.logs[0].text == "hello from\ttrue", (std::string(threaded ? "threaded" : "inline") + ": frame 0 created the part and printed").c_str());
        FrameIn f1; f1.dt = 0.5;
        f1.writes.push_back({mover, "Transparency", Value::number(0.25)});
        f1.events.push_back({mover, "Touched", {Value::instance(mover)}});
        th.submit(std::move(f1));
        th.waitIdle();
        FrameOut o1 = th.take();
        bool sawHostWrite = false, sawScriptMove = false;
        for (auto& c : o1.changes) {
            if (c.kind == Change::Property && c.name == "Transparency" && c.fromHost) sawHostWrite = true;
            if (c.kind == Change::Property && c.name == "Position" && !c.fromHost && std::fabs(c.value.v.x - 0.5f) < 1e-5) sawScriptMove = true;
        }
        std::string diag;
        for (auto& l : o1.logs) diag += " log[" + l.text + "]";
        for (auto& c : o1.changes) if (c.kind == Change::Property) diag += " " + c.name + (c.fromHost ? "(host)" : "");
        check(sawHostWrite && sawScriptMove && o1.logs.size() == 1 && o1.logs[0].text == "touched\tMover" && o1.now == 0.5,
              (std::string(threaded ? "threaded" : "inline") + ": writes applied, events fired, Heartbeat moved the part; now=" + std::to_string(o1.now) + diag).c_str());
        check(th.idle() && (threaded ? wakes == 2 : wakes == 0), "wake handler per frame when threaded");
        check(th.take().changes.empty(), "take drains");
    }

    section("R15. wire codec: every packet kind and value type survive a round trip; garbage is refused");
    {
        NetPacket hello; hello.type = NetPacket::Hello; hello.name = "ada"; hello.userId = -7;
        NetPacket h2;
        check(decodePacket(encodePacket(hello), h2) && h2.type == NetPacket::Hello && h2.protocol == kWireProtocol && h2.name == "ada" && h2.userId == -7, "Hello");

        NetPacket welcome; welcome.type = NetPacket::Welcome; welcome.playerId = 1029;
        Change cr; cr.kind = Change::Create; cr.id = 5; cr.parent = 2; cr.className = "Part"; cr.name = "Floor";
        Change pr; pr.kind = Change::Parent; pr.id = 5; pr.parent = kNoParent; pr.fromHost = true;
        Change ds; ds.kind = Change::Destroy; ds.id = 5;
        welcome.changes = {cr, pr, ds};
        const char* names[] = {"Nil", "Bool", "Number", "String", "Vector3", "Color3", "Ref", "Enum", "Vector2", "UDim", "UDim2", "Font"};
        Value en; en.type = Value::Enum; en.s = "Neon"; en.n = 288;
        Value vals[] = {Value::nil(), Value::boolean(true), Value::number(-1.5e300), Value::string(std::string("a\0b\xff", 4)),
                        Value::vector3(1, -2.5f, 1e30f), Value::color3(0.1f, 0.2f, 0.3f), Value::instance(-99), en, Value::vector2(0.5f, -7),
                        Value::udim(0.5f, -3), Value::udim2(0.1f, 20, 0.9f, -40), Value::font("rbxasset://fonts/families/GothamSSm.json", 700, true)};
        for (int i = 0; i < 12; i++) { Change c; c.kind = Change::Property; c.id = 5; c.name = names[i]; c.value = vals[i]; welcome.changes.push_back(c); }
        NetPacket w2;
        bool okW = decodePacket(encodePacket(welcome), w2) && w2.type == NetPacket::Welcome && w2.playerId == 1029 && w2.changes.size() == 15;
        if (okW) {
            auto& c = w2.changes;
            okW = c[0].kind == Change::Create && c[0].id == 5 && c[0].parent == 2 && c[0].className == "Part" && c[0].name == "Floor" && !c[0].fromHost
               && c[1].kind == Change::Parent && c[1].parent == kNoParent && c[1].fromHost
               && c[2].kind == Change::Destroy && c[2].id == 5;
            for (int i = 0; i < 12 && okW; i++) {
                const Value& a = vals[i]; const Value& b = c[3 + i].value;
                okW = c[3 + i].name == names[i] && a.type == b.type && a.b == b.b && a.n == b.n && a.s == b.s && a.v == b.v && a.c == b.c && a.ref == b.ref;
            }
        }
        check(okW, "Welcome: Create/Parent/Destroy/Property changes, every Value type, fromHost");

        NetPacket sf; sf.type = NetPacket::ServerFrame;
        sf.changes = {cr};
        RemoteMsg ev; ev.kind = RemoteMsg::Event; ev.remote = 40; ev.player = 0;
        ev.args = {NetValue::nil(), NetValue::boolean(false), NetValue::number(3), NetValue::string("hi"), NetValue::vector3({1, 2, 3}), NetValue::instance(40)};
        NetValue cf; cf.type = NetValue::CFrame; cf.v = {1, 2, 3}; for (int i = 0; i < 9; i++) cf.m[i] = (float)i * 0.25f;
        NetValue nen; nen.type = NetValue::Enum; nen.s = "Material.Neon"; nen.n = 288;
        NetValue col; col.type = NetValue::Color3; col.c = {1, 0.5f, 0};
        NetValue inner; inner.type = NetValue::Table; inner.t = std::make_shared<std::vector<std::pair<NetValue, NetValue>>>();
        inner.t->push_back({NetValue::number(1), NetValue::string("one")});
        NetValue tbl; tbl.type = NetValue::Table; tbl.t = std::make_shared<std::vector<std::pair<NetValue, NetValue>>>();
        tbl.t->push_back({NetValue::string("k"), inner});
        tbl.t->push_back({NetValue::number(2), cf});
        NetValue rng; rng.type = NetValue::NumberRange; rng.u[0] = 2; rng.u[1] = 5;
        NetValue seq; seq.type = NetValue::ColorSequence; seq.kp = {0, 1, 0, 0, 1, 0, 0, 1};
        ev.args.push_back(nen); ev.args.push_back(col); ev.args.push_back(tbl); ev.args.push_back(rng); ev.args.push_back(seq);
        RemoteMsg inv; inv.kind = RemoteMsg::Invoke; inv.remote = 41; inv.player = 1029; inv.call = 0xFFFFFFFFFFull; inv.args = {NetValue::number(9)};
        RemoteMsg res; res.kind = RemoteMsg::Result; res.remote = 41; res.call = 12; res.ok = false; res.args = {NetValue::string("boom")};
        sf.remotes = {ev, inv, res};
        NetPacket s2;
        bool okS = decodePacket(encodePacket(sf), s2) && s2.type == NetPacket::ServerFrame && s2.changes.size() == 1 && s2.remotes.size() == 3;
        if (okS) {
            auto& e = s2.remotes[0];
            okS = e.kind == RemoteMsg::Event && e.remote == 40 && e.args.size() == 11
               && e.args[9].type == NetValue::NumberRange && e.args[9].u[0] == 2 && e.args[9].u[1] == 5
               && e.args[10].type == NetValue::ColorSequence && e.args[10].kp.size() == 8 && e.args[10].kp[4] == 1
               && e.args[0].type == NetValue::Nil && e.args[1].type == NetValue::Bool && !e.args[1].b && e.args[2].n == 3 && e.args[3].s == "hi"
               && e.args[4].v == Vec3{1, 2, 3} && e.args[5].type == NetValue::Ref && e.args[5].ref == 40
               && e.args[6].type == NetValue::Enum && e.args[6].s == "Material.Neon" && e.args[6].n == 288
               && e.args[7].type == NetValue::Color3 && e.args[7].c == Col3{1, 0.5f, 0}
               && e.args[8].type == NetValue::Table && e.args[8].t && e.args[8].t->size() == 2
               && (*e.args[8].t)[0].first.s == "k" && (*e.args[8].t)[0].second.type == NetValue::Table && (*(*e.args[8].t)[0].second.t)[0].second.s == "one"
               && (*e.args[8].t)[1].second.type == NetValue::CFrame && (*e.args[8].t)[1].second.m[8] == 2.0f
               && s2.remotes[1].kind == RemoteMsg::Invoke && s2.remotes[1].call == 0xFFFFFFFFFFull && s2.remotes[1].player == 1029
               && s2.remotes[2].kind == RemoteMsg::Result && !s2.remotes[2].ok && s2.remotes[2].args[0].s == "boom";
        }
        check(okS, "ServerFrame: remotes with every NetValue type, nested tables, CFrame, Invoke/Result");

        NetPacket cfr; cfr.type = NetPacket::ClientFrame;
        cfr.writes = {{7, "Position", Value::vector3(1, 2, 3)}, {8, "Jump", Value::boolean(true)}};
        cfr.events = {{7, "Touched", {Value::instance(9)}}, {9, "TouchEnded", {Value::instance(7)}}};
        cfr.remotes = {ev};
        NetPacket c2;
        check(decodePacket(encodePacket(cfr), c2) && c2.type == NetPacket::ClientFrame && c2.writes.size() == 2 && c2.writes[1].prop == "Jump" && c2.writes[1].value.b
              && c2.events.size() == 2 && c2.events[1].event == "TouchEnded" && c2.events[1].args.size() == 1 && c2.events[1].args[0].ref == 7 && c2.remotes.size() == 1 && c2.remotes[0].args.size() == 11,
              "ClientFrame: writes, events, remotes");

        std::string bytes = encodePacket(sf);
        int refused = 0, total = 0;
        for (size_t cut = 0; cut < bytes.size(); cut += 7) { NetPacket x; total++; if (!decodePacket(bytes.substr(0, cut), x)) refused++; }
        NetPacket x;
        bool junk = !decodePacket("", x) && !decodePacket("PB\x09", x) && !decodePacket(bytes + "!", x)
                 && !decodePacket(std::string("PB\x02\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 13), x);
        check(refused == total && junk, "every truncation, trailing bytes, a bad type and an absurd count are refused");
        // ServerFrame, 0 changes, 1 remote (Event, remote 20, player 0, call 0, ok), 1 arg: a table nested 40 deep
        std::string deep = std::string("PB\x03\x00\x01\x00\x28\x00\x00\x01\x01", 11);
        for (int i = 0; i < 40; i++) deep += std::string("\x09\x01\x00", 3);   // Table{1 pair}: key Nil, value = Table{...
        deep += std::string("\x00\x00", 2);
        check(!decodePacket(deep, x), "a 40-deep table is refused");
    }

    section("R16. frame budget: a refused resume keeps its arguments for the retry");
    {
        Log log;
        Runtime::Options o = serverOpts();
        o.budget.maxFrameMillis = 1;
        Runtime rt(o, callbacks(log));
        loadSourceFile(rt, "ServerScriptService/Main.server.luau", R"(
            local RunService = game:GetService("RunService")
            RunService.Heartbeat:Connect(function()
                local t0 = os.clock(); while os.clock() - t0 < 0.003 do end   -- spends the frame
            end)
            RunService.Heartbeat:Connect(function(dt) print("handler", dt) end)   -- refused, retried next step
            task.spawn(function() print("waited", RunService.Heartbeat:Wait()) end)
        )");
        rt.step(0.5);
        bool quiet = true;
        for (auto& l : log.lines) if (l.find("handler") != std::string::npos || l.find("waited") != std::string::npos) quiet = false;
        check(quiet && rt.stats().skipped >= 2, "the second handler and the Wait were refused by the spent frame budget");
        rt.step(0.25);
        bool handler = false, waited = false, errors = false;
        for (auto& l : log.lines) {
            if (l.find("handler\t0.5") != std::string::npos) handler = true;
            if (l.find("waited\t0.5") != std::string::npos) waited = true;
            if (l.rfind("error", 0) == 0 || l.find("nil") != std::string::npos) errors = true;
        }
        check(handler && waited && !errors && rt.stats().errors == 0 && log.errors.empty(), "the retry ran them with the dt of the frame that fired them");
    }

    section("R17. DataStoreService: a local store that outlives the runtime");
    {
        std::string path = "harness_datastore.json";
        std::remove(path.c_str());
        Runtime::Options o = serverOpts();
        o.dataStorePath = path;
        Log log;
        {
            Runtime rt(o, callbacks(log));
            rt.runChunk("ds", R"(
                local DSS = game:GetService("DataStoreService")
                local ds = DSS:GetDataStore("PlayerData")
                print("empty", (ds:GetAsync("7")))
                ds:SetAsync("7", {coins = 10, name = "ada", tags = {"a", "b"}})
                print("inc", ds:IncrementAsync("visits"), ds:IncrementAsync("visits", 2))
                local new = (ds:UpdateAsync("7", function(old) old.coins += 5; return old end))
                local kept = (ds:UpdateAsync("7", function(old) return nil end))
                print("upd", new.coins, ds:GetAsync("7").coins, kept)
                print("same", DSS:GetDataStore("PlayerData") == ds, DSS:GetDataStore("PlayerData", "test") == ds, ds.ClassName, ds.Name)
                DSS:GetDataStore("PlayerData", "test"):SetAsync("7", "scoped")
                print("removed", (ds:RemoveAsync("visits")), (ds:GetAsync("visits")))
                print("bad", select(2, pcall(function() ds:SetAsync("x", nil) end)), select(2, pcall(function() ds:GetAsync("") end)))
            )");
            check(log.has("empty\tnil") && log.has("inc\t1\t3") && log.has("upd\t15\t15\tnil") && log.has("removed\t3\tnil"),
                  "GetAsync/SetAsync/UpdateAsync/IncrementAsync/RemoveAsync on a GlobalDataStore, tables and all");
            check(log.has("same\ttrue\tfalse\tDataStore\tPlayerData"), "GetDataStore returns the same store for a name, another per scope, and it is a DataStore");
            check(log.has("bad\tds:13: Argument 2 missing or nil\tds:13: DataStore key can't be empty or longer than 50 characters"), "nil values and empty keys are refused");
            rt.step(0.016);   // a step after a change writes the file
        }
        Log log2;
        {
            Runtime rt(o, callbacks(log2));
            rt.runChunk("ds2", R"(
                local DSS = game:GetService("DataStoreService")
                local d = DSS:GetDataStore("PlayerData"):GetAsync("7")
                print("back", d.coins, d.name, #d.tags, (DSS:GetDataStore("PlayerData"):GetAsync("visits")), (DSS:GetDataStore("PlayerData", "test"):GetAsync("7")))
            )");
            check(log2.has("back\t15\tada\t2\tnil\tscoped"), "the next runtime reads them back from the file");
        }
        Log logo;
        {
            Runtime rt(o, callbacks(logo));
            rt.runChunk("ord", R"(
                local DSS = game:GetService("DataStoreService")
                local board = DSS:GetOrderedDataStore("Coins")
                for name, coins in {ann = 30, bob = 10, cid = 50, dee = 20, eve = 40} do board:SetAsync(name, coins) end
                print("nsp", (DSS:GetDataStore("Coins"):GetAsync("ann")), (board:GetAsync("ann")), board.ClassName)
                print("int", select(2, pcall(function() board:SetAsync("x", 1.5) end)))
                local pages = board:GetSortedAsync(false, 2)
                local out = {}
                while true do
                    for _, e in pages:GetCurrentPage() do table.insert(out, e.key .. "=" .. e.value) end
                    if pages.IsFinished then break end
                    pages:AdvanceToNextPageAsync()
                end
                print("top", table.concat(out, " "), select(2, pcall(function() pages:AdvanceToNextPageAsync() end)))
                local asc = board:GetSortedAsync(true, 10, 20, 40):GetCurrentPage()
                print("asc", asc[1].key, asc[#asc].key, #asc)
            )");
            check(logo.has("nsp\tnil\t30\tOrderedDataStore") && logo.ends("OrderedDataStore values must be integers"),
                  "GetOrderedDataStore is its own namespace and takes integers only");
            check(logo.has("top\tcid=50 eve=40 ann=30 dee=20 bob=10\tord:14: No more pages to advance to") && logo.has("asc\tdee\teve\t3"),
                  "GetSortedAsync pages through the ranking; IsFinished / AdvanceToNextPageAsync; min/max bounds");
        }
        std::remove(path.c_str());
        Log log3;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime rc(co, callbacks(log3));
        rc.runChunk("dsc", R"(print(select(2, pcall(function() return game:GetService("DataStoreService"):GetDataStore("x") end))))");
        check(log3.ends("DataStore can't be accessed from client"), "a client cannot open one");
    }

    section("R18. input: UserInputService + ContextActionService fed by the host; Vector2");
    {
        Log log;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime rt(co, callbacks(log));
        Instance* ls = addScript(rt, "LocalScript", "Input", rt.dataModel().getService("ReplicatedFirst"), R"(
            local UIS = game:GetService("UserInputService")
            local CAS = game:GetService("ContextActionService")
            UIS.InputBegan:Connect(function(io, gp) print("began", io.UserInputType.Name, io.KeyCode.Name, gp, UIS:IsKeyDown(Enum.KeyCode.E), io.ClassName) end)
            UIS.InputEnded:Connect(function(io, gp) print("ended", io.KeyCode.Name, gp, UIS:IsKeyDown(Enum.KeyCode.E), #UIS:GetKeysPressed()) end)
            UIS.InputChanged:Connect(function(io) print("moved", io.UserInputType.Name, io.Position.X, io.Delta.X, tostring(UIS:GetMouseLocation()), io.Position.Z) end)
            UIS.JumpRequest:Connect(function() print("jump") end)
            CAS:BindAction("Fire", function(name, state, io) print("action", name, state.Name, io.KeyCode.Name, UIS:IsMouseButtonPressed(Enum.UserInputType.MouseButton1)) end,
                           false, Enum.KeyCode.E, Enum.UserInputType.MouseButton1)
            CAS:BindAction("Peek", function(name, state) print("peek", state.Name) return Enum.ContextActionResult.Pass end, false, Enum.KeyCode.Q)
            CAS:BindActionAtPriority("Low", function() print("low") end, false, Enum.ContextActionPriority.Low, Enum.KeyCode.E)
            print("bound", CAS:GetAllBoundActionInfo().Fire.priorityLevel, #CAS:GetAllBoundActionInfo().Fire.inputTypes)
        )");
        rt.startScripts();
        auto key = [&](const char* k, const char* st) { Runtime::UserInput in; in.type = "Keyboard"; in.key = k; in.state = st; rt.input(in); };
        key("E", "Begin"); key("E", "End");
        key("Q", "Begin");
        key("Space", "Begin");
        { Runtime::UserInput in; in.type = "MouseMovement"; in.state = "Change"; in.position = {100, 50, 0}; in.delta = {3, 0, 0}; rt.input(in); }
        { Runtime::UserInput in; in.type = "MouseButton1"; in.state = "Begin"; in.position = {100, 50, 0}; rt.input(in); }
        { Runtime::UserInput in; in.type = "MouseWheel"; in.state = "Change"; in.position = {100, 50, -1}; rt.input(in); }
        check(log.has("bound\t2000\t2"), "BindAction records priority and inputs");
        check(log.has("action\tFire\tBegin\tE\tfalse") && log.has("began\tKeyboard\tE\ttrue\ttrue\tInputObject") && !log.has("low"),
              "a key runs the highest-priority binding, which sinks it (gameProcessed = true); IsKeyDown sees it");
        check(log.has("action\tFire\tEnd\tE\tfalse") && log.has("ended\tE\ttrue\tfalse\t0"), "release: End state, key no longer down");
        check(log.has("peek\tBegin") && log.has("began\tKeyboard\tQ\tfalse\tfalse\tInputObject"), "ContextActionResult.Pass lets UserInputService see it");
        check(log.has("began\tKeyboard\tSpace\tfalse\tfalse\tInputObject") && log.has("jump"), "Space fires JumpRequest");
        check(log.has("moved\tMouseMovement\t100\t3\t100, 50\t0") && log.has("moved\tMouseWheel\t100\t0\t100, 50\t-1"),
              "mouse motion / wheel arrive as InputChanged; GetMouseLocation follows");
        check(log.has("action\tFire\tBegin\tUnknown\ttrue") && log.has("began\tMouseButton1\tUnknown\ttrue\tfalse\tInputObject"),
              "a UserInputType binding catches the mouse button; IsMouseButtonPressed");
        size_t before = log.lines.size();
        rt.runChunk("unb", R"(game:GetService("ContextActionService"):UnbindAction("Fire"))");
        key("E", "Begin");
        check(log.has("low") && log.has("began\tKeyboard\tE\ttrue\ttrue\tInputObject") && log.lines.size() == before + 2, "UnbindAction: the next binding down takes over");
        ls->set("Disabled", Value::boolean(true));
        key("E", "End"); key("E", "Begin");
        check(log.lines.size() == before + 2, "a disabled script's bindings and connections are gone");
        rt.runChunk("v2", R"(
            local a, b = Vector2.new(3, 4), Vector2.new(1, 2)
            print("v2", a.Magnitude, tostring(a + b), tostring(a * 2), tostring(a / b), tostring(-b), a:Dot(b), a:Cross(b), typeof(a), a == Vector2.new(3, 4), tostring(Vector2.one:Lerp(Vector2.zero, 0.5)))
            local p = Instance.new("Part")
            p:SetAttribute("Anchor", Vector2.new(0.5, 0.25))
            print("attr", tostring(p:GetAttribute("Anchor")), typeof(p:GetAttribute("Anchor")))
        )");
        check(log.has("v2\t5\t4, 6\t6, 8\t3, 2\t-1, -2\t11\t2\tVector2\ttrue\t0.5, 0.5") && log.has("attr\t0.5, 0.25\tVector2"), "Vector2: math, methods, an attribute");
        Log slog;
        Runtime srv(serverOpts(), callbacks(slog));
        srv.runChunk("s", R"(game:GetService("UserInputService").InputBegan:Connect(function() print("server saw input") end))");
        { Runtime::UserInput in; in.type = "Keyboard"; in.key = "E"; srv.input(in); }
        srv.runChunk("s2", R"(print("srv", game:GetService("UserInputService"):IsKeyDown(Enum.KeyCode.E)))");
        check(!slog.has("server saw input") && slog.has("srv\tfalse"), "a server ignores input");
    }

    section("R19. GUI: StarterGui -> PlayerGui, ScreenGui / Frame / TextLabel / TextButton, UDim2, clicks from the engine");
    {
        Log log;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime rt(co, callbacks(log));
        rt.runChunk("gui", R"(
            local gui = Instance.new("ScreenGui"); gui.Name = "Hud"
            local frame = Instance.new("Frame"); frame.Name = "Panel"
            frame.Size = UDim2.new(0.5, 0, 0, 120); frame.Position = UDim2.fromScale(0.5, 1); frame.AnchorPoint = Vector2.new(0.5, 1)
            frame.BackgroundColor3 = Color3.fromRGB(30, 30, 30); frame.Parent = gui
            local corner = Instance.new("UICorner"); corner.CornerRadius = UDim.new(0, 12); corner.Parent = frame
            local label = Instance.new("TextLabel"); label.Name = "Title"; label.Text = "Coins: 0"; label.TextSize = 24
            label.Size = UDim2.fromScale(1, 0.5); label.Parent = frame
            local button = Instance.new("TextButton"); button.Name = "Buy"; button.Text = "Buy"
            button.Position = UDim2.fromScale(0, 0.5); button.Size = UDim2.fromScale(1, 0.5); button.Parent = frame
            local ls = Instance.new("LocalScript"); ls.Name = "Hud"; ls.Source = [[
                local gui = script.Parent
                local player = game:GetService("Players").LocalPlayer
                print("gui", gui:GetFullName(), player.PlayerGui.Hud == gui, gui.Panel.Title.Text, tostring(gui.Panel.Size), tostring(gui.Panel.Position), tostring(gui.Panel.AnchorPoint))
                print("defaults", tostring(Instance.new("Frame").Size), gui.Panel.Title.TextColor3.R, gui.Panel.Title.Font.Name, gui.Panel.Title.TextXAlignment.Name, gui.Panel.Buy.AutoButtonColor, gui.Enabled, gui.ResetOnSpawn, tostring(gui.Panel.UICorner.CornerRadius))
                local coins = 0
                gui.Panel.Buy.Activated:Connect(function(input, clicks) coins += 1; gui.Panel.Title.Text = "Coins: " .. coins; print("activated", input.UserInputType.Name, clicks, gui.Panel.Title.Text) end)
                gui.Panel.Buy.MouseButton1Click:Connect(function(...) print("click", select("#", ...)) end)
                gui.Panel.Buy.MouseButton1Down:Connect(function(x, y) print("down", x, y) end)
                gui.Panel.MouseEnter:Connect(function(x, y) print("enter", x, y) end)
                gui.Panel.Buy.InputBegan:Connect(function(io) print("ibegan", io.UserInputType.Name, io.UserInputState.Name) end)
                print("udim", tostring(UDim2.new(0, 10, 0, 20) + UDim2.fromOffset(5, 5)), tostring(UDim2.fromScale(1, 1):Lerp(UDim2.new(), 0.5)), tostring(UDim.new(1, 2) - UDim.new(0.5, 1)), UDim2.new(0.25, 3, 0, 0).X.Scale, UDim2.new(0.25, 3, 0, 0).Width.Offset, typeof(UDim2.new()), typeof(UDim.new()), UDim2.new(1, 2, 3, 4) == UDim2.new(1, 2, 3, 4))
                print("bad", select(2, pcall(function() gui.Panel.Size = Vector2.new(1, 1) end)))
            ]]
            ls.Parent = gui
            gui.Parent = game:GetService("StarterGui")
        )");
        rt.addPlayer("Ann", 5);
        rt.step(0.016);
        check(log.has("gui\tPlayers.Ann.PlayerGui.Hud\ttrue\tCoins: 0\t{0.5, 0}, {0, 120}\t{0.5, 0}, {1, 0}\t0.5, 1"),
              "StarterGui copies into LocalPlayer.PlayerGui and its LocalScript runs there; UDim2 / Vector2 properties");
        check(log.has("defaults\t{0, 100}, {0, 100}\t0\tSourceSans\tCenter\ttrue\ttrue\ttrue\t0, 12"), "Roblox defaults: Size, TextColor3, Font, alignment, AutoButtonColor, Enabled, ResetOnSpawn, UICorner");
        check(log.has("udim\t{0, 15}, {0, 25}\t{0.5, 0}, {0.5, 0}\t0.5, 1\t0.25\t3\tUDim2\tUDim\ttrue"), "UDim / UDim2 math, fromScale / fromOffset, Lerp, X / Width, typeof, ==");
        check(log.ends("(UDim2 expected, got Vector2)"), "a wrong type errors");
        Instance* hud = rt.dataModel().getService("Players")->findFirstChild("Ann")->findFirstChildOfClass("PlayerGui")->findFirstChild("Hud");
        Instance* buy = hud->findFirstChild("Panel")->findFirstChild("Buy");
        rt.guiInput(hud->findFirstChild("Panel")->id(), "Enter", 1, {40, 50});
        rt.guiInput(buy->id(), "Down", 1, {42, 51});
        rt.guiInput(buy->id(), "Up", 1, {42, 51});
        rt.guiInput(buy->id(), "Click", 1, {42, 51});
        rt.guiInput(buy->id(), "Click", 1, {42, 51});
        check(log.has("enter\t40\t50") && log.has("down\t42\t51") && log.has("ibegan\tMouseButton1\tBegin") && log.has("click\t0") &&
              log.has("activated\tMouseButton1\t1\tCoins: 1") && log.has("activated\tMouseButton1\t1\tCoins: 2"),
              "engine clicks fire MouseEnter, MouseButton1Down, InputBegan, MouseButton1Click and Activated(InputObject, clickCount)");
        check(buy->get("Text").s == "Buy" && hud->findFirstChild("Panel")->findFirstChild("Title")->get("Text").s == "Coins: 2", "the handler changed the label");
        rt.runChunk("srv", R"(print("nogui", game:GetService("StarterGui").Hud.Panel.Title.Text, workspace:FindFirstChild("Camera") ~= nil))");
        check(log.has("nogui\tCoins: 0\tfalse"), "the StarterGui original is untouched");
    }

    section("R20. raycasts and the mouse: workspace:Raycast, RaycastParams, Ray, Camera rays, Player:GetMouse(), ClickDetector");
    {
        Log log;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime rt(co, callbacks(log));
        auto makeCamera = [](Runtime& r) {
            DataModel& dm = r.dataModel();
            Instance::Ptr cam = dm.create("Camera", dm.workspace());
            cam->setName("Camera");
            dm.workspace()->set("CurrentCamera", Value::instance(cam->id()));
        };
        makeCamera(rt);
        rt.addPlayer("Ann", 5);
        rt.runChunk("rays", R"(
            local function part(name, pos, size)
                local p = Instance.new("Part"); p.Name = name; p.Position = pos; p.Size = size; p.Anchored = true; p.Parent = workspace; return p
            end
            local wall = part("Wall", Vector3.new(0, 0, 0), Vector3.new(10, 10, 2)); wall.Material = Enum.Material.Wood
            local ball = part("Ball", Vector3.new(0, 0, -10), Vector3.new(4, 4, 4)); ball.Shape = Enum.PartType.Ball
            local ghost = part("Ghost", Vector3.new(0, 0, 5), Vector3.new(2, 2, 2)); ghost.CanQuery = false
            local tilted = part("Tilted", Vector3.new(20, 0, 0), Vector3.new(10, 2, 10)); tilted.Orientation = Vector3.new(0, 45, 0)
            local model = Instance.new("Model"); model.Name = "Bush"; model.Parent = workspace
            local leaf = part("Leaf", Vector3.new(0, 20, 0), Vector3.new(2, 2, 2)); leaf.Parent = model
            local r = workspace:Raycast(Vector3.new(0, 0, 20), Vector3.new(0, 0, -40))
            print("ray", r.Instance.Name, r.Position, r.Normal, r.Distance, r.Material.Name)
            local params = RaycastParams.new()
            params.FilterDescendantsInstances = {wall}
            print("params", #params.FilterDescendantsInstances, params.FilterType.Name, params.IgnoreWater, params.RespectCanCollide, typeof(params))
            local r2 = workspace:Raycast(Vector3.new(0, 0, 20), Vector3.new(0, 0, -40), params)
            print("exclude", r2.Instance.Name, r2.Position.Z, r2.Normal.Z, r2.Distance)
            params.FilterType = Enum.RaycastFilterType.Include
            local r3 = workspace:Raycast(Vector3.new(0, 0, 20), Vector3.new(0, 0, -40), params)
            print("include", r3.Instance.Name, r3.Distance, params.FilterType.Name)
            params:AddToFilter(model)
            local r4 = workspace:Raycast(Vector3.new(0, 20, 20), Vector3.new(0, 0, -40), params)
            print("model", r4.Instance.Name, r4.Distance, #params.FilterDescendantsInstances)
            print("miss", workspace:Raycast(Vector3.new(0, 0, 20), Vector3.new(0, 0, -5)), workspace:Raycast(Vector3.new(50, 0, 20), Vector3.new(0, 0, -100)), workspace:Raycast(Vector3.new(0, 0, 0), Vector3.new(0, 0, -5)) == nil)
            local r5 = workspace:Raycast(Vector3.new(21, 0, 20), Vector3.new(0, 0, -40))
            print("tilted", r5.Instance.Name, string.format("%.2f %.2f %.2f %.2f", r5.Distance, r5.Normal.X, r5.Normal.Y, r5.Normal.Z))
            local r6 = workspace:Raycast(Vector3.new(0, 0, 20), Vector3.new(0, 0, -2))
            print("short", r6, workspace:Raycast(Vector3.new(0, 0, 20), Vector3.new(0, 0, -19)) ~= nil)
            print("side", workspace:Raycast(Vector3.new(0, 30, 0), Vector3.new(0, -100, 0)).Normal, workspace:Raycast(Vector3.new(-30, 0, 0), Vector3.new(100, 0, 0)).Normal)
            local ray = Ray.new(Vector3.new(0, 0, 20), Vector3.new(0, 0, -50))
            print("Ray", ray.Origin, ray.Unit.Direction, ray:ClosestPoint(Vector3.new(3, 0, 10)), ray:Distance(Vector3.new(3, 0, 10)), typeof(ray), tostring(ray))
            local hit, pos, normal, mat = workspace:FindPartOnRay(ray)
            local hit2 = workspace:FindPartOnRayWithIgnoreList(ray, {wall, ghost})
            print("legacy", hit.Name, pos, normal, mat.Name, hit2.Name, (workspace:FindPartOnRay(Ray.new(Vector3.new(0, 50, 0), Vector3.new(0, 1, 0)))))
            local cam = workspace.CurrentCamera
            cam.CFrame = CFrame.new(0, 0, 20)
            print("viewport", cam.ViewportSize, cam.FieldOfView)
            local mid = cam:ViewportPointToRay(512, 384)
            print("center ray", mid.Origin, mid.Direction, typeof(mid))
            local p, inView = cam:WorldToViewportPoint(Vector3.new(0, 0, 0))
            local p2, inView2 = cam:WorldToViewportPoint(Vector3.new(0, 0, 30))
            local p3, inView3 = cam:WorldToScreenPoint(Vector3.new(0, 0, 10))
            print("world to viewport", p, inView, inView2, p3, inView3)
            local corner = cam:ScreenPointToRay(100, 100, 5)
            local back = cam:WorldToViewportPoint(corner.Origin)
            local along = cam:WorldToViewportPoint(corner.Origin + corner.Direction * 10)
            print("round trip", string.format("%.1f %.1f %.1f %.1f %.1f", back.X, back.Y, back.Z, along.X, along.Y), corner.Direction.X < 0, corner.Direction.Y > 0, string.format("%.3f", corner.Direction.Magnitude))
            local mouse = game.Players.LocalPlayer:GetMouse()
            print("mouse", mouse.ClassName, mouse.X, mouse.Y, mouse.Target, math.floor((mouse.Hit.Position - mouse.Origin.Position).Magnitude + 0.5), mouse.ViewSizeX, mouse.ViewSizeY, typeof(mouse.UnitRay))
            mouse.Move:Connect(function() print("move", mouse.X, mouse.Y, mouse.Target and mouse.Target.Name, mouse.Hit.Position, mouse.Origin.Position, mouse.ViewSizeX, mouse.ViewSizeY) end)
            mouse.Button1Down:Connect(function() print("b1down", mouse.Target.Name) end)
            mouse.Button1Up:Connect(function() print("b1up") end)
            mouse.WheelForward:Connect(function() print("wheel") end)
            mouse.TargetFilter = wall
            print("bad", select(2, pcall(function() return workspace:Raycast(1, 2) end)):match("vector expected, got number") ~= nil, select(2, pcall(function() params.FilterType = 3 end)):match("RaycastFilterType") ~= nil)
        )");
        rt.step(0.016);
        check(log.has("ray\tWall\t0, 0, 1\t0, 0, 1\t19\tWood"), "workspace:Raycast: the nearest CanQuery part, hit point, face normal, distance, Material");
        check(log.has("params\t1\tExclude\ttrue\tfalse\tRaycastParams"), "RaycastParams defaults");
        check(log.has("exclude\tBall\t-8\t1\t28"), "FilterType Exclude skips the wall; a Ball is a sphere");
        check(log.has("include\tWall\t19\tInclude") && log.has("model\tLeaf\t19\t2"), "FilterType Include sees only the list; AddToFilter with a Model covers its descendants");
        check(log.has("miss\tnil\tnil\ttrue"), "too short, off to the side, or starting inside: nil");
        check(log.has("tilted\tTilted\t13.93 0.71 0.00 0.71"), "a rotated box is hit on its rotated face");
        check(log.has("short\tnil\ttrue") && log.has("side\t0, 1, 0\t-1, 0, 0"), "direction length is the reach; normals face the ray");
        check(log.has("Ray\t0, 0, 20\t0, 0, -1\t0, 0, 10\t3\tRay\t{0, 0, 20}, {0, 0, -50}"), "Ray.new, Unit, ClosestPoint, Distance, typeof, tostring");
        check(log.has("legacy\tWall\t0, 0, 1\t0, 0, 1\tWood\tBall\tnil"), "FindPartOnRay / FindPartOnRayWithIgnoreList");
        check(log.has("viewport\t1024, 768\t70") && log.has("center ray\t0, 0, 20\t0, 0, -1\tRay"), "Camera.ViewportSize; the center pixel's ray is the LookVector");
        check(log.has("world to viewport\t512, 384, 20\ttrue\tfalse\t512, 384, 10\ttrue"), "WorldToViewportPoint: pixel + depth, inView false behind the camera");
        check(log.has("round trip\t100.0 100.0 5.0 100.0 100.0\ttrue\ttrue\t1.000"), "ScreenPointToRay(x, y, depth) inverts WorldToViewportPoint: depth is along the look axis; unit direction");
        check(log.has("mouse\tMouse\t0\t0\tnil\t1000\t1024\t768\tRay"), "Player:GetMouse(): at the corner before any input, Hit 1000 studs out where nothing is, ViewSize from the camera");
        Runtime::UserInput in;
        in.type = "MouseMovement"; in.state = "Change"; in.position = {512, 384, 0}; in.delta = {1, 0, 0};
        rt.input(in);
        check(log.has("move\t512\t384\tBall\t0, 0, -8\t0, 0, 20\t1024\t768"), "Move fires with X / Y; Hit / Target skip the TargetFilter");
        in.type = "MouseButton1"; in.state = "Begin"; rt.input(in);
        in.state = "End"; rt.input(in);
        in.type = "MouseWheel"; in.state = "Change"; in.position.z = 1; rt.input(in);
        check(log.has("b1down\tBall") && log.has("b1up") && log.has("wheel"), "Button1Down / Button1Up / WheelForward");
        check(log.has("bad\ttrue\ttrue"), "wrong types error");

        // ClickDetector: a click on the part reaches the server as MouseClick(player).
        Log slog, alog;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        DataModel& sdm = server.dataModel();
        server.runChunk("world", R"(
            local button = Instance.new("Part"); button.Name = "Button"; button.Size = Vector3.new(4, 4, 4); button.Anchored = true; button.Parent = workspace
            local cd = Instance.new("ClickDetector"); cd.MaxActivationDistance = 1000; cd.Parent = button
            local far = Instance.new("Part"); far.Name = "Far"; far.Size = Vector3.new(4, 4, 4); far.Position = Vector3.new(8, 0, 0); far.Anchored = true; far.Parent = workspace
            local cd2 = Instance.new("ClickDetector"); cd2.MaxActivationDistance = 1; cd2.Parent = far
            print("cd", cd.MaxActivationDistance, cd2.CursorIcon == "")
            cd.MouseClick:Connect(function(player) print("server click", player.Name, player.ClassName) end)
            cd.RightMouseClick:Connect(function(player) print("server rclick", player.Name) end)
            cd.MouseHoverEnter:Connect(function(player) print("server hover", player.Name) end)
            cd.MouseHoverLeave:Connect(function(player) print("server leave", player.Name) end)
            cd2.MouseClick:Connect(function(player) print("server far click", player.Name) end)
        )");
        addScript(server, "LocalScript", "Clicker", sdm.getService("StarterPlayer")->findFirstChildOfClass("StarterPlayerScripts"), R"(
            workspace.CurrentCamera.CFrame = CFrame.new(0, 0, 20)
            local cd = workspace:WaitForChild("Button"):WaitForChild("ClickDetector")
            cd.MouseClick:Connect(function(player) print("client click", player.Name, player == game.Players.LocalPlayer) end)
            cd.MouseHoverEnter:Connect(function(player) print("client hover", player.Name) end)
            workspace.Far.ClickDetector.MouseClick:Connect(function() print("client far click") end)
        )");
        server.startScripts();
        server.step(0.05);
        check(slog.has("cd\t1000\ttrue"), "ClickDetector defaults");
        makeCamera(alice);
        LocalSession session(server);
        session.join(alice, "Alice", 11);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        in.type = "MouseMovement"; in.state = "Change"; in.position = {512, 384, 0}; alice.input(in);
        check(alog.has("client hover\tAlice"), "MouseHoverEnter on the client when the pointer reaches the part");
        in.type = "MouseButton1"; in.state = "Begin"; alice.input(in);
        in.state = "End"; alice.input(in);
        in.type = "MouseButton2"; in.state = "Begin"; alice.input(in);
        check(alog.has("client click\tAlice\ttrue"), "MouseClick(player) fires on the clicking client");
        session.stepAll(0.05);
        check(slog.has("server hover\tAlice") && slog.has("server click\tAlice\tPlayer") && slog.has("server rclick\tAlice"), "and on the server with the Player, plus hover and RightMouseClick");
        in.type = "MouseMovement"; in.state = "Change"; in.position = {731, 384, 0}; alice.input(in);
        in.type = "MouseButton1"; in.state = "Begin"; alice.input(in);
        in.processed = true; in.position = {512, 384, 0}; alice.input(in);
        session.stepAll(0.05);
        check(slog.has("server leave\tAlice") && !slog.has("server far click") && !alog.has("client far click"), "MouseHoverLeave; a detector beyond MaxActivationDistance ignores the click");
        check(slog.count("server click\tAlice\tPlayer") == 1, "a click the GUI took first does not reach a ClickDetector");
        check(slog.errors.empty() && alog.errors.empty() && log.errors.empty(), "no errors");
    }

    section("R21. tools: StarterPack -> Backpack, the hotbar, Equipped / Activated on both sides, Humanoid:EquipTool, drops");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        DataModel& sdm = server.dataModel();
        server.runChunk("world", R"(
            local sp = game:GetService("StarterPack")
            local function tool(name)
                local t = Instance.new("Tool"); t.Name = name
                local handle = Instance.new("Part"); handle.Name = "Handle"; handle.Size = Vector3.new(1, 1, 4); handle.Parent = t
                local tip = Instance.new("Part"); tip.Name = "Tip"; tip.Size = Vector3.new(0.5, 0.5, 0.5); tip.Position = Vector3.new(0, 0, -2); tip.Parent = t
                t.Parent = sp
                return t
            end
            local wand = tool("Wand")
            tool("Sword")
            print("tool", wand.ClassName, wand:IsA("BackpackItem"), wand:IsA("Model"), wand.RequiresHandle, wand.CanBeDropped, wand.Enabled, wand.ToolTip == "", wand.ManualActivationOnly)
            game.Players.PlayerAdded:Connect(function(player)
                local gem = Instance.new("Tool"); gem.Name = "Gem"
                local h = Instance.new("Part"); h.Name = "Handle"; h.Parent = gem
                gem.Parent = player.StarterGear
                player.CharacterAdded:Connect(function(char)
                    local names = {}
                    for _, c in player.Backpack:GetChildren() do if c:IsA("Tool") then table.insert(names, c.Name) end end
                    print("backpack", table.concat(names, ","), char:FindFirstChildOfClass("Tool"))
                end)
            end)
        )");
        addScript(server, "Script", "Server", sdm.getService("StarterPack")->findFirstChild("Wand"), R"(
            local tool = script.Parent
            print("tool script", tool.Parent.ClassName)
            tool.Equipped:Connect(function(mouse) print("server equipped", tool.Parent.Name, mouse) end)
            tool.Unequipped:Connect(function() print("server unequipped", tool.Parent and tool.Parent.ClassName) end)
            tool.Activated:Connect(function() print("server activated", tool.Parent.Name) end)
            tool.Deactivated:Connect(function() print("server deactivated") end)
        )");
        addScript(server, "LocalScript", "Client", sdm.getService("StarterPack")->findFirstChild("Wand"), R"(
            local tool = script.Parent
            print("tool local", tool.Parent.ClassName, tool.Parent.Parent == game.Players.LocalPlayer)
            tool.Equipped:Connect(function(mouse) print("client equipped", mouse.ClassName, tool.Parent == game.Players.LocalPlayer.Character) end)
            tool.Unequipped:Connect(function() print("client unequipped") end)
            tool.Activated:Connect(function() print("client activated") end)
            tool.Deactivated:Connect(function() print("client deactivated") end)
        )");
        server.startScripts();
        server.step(0.05);
        check(slog.has("tool\tTool\ttrue\ttrue\ttrue\ttrue\ttrue\ttrue\tfalse"), "Tool: a BackpackItem Model with RequiresHandle / CanBeDropped / Enabled / ToolTip / ManualActivationOnly");
        check(!slog.has("tool script\t"), "a Script in StarterPack does not run there");
        LocalSession session(server);
        session.join(alice, "Alice", 11);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        check(slog.has("backpack\tWand,Sword,Gem\tnil"), "the server fills the Backpack from StarterPack and the player's StarterGear on spawn");
        check(slog.has("tool script\tBackpack"), "a Script in a Backpack tool runs on the server");
        check(alog.has("tool local\tBackpack\ttrue"), "the Backpack replicates down; a LocalScript in the tool runs on the client");
        auto key = [&](const char* k) { Runtime::UserInput in; in.type = "Keyboard"; in.key = k; in.state = "Begin"; alice.input(in); in.state = "End"; alice.input(in); };
        auto click = [&](const char* state) { Runtime::UserInput in; in.type = "MouseButton1"; in.state = state; in.position = {512, 384, 0}; alice.input(in); };
        key("One");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(slog.has("server equipped\tAlice\tnil"), "the 1 key asks the server, which moves the tool into the character: Equipped on the server");
        check(alog.has("client equipped\tMouse\ttrue"), "the move replicates; Equipped(mouse) on the client");
        alice.runChunk("held", R"(print("held", game.Players.LocalPlayer.Character:FindFirstChildOfClass("Tool").Name, #game.Players.LocalPlayer.Backpack:GetChildren()))");
        check(alog.has("held\tWand\t2"), "the client sees the Wand in its character, the others still in the Backpack");
        click("Begin");
        check(alog.has("client activated"), "a click with the tool in hand: Activated on the client at once");
        click("End");
        session.stepAll(0.05);
        check(slog.has("server activated\tAlice") && slog.has("server deactivated") && alog.has("client deactivated"), "and Activated / Deactivated on the server");
        key("One");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(slog.has("server unequipped\tBackpack") && alog.has("client unequipped"), "the held tool's key unequips it: back to the Backpack, Unequipped on both sides");
        key("Two");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        server.runChunk("who", R"(local c = game.Players.Alice.Character; print("holding", c:FindFirstChildOfClass("Tool").Name, #game.Players.Alice.Backpack:GetChildren()))");
        check(slog.has("holding\tSword\t2"), "2 equips the second tool: hotbar slots stay put after an unequip");
        key("One");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        server.runChunk("who2", R"(local c = game.Players.Alice.Character; print("holding2", c:FindFirstChildOfClass("Tool").Name, game.Players.Alice.Backpack:FindFirstChild("Sword") ~= nil, game.Players.Alice.Backpack:FindFirstChild("Wand")))");
        check(slog.has("holding2\tWand\ttrue\tnil"), "equipping another puts the held one back");
        server.runChunk("api", R"(
            local player = game.Players.Alice
            local hum = player.Character.Humanoid
            hum:UnequipTools()
            print("api1", player.Character:FindFirstChildOfClass("Tool"), player.Backpack.Wand.Parent.ClassName)
            hum:EquipTool(player.Backpack.Wand)
            print("api2", player.Character.Wand.Parent.Name)
            player.Backpack.Sword.Parent = player.Character
            print("api3", player.Character:FindFirstChildOfClass("Tool").Name, player.Backpack.Wand.ClassName)
            player.Character.Sword:Activate()
            player.Backpack.Wand:Activate()
            player.Character.Sword.Parent = player.Backpack
            print("api4", #player.Backpack:GetChildren())
        )");
        check(slog.has("api1\tnil\tBackpack") && slog.has("api2\tAlice"), "Humanoid:UnequipTools() / EquipTool() on the server");
        check(slog.has("api3\tSword\tTool"), "parenting a tool into the character equips it; the other is put back");
        check(slog.count("server unequipped\tBackpack") == 3, "Unequipped each time it left the character");
        check(slog.count("server activated\tAlice") == 1, "Tool:Activate() fires Activated only in hand");
        server.runChunk("respawn", R"(game.Players.Alice:LoadCharacter())");
        server.step(0.05);
        check(slog.count("backpack\tWand,Sword,Gem\tnil") == 2, "a respawn starts the Backpack over");
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        key("One");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        key("Backspace");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        server.runChunk("dropped", R"(
            local wand = workspace:FindFirstChild("Wand")
            print("dropped", wand and wand.Parent.Name, wand and wand.Handle.Position, wand and wand.Tip.Position, wand and wand.Handle.Anchored, game.Players.Alice.Character:FindFirstChildOfClass("Tool"))
        )");
        check(slog.has("dropped\tWorkspace\t0, 4, -3\t0, 4, -5\tfalse\tnil"), "Backspace drops the tool in front of the character, unanchored");
        alice.runChunk("dropped2", R"(print("dropped2", workspace:FindFirstChild("Wand") ~= nil, game.Players.LocalPlayer.Character:FindFirstChildOfClass("Tool")))");
        check(alog.has("dropped2\ttrue\tnil"), "the client sees the drop");
        server.runChunk("grip", R"(
            local wand = workspace.Wand
            print("grip0", wand.Grip == CFrame.new(), wand.GripPos, wand.GripForward, wand.GripRight, wand.GripUp)
            wand.Grip = CFrame.new(0, 0, -1) * CFrame.Angles(math.rad(90), 0, 0)
            local function r(v) return Vector3.new(math.round(v.X) + 0, math.round(v.Y) + 0, math.round(v.Z) + 0) end
            print("grip1", r(wand.GripPos), r(wand.GripForward), r(wand.GripRight), r(wand.GripUp))
            local g = wand.Grip
            print("grip2", (g.Position - Vector3.new(0, 0, -1)).Magnitude < 1e-5, (g.LookVector - Vector3.new(0, 1, 0)).Magnitude < 1e-5, (g.UpVector - Vector3.new(0, 0, 1)).Magnitude < 1e-5)
            wand.GripUp = Vector3.new(0, 1, 0); wand.GripForward = Vector3.new(0, 0, -1)
            print("grip3", wand.Grip == CFrame.new(0, 0, -1), pcall(function() wand.Grip = Vector3.new() end))
        )");
        check(slog.has("grip0\ttrue\t0, 0, 0\t0, 0, -1\t1, 0, 0\t0, 1, 0"), "Tool.Grip is CFrame.new(): GripPos, GripForward, GripRight, GripUp");
        check(slog.has("grip1\t0, 0, -1\t0, 1, 0\t1, 0, 0\t0, 0, 1") && slog.has("grip2\ttrue\ttrue\ttrue"), "setting Grip sets the four; reading it puts them back together");
        check(slog.has("grip3\ttrue\tfalse\tgrip:10: invalid argument #3 to 'Grip' (CFrame expected, got Vector3)"), "and the four make the Grip");
        session.stepAll(0.05);
        alice.runChunk("grip", R"(local w = workspace.Wand; print("gripc", w.Grip == CFrame.new(0, 0, -1), w.GripPos))");
        check(alog.has("gripc\ttrue\t0, 0, -1"), "the Grip replicates");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R22. Rojo JSON: *.model.json subtrees, init.meta.json / *.meta.json classes and properties");
    {
        Log log; Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        SourceFileInfo fi;
        check(classifySourceFile("Workspace/Map/Lava.model.json", fi) && fi.kind == SourceFileInfo::Model && fi.className.empty() && fi.name == "Lava" && fi.containers.size() == 2 && !fi.initMeta, "x.model.json -> a model file named by the file");
        check(classifySourceFile("StarterPack/Wand/init.meta.json", fi) && fi.kind == SourceFileInfo::Meta && fi.initMeta && fi.name == "Wand" && fi.containers.size() == 1 && fi.path() == "StarterPack/Wand", "init.meta.json describes its directory");
        check(classifySourceFile("ServerScriptService/Main.meta.json", fi) && fi.kind == SourceFileInfo::Meta && !fi.initMeta && fi.name == "Main", "x.meta.json describes its sibling x");
        check(!classifySourceFile("Workspace/init.model.json", fi) && !classifySourceFile("Workspace/.meta.json", fi) && !classifySourceFile("Workspace/x.json", fi), "a model can't be init; other .json files are not sources");
        std::string err;
        Instance* lava = loadSourceFile(rt, "Workspace/Map/Lava.model.json", R"JSON({
            "className": "Part",
            "properties": {
                "Size": [20, 1, 20],
                "Position": {"Vector3": [0, -5, 0]},
                "Color": [255, 80, 0],
                "Material": "Neon",
                "Shape": {"Enum": 0},
                "Anchored": true,
                "Transparency": {"Float32": 0.25},
                "CFrame": {"position": [4, 5, 6], "orientation": [[0, 0, 1], [0, 1, 0], [-1, 0, 0]]}
            },
            "attributes": {"Damage": 25, "Kind": "lava", "Hot": true, "Tint": [1, 0, 0]},
            "children": [
                {"name": "Glow", "className": "Part", "properties": {"Color": {"Color3": [0.5, 0.25, 0]}, "CanCollide": false}},
                {"name": "Burn", "className": "Script", "properties": {"Source": "print('burn', script.Parent.Name, script.Parent.Parent.Name, script.Parent:GetAttribute('Damage'))"}}
            ]
        })JSON", &err);
        check(lava && lava->className() == "Part" && lava->fullName() == "Workspace.Map.Lava", ("a model.json builds the instance under its containers: " + err).c_str());
        rt.startScripts();
        rt.step(0.05);
        rt.runChunk("lava", R"(
            local lava = workspace.Map.Lava
            print("lava", lava.Size, lava.Position, lava.Orientation, lava.Color, lava.Material, lava.Shape, lava.Anchored, lava.Transparency)
            print("attrs", lava:GetAttribute("Damage"), lava:GetAttribute("Kind"), lava:GetAttribute("Hot"), lava:GetAttribute("Tint"))
            print("kids", lava.Glow.Color, lava.Glow.CanCollide, lava.Burn.ClassName)
        )");
        check(log.has("lava\t20, 1, 20\t4, 5, 6\t0, 90, 0\t1, 0.313726, 0\tEnum.Material.Neon\tEnum.PartType.Ball\ttrue\t0.25"), "properties: implicit Vector3 / 0-255 Color3 / enum by name, explicit {Vector3} / {Enum} / {Float32}, CFrame -> Position + Orientation");
        check(log.has("attrs\t25\tlava\ttrue\t1, 0, 0"), "attributes: number, string, bool, [x, y, z]");
        check(log.has("kids\t0.5, 0.25, 0\tfalse\tScript") && log.has("burn\tLava\tMap\t25"), "children by name, with a Script that runs");
        // a bad property is reported, the rest applied
        Instance* bad = loadSourceFile(rt, "Workspace/Bad.model.json", R"JSON({"className": "Part", "properties": {"Nope": 1, "Size": "big", "Anchored": true}})JSON", &err);
        check(bad && bad->get("Anchored").b && log.errors.size() == 2 && log.errors[0] == "Nope is not a property of Part" && log.errors[1] == "Size: not a Vector3", "unknown / mistyped properties are reported, the others applied");
        check(!loadSourceFile(rt, "Workspace/Broken.model.json", R"JSON({"className": "Part",)JSON", &err) && err.find(" at offset ") != std::string::npos, "malformed JSON is an error");
        check(!loadSourceFile(rt, "Workspace/Weird.model.json", R"JSON({"className": "Nope"})JSON", &err) && err == "Weird: Nope is not a valid class", "an unknown class is an error");
        log.errors.clear();
        // init.meta.json: the directory is a Tool, its scripts are the tool's
        Instance* swing = loadSourceFile(rt, "StarterPack/Wand/Swing.server.luau", "print('swing', script.Parent.ClassName, script.Parent.ToolTip)", &err);
        check(swing && swing->parent()->className() == "Folder", "a directory is a Folder first");
        Instance* wand = loadSourceFile(rt, "StarterPack/Wand/init.meta.json", R"JSON({"className": "Tool", "properties": {"ToolTip": "Wave it", "CanBeDropped": false}})JSON", &err);
        check(wand && wand->className() == "Tool" && wand->name() == "Wand" && wand->parent() == dm.getService("StarterPack") && swing->parent() == wand && !swing->destroyed(), "init.meta.json turns the Folder into a Tool, children kept");
        check(wand->get("ToolTip").s == "Wave it" && !wand->get("CanBeDropped").b, "with its properties");
        Instance* handle = loadSourceFile(rt, "StarterPack/Wand/Handle.model.json", R"JSON({"className": "Part", "properties": {"Size": [0.4, 0.4, 3]}})JSON", &err);
        check(handle && handle->parent() == wand, "a model.json under it is the tool's part");
        Instance* wand2 = loadSourceFile(rt, "StarterPack/Wand/init.meta.json", R"JSON({"className": "Tool", "properties": {"ToolTip": "Waved"}})JSON", &err);
        check(wand2 == wand && wand->get("ToolTip").s == "Waved" && handle->parent() == wand, "saving it again just updates the properties");
        // x.meta.json next to x.server.luau, in either order
        Instance* main = loadSourceFile(rt, "ServerScriptService/Main.server.luau", "print('main ran')", &err);
        check(main && loadSourceFile(rt, "ServerScriptService/Main.meta.json", R"JSON({"properties": {"Disabled": true}})JSON", &err) == main && main->get("Disabled").b, "Main.meta.json sets Disabled on the Script");
        Instance* meta = loadSourceFile(rt, "ServerScriptService/Early.meta.json", R"JSON({"properties": {"Disabled": true}})JSON", &err);
        check(meta == dm.getService("ServerScriptService") && !dm.getService("ServerScriptService")->findFirstChild("Early"), "one for a file not yet loaded waits");
        Instance* early = loadSourceFile(rt, "ServerScriptService/Early.server.luau", "print('early ran')", &err);
        check(early && early->get("Disabled").b && rt.impl().pendingMeta.empty(), "and applies when it arrives");
        rt.step(0.05);
        check(!log.has("early ran"), "so the Script does not run");
        check(unloadSourceFile(rt, "ServerScriptService/Early.meta.json") && early->get("Disabled").b, "unloading a meta file changes nothing already applied");
        Instance* folderMeta = loadSourceFile(rt, "ReplicatedStorage/Config/init.meta.json", R"JSON({"className": "Configuration", "attributes": {"MaxPlayers": 8}})JSON", &err);
        check(folderMeta && folderMeta->className() == "Configuration" && folderMeta->fullName() == "ReplicatedStorage.Config" && folderMeta->attributes().at("MaxPlayers").n == 8, "init.meta.json alone makes the directory instance");
        check(unloadSourceFile(rt, "StarterPack/Wand/init.meta.json") && dm.getService("StarterPack")->findFirstChild("Wand")->className() == "Folder" && swing->parent() == dm.getService("StarterPack")->findFirstChild("Wand") && wand->destroyed(), "removing init.meta.json: a Folder again, children kept");
        check(unloadSourceFile(rt, "Workspace/Map/Lava.model.json") && lava->destroyed() && !unloadSourceFile(rt, "Workspace/Map/Lava.model.json"), "removing a model.json destroys the subtree");
        // A project's "$properties" on a service arrive as its init.meta.json (Lighting/init.meta.json)
        Instance* lighting = loadSourceFile(rt, "Lighting/init.meta.json", R"JSON({"className": "Lighting", "properties": {"ClockTime": 18, "Ambient": [0.5, 0.5, 0.5]}, "attributes": {"Season": "winter"}})JSON", &err);
        check(lighting == dm.getService("Lighting") && lighting->get("ClockTime").n == 18 && lighting->get("Ambient").c.r == 0.5f && lighting->attributes().at("Season").s == "winter", "a top-level init.meta.json describes the service");
        check(!loadSourceFile(rt, "Lighting/init.meta.json", R"JSON({"className": "Folder"})JSON", &err) && err == "Lighting is a Lighting, not a Folder", "which keeps its class");
        check(!loadSourceFile(rt, "Nope/init.meta.json", R"JSON({"properties": {}})JSON", &err) && err == "'Nope' is not a valid Service name", "and must be a service");
        check(unloadSourceFile(rt, "Lighting/init.meta.json") && dm.getService("Lighting") == lighting && lighting->get("ClockTime").n == 18, "unloading it leaves the service as it is");
        check(log.errors.empty(), "no errors");
    }

    section("R23. lights: PointLight / SpotLight / SurfaceLight in a part, replicated");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        server.runChunk("lights", R"(
            local lamp = Instance.new("Part"); lamp.Name = "Lamp"; lamp.Parent = workspace
            local pl = Instance.new("PointLight"); pl.Parent = lamp
            print("point", pl.ClassName, pl:IsA("Light"), pl.Brightness, pl.Range, pl.Color, pl.Enabled, pl.Shadows)
            local sl = Instance.new("SpotLight"); sl.Face = Enum.NormalId.Bottom; sl.Angle = 60; sl.Parent = lamp
            print("spot", sl.Range, sl.Angle, sl.Face, Instance.new("SurfaceLight").Face)
            local ok, err = pcall(function() return Instance.new("Light") end)
            print("abstract", ok, err)
            local ok2, err2 = pcall(function() pl.Range = "far" end)
            print("typed", ok2, err2)
        )");
        check(slog.has("point	PointLight	true	1	8	1, 1, 1	true	false"), "PointLight: Brightness 1, Range 8, white, Enabled, no Shadows");
        check(slog.has("spot	16	60	Enum.NormalId.Bottom	Enum.NormalId.Front"), "SpotLight / SurfaceLight: Range 16, Angle, Face out of the part");
        check(slog.has("abstract	false	lights:7: Unable to create an Instance of type \"Light\""), "Light itself is abstract");
        check(slog.has("typed	false	lights:9: invalid argument #3 to 'Range' (number expected, got string)"), "properties are typed");
        LocalSession session(server);
        session.join(alice, "Alice", 12);
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("see", R"(local l = workspace.Lamp.SpotLight; print("client", l.Face, l.Angle, workspace.Lamp.PointLight.Range))");
        check(alog.has("client	Enum.NormalId.Bottom	60	8"), "lights replicate with the part");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R24. ProximityPrompt: shown near the character, its key triggers on both sides");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        server.runChunk("prompts", R"(
            local function part(name, pos, size)
                local p = Instance.new("Part"); p.Name = name; p.Anchored = true; p.Position = pos; p.Size = size or Vector3.new(2, 2, 2); p.Parent = workspace
                return p
            end
            local chest = part("Chest", Vector3.new(4, 2, 0))
            local open = Instance.new("ProximityPrompt"); open.Name = "Open"; open.ActionText = "Open"; open.ObjectText = "Chest"; open.Parent = chest
            print("defaults", open.KeyboardKeyCode, open.MaxActivationDistance, open.HoldDuration, open.Enabled, open.RequiresLineOfSight, open.Exclusivity, open.Style, open.Shown)
            local ok, err = pcall(function() open.Shown = true end)
            print("readonly", ok, err)
            open.Triggered:Connect(function(player) print("server triggered", player.Name) end)
            open.TriggerEnded:Connect(function(player) print("server trigger ended", player.Name) end)
            local pps = game:GetService("ProximityPromptService")
            print("service", pps.ClassName, pps.Enabled, pps.MaxPromptsVisible)
            pps.PromptTriggered:Connect(function(prompt, player) print("service triggered", prompt.Name, player.Name) end)
            -- one too far, one behind a wall
            local far = part("Far", Vector3.new(40, 2, 0))
            local farPrompt = Instance.new("ProximityPrompt"); farPrompt.Name = "FarPrompt"; farPrompt.Parent = far
            part("Wall", Vector3.new(-3, 4, 0), Vector3.new(1, 10, 10))
            local hidden = part("Hidden", Vector3.new(-6, 2, 0))
            local behind = Instance.new("ProximityPrompt"); behind.Name = "Behind"; behind.Parent = hidden
            -- a Model with a PrimaryPart: a hold on F
            local lever = Instance.new("Model"); lever.Name = "Lever"; lever.Parent = workspace
            local base = part("Base", Vector3.new(0, 2, 5)); base.Parent = lever
            lever.PrimaryPart = base
            local pull = Instance.new("ProximityPrompt"); pull.Name = "Pull"; pull.KeyboardKeyCode = Enum.KeyCode.F; pull.HoldDuration = 0.5; pull.Parent = lever
            pull.Triggered:Connect(function(player) print("lever pulled", player.Name) end)
        )");
        check(slog.has("defaults	Enum.KeyCode.E	10	0	true	true	Enum.ProximityPromptExclusivity.OnePerButton	Enum.ProximityPromptStyle.Default	false"), "ProximityPrompt defaults: E, 10 studs, no hold, line of sight, one per button");
        check(slog.has("readonly	false	prompts:9: Unable to assign property Shown. Property is read only"), "Shown is the engine's to set");
        check(slog.has("service	ProximityPromptService	true	1"), "ProximityPromptService: Enabled, MaxPromptsVisible 1");
        alice.runChunk("hooks", R"(
            local pps = game:GetService("ProximityPromptService")
            pps.MaxPromptsVisible = 3
            pps.PromptShown:Connect(function(prompt, inputType) print("shown", prompt.Name, inputType) end)
            pps.PromptHidden:Connect(function(prompt) print("hidden", prompt.Name) end)
            pps.PromptTriggered:Connect(function(prompt, player) print("client triggered", prompt.Name, player.Name) end)
            pps.PromptTriggerEnded:Connect(function(prompt, player) print("client trigger ended", prompt.Name, player.Name) end)
            pps.PromptButtonHoldBegan:Connect(function(prompt) print("hold began", prompt.Name) end)
            pps.PromptButtonHoldEnded:Connect(function(prompt) print("hold ended", prompt.Name) end)
        )");
        LocalSession session(server);
        session.join(alice, "Alice", 13);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        alice.runChunk("own", R"(
            local open = workspace.Chest.Open
            open.PromptShown:Connect(function(inputType) print("own shown", inputType) end)
            open.Triggered:Connect(function(player) print("own triggered", player.Name) end)
            print("shown prop", open.Shown, workspace.Lever.Pull.Shown, workspace.Hidden.Behind.Shown, workspace.Far.FarPrompt.Shown)
        )");
        check(alog.has("shown	Open	Enum.ProximityPromptInputType.Keyboard") && alog.has("shown	Pull	Enum.ProximityPromptInputType.Keyboard"), "the prompts within MaxActivationDistance of the character are shown: PromptShown on the service");
        check(!alog.has("shown	FarPrompt") && !alog.has("shown	Behind"), "not the one out of range, nor the one behind a wall (RequiresLineOfSight)");
        check(alog.has("shown prop	true	true	false	false"), "the hidden Shown property says which (the engine draws those)");
        auto key = [&](const char* k, const char* st) { Runtime::UserInput in; in.type = "Keyboard"; in.key = k; in.state = st; alice.input(in); };
        key("E", "Begin");
        check(alog.has("client triggered	Open	Alice") && alog.has("own triggered	Alice"), "E: Triggered(player) on the client at once, on the prompt and the service");
        session.stepAll(0.05);
        check(slog.has("server triggered	Alice") && slog.has("service triggered	Open	Alice"), "and on the server a frame later, after its own distance check");
        key("E", "End");
        session.stepAll(0.05);
        check(alog.has("client trigger ended	Open	Alice") && slog.has("server trigger ended	Alice"), "releasing it: TriggerEnded on both sides");
        // a hold: F for 0.5s
        key("F", "Begin");
        check(alog.has("hold began	Pull"), "a prompt with a HoldDuration: PromptButtonHoldBegan on the key");
        for (int i = 0; i < 4; i++) session.stepAll(0.05);
        key("F", "End");
        check(alog.has("hold ended	Pull") && !alog.has("client triggered	Pull"), "let go early: PromptButtonHoldEnded, no trigger");
        key("F", "Begin");
        for (int i = 0; i < 12; i++) session.stepAll(0.05);
        check(alog.has("client triggered	Pull	Alice") && alog.count("hold ended	Pull") == 2, "held for the duration: it triggers");
        check(slog.has("lever pulled	Alice"), "on the server too (a prompt in a Model uses its PrimaryPart)");
        key("F", "End");
        // exclusivity and the visible cap
        alice.runChunk("cap", R"(game:GetService("ProximityPromptService").MaxPromptsVisible = 1)");
        session.stepAll(0.05);
        check(alog.has("hidden	Pull") && !alog.has("hidden	Open"), "MaxPromptsVisible 1: only the nearest stays");
        alice.runChunk("always", R"(workspace.Lever.Pull.Exclusivity = Enum.ProximityPromptExclusivity.AlwaysShow)");
        session.stepAll(0.05);
        check(alog.count("shown	Pull	Enum.ProximityPromptInputType.Keyboard") == 2, "an AlwaysShow prompt is shown regardless");
        alice.runChunk("global", R"(
            workspace.Lever.Pull.Exclusivity = Enum.ProximityPromptExclusivity.OnePerButton
            workspace.Chest.Open.Exclusivity = Enum.ProximityPromptExclusivity.OneGlobally
            game:GetService("ProximityPromptService").MaxPromptsVisible = 3
        )");
        session.stepAll(0.05);
        check(alog.count("hidden	Pull") == 2 && alog.count("shown	Pull	Enum.ProximityPromptInputType.Keyboard") == 2, "a OneGlobally prompt nearest keeps the others hidden");
        // walking away hides; a key then does nothing
        server.runChunk("away", R"(game.Players.Alice.Character.HumanoidRootPart.Position = Vector3.new(0, 5, 30))");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(alog.has("hidden	Open"), "walking out of range: PromptHidden");
        key("E", "Begin"); key("E", "End");
        session.stepAll(0.05);
        check(alog.count("client triggered	Open	Alice") == 1 && slog.count("server triggered	Alice") == 1, "its key does nothing while hidden");
        server.runChunk("back", R"(game.Players.Alice.Character.HumanoidRootPart.Position = Vector3.new(0, 5, 0))");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(alog.count("shown	Open	Enum.ProximityPromptInputType.Keyboard") == 2, "back in range: shown again");
        server.runChunk("off", R"(workspace.Chest.Open.Enabled = false)");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(alog.count("hidden	Open") == 2, "Enabled = false hides it");
        server.runChunk("on", R"(workspace.Chest.Open.Enabled = true)");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("api", R"(
            local open = workspace.Chest.Open
            open:InputHoldBegin()
            open:InputHoldEnd()
            print("api", open.Shown)
        )");
        session.stepAll(0.05);
        check(alog.count("client triggered	Open	Alice") == 2 && alog.count("client trigger ended	Open	Alice") == 2 && slog.count("server triggered	Alice") == 2, "InputHoldBegin / InputHoldEnd press it from a LocalScript (a Custom-style prompt's own UI)");
        server.runChunk("gone", R"(workspace.Chest.Open:Destroy())");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        key("E", "Begin"); key("E", "End");
        session.stepAll(0.05);
        check(alog.count("client triggered	Open	Alice") == 2, "a destroyed prompt is gone from the client's set");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R25. Sound: Play / Stop / Pause / Resume, the clock behind Playing, Ended and DidLoop on both sides");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        server.runChunk("sounds", R"(
            local speaker = Instance.new("Part"); speaker.Name = "Speaker"; speaker.Anchored = true; speaker.Position = Vector3.new(3, 2, 0); speaker.Parent = workspace
            local s = Instance.new("Sound"); s.Name = "Thud"; s.SoundId = "sounds/thud.ogg"; s.Parent = speaker
            print("defaults", s.Volume, s.PlaybackSpeed, s.Looped, s.Playing, s.TimePosition, s.IsLoaded, s.TimeLength, s.IsPlaying, s.RollOffMaxDistance, s.RollOffMinDistance, s.RollOffMode, s.PlayOnRemove)
            local ok, err = pcall(function() s.TimeLength = 3 end)
            print("readonly", ok, err)
            for _, ev in ipairs({"Played", "Stopped", "Paused", "Resumed", "Ended", "Loaded"}) do
                s[ev]:Connect(function(id) print("server " .. ev:lower(), id) end)
            end
            s.DidLoop:Connect(function(id, n) print("server didloop", id, n) end)
            local ss = game:GetService("SoundService")
            print("service", ss.ClassName, ss.RespectFilteringEnabled)
            s:Play()
            print("playing", s.Playing, s.IsPlaying, s.TimePosition)
            local loose = Instance.new("Sound"); loose.SoundId = "sounds/loose.ogg"
            loose:Play()
            print("loose", loose.Playing)
            -- SoundGroups: a Sound belongs to one, and groups nest
            local music = Instance.new("SoundGroup"); music.Name = "Music"; music.Volume = 0.5; music.Parent = game:GetService("SoundService")
            local master = Instance.new("SoundGroup"); master.Name = "Master"; master.Volume = 0.8; master.Parent = game:GetService("SoundService")
            music.SoundGroup = master
            s.SoundGroup = music
            print("group", s.SoundGroup.Name, s.SoundGroup.Volume, music.SoundGroup.Name, s.SoundGroup:IsA("SoundGroup"))
            -- and a Sound can sit in an Attachment, where it plays from that spot
            local horn = Instance.new("Attachment"); horn.Name = "Horn"; horn.Position = Vector3.new(0, 2, 0); horn.Parent = speaker
            local beep = Instance.new("Sound"); beep.Name = "Beep"; beep.SoundId = "sounds/thud.ogg"; beep.Parent = horn
            print("attached", beep.Parent.Name, beep.Parent.Parent.Name)
        )");
        check(slog.has("defaults	0.5	1	false	false	0	false	0	false	10000	10	Enum.RollOffMode.Inverse	false"), "Sound defaults: Volume 0.5, speed 1, not looped, not playing, RollOff 10..10000 Inverse");
        check(slog.has("readonly	false	sounds:5: Unable to assign property TimeLength. Property is read only"), "TimeLength is the engine's");
        check(slog.has("service	SoundService	true"), "SoundService");
        check(slog.has("server played	sounds/thud.ogg") && slog.has("playing	true	false	0"), "Play(): Playing at once, Played(soundId); IsPlaying waits for the engine's load");
        check(slog.has("loose	false"), "a Sound outside the DataModel does not play");
        check(slog.has("group	Music	0.5	Master	true"), "a Sound belongs to a SoundGroup, and a group to another");
        check(slog.has("attached	Horn	Speaker"), "a Sound can sit in an Attachment on a part");
        Instance* thud = server.dataModel().workspace()->findFirstChild("Speaker")->findFirstChild("Thud");
        server.hostWrite(thud->id(), "TimeLength", Value::number(1));
        server.hostWrite(thud->id(), "IsLoaded", Value::boolean(true));
        check(slog.has("server loaded	sounds/thud.ogg") && server.eval("workspace.Speaker.Thud.IsPlaying") == "true", "the engine writes TimeLength / IsLoaded: Loaded(soundId), IsPlaying");
        for (int i = 0; i < 10; i++) server.step(0.05);
        server.runChunk("pos", R"(local s = workspace.Speaker.Thud; print("pos", math.abs(s.TimePosition - 0.5) < 0.01, s.Playing))");
        check(slog.has("pos	true	true"), "TimePosition ticks with the clock while Playing");
        for (int i = 0; i < 11; i++) server.step(0.05);
        check(slog.has("server ended	sounds/thud.ogg") && !slog.has("server stopped	sounds/thud.ogg") && server.eval("workspace.Speaker.Thud.Playing") == "false", "at TimeLength: Ended(soundId), Playing false");
        server.runChunk("ctl", R"(
            local s = workspace.Speaker.Thud
            s:Play()
            task.wait(0.3)
            s:Pause()
            local at = s.TimePosition
            print("paused at", at > 0.2 and at <= 0.3, s.Playing)
            task.wait(0.3)
            print("still", s.TimePosition == at)
            s:Resume()
            task.wait(0.2)
            print("resumed to", s.TimePosition > at + 0.1 and s.TimePosition <= at + 0.2, s.Playing)
            s:Stop()
            print("stopped", s.TimePosition, s.Playing)
            s.PlaybackSpeed = 2
            s:Play()
            task.wait(0.2)
            print("fast", s.TimePosition > 0.3 and s.TimePosition <= 0.55)
            s:Play()
            print("restart", s.TimePosition)
            s.Ended:Wait()
            s.PlaybackSpeed = 1
            s.Looped = true
            s:Play()
            task.wait(2.55)
            print("looped", s.Playing, math.abs(s.TimePosition - 0.55) < 0.05)
            s.Playing = false
            print("prop stop", s.Playing)
        )");
        for (int i = 0; i < 90; i++) server.step(0.05);
        check(slog.has("paused at	true	false") && slog.has("server paused	sounds/thud.ogg") && slog.has("still	true"), "Pause(): Paused(soundId), the clock stops where it is");
        check(slog.has("resumed to	true	true") && slog.has("server resumed	sounds/thud.ogg"), "Resume(): Resumed(soundId), on from there");
        check(slog.has("stopped	0	false") && slog.indexOf("server stopped	sounds/thud.ogg") < slog.indexOf("stopped	0	false"), "Stop(): Stopped(soundId), back to 0");
        check(slog.has("fast	true"), "PlaybackSpeed 2 runs the clock twice as fast");
        check(slog.has("restart	0") && slog.count("server played	sounds/thud.ogg") == 5, "Play() while playing restarts from 0 (Played again)");
        check(slog.count("server didloop	sounds/thud.ogg	1") == 1 && slog.count("server didloop	sounds/thud.ogg	2") == 1 && slog.has("looped	true	true"), "Looped: DidLoop(soundId, n) each time round, still Playing");
        check(slog.has("prop stop	false") && slog.count("server stopped	sounds/thud.ogg") == 2, "Playing = false is a Stop");
        // a client: the replicated sound plays there too, with its own clock
        alice.runChunk("hooks", R"(
            local s = workspace:WaitForChild("Speaker"):WaitForChild("Thud")
            s.Played:Connect(function(id) print("client played", id) end)
            s.Ended:Connect(function(id) print("client ended", id) end)
            s.Stopped:Connect(function(id) print("client stopped", id) end)
        )");
        LocalSession session(server);
        session.join(alice, "Alice", 13);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        server.runChunk("play", R"(workspace.Speaker.Thud.Looped = false; workspace.Speaker.Thud:Play())");
        session.stepAll(0.05);
        check(alog.has("client played	sounds/thud.ogg"), "a server Play() reaches the client: Played there");
        Instance* athud = alice.dataModel().workspace()->findFirstChild("Speaker")->findFirstChild("Thud");
        alice.hostWrite(athud->id(), "TimeLength", Value::number(1));
        alice.hostWrite(athud->id(), "IsLoaded", Value::boolean(true));
        for (int i = 0; i < 22; i++) session.stepAll(0.05);
        check(alog.has("client ended	sounds/thud.ogg") && slog.count("server ended	sounds/thud.ogg") == 3, "and it ends on both sides");
        alice.runChunk("local", R"(
            local s = workspace.Speaker.Thud
            s:Play()
            local ui = Instance.new("Sound"); ui.SoundId = "sounds/click.ogg"
            ui.Played:Connect(function(id) print("ui played", id) end)
            game:GetService("SoundService"):PlayLocalSound(ui)
            print("ui", ui.Playing, ui.Parent)
        )");
        session.stepAll(0.05);
        check(alog.count("client played	sounds/thud.ogg") == 2 && slog.count("server played	sounds/thud.ogg") == 6, "a LocalScript's Play() is heard on its client only");
        check(alog.has("ui played	sounds/click.ogg") && alog.has("ui	true	nil"), "SoundService:PlayLocalSound plays a Sound that is not even in the tree");
        server.runChunk("pls", R"(local s = Instance.new("Sound"); game:GetService("SoundService"):PlayLocalSound(s); print("server local", s.Playing))");
        check(slog.has("server local	false"), "a server has nobody to hear PlayLocalSound");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R26. the hotbar: Tool.HotbarSlot on the client, a slot click equips, StarterGui:SetCoreGuiEnabled");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        server.runChunk("world", R"(
            local sp = game:GetService("StarterPack")
            for _, name in ipairs({"Wand", "Sword", "Gem"}) do
                local t = Instance.new("Tool"); t.Name = name
                local handle = Instance.new("Part"); handle.Name = "Handle"; handle.Parent = t
                t.Parent = sp
            end
            print("server slot", sp.Wand.HotbarSlot)
            local ok, err = pcall(function() game:GetService("StarterGui"):SetCoreGuiEnabled(Enum.CoreGuiType.Backpack, false) end)
            print("server core", ok, err)
        )");
        check(slog.has("server slot	0"), "HotbarSlot is 0 on the server: the hotbar is each client's");
        check(slog.has("server core	false	world:9: SetCoreGuiEnabled can only be called from a LocalScript"), "SetCoreGuiEnabled is a LocalScript's");
        alice.runChunk("hooks", R"(
            local player = game.Players.LocalPlayer
            local sg = game:GetService("StarterGui")
            print("core", sg.ShowDevelopmentGui, sg:GetCoreGuiEnabled(Enum.CoreGuiType.Backpack), sg:GetCoreGuiEnabled(Enum.CoreGuiType.All), sg.CoreGuiHidden)
        )");
        check(alog.has("core	true	true	true	0"), "GetCoreGuiEnabled: everything on to begin with");
        const std::string slotsFn = R"(
            local player = game.Players.LocalPlayer
            local function slots()
                local out = {}
                for _, t in ipairs(player.Backpack:GetChildren()) do table.insert(out, t.Name .. "=" .. t.HotbarSlot) end
                local held = player.Character and player.Character:FindFirstChildOfClass("Tool")
                if held then table.insert(out, "[" .. held.Name .. "=" .. held.HotbarSlot .. "]") end
                table.sort(out)
                return table.concat(out, " ")
            end
        )";
        LocalSession session(server);
        session.join(alice, "Alice", 12);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        alice.runChunk("s1", slotsFn + R"(print("s1", slots())
            player.Backpack.ChildAdded:Connect(function(t) task.defer(function() print("added", t.Name, t.HotbarSlot) end) end))");
        check(alog.has("s1	Gem=3 Sword=2 Wand=1"), "the Backpack's tools take slots 1-3 as they arrive");
        alice.hotbarSelect(1);
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("s2", slotsFn + R"(print("s2", slots()))");
        check(alog.has("s2	Gem=3 Wand=1 [Sword=2]"), "a click on slot 2 equips the Sword; it keeps its slot in hand");
        alice.hotbarSelect(1);
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("s3", slotsFn + R"(print("s3", slots()))");
        check(alog.has("s3	Gem=3 Sword=2 Wand=1"), "a click on the held one's slot unequips it");
        alice.hotbarSelect(7);
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("s4", slotsFn + R"(print("s4", slots()))");
        check(alog.has("s4	Gem=3 Sword=2 Wand=1"), "an empty slot does nothing");
        server.runChunk("take", R"(game.Players.Alice.Backpack.Sword:Destroy())");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("s5", slotsFn + R"(print("s5", slots()))");
        check(alog.has("s5	Gem=2 Wand=1"), "a tool that is gone frees its slot; the rest close up");
        server.runChunk("give", R"(local t = Instance.new("Tool"); t.Name = "Rope"; local h = Instance.new("Part"); h.Name = "Handle"; h.Parent = t; t.Parent = game.Players.Alice.Backpack)");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("s6", slotsFn + R"(print("s6", slots()))");
        check(alog.has("added	Rope	3") && alog.has("s6	Gem=2 Rope=3 Wand=1"), "a new tool goes on the end, its slot set by the time ChildAdded handlers see it");
        alice.runChunk("hide", R"(
            local sg = game:GetService("StarterGui")
            sg:SetCoreGuiEnabled(Enum.CoreGuiType.Backpack, false)
            print("hidden", sg:GetCoreGuiEnabled(Enum.CoreGuiType.Backpack), sg:GetCoreGuiEnabled(Enum.CoreGuiType.Health), sg:GetCoreGuiEnabled(Enum.CoreGuiType.All), sg.CoreGuiHidden)
            sg:SetCoreGuiEnabled(Enum.CoreGuiType.All, false)
            print("all off", sg:GetCoreGuiEnabled(Enum.CoreGuiType.Health), sg.CoreGuiHidden)
            sg:SetCoreGuiEnabled(Enum.CoreGuiType.All, true)
            print("all on", sg:GetCoreGuiEnabled(Enum.CoreGuiType.Backpack), sg.CoreGuiHidden)
        )");
        check(alog.has("hidden	false	true	false	4"), "SetCoreGuiEnabled(Backpack, false): off, All reads false");
        check(alog.has("all off	false	255") && alog.has("all on	true	0"), "CoreGuiType.All turns everything off / on");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R27. BillboardGui: in a part or over an Adornee, replicated, the engine's AbsoluteSize; one in StarterGui is the player's");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        server.runChunk("world", R"(
            local sign = Instance.new("Part"); sign.Name = "Sign"; sign.Anchored = true; sign.Position = Vector3.new(0, 4, -8); sign.Parent = workspace
            local tag = Instance.new("BillboardGui"); tag.Name = "Tag"
            tag.Size = UDim2.new(4, 0, 1, 0); tag.StudsOffset = Vector3.new(0, 2.5, 0); tag.AlwaysOnTop = true
            local label = Instance.new("TextLabel"); label.Name = "Label"; label.Text = "Clicks: 0"; label.Size = UDim2.fromScale(1, 1)
            label.BackgroundTransparency = 1; label.TextColor3 = Color3.new(1, 1, 1); label.TextScaled = true; label.Parent = tag
            tag.Parent = sign
            print("tag", tag:IsA("LayerCollector"), tag:IsA("GuiBase2d"), tag:IsA("GuiObject"), tag.Adornee, tag.MaxDistance, tag.Enabled, tag.ResetOnSpawn, tag.LightInfluence, tag.DistanceUpperLimit, tostring(tag.Size), tostring(tag.SizeOffset), tostring(tag.ExtentsOffset))
            -- a name over a Model: the gui in StarterGui, its Adornee the part
            local npc = Instance.new("Model"); npc.Name = "Guard"
            local head = Instance.new("Part"); head.Name = "Head"; head.Position = Vector3.new(6, 5, -8); head.Parent = npc
            npc.Parent = workspace
            local nameTag = Instance.new("BillboardGui"); nameTag.Name = "GuardName"; nameTag.Adornee = head
            nameTag.Size = UDim2.new(0, 200, 0, 50); nameTag.ExtentsOffset = Vector3.new(0, 1.5, 0)
            local t = Instance.new("TextLabel"); t.Text = "Guard"; t.Size = UDim2.fromScale(1, 1); t.Parent = nameTag
            nameTag.Parent = game:GetService("StarterGui")
            -- ResetOnSpawn: the Hud starts over each life, the Persist gui does not
            local sg = game:GetService("StarterGui")
            for _, spec in {{"Hud", true}, {"Persist", false}} do
                local g = Instance.new("ScreenGui"); g.Name = spec[1]; g.ResetOnSpawn = spec[2]
                local ls = Instance.new("LocalScript"); ls.Name = "Run"
                ls.Source = 'local n = (script.Parent:GetAttribute("Runs") or 0) + 1; script.Parent:SetAttribute("Runs", n); print("gui ran", script.Parent.Name, n)'
                ls.Parent = g; g.Parent = sg
            end
            local bad = pcall(function() tag.Adornee = "Sign" end)
            print("bad adornee", bad)
        )");
        check(slog.has("tag	true	true	false	nil	inf	true	true	1	-1	{4, 0}, {1, 0}	0, 0	0, 0, 0"), "a BillboardGui is a LayerCollector, not a GuiObject; Roblox defaults: no Adornee, MaxDistance inf, Size 1x1 studs");
        check(slog.has("bad adornee	false"), "Adornee takes an Instance");
        alice.runChunk("hooks", R"(
            local tag = workspace:WaitForChild("Sign"):WaitForChild("Tag")
            print("client tag", tag.ClassName, tag.AlwaysOnTop, tostring(tag.StudsOffset), tag.Label.Text, tostring(tag.AbsoluteSize))
            tag:GetPropertyChangedSignal("AbsoluteSize"):Connect(function() print("abs", tostring(tag.AbsoluteSize), tostring(tag.AbsolutePosition)) end)
            tag.Label:GetPropertyChangedSignal("Text"):Connect(function() print("text", tag.Label.Text) end)
        )");
        LocalSession session(server);
        session.join(alice, "Alice", 14);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        check(alog.has("client tag	BillboardGui	true	0, 2.5, 0	Clicks: 0	0, 0"), "the gui and its label replicate with the part");
        Instance* atag = alice.dataModel().workspace()->findFirstChild("Sign")->findFirstChild("Tag");
        alice.hostWrite(atag->id(), "AbsolutePosition", Value::vector2(500, 120));
        alice.hostWrite(atag->id(), "AbsoluteSize", Value::vector2(160, 40));
        session.stepAll(0.05);
        check(alog.has("abs	160, 40	500, 120"), "the engine writes where it drew it: AbsolutePosition / AbsoluteSize and their changed signals");
        server.runChunk("click", R"(workspace.Sign.Tag.Label.Text = "Clicks: 1")");
        session.stepAll(0.05);
        check(alog.has("text	Clicks: 1"), "a server change to the label reaches the client");
        alice.runChunk("mine", R"(
            local pg = game.Players.LocalPlayer.PlayerGui
            local n = pg:WaitForChild("GuardName")
            print("mine", n.ClassName, n.Adornee and n.Adornee:GetFullName(), n.TextLabel.Text, tostring(n.ExtentsOffset), n.Parent == pg)
            local ok, err = pcall(function() n.AbsoluteSize = Vector2.new(1, 1) end)
            print("readonly", ok, err)
        )");
        session.stepAll(0.05);
        check(alog.has("mine	BillboardGui	Workspace.Guard.Head	Guard	0, 1.5, 0	true"), "StarterGui's BillboardGui copies into the PlayerGui with its Adornee still the part in the world");
        check(alog.has("readonly	false	mine:5: Unable to assign property AbsoluteSize. Property is read only"), "AbsoluteSize is the engine's");
        alice.runChunk("mark", R"(
            local pg = game.Players.LocalPlayer.PlayerGui
            pg.Hud:SetAttribute("Mark", "old"); pg.Persist:SetAttribute("Mark", "old"); pg.GuardName:SetAttribute("Mark", "old")
            pg.Hud.Destroying:Connect(function() print("hud destroying") end)
            pg.Persist.Destroying:Connect(function() print("persist destroying") end)
            print("guis0", pg.Hud:GetAttribute("Runs"), pg.Persist:GetAttribute("Runs"), #pg:GetChildren())
        )");
        check(alog.has("gui ran	Hud	1") && alog.has("gui ran	Persist	1") && alog.has("guis0	1	1	3"), "StarterGui's ScreenGuis and their LocalScripts are the player's at the first spawn");
        server.runChunk("respawn", R"(game.Players.Alice:LoadCharacter())");
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        alice.runChunk("guis", R"(
            local pg = game.Players.LocalPlayer.PlayerGui
            print("guis1", pg.Hud:GetAttribute("Runs"), pg.Hud:GetAttribute("Mark"), pg.Persist:GetAttribute("Runs"), pg.Persist:GetAttribute("Mark"), pg.GuardName:GetAttribute("Mark"), #pg:GetChildren())
        )");
        check(alog.has("hud destroying") && !alog.has("persist destroying") && alog.count("gui ran	Hud	1") == 2 && alog.count("gui ran	Persist	1") == 1,
              "ResetOnSpawn: a respawn destroys the Hud and copies it from StarterGui again, its LocalScript runs anew; Persist (ResetOnSpawn = false) stays");
        check(alog.has("guis1	1	nil	1	old	nil	3"), "the new Hud and BillboardGui are fresh copies, the Persist gui is the same one");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R28. the name over the head: Humanoid.DisplayName and the display distances, from StarterPlayer");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        server.runChunk("setup", R"(
            local sp = game:GetService("StarterPlayer")
            print("starter", sp.NameDisplayDistance, sp.HealthDisplayDistance)
            sp.NameDisplayDistance = 40
            local hum = Instance.new("Humanoid")
            print("defaults", hum.DisplayName == "", hum.NameDisplayDistance, hum.HealthDisplayDistance, hum.DisplayDistanceType, hum.HealthDisplayType, hum.NameOcclusion)
            hum.DisplayDistanceType = Enum.HumanoidDisplayDistanceType.Subject
            hum.HealthDisplayType = "AlwaysOn"
            hum.NameOcclusion = Enum.NameOcclusion.NoOcclusion
            print("set", hum.DisplayDistanceType, hum.HealthDisplayType == Enum.HumanoidHealthDisplayType.AlwaysOn, hum.NameOcclusion.Value)
            game.Players.PlayerAdded:Connect(function(p)
                p.CharacterAdded:Connect(function(c)
                    local h = c:WaitForChild("Humanoid")
                    print("spawned", h.DisplayName, h.NameDisplayDistance, h.HealthDisplayDistance)
                end)
            end)
        )");
        check(slog.has("starter	100	100"), "StarterPlayer.NameDisplayDistance / HealthDisplayDistance default to 100");
        check(slog.has("defaults	true	100	100	Enum.HumanoidDisplayDistanceType.Viewer	Enum.HumanoidHealthDisplayType.DisplayWhenDamaged	Enum.NameOcclusion.OccludeAll"), "a Humanoid's name-tag properties, Roblox's defaults");
        check(slog.has("set	Enum.HumanoidDisplayDistanceType.Subject	true	0"), "the three enums take items or names");
        LocalSession session(server);
        session.join(alice, "Alice", 14);
        alice.runChunk("watch", R"(
            local me = game.Players.LocalPlayer
            local c = me.Character or me.CharacterAdded:Wait()
            local h = c:WaitForChild("Humanoid")
            print("mine", h.DisplayName, h.NameDisplayDistance)
            h:GetPropertyChangedSignal("DisplayName"):Connect(function() print("renamed", h.DisplayName) end)
        )");
        for (int i = 0; i < 4; i++) session.stepAll(0.05);
        check(slog.has("spawned	Alice	40	100"), "a new character's Humanoid takes the player's name and StarterPlayer's distances");
        check(alog.has("mine	Alice	40"), "and they replicate");
        server.runChunk("rename", R"(game.Players.Alice.Character.Humanoid.DisplayName = "Al")");
        session.stepAll(0.05);
        check(alog.has("renamed	Al"), "a DisplayName change reaches the client");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R29. BrickColor and Teams: the palette, Part.BrickColor, auto-assignment, team spawns, AllowTeamChangeOnTouch");
    {
        Log slog, alog, blog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog)), bob(co, callbacks(blog));
        server.runChunk("bricks", R"(
            local red = BrickColor.new("Bright red")
            print("brick", red.Name, red.Number, tostring(red.Color), red == BrickColor.new(21), red == BrickColor.Red(), typeof(red), tostring(red))
            print("lookup", BrickColor.new(1004).Name, BrickColor.new("No such colour").Name, BrickColor.new(Color3.fromRGB(0, 255, 0)).Name, BrickColor.new(0, 0, 1).Name, BrickColor.Blue().Name, BrickColor.Gray().Number)
            local p = Instance.new("Part"); p.Name = "Brick"; p.Anchored = true; p.Parent = workspace
            p.BrickColor = BrickColor.new("Lime green")
            print("part", tostring(p.Color), p.BrickColor.Name, p.Color == BrickColor.new("Lime green").Color)
            p.Color = Color3.fromRGB(250, 0, 5)
            print("nearest", p.BrickColor.Name)
            p.BrickColor = "Really blue"
            print("byname", p.BrickColor.Name)
            local ok, err = pcall(function() p.BrickColor = 5 end)
            print("bad", ok, err)
            p:GetPropertyChangedSignal("BrickColor"):Connect(function() print("changed", p.BrickColor.Name) end)
            p.BrickColor = BrickColor.Yellow()
        )");
        check(slog.has("brick	Bright red	21	0.768627, 0.156863, 0.109804	true	true	BrickColor	Bright red"), "BrickColor.new(name): Name, Number, Color; equal by number; BrickColor.Red()");
        check(slog.has("lookup	Really red	Medium stone grey	Lime green	Really blue	Bright blue	194"), "by number, an unknown name is Medium stone grey, a Color3 or r,g,b the nearest");
        check(slog.has("part	0, 1, 0	Lime green	true"), "Part.BrickColor sets Color");
        check(slog.has("nearest	Really red"), "and reads Color back on the palette");
        check(slog.has("byname	Really blue"), "a name assigns too");
        check(slog.has("bad	false	bricks:12: invalid argument #3 to 'BrickColor' (BrickColor expected, got number)"), "a number does not");
        check(slog.has("changed	Bright yellow"), "GetPropertyChangedSignal(\"BrickColor\") is Color's");

        server.runChunk("teams", R"(
            local Teams = game:GetService("Teams")
            local red = Instance.new("Team"); red.Name = "Red"; red.TeamColor = BrickColor.new("Bright red"); red.Parent = Teams
            local blue = Instance.new("Team"); blue.Name = "Blue"; blue.TeamColor = BrickColor.new("Bright blue"); blue.Parent = Teams
            local refs = Instance.new("Team"); refs.Name = "Referees"; refs.TeamColor = BrickColor.new("Black"); refs.AutoAssignable = false; refs.Parent = Teams
            print("teams", #Teams:GetTeams(), red.TeamColor.Name, tostring(red.TeamColor), refs.AutoAssignable)
            for _, t in ipairs(Teams:GetTeams()) do
                t.PlayerAdded:Connect(function(p) print("joined", t.Name, p.Name, p.TeamColor.Name, p.Neutral) end)
                t.PlayerRemoved:Connect(function(p) print("left", t.Name, p.Name) end)
            end
            -- spawns: one per team, one for everyone, and a touch pad that swaps you to Blue
            local function spawn(name, color, neutral, x, touch)
                local s = Instance.new("SpawnLocation"); s.Name = name; s.Anchored = true; s.Position = Vector3.new(x, 0, 0)
                s.Neutral = neutral; if color then s.TeamColor = color end; s.AllowTeamChangeOnTouch = touch or false; s.Parent = workspace
                return s
            end
            spawn("RedSpawn", BrickColor.new("Bright red"), false, -20)
            spawn("BlueSpawn", BrickColor.new("Bright blue"), false, 20)
            spawn("Lobby", nil, true, 0)
            local pad = spawn("JoinBlue", BrickColor.new("Bright blue"), false, 40, true)
            print("spawnprops", pad.TeamColor.Name, pad.AllowTeamChangeOnTouch, workspace.Lobby.TeamColor.Name, workspace.Lobby.Neutral)
        )");
        check(slog.has("teams	3	Bright red	Bright red	false"), "Teams:GetTeams, Team.TeamColor a BrickColor, AutoAssignable");
        check(slog.has("spawnprops	Bright blue	true	Medium stone grey	true"), "SpawnLocation.TeamColor / AllowTeamChangeOnTouch / Neutral, Roblox's defaults");
        LocalSession session(server);
        session.join(alice, "Alice", 14);
        session.join(bob, "Bob", 15);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        server.runChunk("who", R"(
            local Players = game.Players
            for _, p in ipairs(Players:GetPlayers()) do
                print("player", p.Name, p.Team and p.Team.Name, p.TeamColor.Name, p.Neutral, p.Character.HumanoidRootPart.Position.X)
            end
            print("members", #game.Teams.Red:GetPlayers(), #game.Teams.Blue:GetPlayers(), #game.Teams.Referees:GetPlayers())
        )");
        check(slog.has("joined	Red	Alice	Bright red	false") || slog.has("joined	Blue	Alice	Bright blue	false"), "a new player joins an AutoAssignable team: Team.PlayerAdded, TeamColor set, Neutral off");
        check(slog.has("player	Alice	Red	Bright red	false	-20") && (slog.has("player	Bob	Blue	Bright blue	false	20") || slog.has("player	Bob	Blue	Bright blue	false	0") || slog.has("player	Bob	Blue	Bright blue	false	40")),
              "the second player fills the emptier team; each spawns at a SpawnLocation of its team's colour or a Neutral one");
        check(slog.has("members	1	1	0"), "Team:GetPlayers");
        alice.runChunk("mine", R"(
            local me = game.Players.LocalPlayer
            print("client", me.Team.Name, me.TeamColor.Name, me.Neutral, #game:GetService("Teams"):GetTeams())
            me:GetPropertyChangedSignal("Team"):Connect(function() print("moved", me.Team and me.Team.Name) end)
            game.Teams.Referees.PlayerAdded:Connect(function(p) print("ref", p.Name) end)
        )");
        check(alog.has("client	Red	Bright red	false	3") || alog.has("client	Blue	Bright blue	false	3"), "the client sees its Team, TeamColor and the Teams service");
        // a Script moves Alice: Team sets the colour; the colour alone finds the Team
        server.runChunk("move", R"(
            local a = game.Players.Alice
            a.Team = game.Teams.Referees
            print("moved", a.Team.Name, a.TeamColor.Name, a.Neutral)
            a.TeamColor = BrickColor.new("Bright red")
            print("bycolor", a.Team.Name, a.Neutral)
            a.Team = nil
            print("neutral", a.Team, a.Neutral, a.TeamColor.Name)
            a:LoadCharacter()
            -- Bob may be standing on the Lobby: the spawn steps aside, but not to a team's spawn
            print("lobby", math.abs(a.Character.HumanoidRootPart.Position.X) < 10)
        )");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(slog.has("moved	Referees	Black	false"), "Player.Team = team: TeamColor follows, Neutral off");
        check(slog.has("joined	Referees	Alice	Black	false") && (slog.has("left	Red	Alice") || slog.has("left	Blue	Alice")), "Team.PlayerAdded on the new team, PlayerRemoved on the old");
        check(slog.has("bycolor	Red	false"), "Player.TeamColor = a team's colour joins that team");
        check(slog.has("neutral	nil	true	Bright red"), "Team = nil makes the player Neutral (the colour stays)");
        check(slog.has("lobby	true"), "a Neutral player spawns at a Neutral SpawnLocation only");
        check(alog.has("moved	Referees") && alog.has("ref	Alice") && alog.has("moved	nil"), "the client sees each move and the Team's PlayerAdded");
        // touching the pad
        Instance* pad = server.dataModel().workspace()->findFirstChild("JoinBlue");
        Instance* alicesChar = server.dataModel().find(server.dataModel().getService("Players")->findFirstChild("Alice")->get("Character").ref);
        server.fireEvent(*pad, "Touched", {Value::instance(alicesChar->findFirstChild("Left Leg")->id())});
        server.runChunk("after", R"(print("touched", game.Players.Alice.Team.Name, game.Players.Alice.Neutral))");
        check(slog.has("touched	Blue	false"), "a SpawnLocation with AllowTeamChangeOnTouch puts the toucher on its team");
        // Rojo files: a Team's colour by name or number, a Part's BrickColor
        std::string err;
        Instance* green = loadSourceFile(server, "Teams/Green.model.json", R"JSON({"className": "Team", "properties": {"TeamColor": "Lime green", "AutoAssignable": false}})JSON", &err);
        Instance* jsonPart = loadSourceFile(server, "Workspace/Painted.model.json", R"JSON({"className": "Part", "properties": {"BrickColor": {"BrickColor": 1004}, "Anchored": true},
            "children": [{"name": "Pad", "className": "SpawnLocation", "properties": {"TeamColor": {"BrickColor": 21}, "Neutral": false, "AllowTeamChangeOnTouch": true}}]})JSON", &err);
        server.runChunk("json", R"(
            print("json", game.Teams.Green.TeamColor.Name, workspace.Painted.BrickColor.Name, workspace.Painted.Pad.TeamColor.Name, workspace.Painted.Pad.Neutral)
        )");
        check(green && jsonPart && slog.has("json	Lime green	Really red	Bright red	false"), ("model.json: TeamColor by name or {\"BrickColor\": n}, Part.BrickColor: " + err).c_str());
        check(slog.errors.empty() && alog.errors.empty() && blog.errors.empty(), "no errors");
    }

    section("R30. Chat: Player.Chatted, the lines every client draws, Chat:Chat bubbles, SetCore ChatMakeSystemMessage");
    {
        Log slog, alog, blog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog)), bob(co, callbacks(blog));
        server.runChunk("listen", R"(
            game.Players.PlayerAdded:Connect(function(p)
                p.Chatted:Connect(function(msg, recipient) print("chatted", p.Name, msg, recipient) end)
            end)
            print("chatprops", game.Chat.BubbleChatEnabled, game.Chat.LoadDefaultChat, game:GetService("Chat"):FilterStringAsync("hello", nil, nil))
        )");
        check(slog.has("chatprops	true	true	hello"), "Chat.BubbleChatEnabled / LoadDefaultChat; FilterStringAsync returns the text");
        LocalSession session(server);
        session.join(alice, "Alice", 16);
        session.join(bob, "Bob", 17);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        alice.runChunk("watch", R"(
            game.Players.Bob.Chatted:Connect(function(msg) print("bobsaid", msg) end)
        )");
        alice.chat("  hello there  ");
        bob.chat("   ");
        bob.chat(std::string(300, 'x'));
        session.stepAll(0.05);
        session.stepAll(0.05);
        check(slog.has("chatted	Alice	hello there	nil") && slog.has("chatted	Bob	" + std::string(200, 'x') + "	nil"), "Player.Chatted(message, nil) on the server: trimmed, 200 characters at most, blank dropped");
        std::vector<Runtime::ChatLine> al = alice.takeChat(), bl = bob.takeChat();
        int64_t aliceId = session.playerOf(alice), bobId = session.playerOf(bob);
        check(al.size() == 2 && al[0].speaker == aliceId && al[0].name == "Alice" && al[0].text == "hello there" && !al[0].system && !al[0].part
              && al[1].speaker == bobId && al[1].text.size() == 200, "every client gets the lines, its own included, with the speaker and its name");
        check(bl.size() == 2 && bl[0].speaker == aliceId && bl[0].text == "hello there", "Bob too");
        check(alog.has("bobsaid	" + std::string(200, 'x')), "and Player.Chatted fires on the clients");
        check(server.takeChat().empty() && alice.takeChat().empty(), "the server draws nothing; taken lines are gone");
        // Chat:Chat: a bubble over a part or a character's Head, from the server to everyone
        server.runChunk("bubble", R"(
            local guard = Instance.new("Part"); guard.Name = "Guard"; guard.Anchored = true; guard.Parent = workspace
            game.Chat:Chat(guard, "Halt!", Enum.ChatColor.Red)
            game.Chat:Chat(game.Players.Alice.Character, "Hi Bob", Color3.new(0, 0, 1))
            print("guard", guard, game.Players.Alice.Character.Head)
        )");
        session.stepAll(0.05);
        bl = bob.takeChat();
        int64_t guardId = server.dataModel().workspace()->findFirstChild("Guard")->id();
        int64_t headId = server.dataModel().find(server.dataModel().getService("Players")->findFirstChild("Alice")->get("Character").ref)->findFirstChild("Head")->id();
        check(bl.size() == 2 && bl[0].part == guardId && bl[0].text == "Halt!" && !bl[0].speaker && bl[0].color.r == 1 && bl[0].color.g == 0.5f
              && bl[1].part == headId && bl[1].text == "Hi Bob" && bl[1].color.b == 1, "Chat:Chat(part | character, text, ChatColor | Color3): a bubble line over the part / the Head");
        al = alice.takeChat();
        check(al.size() == 2 && al[0].part == guardId && al[1].part == headId, "on every client");
        // a client's own: local, and the system message
        bob.runChunk("local", R"(
            game.Chat:Chat(game.Players.Bob.Character, "to myself")
            game.StarterGui:SetCore("ChatMakeSystemMessage", {Text = "Welcome!", Color = Color3.fromRGB(255, 255, 0)})
            print("setcore", pcall(function() game.StarterGui:SetCore("ResetButtonCallback", false) end))
        )");
        session.stepAll(0.05);
        bl = bob.takeChat();
        check(bl.size() == 2 && bl[0].part && bl[0].text == "to myself" && bl[1].system && bl[1].text == "Welcome!" && bl[1].color.r == 1 && bl[1].color.b == 0 && alice.takeChat().empty(), "a client's Chat:Chat and ChatMakeSystemMessage stay on that client");
        check(blog.has("setcore	true"), "other SetCore names are accepted");
        bob.runChunk("notify", R"(
            local answer = Instance.new("BindableFunction")
            answer.OnInvoke = function(pressed) print("pressed", pressed) end
            game.StarterGui:SetCore("SendNotification", {Title = "Fishing Rod", Text = "You got a Fishing Rod.", Duration = 3,
                Button1 = "Nice", Button2 = "Close", Callback = answer})
            _G.answer = answer
            print("notify missing", (pcall(function() game.StarterGui:SetCore("SendNotification", {Title = "no text"}) end)))
        )");
        auto cards = bob.takeNotifications();
        check(cards.size() == 1 && cards[0].title == "Fishing Rod" && cards[0].text == "You got a Fishing Rod." && cards[0].duration == 3
              && cards[0].button1 == "Nice" && cards[0].button2 == "Close" && cards[0].callback != 0 && alice.takeNotifications().empty(),
              "SetCore SendNotification: Title, Text, Duration, buttons and Callback, on that client only");
        check(blog.has("notify missing\tfalse"), "SendNotification wants a Text");
        if (cards.size() == 1) bob.notificationButton(cards[0].callback, "Nice");
        check(blog.has("pressed\tNice"), "pressing a button invokes the Callback with its text");
        check(slog.errors.empty() && alog.errors.empty() && blog.errors.empty(), "no errors");
        std::string err = server.runChunk("srv", R"(game.StarterGui:SetCore("ChatMakeSystemMessage", {Text = "x"}))");
        check(err.find("SetCore can only be called from a LocalScript") != std::string::npos, "SetCore is a LocalScript's");
    }

    section("R31. TextChatService: the default channels and TextSources, SendAsync, OnIncomingMessage, ShouldDeliverCallback, commands");
    {
        Log slog, alog, blog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog)), bob(co, callbacks(blog));
        server.runChunk("setup", R"(
            local tcs = game:GetService("TextChatService")
            local general = tcs.TextChannels.RBXGeneral
            print("defaults", tcs.ChatVersion, tcs.CreateDefaultTextChannels, general.ClassName, tcs.TextChannels.RBXSystem.ClassName, tcs.TextChatCommands.ClassName, #general:GetChildren())
            game.Players.PlayerAdded:Connect(function(p)
                p.Chatted:Connect(function(msg, recipient) print("chatted", p.Name, msg, recipient) end)
            end)
            general.MessageReceived:Connect(function(msg) print("srvchan", msg.Text, msg.TextSource.Name, msg.Status, msg.TextChannel.Name, msg.MessageId ~= "") end)
            tcs.MessageReceived:Connect(function(msg) print("srvtcs", msg.Text, msg.TextSource.UserId) end)
            general.ShouldDeliverCallback = function(msg, source)
                return msg.Text ~= "secret" or source.UserId == msg.TextSource.UserId
            end
            local cmd = Instance.new("TextChatCommand")
            cmd.Name = "HelloCommand"; cmd.PrimaryAlias = "/hello"; cmd.SecondaryAlias = "/hi"
            cmd.Triggered:Connect(function(source, text) print("cmd", source.Name, text) end)
            cmd.Parent = tcs.TextChatCommands
        )");
        check(slog.has("defaults\tEnum.ChatVersion.TextChatService\ttrue\tTextChannel\tTextChannel\tFolder\t0"), "TextChatService.TextChannels.RBXGeneral / RBXSystem and TextChatCommands from the start");
        LocalSession session(server);
        session.join(alice, "Alice", 16);
        session.join(bob, "Bob", 17);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        int64_t aliceId = session.playerOf(alice), bobId = session.playerOf(bob);
        server.runChunk("sources", R"(
            local general = game.TextChatService.TextChannels.RBXGeneral
            local a, b = general.Alice, general.Bob
            print("sources", a.ClassName, a.UserId, a.CanSend, b.UserId, game.TextChatService.TextChannels.RBXSystem.Bob.UserId, #general:GetChildren())
        )");
        check(slog.has("sources\tTextSource\t16\ttrue\t17\t17\t2"), "a player joining gets a TextSource (its name, UserId) in each default channel");
        bob.runChunk("watch", R"(
            local tcs = game:GetService("TextChatService")
            local general = tcs.TextChannels.RBXGeneral
            print("bobsees", general.Alice.UserId, general.Bob.UserId, tcs.TextChatCommands.HelloCommand.PrimaryAlias)
            tcs.OnIncomingMessage = function(msg)
                local props = Instance.new("TextChatMessageProperties")
                if msg.TextSource then props.PrefixText = "[" .. msg.TextSource.Name .. "]:" end
                return props
            end
            tcs.MessageReceived:Connect(function(msg) print("bobgot", msg.Text, msg.PrefixText, msg.Status, msg.TextChannel.Name, msg.TextSource and msg.TextSource.Name) end)
            general.MessageReceived:Connect(function(msg) print("bobchan", msg.Text) end)
            game.Players.Alice.Chatted:Connect(function(msg) print("alicesaid", msg) end)
            tcs.TextChatCommands.HelloCommand.Triggered:Connect(function(source, text) print("bobcmd", source.Name, text) end)
        )");
        check(blog.has("bobsees\t16\t17\t/hello"), "and the clients see the TextSources and commands");
        alice.runChunk("send", R"(
            local tcs = game:GetService("TextChatService")
            local general = tcs.TextChannels.RBXGeneral
            tcs.SendingMessage:Connect(function(msg) print("sending", msg.Text, msg.Status) end)
            tcs.MessageReceived:Connect(function(msg) print("alicegot", msg.Text, msg.PrefixText, msg.Status) end)
            local msg = general:SendAsync("  hey all  ")
            print("sent", msg.ClassName, msg.Text, msg.Status, msg.TextSource.Name, msg.TextChannel.Name, msg.MessageId ~= "")
            task.delay(0.4, function() print("later", msg.Status, msg.Text, msg.MessageId:sub(1, 1)) end)
        )");
        for (int i = 0; i < 10; i++) session.stepAll(0.05);
        check(alog.has("sending\they all\tEnum.TextChatMessageStatus.Sending") && alog.has("sent\tTextChatMessage\they all\tEnum.TextChatMessageStatus.Sending\tAlice\tRBXGeneral\ttrue"),
              "TextChannel:SendAsync(text): TextChatService.SendingMessage, then the TextChatMessage (Status Sending, its TextSource and TextChannel)");
        check(slog.has("srvchan\they all\tAlice\tEnum.TextChatMessageStatus.Success\tRBXGeneral\ttrue") && slog.has("srvtcs\they all\t16") && slog.has("chatted\tAlice\they all\tnil"),
              "the server: TextChannel.MessageReceived, TextChatService.MessageReceived and Player.Chatted");
        check(blog.has("bobgot\they all\t[Alice]:\tEnum.TextChatMessageStatus.Success\tRBXGeneral\tAlice") && blog.has("bobchan\they all") && blog.has("alicesaid\they all"),
              "every client: OnIncomingMessage's TextChatMessageProperties set the PrefixText, then MessageReceived and Player.Chatted");
        check(alog.has("later\tEnum.TextChatMessageStatus.Success\they all\ts"), "the sender's message becomes Success with the server's MessageId");
        std::vector<Runtime::ChatLine> al = alice.takeChat(), bl = bob.takeChat();
        check(bl.size() == 1 && bl[0].speaker == aliceId && bl[0].name == "Alice" && bl[0].text == "hey all" && bl[0].rich == "[Alice]:hey all", "the line the engine draws carries PrefixText .. Text as rich text");
        check(al.size() == 1 && al[0].rich.rfind("<font color=\"#", 0) == 0 && al[0].rich.find("\">Alice</font>: hey all") != std::string::npos && al[0].color.r != 1.f,
              "the default PrefixText: the name in its classic chat colour");
        check(alog.has("alicegot\they all\t<font color=\"#DA8541\">Alice</font>: \tEnum.TextChatMessageStatus.Success") && server.takeChat().empty(), "the sender gets it back like everyone else; the server draws nothing");
        // ShouldDeliverCallback
        alice.runChunk("secret", R"(game.TextChatService.TextChannels.RBXGeneral:SendAsync("secret"))");
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        check(slog.has("srvchan\tsecret\tAlice\tEnum.TextChatMessageStatus.Success\tRBXGeneral\ttrue") && alice.takeChat().size() == 1 && bob.takeChat().empty() && !blog.has("bobgot\tsecret") && alog.errors.empty(),
              "TextChannel.ShouldDeliverCallback(message, source) false keeps it from that recipient");
        // a system message
        bob.runChunk("system", R"(
            local msg = game.TextChatService.TextChannels.RBXSystem:DisplaySystemMessage("Welcome!", "welcome")
            print("system", msg.Text, msg.Status, msg.TextSource, msg.Metadata, msg.TextChannel.Name)
        )");
        session.stepAll(0.05);
        bl = bob.takeChat();
        check(blog.has("system\tWelcome!\tEnum.TextChatMessageStatus.Success\tnil\twelcome\tRBXSystem") && blog.has("bobgot\tWelcome!\t\tEnum.TextChatMessageStatus.Success\tRBXSystem\tnil")
              && bl.size() == 1 && bl[0].system && bl[0].text == "Welcome!" && bl[0].rich == "Welcome!" && alice.takeChat().empty() && !slog.has("Welcome!"),
              "TextChannel:DisplaySystemMessage: a message with no TextSource, on this client only, through OnIncomingMessage / MessageReceived");
        // commands
        alice.runChunk("command", R"(
            game.TextChatService.TextChatCommands.HelloCommand.Triggered:Connect(function(source, text) print("alicecmd", source.Name, text) end)
            local msg = game.TextChatService.TextChannels.RBXGeneral:SendAsync("/hi there")
            print("cmdmsg", msg.Status)
        )");
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        check(alog.has("alicecmd\tAlice\t/hi there") && slog.has("cmd\tAlice\t/hi there") && alog.has("cmdmsg\tEnum.TextChatMessageStatus.Success")
              && !blog.has("bobcmd") && !slog.has("srvchan\t/hi there") && alice.takeChat().empty() && bob.takeChat().empty(),
              "a TextChatCommand's alias: Triggered(source, text) on the sender and the server, and no message");
        // a channel of your own
        server.runChunk("team", R"(
            local ch = Instance.new("TextChannel"); ch.Name = "Whisper"; ch.Parent = game.TextChatService.TextChannels
            local src, ok = ch:AddUserAsync(17)
            print("added", src.ClassName, src.Name, src.UserId, ok, ch:AddUserAsync(999))
            print("sendsrv", pcall(function() ch:SendAsync("x") end))
        )");
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(slog.has("added\tTextSource\tBob\t17\ttrue\tnil\tfalse") && slog.has("sendsrv\tfalse\tteam:5: SendAsync can only be called from a LocalScript"),
              "TextChannel:AddUserAsync(userId) -> TextSource, true (nil, false for nobody); SendAsync is a client's");
        bob.runChunk("whisper", R"(local m = game.TextChatService.TextChannels.Whisper:SendAsync("psst"); print("whisper", m.Status))");
        alice.runChunk("whisper", R"(local m = game.TextChatService.TextChannels.Whisper:SendAsync("psst"); print("whisper", m.Status))");
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        bl = bob.takeChat();
        check(bl.size() == 1 && bl[0].text == "psst" && bl[0].speaker == bobId && alice.takeChat().empty() && blog.has("whisper\tEnum.TextChatMessageStatus.Sending")
              && alog.has("whisper\tEnum.TextChatMessageStatus.InvalidTextChannelPermissions") && slog.has("chatted\tBob\tpsst\tnil"),
              "a message reaches the channel's TextSources only; no TextSource there: InvalidTextChannelPermissions");
        // the legacy chat
        server.runChunk("legacy", R"(game.TextChatService.ChatVersion = Enum.ChatVersion.LegacyChatService)");
        session.stepAll(0.05);
        alice.chat("old school");
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        bl = bob.takeChat();
        check(slog.has("chatted\tAlice\told school\tnil") && bl.size() == 1 && bl[0].speaker == aliceId && bl[0].rich.find("Alice</font>: old school") != std::string::npos
              && !blog.has("bobgot\told school") && blog.has("alicesaid\told school"),
              "ChatVersion.LegacyChatService: the engine's chat box goes the legacy way (Player.Chatted, no TextChatMessage)");
        check(slog.errors.empty() && alog.errors.empty() && blog.errors.empty(), "no errors");
    }

    section("R32. spatial queries: GetPartBoundsInBox / InRadius, GetPartsInPart, OverlapParams, Blockcast / Spherecast / Shapecast, GetTouchingParts");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        rt.runChunk("space", R"(
            local function part(name, pos, size)
                local p = Instance.new("Part"); p.Name = name; p.Position = pos; p.Size = size; p.Anchored = true; p.Parent = workspace; return p
            end
            local function f(v) return string.format("%.2f, %.2f, %.2f", v.X, v.Y, v.Z) end
            local function names(t) local n = {}; for _, p in t do table.insert(n, p.Name) end; table.sort(n); return table.concat(n, ",") end
            local floor = part("Floor", Vector3.new(0, -1, 0), Vector3.new(50, 2, 50))
            local crate = part("Crate", Vector3.new(0, 2, 0), Vector3.new(4, 4, 4))
            local ball = part("Ball", Vector3.new(10, 3, 0), Vector3.new(4, 4, 4)); ball.Shape = Enum.PartType.Ball
            local tilted = part("Tilted", Vector3.new(0, 2, 12), Vector3.new(4, 4, 4)); tilted.Orientation = Vector3.new(0, 45, 0)
            local ghost = part("Ghost", Vector3.new(1, 2, 1), Vector3.new(1, 1, 1)); ghost.CanQuery = false
            local probe = part("Probe", Vector3.new(2.5, 2, 14), Vector3.new(0.2, 1, 1))
            local bush = Instance.new("Model"); bush.Name = "Bush"; bush.Parent = workspace
            local leaf = part("Leaf", Vector3.new(-10, 2, 0), Vector3.new(2, 2, 2)); leaf.Parent = bush
            print("inbox", names(workspace:GetPartBoundsInBox(CFrame.new(0, 2, 0), Vector3.new(6, 6, 6))))
            print("bounds", names(workspace:GetPartBoundsInBox(probe.CFrame, probe.Size)), names(workspace:GetPartsInPart(probe)))
            print("radius", names(workspace:GetPartBoundsInRadius(Vector3.new(10, 3, 0), 2.5)), names(workspace:GetPartBoundsInRadius(Vector3.new(0, 2, 0), 100)))
            local params = OverlapParams.new()
            print("params", typeof(params), tostring(params), params.MaxParts, params.FilterType.Name, params.RespectCanCollide, params.BruteForceAllSlow)
            params.MaxParts = 2
            print("max", #workspace:GetPartBoundsInRadius(Vector3.new(0, 2, 0), 100, params))
            params.MaxParts = 0; params.FilterDescendantsInstances = {crate, bush}
            print("exclude", names(workspace:GetPartBoundsInRadius(Vector3.new(0, 2, 0), 100, params)))
            params.FilterType = Enum.RaycastFilterType.Include
            print("include", names(workspace:GetPartBoundsInRadius(Vector3.new(0, 2, 0), 100, params)))
            print("badparams", select(2, pcall(function() workspace:GetPartsInPart(crate, RaycastParams.new()) end)), select(2, pcall(function() params.IgnoreWater = true end)))
            print("touching", names(crate:GetTouchingParts()), names(workspace:GetPartsInPart(crate)), names(ball:GetTouchingParts()))
            local r = workspace:Spherecast(Vector3.new(0, 10, 0), 1, Vector3.new(0, -20, 0))
            print("sphere", r.Instance.Name, f(r.Position), f(r.Normal), r.Distance, r.Material.Name)
            r = workspace:Spherecast(Vector3.new(2.5, 10, 0), 1, Vector3.new(0, -20, 0))
            print("sphere edge", r.Instance.Name, f(r.Position), f(r.Normal), string.format("%.3f", r.Distance))
            r = workspace:Spherecast(Vector3.new(0, 2, 0), 1, Vector3.new(0, -10, 0))
            print("sphere inside", r.Instance.Name, f(r.Position), r.Distance)
            r = workspace:Spherecast(Vector3.new(10, 10, 0), 1, Vector3.new(0, -20, 0))
            print("sphere ball", r.Instance.Name, f(r.Position), f(r.Normal), r.Distance)
            print("sphere miss", workspace:Spherecast(Vector3.new(0, 10, 0), 1, Vector3.new(0, -3, 0)))
            r = workspace:Blockcast(CFrame.new(0, 10, 0), Vector3.new(2, 2, 2), Vector3.new(0, -20, 0))
            print("block", r.Instance.Name, r.Position.Y, f(r.Normal), r.Distance)
            r = workspace:Blockcast(CFrame.new(0, 10, 0) * CFrame.Angles(0, math.rad(45), 0), Vector3.new(2, 2, 2), Vector3.new(0, -20, 0))
            print("block turned", r.Instance.Name, r.Position.Y, f(r.Normal), string.format("%.3f", r.Distance))
            r = workspace:Blockcast(CFrame.new(10, 10, 0), Vector3.new(2, 2, 2), Vector3.new(0, -20, 0))
            print("block ball", r.Instance.Name, f(r.Position), f(r.Normal), r.Distance)
            local rp = RaycastParams.new(); rp.FilterDescendantsInstances = {crate}
            r = workspace:Blockcast(CFrame.new(0, 10, 0), Vector3.new(2, 2, 2), Vector3.new(0, -20, 0), rp)
            print("block exclude", r.Instance.Name, r.Distance)
            r = workspace:Shapecast(crate, Vector3.new(20, 0, 0))
            print("shape", r.Instance.Name, f(r.Position), f(r.Normal), r.Distance)
            print("shape miss", workspace:Shapecast(crate, Vector3.new(0, 20, 0)), workspace:Blockcast(CFrame.new(0, 2, 0), Vector3.new(2, 2, 2), Vector3.new(0, 1, 0)))
            print("badcast", select(2, pcall(function() workspace:Blockcast(Vector3.new(), Vector3.new(1, 1, 1), Vector3.new(0, 1, 0)) end)))
        )");
        check(log.has("inbox	Crate,Floor"), "GetPartBoundsInBox: the parts whose bounds overlap the box; CanQuery = false is not seen");
        check(log.has("bounds	Probe,Tilted	"), "a turned part's world bounds reach where its shape does not: GetPartBoundsInBox sees it, GetPartsInPart does not");
        check(log.has("radius	Ball	Ball,Crate,Floor,Leaf,Probe,Tilted"), "GetPartBoundsInRadius");
        check(log.has("params	OverlapParams	OverlapParams	20	Exclude	false	false") && log.has("max	2"), "OverlapParams: MaxParts (20; 0 is all), FilterType, RespectCanCollide, BruteForceAllSlow");
        check(log.has("exclude	Ball,Floor,Probe,Tilted") && log.has("include	Crate,Leaf"), "Exclude / Include lists take subtrees, as RaycastParams' do");
        check(log.has("badparams	space:26: invalid argument #3 to 'GetPartsInPart' (OverlapParams expected, got RaycastParams)	space:26: IgnoreWater is not a valid member of OverlapParams"), "an OverlapParams is not a RaycastParams");
        check(log.has("touching	Floor	Floor	"), "GetTouchingParts / GetPartsInPart: the parts the shape overlaps, itself not counted");
        check(log.has("sphere	Crate	0.00, 4.00, 0.00	0.00, 1.00, 0.00	5	Plastic"), "Spherecast: a RaycastResult where the sphere's surface first meets a part");
        check(log.has("sphere edge	Crate	2.00, 4.00, 0.00	0.50, 0.87, 0.00	5.134"), "a sphere catching an edge: the contact is on the edge, the normal from it");
        check(log.has("sphere inside	Floor	0.00, 0.00, 0.00	1"), "a part the sphere starts inside is missed");
        check(log.has("sphere ball	Ball	10.00, 5.00, 0.00	0.00, 1.00, 0.00	4") && log.has("sphere miss	nil"), "against a ball; nil within reach of nothing");
        check(log.has("block	Crate	4	0.00, 1.00, 0.00	5") && log.has("block turned	Crate	4	0.00, 1.00, 0.00	5.000"), "Blockcast: an oriented box swept, the contact on the part's face");
        check(log.has("block ball	Ball	10.00, 5.00, 0.00	0.00, 1.00, 0.00	4") && log.has("block exclude	Floor	9"), "a block against a ball; RaycastParams filter it");
        check(log.has("shape	Ball	8.00, 3.00, 0.00	-1.00, 0.00, 0.00	6") && log.has("shape miss	nil	nil"), "Shapecast(part, direction) sweeps the part's own shape, not counting itself; a start inside misses");
        check(log.has("badcast	space:49: invalid argument #2 to 'Blockcast' (CFrame expected, got Vector3)"), "argument errors read like Roblox");
        check(log.errors.empty(), "no errors");
    }

    section("R33. TextBox: focus and typing, CaptureFocus / ReleaseFocus / IsFocused, FocusLost(enterPressed), GetFocusedTextBox");
    {
        Log log;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime rt(co, callbacks(log));
        rt.runChunk("gui", R"(
            local gui = Instance.new("ScreenGui"); gui.Name = "Hud"
            local name = Instance.new("TextBox"); name.Name = "NameBox"; name.PlaceholderText = "Your name"; name.Text = "old"; name.Parent = gui
            local note = Instance.new("TextBox"); note.Name = "Note"; note.ClearTextOnFocus = false; note.Text = "keep"; note.Parent = gui
            local ls = Instance.new("LocalScript"); ls.Name = "Form"; ls.Source = [[
                local gui = script.Parent
                local uis = game:GetService("UserInputService")
                local name, note = gui.NameBox, gui.Note
                print("defaults", name.CursorPosition, name.ClearTextOnFocus, name.TextEditable, name.MultiLine, name:IsFocused(), uis:GetFocusedTextBox())
                for _, box in {name, note} do
                    box.Focused:Connect(function() print("focused", box.Name, box.Text, box.CursorPosition, box:IsFocused(), uis:GetFocusedTextBox() == box) end)
                    box.FocusLost:Connect(function(enter, input) print("lost", box.Name, enter, input, box.Text, box.CursorPosition, box:IsFocused()) end)
                    box:GetPropertyChangedSignal("Text"):Connect(function() print("text", box.Name, box.Text, box.CursorPosition) end)
                end
                uis.TextBoxFocused:Connect(function(box) print("uis focused", box.Name) end)
                uis.TextBoxFocusReleased:Connect(function(box) print("uis released", box.Name, uis:GetFocusedTextBox()) end)
            ]]
            ls.Parent = gui
            gui.Parent = game:GetService("StarterGui")
        )");
        rt.addPlayer("Ann", 5);
        rt.step(0.016);
        check(log.has("defaults	-1	true	true	false	false	nil"), "TextBox defaults: CursorPosition -1 unfocused, ClearTextOnFocus, TextEditable, not MultiLine; nothing focused");
        Instance* hud = rt.dataModel().getService("Players")->findFirstChild("Ann")->findFirstChildOfClass("PlayerGui")->findFirstChild("Hud");
        Instance* name = hud->findFirstChild("NameBox");
        Instance* note = hud->findFirstChild("Note");
        rt.guiText(name->id(), "Focus");                                  // clicked into
        rt.guiText(name->id(), "Change", "A", 2);                         // typed
        rt.guiText(name->id(), "Change", "An", 3);
        rt.guiText(name->id(), "Blur", "", -1, true);                     // Enter
        check(log.has("focused	NameBox		1	true	true") && log.has("uis focused	NameBox"), "the engine's box taking focus: ClearTextOnFocus empties the text, the caret at 1, Focused and TextBoxFocused fire");
        check(log.has("text	NameBox	A	2") && log.has("text	NameBox	An	3"), "typing writes Text and CursorPosition, GetPropertyChangedSignal(\"Text\") sees each key");
        check(log.has("lost	NameBox	true	nil	An	-1	false") && log.has("uis released	NameBox	nil"), "Enter: FocusLost(true), the text stays, CursorPosition back to -1");
        rt.runChunk("code", R"(
            local hud = game:GetService("Players").LocalPlayer.PlayerGui.Hud
            local name, note = hud.NameBox, hud.Note
            note:CaptureFocus()
            print("captured", note:IsFocused(), note.Text, note.CursorPosition, game:GetService("UserInputService"):GetFocusedTextBox().Name)
            name:CaptureFocus()
            print("switched", note:IsFocused(), name:IsFocused())
            name:ReleaseFocus()
            name:ReleaseFocus(true)
            print("released", name:IsFocused(), game:GetService("UserInputService"):GetFocusedTextBox())
        )");
        check(log.has("focused	Note	keep	5	true	true") && log.has("captured	true	keep	5	Note"), "CaptureFocus from a script: ClearTextOnFocus = false keeps the text, the caret after it");
        check(log.has("lost	Note	false	nil	keep	-1	false") && log.has("switched	false	true"), "another box taking focus: the first loses it, FocusLost(false)");
        check(log.has("lost	NameBox	false	nil		-1	false") && log.starts("lost	NameBox") == 2 && log.has("released	false	nil"), "ReleaseFocus: FocusLost(false) once, a second ReleaseFocus is nothing");
        rt.guiText(name->id(), "Blur", "", -1, false);
        check(log.starts("lost	NameBox") == 2 && log.starts("uis released") == 3, "the engine reporting a blur the runtime already did is nothing");
        check(log.errors.empty(), "no errors");
        (void)note;
    }

    section("R34. UIGridLayout, UIScale, UIAspectRatioConstraint, UISizeConstraint, UITextSizeConstraint, ScrollingFrame");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        rt.runChunk("ui", R"(
            local grid = Instance.new("UIGridLayout")
            print("grid", grid.CellSize, grid.CellPadding, grid.FillDirection.Name, grid.FillDirectionMaxCells, grid.StartCorner.Name,
                grid.HorizontalAlignment.Name, grid.VerticalAlignment.Name, grid.SortOrder.Name, grid.AbsoluteContentSize, grid.AbsoluteCellCount, grid.AbsoluteCellSize)
            grid.CellSize = UDim2.fromOffset(40, 30); grid.StartCorner = Enum.StartCorner.BottomRight; grid.FillDirectionMaxCells = 3
            print("grid set", grid.CellSize, grid.StartCorner.Value, grid.FillDirectionMaxCells, grid:IsA("UIComponent"), grid:IsA("UIGridLayout"))
            print("ro", select(2, pcall(function() grid.AbsoluteCellCount = Vector2.new(1, 1) end)))
            local scale = Instance.new("UIScale"); print("scale", scale.Scale)
            local ar = Instance.new("UIAspectRatioConstraint"); print("aspect", ar.AspectRatio, ar.AspectType.Name, ar.DominantAxis.Name)
            ar.AspectType = Enum.AspectType.ScaleWithParentSize; ar.DominantAxis = Enum.DominantAxis.Height
            print("aspect set", ar.AspectType.Value, ar.DominantAxis.Value)
            local sc = Instance.new("UISizeConstraint"); print("size", sc.MinSize, sc.MaxSize)
            local tc = Instance.new("UITextSizeConstraint"); print("textsize", tc.MinTextSize, tc.MaxTextSize)
            local sf = Instance.new("ScrollingFrame")
            print("scroll", sf.CanvasSize, sf.CanvasPosition, sf.AutomaticCanvasSize.Name, sf.ScrollingEnabled, sf.ScrollingDirection.Name,
                sf.ScrollBarThickness, sf.ScrollBarImageColor3, sf.ElasticBehavior.Name, sf.VerticalScrollBarPosition.Name, sf.VerticalScrollBarInset.Name,
                sf.AbsoluteCanvasSize, sf.AbsoluteWindowSize, sf:IsA("GuiObject"), sf:IsA("Frame"))
            sf.CanvasSize = UDim2.new(0, 0, 0, 800); sf.CanvasPosition = Vector2.new(0, 120); sf.ScrollingDirection = Enum.ScrollingDirection.Y
            print("scroll set", sf.CanvasSize, sf.CanvasPosition, sf.ScrollingDirection.Value)
            print("bad", select(2, pcall(function() sf.CanvasPosition = 5 end)), select(2, pcall(function() grid.FillDirection = Enum.SortOrder.Name end)))
        )");
        check(log.has("grid	{0, 100}, {0, 100}	{0, 5}, {0, 5}	Horizontal	0	TopLeft	Left	Top	Name	0, 0	0, 0	0, 0"), "UIGridLayout defaults: CellSize / CellPadding, Horizontal, unlimited cells, TopLeft");
        check(log.has("grid set	{0, 40}, {0, 30}	3	3	true	true") && log.has("ro	ui:7: Unable to assign property AbsoluteCellCount. Property is read only"), "its properties, Enum.StartCorner; the Absolute* ones are read-only");
        check(log.has("scale	1") && log.has("aspect	1	FitWithinMaxSize	Width") && log.has("aspect set	1	1"), "UIScale.Scale; UIAspectRatioConstraint with Enum.AspectType / Enum.DominantAxis");
        check(log.has("size	0, 0	inf, inf") && log.has("textsize	1	100"), "UISizeConstraint MinSize / MaxSize, UITextSizeConstraint Min / MaxTextSize");
        check(log.has("scroll	{0, 0}, {0, 0}	0, 0	None	true	XY	12	0, 0, 0	WhenScrollable	Right	None	0, 0	0, 0	true	false"), "ScrollingFrame defaults: a GuiObject (not a Frame) with CanvasSize / CanvasPosition, the scroll bar, Enum.ScrollingDirection");
        check(log.has("scroll set	{0, 0}, {0, 800}	0, 120	2"), "CanvasSize / CanvasPosition / ScrollingDirection set");
        check(log.has("bad	ui:20: invalid argument #3 to 'CanvasPosition' (Vector2 expected, got number)	ui:20: Invalid value for enum FillDirection"), "typed like everything else");
        check(log.errors.empty(), "no errors");
    }

    section("R35. ImageLabel / ImageButton images, Mouse.Icon");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        rt.runChunk("img", R"(
            local il = Instance.new("ImageLabel")
            print("image", il.Image, il.ImageColor3, il.ImageTransparency, il.ScaleType.Name, il.ResampleMode.Name, il.ImageRectOffset, il.ImageRectSize,
                il.TileSize, il.IsLoaded, il.ContentImageSize, il:IsA("GuiObject"), il:IsA("GuiButton"))
            il.Name = "Logo"; il.Image = "images/sheet.png"; il.ImageRectOffset = Vector2.new(16, 0); il.ImageRectSize = Vector2.new(16, 16)
            il.ScaleType = Enum.ScaleType.Fit; il.ResampleMode = Enum.ResamplerMode.Pixelated
            il.Parent = game:GetService("StarterGui")
            il:GetPropertyChangedSignal("IsLoaded"):Connect(function() print("loaded", il.IsLoaded, il.ContentImageSize) end)
            print("image set", il.Image, il.ImageRectOffset, il.ImageRectSize, il.ScaleType.Value, il.ResampleMode.Value)
            print("ro", select(2, pcall(function() il.IsLoaded = true end)), select(2, pcall(function() il.ContentImageSize = Vector2.new(1, 1) end)))
            local ib = Instance.new("ImageButton")
            print("button", ib.HoverImage, ib.PressedImage, ib.AutoButtonColor, ib:IsA("GuiButton"), ib.Image)
            ib.HoverImage = "images/hover.png"; ib.PressedImage = "images/pressed.png"
            print("button set", ib.HoverImage, ib.PressedImage)
            print("bad", select(2, pcall(function() il.Image = Vector2.new() end)), select(2, pcall(function() il.ImageRectSize = 16 end)), select(2, pcall(function() il.HoverImage = "x" end)))
        )");
        check(log.has("image		1, 1, 1	0	Stretch	Default	0, 0	0, 0	{1, 0}, {1, 0}	false	0, 0	true	false"), "ImageLabel defaults: no Image, white, Stretch, no sprite rect, not loaded");
        check(log.has("image set	images/sheet.png	16, 0	16, 16	3	1") && log.has("ro	img:10: Unable to assign property IsLoaded. Property is read only	img:10: Unable to assign property ContentImageSize. Property is read only"), "Image / ImageRectOffset / ImageRectSize / Enum.ScaleType / Enum.ResamplerMode set; IsLoaded / ContentImageSize read-only");
        check(log.has("button			true	true	") && log.has("button set	images/hover.png	images/pressed.png"), "ImageButton: a GuiButton with HoverImage / PressedImage");
        check(log.has("bad	img:15: invalid argument #3 to 'Image' (string expected, got Vector2)	img:15: invalid argument #3 to 'ImageRectSize' (Vector2 expected, got number)	img:15: HoverImage is not a valid member of ImageLabel \"StarterGui.Logo\""), "typed; HoverImage is the button's alone");
        Instance* logo = rt.dataModel().getService("StarterGui")->findFirstChild("Logo");
        rt.hostWrite(logo->id(), "ContentImageSize", Value::vector2(32, 16));
        rt.hostWrite(logo->id(), "IsLoaded", Value::boolean(true));
        rt.step(0.05);
        check(log.has("loaded	true	32, 16"), "the engine writes IsLoaded / ContentImageSize once it read the file; the changed signal fires");
        Log clog; Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime client(co, callbacks(clog));
        client.addPlayer("ada", 42);
        client.runChunk("icon", R"(
            local mouse = game.Players.LocalPlayer:GetMouse()
            print("icon", mouse.Icon)
            mouse.Icon = "images/cursor.png"
            print("icon set", mouse.Icon, select(2, pcall(function() mouse.Icon = Vector2.new() end)))
        )");
        check(clog.has("icon	") && clog.has("icon set	images/cursor.png	icon:5: invalid argument #3 to 'Icon' (string expected, got Vector2)"), "Mouse.Icon: a string, the engine's pointer");
        check(log.errors.empty() && clog.errors.empty(), "no errors");
    }

    section("R36. MeshPart, SpecialMesh / BlockMesh / CylinderMesh");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        rt.runChunk("mesh", R"(
            local mp = Instance.new("MeshPart")
            print("meshpart", mp.MeshId, mp.TextureID, mp.CollisionFidelity.Name, mp.RenderFidelity.Name, mp.DoubleSided, mp.MeshSize, mp:IsA("BasePart"), mp.Size)
            mp.Name = "Rock"; mp.TextureID = "meshes/rock.png"; mp.CollisionFidelity = Enum.CollisionFidelity.Box
            mp.Size = Vector3.new(6, 4.5, 6); mp.Anchored = true; mp.Parent = workspace
            mp:GetPropertyChangedSignal("MeshSize"):Connect(function() print("sized", mp.MeshSize) end)
            print("meshpart set", select(2, pcall(function() mp.MeshId = "meshes/rock.obj" end)), mp.TextureID, mp.CollisionFidelity.Value, select(2, pcall(function() mp.MeshSize = Vector3.new(1, 1, 1) end)))
            local sm = Instance.new("SpecialMesh")
            print("special", sm.MeshType.Name, sm.MeshId, sm.TextureId, sm.Scale, sm.Offset, sm.VertexColor, sm:IsA("FileMesh"), sm:IsA("DataModelMesh"))
            sm.MeshType = Enum.MeshType.FileMesh; sm.MeshId = "meshes/cube.glb"; sm.Scale = Vector3.new(3, 3, 3); sm.Offset = Vector3.new(0, 1, 0)
            local crate = Instance.new("Part"); crate.Name = "Crate"; crate.Parent = workspace
            sm.Parent = crate
            print("special set", sm.MeshType.Value, sm.MeshId, sm.Scale, sm.Offset, sm.Parent.Name)
            local bm, cm = Instance.new("BlockMesh"), Instance.new("CylinderMesh")
            print("bevel", bm.Bevel, bm.Scale, bm:IsA("BevelMesh"), bm:IsA("DataModelMesh"), cm:IsA("DataModelMesh"), bm:IsA("FileMesh"),
                select(2, pcall(function() bm.MeshType = Enum.MeshType.Brick end)))
            print("bad", select(2, pcall(function() mp.CollisionFidelity = Enum.MeshType.Head end)), select(2, pcall(function() sm.Scale = 2 end)),
                select(2, pcall(function() Instance.new("DataModelMesh") end)))
        )");
        check(log.has("meshpart			Default	Automatic	false	0, 0, 0	true	4, 1.2, 2"), "MeshPart defaults: no MeshId / TextureID, CollisionFidelity Default, RenderFidelity Automatic, MeshSize 0");
        check(log.has("meshpart set	mesh:7: Unable to assign property MeshId. Script write access is restricted	meshes/rock.png	2	mesh:7: Unable to assign property MeshSize. Property is read only"), "MeshId is the host's to set; TextureID / Enum.CollisionFidelity set; MeshSize read-only");
        check(log.has("special	Head			1, 1, 1	0, 0, 0	1, 1, 1	true	true") && log.has("special set	5	meshes/cube.glb	3, 3, 3	0, 1, 0	Crate"), "SpecialMesh: a FileMesh, a DataModelMesh; MeshType FileMesh, MeshId, Scale, Offset");
        check(log.has("bevel	0	1, 1, 1	true	true	true	false	mesh:16: MeshType is not a valid member of BlockMesh \"BlockMesh\""), "BlockMesh / CylinderMesh: BevelMeshes with Scale, no MeshType");
        check(log.has("bad	mesh:17: Invalid value for enum CollisionFidelity	mesh:17: invalid argument #3 to 'Scale' (Vector3 expected, got number)	mesh:18: Unable to create an Instance of type \"DataModelMesh\""), "typed; the abstract bases are not creatable");
        Instance* rock = rt.dataModel().workspace()->findFirstChild("Rock");
        check(rock != nullptr, "the MeshPart reached the workspace");
        if (rock) {
            rt.hostWrite(rock->id(), "MeshId", Value::string("meshes/rock.obj"));
            rt.hostWrite(rock->id(), "MeshSize", Value::vector3(2, 1.5, 2));
            rt.step(0.05);
            check(log.has("sized	2, 1.5, 2"), "the engine writes MeshSize once it read the file; the changed signal fires");
        }
        check(log.errors.empty(), "no errors");
    }

    section("R37. SurfaceGui");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        rt.runChunk("sgui", R"(
            local sg = Instance.new("SurfaceGui")
            print("surface", sg.Face.Name, sg.Adornee, sg.CanvasSize, sg.SizingMode.Name, sg.PixelsPerStud, sg.AlwaysOnTop, sg.Brightness, sg.LightInfluence,
                sg.ZOffset, sg.MaxDistance, sg.Enabled, sg.ClipsDescendants, sg:IsA("SurfaceGuiBase"), sg:IsA("LayerCollector"), sg:IsA("GuiObject"))
            local board = Instance.new("Part"); board.Name = "Board"; board.Size = Vector3.new(8, 4, 0.5); board.Parent = workspace
            sg.Face = Enum.NormalId.Top; sg.CanvasSize = Vector2.new(400, 200); sg.SizingMode = Enum.SurfaceGuiSizingMode.PixelsPerStud; sg.PixelsPerStud = 100
            sg.Adornee = board
            local b = Instance.new("TextButton"); b.Name = "Go"; b.Size = UDim2.fromScale(0.5, 0.5); b.Parent = sg
            sg.Parent = board
            print("surface set", sg.Face.Value, sg.CanvasSize, sg.SizingMode.Value, sg.PixelsPerStud, sg.Adornee.Name, sg.Go.Parent.Name, sg:GetFullName())
            print("bad", select(2, pcall(function() sg.Face = "Top" end)), select(2, pcall(function() sg.SizingMode = Enum.NormalId.Top end)),
                select(2, pcall(function() sg.CanvasSize = 800 end)), select(2, pcall(function() Instance.new("SurfaceGuiBase") end)))
        )");
        check(log.has("surface	Front	nil	800, 600	FixedSize	50	false	1	0	0	0	true	false	true	true	false"), "SurfaceGui defaults: Face Front, no Adornee, CanvasSize 800 by 600, FixedSize, 50 a stud, LightInfluence 0, MaxDistance 0 (any distance)");
        check(log.has("surface set	1	400, 200	1	100	Board	Screen	Workspace.Board.Screen") || log.has("surface set	1	400, 200	1	100	Board	SurfaceGui	Workspace.Board.SurfaceGui"), "Face / CanvasSize / SizingMode / PixelsPerStud / Adornee set; GuiObjects go in it");
        check(log.has("bad	nil	sgui:11: Invalid value for enum SurfaceGuiSizingMode	sgui:12: invalid argument #3 to 'CanvasSize' (Vector2 expected, got number)	sgui:12: Unable to create an Instance of type \"SurfaceGuiBase\""), "typed (an enum by its name is fine, as on Roblox); SurfaceGuiBase is not creatable");
        check(log.errors.empty(), "no errors");
    }

    section("R38. PathfindingService");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        rt.runChunk("path", R"(
            local PS = game:GetService("PathfindingService")
            local function part(name, size, pos, props)
                local p = Instance.new("Part"); p.Name = name; p.Size = size; p.Position = pos; p.Anchored = true
                for k, v in pairs(props or {}) do p[k] = v end
                p.Parent = workspace; return p
            end
            part("Floor", Vector3.new(64, 1, 64), Vector3.new(0, -0.5, 0))
            local wall = part("Wall", Vector3.new(20, 6, 1), Vector3.new(0, 3, 0))
            local path = PS:CreatePath({ AgentRadius = 2, AgentHeight = 5, AgentCanJump = true })
            print("path", path.ClassName, path.Status.Name, #path:GetWaypoints(), path:IsA("Path"))
            path:ComputeAsync(Vector3.new(0, 3, -10), Vector3.new(0, 3, 10))
            local wps = path:GetWaypoints()
            local inWall, first, last = false, wps[1], wps[#wps]
            local far = 0
            for _, wp in ipairs(wps) do
                if math.abs(wp.Position.X) < 10.5 and math.abs(wp.Position.Z) < 2.5 then inWall = true end
                far = math.max(far, math.abs(wp.Position.X))
            end
            print("around", path.Status.Name, #wps > 4, inWall, first.Position, last.Position, far >= 12, wps[2].Action.Name, typeof(wps[2].Label))
            local gaps = 0
            for i = 2, #wps do if wps[i].Action ~= Enum.PathWaypointAction.Jump then gaps = math.max(gaps, (wps[i].Position - wps[i - 1].Position).Magnitude) end end
            print("spacing", gaps <= 4.01)
            path:ComputeAsync(Vector3.new(0, 3, -10), Vector3.new(100, 3, 100))
            print("nopath", path.Status.Name, #path:GetWaypoints())
            -- a ledge 5 studs up: a jump reaches it, a walk cannot
            part("Ledge", Vector3.new(8, 5, 8), Vector3.new(20, 2.5, 20))
            path:ComputeAsync(Vector3.new(20, 3, 8), Vector3.new(20, 8, 20))
            local jumps = 0
            for _, wp in ipairs(path:GetWaypoints()) do if wp.Action == Enum.PathWaypointAction.Jump then jumps += 1 end end
            local walker = PS:CreatePath({ AgentCanJump = false })
            walker:ComputeAsync(Vector3.new(20, 3, 8), Vector3.new(20, 8, 20))
            print("jump", path.Status.Name, jumps, walker.Status.Name)
            -- Costs by material: a lava strip across the way, never crossed at math.huge
            part("Lava", Vector3.new(6, 0.2, 40), Vector3.new(-14, 0.1, -10), { Material = Enum.Material.Neon })
            local straight = PS:CreatePath()
            straight:ComputeAsync(Vector3.new(-24, 3, -16), Vector3.new(-4, 3, -16))
            local careful = PS:CreatePath({ Costs = { Neon = math.huge } })
            careful:ComputeAsync(Vector3.new(-24, 3, -16), Vector3.new(-4, 3, -16))
            local function onLava(p)
                for _, wp in ipairs(p:GetWaypoints()) do if math.abs(wp.Position.X + 14) < 3 and math.abs(wp.Position.Z + 10) < 20 then return true end end
                return false
            end
            print("costs", straight.Status.Name, onLava(straight), careful.Status.Name, onLava(careful), #careful:GetWaypoints() > #straight:GetWaypoints())
            -- a PathfindingModifier: a label the Costs know, or PassThrough (the wall is air)
            local mod = Instance.new("PathfindingModifier"); mod.Label = "Danger"; mod.Parent = workspace.Lava
            local labelled = PS:CreatePath({ Costs = { Danger = math.huge } })
            labelled:ComputeAsync(Vector3.new(-24, 3, -16), Vector3.new(-4, 3, -16))
            local ghost = Instance.new("PathfindingModifier"); ghost.PassThrough = true; ghost.Parent = wall
            path:ComputeAsync(Vector3.new(0, 3, -10), Vector3.new(0, 3, 10))
            local through = 0
            for _, wp in ipairs(path:GetWaypoints()) do through = math.max(through, math.abs(wp.Position.X)) end
            print("modifier", labelled.Status.Name, onLava(labelled), path.Status.Name, through < 1, mod.PassThrough, mod.ModifierEnabled)
            ghost:Destroy()
            print("occupied", path:CheckOccupancyAsync(Vector3.new(0, 1, 0)), path:CheckOccupancyAsync(Vector3.new(0, 1, -10)))
            print("legacy", PS:FindPathAsync(Vector3.new(0, 3, -10), Vector3.new(0, 3, 10)).Status.Name)
            print("bad", select(2, pcall(function() Instance.new("Path") end)), select(2, pcall(function() path.Status = Enum.PathStatus.Success end)),
                select(2, pcall(function() path:ComputeAsync(1, 2) end)))
            -- Blocked: a part lands on a computed path's waypoint
            path:ComputeAsync(Vector3.new(-24, 3, -20), Vector3.new(24, 3, -20))
            path.Blocked:Connect(function(i) print("blocked", i, path:GetWaypoints()[i].Position.Z) end)
            path.Unblocked:Connect(function(i) print("unblocked", i) end)
            task.delay(0.1, function() part("Rock", Vector3.new(4, 4, 4), Vector3.new(0, 2, -20)) end)
            task.delay(1.2, function() workspace.Rock:Destroy() end)
        )");
        for (int i = 0; i < 40; i++) rt.step(0.05);
        check(log.has("path	Path	NoPath	0	true"), "CreatePath: a Path, Status NoPath and no waypoints until computed");
        check(log.has("around	Success	true	false	0, 0, -10	0, 0, 10	true	Walk	string"), "ComputeAsync goes round the wall: Success, waypoints from start to goal, none in the wall, Action Walk, Label a string");
        check(log.has("spacing	true"), "waypoints at most WaypointSpacing (4) studs apart");
        check(log.has("nopath	NoPath	0"), "a goal off the floor: NoPath, no waypoints");
        check(log.has("jump	Success	1	NoPath"), "a 5-stud ledge: reached with one Jump waypoint when AgentCanJump, NoPath when not");
        check(log.has("costs	Success	true	Success	false	true"), "Costs by material: the lava is crossed at cost 1, walked round at math.huge");
        check(log.has("modifier	Success	false	Success	true	false	true"), "a PathfindingModifier's Label is a cost too; PassThrough makes the wall air");
        check(log.has("occupied	true	false"), "CheckOccupancyAsync: inside the wall, on the open floor");
        check(log.has("legacy	Success"), "FindPathAsync: the older one-shot Path");
        check(log.has("bad	path:57: Unable to create an Instance of type \"Path\"	path:57: Unable to assign property Status. Property is read only	path:58: invalid argument #2 to 'ComputeAsync' (vector expected, got number)"), "typed; Path is not creatable, Status read only");
        int blockedAt = 0;
        for (auto& l : log.lines) if (l.rfind("blocked	", 0) == 0) blockedAt = std::atoi(l.c_str() + 8);
        check(blockedAt > 1 && log.has("blocked	" + std::to_string(blockedAt) + "	-20") && log.has("unblocked	" + std::to_string(blockedAt)), "Blocked fires with the index of the waypoint a part landed on, Unblocked once it is gone");
        check(log.errors.empty(), "no errors");
    }

    section("R39. Font faces: the Font datatype, FontFace and Enum.Font coupled");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        rt.runChunk("fonts", R"(
            local w = Enum.FontWeight
            print("enums", w.Bold.Value, w.Thin.Value, w.Heavy.Value, Enum.FontStyle.Italic.Value, Enum.Font.Unknown.Value)
            local f = Font.new("rbxasset://fonts/families/GothamSSm.json", Enum.FontWeight.Bold)
            print("font", f.Family, f.Weight.Name, f.Style.Name, f.Bold, tostring(f), typeof(f))
            local g = Font.fromEnum(Enum.Font.SourceSansItalic)
            local id = Font.fromId(123, "Light", Enum.FontStyle.Italic)
            print("from", g.Family, g.Weight.Name, g.Style.Name, Font.fromName("Arial", Enum.FontWeight.Bold).Family, id.Family, id.Weight.Name, id.Style.Name)
            print("eq", f == Font.fromEnum(Enum.Font.GothamBold), f == g, f ~= Font.new("rbxasset://fonts/families/GothamSSm.json", Enum.FontWeight.Bold, Enum.FontStyle.Italic))
            f.Bold = false; g.Weight = Enum.FontWeight.Heavy; g.Style = "Normal"; g.Family = "x"
            print("mut", f.Weight.Name, f.Bold, g.Weight.Value, g.Style.Name, g.Family)
            local label = Instance.new("TextLabel")
            print("default", label.Font.Name, label.FontFace.Family, label.FontFace.Weight.Name, label.FontFace.Style.Name)
            local changes = {}
            label:GetPropertyChangedSignal("FontFace"):Connect(function() table.insert(changes, "face") end)
            label:GetPropertyChangedSignal("Font"):Connect(function() table.insert(changes, "font") end)
            label.Font = Enum.Font.GothamBold
            print("couple", label.FontFace.Family, label.FontFace.Weight.Name, label.FontFace.Bold)
            label.FontFace = Font.fromEnum(Enum.Font.Code)
            print("couple2", label.Font.Name, label.FontFace.Family)
            label.FontFace = Font.new("rbxasset://fonts/families/GothamSSm.json", Enum.FontWeight.Thin)
            local face = label.FontFace; face.Weight = Enum.FontWeight.Bold
            print("unknown", label.Font.Name, label.FontFace.Weight.Name, #changes, table.concat(changes, ","))
            label.Font = Enum.Font.SourceSansItalic
            print("italic", label.FontFace.Style.Name, label.FontFace.Weight.Name, label.Font.Name)
            local box = Instance.new("TextBox"); box.Font = "Code"
            local button = Instance.new("TextButton"); button.FontFace = Font.fromEnum(Enum.Font.Arcade)
            print("kinds", box.FontFace.Family, button.Font.Name)
            print("bad", select(2, pcall(function() label.FontFace = 5 end)), select(2, pcall(function() Font.new() end)),
                select(2, pcall(function() Font.new("a", "Fat") end)), select(2, pcall(function() f.Size = 3 end)))
        )");
        check(log.has("enums	700	100	900	1	100"), "Enum.FontWeight (100..900), Enum.FontStyle, Enum.Font.Unknown");
        check(log.has("font	rbxasset://fonts/families/GothamSSm.json	Bold	Normal	true	Font { Family = rbxasset://fonts/families/GothamSSm.json, Weight = Bold, Style = Normal }	Font"), "Font.new(family, weight, style): Family / Weight / Style / Bold, tostring, typeof");
        check(log.has("from	rbxasset://fonts/families/SourceSansPro.json	Regular	Italic	rbxasset://fonts/families/Arial.json	rbxassetid://123	Light	Italic"), "Font.fromEnum / fromName / fromId; weights and styles by name too");
        check(log.has("eq	true	false	true"), "equal by family, weight and style");
        check(log.has("mut	Regular	false	900	Normal	x"), "Bold / Weight / Style / Family assignable");
        check(log.has("default	SourceSans	rbxasset://fonts/families/SourceSansPro.json	Regular	Normal"), "a TextLabel's FontFace defaults to SourceSansPro Regular, its Font to SourceSans");
        check(log.has("couple	rbxasset://fonts/families/GothamSSm.json	Bold	true"), "Font = GothamBold: FontFace is GothamSSm at Bold");
        check(log.has("couple2	Code	rbxasset://fonts/families/Inconsolata.json"), "FontFace = Inconsolata: Font is Code");
        check(log.has("unknown	Unknown	Thin	6	face,font,font,face,font,face"), "a face no Enum.Font names is Font.Unknown; a FontFace read is a copy; both properties' Changed fire");
        check(log.has("italic	Italic	Regular	SourceSansItalic"), "SourceSansItalic is SourceSansPro Regular Italic");
        check(log.has("kinds	rbxasset://fonts/families/Inconsolata.json	Arcade"), "a TextBox and a TextButton the same");
        check(log.has("bad	fonts:29: invalid argument #3 to 'FontFace' (Font expected, got number)	fonts:29: missing argument #1 to 'Font.new' (string expected)	fonts:30: Invalid value for enum FontWeight	fonts:30: Size is not a valid member of Font"), "typed");
        std::string err;
        Instance* a = loadSourceFile(rt, "Workspace/A.model.json", R"JSON({"className": "TextLabel", "properties": {"FontFace": {"Font": {"family": "rbxasset://fonts/families/GothamSSm.json", "weight": "Bold", "style": "Italic"}}}})JSON", &err);
        Instance* b = loadSourceFile(rt, "Workspace/B.model.json", R"JSON({"className": "TextLabel", "properties": {"Font": "GothamBold"}})JSON", &err);
        Instance* c = loadSourceFile(rt, "Workspace/C.model.json", R"JSON({"className": "TextLabel", "properties": {"FontFace": "Code", "Text": "hi"}})JSON", &err);
        check(a && a->get("FontFace") == Value::font("rbxasset://fonts/families/GothamSSm.json", 700, true) && a->get("Font").s == "Unknown", ("a model.json's FontFace: Rojo's {family, weight, style}: " + err).c_str());
        check(b && b->get("FontFace") == Value::font("rbxasset://fonts/families/GothamSSm.json", 700, false), "and a Font enum in one sets the FontFace");
        check(c && c->get("Font").s == "Code" && c->get("FontFace").s == "rbxasset://fonts/families/Inconsolata.json", "or an Enum.Font name as the FontFace");
        check(log.errors.empty(), "no errors");
    }

    section("R40. The escape menu: GuiService.MenuIsOpen, Reset Character, SetCore ResetButtonCallback / TopbarEnabled / ChatActive");
    {
        Log slog, alog;
        Runtime::Options co = serverOpts(); co.isServer = false;
        Runtime server(serverOpts(), callbacks(slog));
        Runtime alice(co, callbacks(alog));
        server.runChunk("listen", R"(
            game.Players.PlayerAdded:Connect(function(p)
                p.CharacterAdded:Connect(function(c)
                    c:WaitForChild("Humanoid").Died:Connect(function() print("died", p.Name) end)
                end)
            end)
            print("server", pcall(function() game.StarterGui:SetCore("TopbarEnabled", false) end))
        )");
        LocalSession session(server);
        session.join(alice, "Alice", 16);
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        alice.runChunk("watch", R"(
            local gs = game:GetService("GuiService")
            gs.MenuOpened:Connect(function() print("menu", "opened", gs.MenuIsOpen) end)
            gs.MenuClosed:Connect(function() print("menu", "closed", gs.MenuIsOpen) end)
            print("menu", "start", gs.MenuIsOpen, pcall(function() gs.MenuIsOpen = true end))
            local sg = game.StarterGui
            print("core", sg:GetCore("ChatActive"), sg:GetCore("TopbarEnabled"))
            sg:SetCore("ChatActive", false); sg:SetCore("TopbarEnabled", false)
            print("core2", sg:GetCore("ChatActive"), sg:GetCore("TopbarEnabled"), select(2, pcall(function() sg:GetCore("Nope") end)))
            print("bad", select(2, pcall(function() sg:SetCore("ResetButtonCallback", 5) end)))
        )");
        alice.menu(true); alice.menu(true); alice.menu(false);
        check(alog.has("menu	start	false	false	watch:5: Unable to assign property MenuIsOpen. Property is read only")
              && alog.has("menu	opened	true") && alog.has("menu	closed	false") && !alog.has("menu	opened	false"),
              "GuiService.MenuIsOpen (read-only) with MenuOpened / MenuClosed as the engine's menu goes; opening twice fires once");
        check(alog.has("core	true	true") && alog.has("core2	false	false	watch:9: GetCore: Nope is not a known core"),
              "SetCore / GetCore ChatActive and TopbarEnabled");
        check(alog.has("bad	watch:10: SetCore: ResetButtonCallback expects a BindableEvent or a boolean") && slog.has("server	false	listen:7: SetCore can only be called from a LocalScript"),
              "typed, client only");
        // Reset Character: the server kills the character, then respawns it
        alice.resetCharacter();
        for (int i = 0; i < 3; i++) session.stepAll(0.05);
        Instance* aliceP = server.dataModel().getService("Players")->findFirstChild("Alice");
        Instance* character = aliceP ? server.dataModel().find(aliceP->get("Character").ref) : nullptr;
        Instance* hum = character ? character->findFirstChildOfClass("Humanoid") : nullptr;
        check(slog.has("died	Alice") && hum && hum->get("Health").n <= 0, "Reset Character: the server sets the Humanoid's Health to 0, Died fires");
        // ResetButtonCallback: a BindableEvent takes the button over; false disables it
        alice.runChunk("hook", R"(
            local b = Instance.new("BindableEvent")
            b.Event:Connect(function() print("reset", "hooked") end)
            game.StarterGui:SetCore("ResetButtonCallback", b)
        )");
        size_t before = slog.lines.size();
        alice.resetCharacter();
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        alice.runChunk("off", R"( game.StarterGui:SetCore("ResetButtonCallback", false) )");
        alice.resetCharacter();
        for (int i = 0; i < 2; i++) session.stepAll(0.05);
        check(alog.has("reset	hooked") && slog.lines.size() == before, "a ResetButtonCallback BindableEvent fires instead of a reset; false: the button does nothing");
        check(slog.errors.empty() && alog.errors.empty(), "no errors");
    }

    section("R41. *.rbxmx: Studio's XML model files load like a *.model.json");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        SourceFileInfo fi;
        check(classifySourceFile("Workspace/Map/Shrine.rbxmx", fi) && fi.kind == SourceFileInfo::Model && fi.rbxmx && fi.name == "Shrine" && fi.containers.size() == 2, "x.rbxmx is a model file named x");
        // As Studio writes it: serialized names (size, shape, Color3uint8), tokens, a CoordinateFrame, CDATA source, Refs, internals
        const char* shrine = R"XML(<?xml version="1.0" encoding="utf-8"?>
<!-- saved from Studio -->
<roblox xmlns:xmime="http://www.w3.org/2005/05/xmlmime" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://www.roblox.com/roblox.xsd" version="4">
  <Meta name="ExplicitAutoJoints">true</Meta>
  <External>null</External>
  <Item class="Model" referent="RBX0">
    <Properties>
      <string name="Name">ShrineModel</string>
      <Ref name="PrimaryPart">RBX1</Ref>
      <CoordinateFrame name="WorldPivotData"><X>0</X><Y>0</Y><Z>0</Z></CoordinateFrame>
      <BinaryString name="AttributesSerialize">BAAAAAoAAABNYXhQbGF5ZXJzBgAAAAAAACBABAAAAE9wZW4DAQUAAABNb3R0bwIFAAAAcHJheSEGAAAAT2Zmc2V0EQAAgD8AAABAAABAQA==</BinaryString>
      <UniqueId name="UniqueId">44b5d0f00b8e6f1e0353a94c00000001</UniqueId>
    </Properties>
    <Item class="Part" referent="RBX1">
      <Properties>
        <string name="Name">Altar</string>
        <bool name="Anchored">true</bool>
        <bool name="CanCollide">true</bool>
        <CoordinateFrame name="CFrame"><X>4</X><Y>2.5</Y><Z>-3</Z><R00>0</R00><R01>0</R01><R02>1</R02><R10>0</R10><R11>1</R11><R12>0</R12><R20>-1</R20><R21>0</R21><R22>0</R22></CoordinateFrame>
        <Color3uint8 name="Color3uint8">4288914016</Color3uint8>
        <token name="Material">288</token>
        <float name="Transparency">0.25</float>
        <token name="formFactorRaw">1</token>
        <token name="shape">0</token>
        <Vector3 name="size"><X>4</X><Y>1</Y><Z>4</Z></Vector3>
        <token name="TopSurface">0</token>
        <string name="Tags"></string>
      </Properties>
      <Item class="ProximityPrompt" referent="RBX2">
        <Properties>
          <string name="Name">Pray</string>
          <string name="ActionText">Pray &amp; &lt;wait&gt;</string>
          <token name="KeyboardKeyCode">101</token>
        </Properties>
      </Item>
      <Item class="SurfaceAppearance" referent="RBX9">
        <Properties><string name="Name">Weld</string><Ref name="Part0">RBX1</Ref></Properties>
      </Item>
      <Item class="CurveAnimation" referent="RBX10">
        <Properties><string name="Name">Spine</string></Properties>
      </Item>
    </Item>
    <Item class="Script" referent="RBX3">
      <Properties>
        <string name="Name">Bless</string>
        <ProtectedString name="Source"><![CDATA[print("bless", script.Parent.Name, script.Parent.PrimaryPart.Name, script.Parent.Target.Value.Name)
if 1 < 2 then print("shape", script.Parent.PrimaryPart.Shape, script.Parent.PrimaryPart.Material) end]]></ProtectedString>
        <bool name="Disabled">false</bool>
      </Properties>
    </Item>
    <Item class="ObjectValue" referent="RBX4">
      <Properties><string name="Name">Target</string><Ref name="Value">RBX1</Ref></Properties>
    </Item>
    <Item class="StringValue" referent="RBX5">
      <Properties><string name="Name">Motto</string><string name="Value">Be &quot;kind&quot;</string></Properties>
    </Item>
    <Item class="TextLabel" referent="RBX6">
      <Properties>
        <string name="Name">Sign</string>
        <UDim2 name="Size"><XS>0.5</XS><XO>0</XO><YS>0</YS><YO>40</YO></UDim2>
        <Color3 name="TextColor3"><R>1</R><G>0.5</G><B>0</B></Color3>
        <Font name="FontFace"><Family><url>rbxasset://fonts/families/GothamSSm.json</url></Family><Weight>700</Weight><Style>Normal</Style><CachedFaceId><url></url></CachedFaceId></Font>
        <string name="Text">Hi</string>
      </Properties>
    </Item>
  </Item>
</roblox>)XML";
        Instance* model = loadSourceFile(rt, "Workspace/Map/Shrine.rbxmx", shrine, &err);
        check(model && model->className() == "Model" && model->name() == "Shrine" && model->fullName() == "Workspace.Map.Shrine", "one root Item is the instance, named after the file");
        Instance* altar = model ? model->findFirstChild("Altar") : nullptr;
        check(altar && altar->className() == "Part" && altar->get("Anchored").b && altar->get("Size").v.x == 4 && altar->get("Size").v.y == 1 && altar->get("Shape").s == "Ball" && altar->get("Material").s == "Neon" && altar->get("Transparency").n == 0.25,
              "size / shape / Material tokens / floats by their serialized names");
        check(altar && altar->get("Position").v.x == 4 && altar->get("Position").v.y == 2.5f && altar->get("Position").v.z == -3 && std::fabs(altar->get("Orientation").v.y - 90) < 0.01f, "a CoordinateFrame becomes Position + Orientation");
        check(altar && std::fabs(altar->get("Color").c.r - 163 / 255.f) < 0.005f && std::fabs(altar->get("Color").c.g - 162 / 255.f) < 0.005f && std::fabs(altar->get("Color").c.b - 96 / 255.f) < 0.005f, "Color3uint8 (one packed integer) is the Color");
        check(model && model->get("PrimaryPart").ref == altar->id() && model->findFirstChild("Target") && model->findFirstChild("Target")->get("Value").ref == altar->id(), "Refs resolve by referent: PrimaryPart, an ObjectValue's Value");
        Instance* pray = altar ? altar->findFirstChild("Pray") : nullptr;
        check(pray && pray->className() == "ProximityPrompt" && pray->get("ActionText").s == "Pray & <wait>" && pray->get("KeyboardKeyCode").s == "E", "entities and enum tokens under it");
        check(model->findFirstChild("Motto") && model->findFirstChild("Motto")->get("Value").s == "Be \"kind\"", "&quot; in a string");
        Instance* weld = altar ? altar->findFirstChild("Weld") : nullptr;
        check(weld && weld->className() == "SurfaceAppearance", "SurfaceAppearance loads as itself (the runtime carries it now)");
        Instance* bone = altar ? altar->findFirstChild("Spine") : nullptr;
        check(bone && bone->className() == "Folder", "a class this runtime lacks (CurveAnimation) still loads as a Folder, so the rest of the model survives");
        Instance* sign = model->findFirstChild("Sign");
        check(sign && sign->get("Size").u[0] == 0.5f && sign->get("Size").u[3] == 40 && sign->get("TextColor3").c.g == 0.5f && sign->get("FontFace").s == "rbxasset://fonts/families/GothamSSm.json" && sign->get("FontFace").n == 700 && !sign->get("FontFace").b && sign->get("Font").s == "GothamBold",
              "UDim2, Color3, a Font face (coupled to Enum.Font)");
        rt.startScripts();
        rt.step(0.05);
        check(log.has("bless	Shrine	Altar	Altar") && log.has("shape	Enum.PartType.Ball	Enum.Material.Neon"), "its Script runs from the CDATA Source");
        check(model->attributes().size() == 4 && model->attributes().at("MaxPlayers").n == 8 && model->attributes().at("Open").b && model->attributes().at("Motto").s == "pray!" && model->attributes().at("Offset").v.z == 3,
              "AttributesSerialize (Studio's base64 blob) becomes the attributes: a number, a boolean, a string, a Vector3");
        check(log.errors.empty(), "internals (WorldPivotData, UniqueId, formFactorRaw, Tags) are skipped quietly");
        // several root Items sit in a Folder named after the file; a saved file replaces the tree; unloading removes it
        Instance* two = loadSourceFile(rt, "ReplicatedStorage/Props.rbxmx", R"XML(<roblox version="4"><Item class="Part" referent="A"><Properties><string name="Name">One</string></Properties></Item><Item class="Part" referent="B"><Properties><string name="Name">Two</string></Properties></Item></roblox>)XML", &err);
        check(two && two->className() == "Folder" && two->name() == "Props" && two->findFirstChild("One") && two->findFirstChild("Two"), "several root Items: a Folder named after the file holds them");
        Instance* again = loadSourceFile(rt, "ReplicatedStorage/Props.rbxmx", R"XML(<roblox version="4"><Item class="Part" referent="A"><Properties><string name="Name">Three</string></Properties></Item></roblox>)XML", &err);
        check(again && again != two && two->destroyed() && again->className() == "Part" && again->name() == "Props", "saving the file again rebuilds it");
        check(unloadSourceFile(rt, "ReplicatedStorage/Props.rbxmx") && again->destroyed(), "deleting it removes the tree");
        check(!loadSourceFile(rt, "Workspace/Bad.rbxmx", "<roblox><Item class=\"Part\"></roblox>", &err) && err == "</roblox> closes <Item> at offset 29", "malformed XML is an error");
        check(!loadSourceFile(rt, "Workspace/Bad.rbxmx", "<html></html>", &err) && err == "not a Roblox model: the root is <html>", "and so is a file that is not a model");
    }

    section("R42. *.rbxm: the binary model files Studio and Rojo write load too");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        SourceFileInfo fi;
        check(classifySourceFile("Workspace/Map/Props.rbxm", fi) && fi.kind == SourceFileInfo::Model && fi.rbxm && !fi.rbxmx && fi.name == "Props" && fi.containers.size() == 2, "x.rbxm is a model file named x");
        // the sync channel carries text, so a .rbxm's source is its base64
        auto fixture = [](const char* name) {
            std::string path = std::string("test/fixtures/") + name;
            FILE* f = fopen(path.c_str(), "rb");
            std::string bytes;
            if (f) { char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) bytes.append(buf, n); fclose(f); }
            static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            for (size_t i = 0; i < bytes.size(); i += 3) {
                unsigned v = (unsigned char)bytes[i] << 16;
                if (i + 1 < bytes.size()) v |= (unsigned char)bytes[i + 1] << 8;
                if (i + 2 < bytes.size()) v |= (unsigned char)bytes[i + 2];
                out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
                out += i + 1 < bytes.size() ? tbl[(v >> 6) & 63] : (char)61;
                out += i + 2 < bytes.size() ? tbl[v & 63] : (char)61;
            }
            return out;
        };
        // rbx-dom's three-unique-parts: three Parts (LZ4 chunks, interleaved arrays, CFrame rotation ids, Color3uint8 planes, tokens)
        Instance* props = loadSourceFile(rt, "Workspace/Map/Props.rbxm", fixture("three-unique-parts.rbxm"), &err);
        check(props && props->className() == "Folder" && props->name() == "Props" && props->children().size() == 3, err.empty() ? "three root instances: a Folder named after the file holds them" : ("load failed: " + err).c_str());
        Instance* brush = props ? props->findFirstChild("Brush your teeth") : nullptr;
        Instance* live = props ? props->findFirstChild("Live wildly") : nullptr;
        check(brush && brush->className() == "Part" && brush->get("Size").v.x == 1 && brush->get("Size").v.y == 2 && brush->get("Size").v.z == 3 && live && live->get("Size").v.z == 9, "size by its serialized name, an interleaved float array");
        check(brush && brush->get("Position").v.x == -5.5f && brush->get("Position").v.y == 4 && brush->get("Position").v.z == -12.5f && brush->get("Orientation").v.y == 0, "a CFrame with a rotation id: position, no rotation");
        check(brush && brush->get("Color").c.r == 0 && brush->get("Color").c.g == 1 && brush->get("Color").c.b == 1 && live && std::fabs(live->get("Color").c.b - 191 / 255.f) < 0.005f, "Color3uint8: the red, green and blue planes");
        check(brush && brush->get("Material").s == "Plastic" && brush->get("Shape").s == "Block" && brush->get("CanCollide").b && !brush->get("Anchored").b && brush->get("Transparency").n == 0, "tokens, bools and floats");
        // ref-parent: an ObjectValue pointing at its parent
        Instance* target = loadSourceFile(rt, "ReplicatedStorage/RefParent.rbxm", fixture("ref-parent.rbxm"), &err);
        check(target && target->className() == "Folder" && target->name() == "RefParent" && target->findFirstChild("Value") && target->findFirstChild("Value")->get("Value").ref == target->id(), "one root: the instance, named after the file; a Ref resolves by referent");
        // default-inserted-modulescript: its Source, and it can be required
        Instance* mod = loadSourceFile(rt, "ReplicatedStorage/Mod.rbxm", fixture("default-inserted-modulescript.rbxm"), &err);
        check(mod && mod->className() == "ModuleScript" && mod->get("Source").s == "local module = {}\n\nreturn module\n", "a ModuleScript keeps its Source");
        loadSourceFile(rt, "ServerScriptService/Use.server.luau", "print(\"mod\", type(require(game.ReplicatedStorage.Mod)))", &err);
        // attributes: every attribute type Studio writes, in one blob
        Instance* attrs = loadSourceFile(rt, "ReplicatedStorage/Attrs.rbxm", fixture("attributes.rbxm"), &err);
        const auto& a = attrs->attributes();
        check(attrs && a.size() == 11 && a.at("String").s == "Hello, world!" && a.at("Boolean").b && a.at("Number").n == 12345 && std::isnan(a.at("NaN").n) && std::isinf(a.at("Infinity").n),
              "strings, booleans, numbers (NaN and Infinity too)");
        check(attrs && a.at("Vector3").v.z == 3 && a.at("Vector2").type == Value::Vector2 && a.at("Vector2").v.y == 50 && a.at("UDim").type == Value::UDim && a.at("UDim").u[1] == 100 && a.at("UDim2").type == Value::UDim2 && a.at("UDim2").u[3] == 30,
              "Vector3, Vector2, UDim, UDim2");
        check(attrs && a.at("Color3").type == Value::Color3 && std::fabs(a.at("Color3").c.r - 0.635f) < 0.001f && a.at("Color3").c.b == 1 && a.at("BrickColor").type == Value::Color3 && a.at("BrickColor").c.r == 1 && a.at("BrickColor").c.g == 0, "Color3, and a BrickColor (1004, Really red) as its Color3");
        check(attrs && !a.count("NumberSequence") && !a.count("ColorSequence") && !a.count("Rect") && !a.count("NumberRange"), "sequences, Rects and ranges are stepped over, not kept");
        Instance* fa = loadSourceFile(rt, "ReplicatedStorage/FontAttr.rbxm", fixture("folder-with-font-attribute.rbxm"), &err);
        check(fa && fa->attributes().count("AFontAttribute") && fa->attributes().at("AFontAttribute").type == Value::Font && fa->attributes().at("AFontAttribute").s == "rbxasset://fonts/families/Creepster.json" && fa->attributes().at("AFontAttribute").n == 400, "a Font attribute");
        // text-label-with-font: a gui object's UDim2s, Color3s, a FontFace
        Instance* label = loadSourceFile(rt, "StarterGui/Label.rbxm", fixture("text-label-with-font.rbxm"), &err);
        check(label && label->className() == "TextLabel" && label->get("Text").s == "My Text" && label->get("Size").u[1] == 200 && label->get("Size").u[3] == 50 && label->get("TextXAlignment").s == "Center" && label->get("BackgroundColor3").c.g == 1 && label->get("TextSize").n == 14, "UDim2, Color3, tokens on a TextLabel");
        check(label && label->get("FontFace").s == "rbxasset://fonts/families/RobotoMono.json" && label->get("FontFace").n == 700 && label->get("FontFace").b, "a FontFace: family, weight, italic");
        rt.startScripts();
        rt.step(0.05);
        check(log.has("mod	table"), "require() of the loaded ModuleScript works");
        check(log.errors.empty(), "the properties this runtime lacks are skipped quietly");
        // saving again rebuilds; deleting removes; an rbxmx renamed .rbxm still loads; bad files are errors
        Instance* again = loadSourceFile(rt, "Workspace/Map/Props.rbxm", fixture("ref-parent.rbxm"), &err);
        check(again && again != props && props->destroyed() && again->className() == "Folder" && again->findFirstChild("Value"), "saving the file again rebuilds it");
        check(unloadSourceFile(rt, "Workspace/Map/Props.rbxm") && again->destroyed(), "deleting it removes the tree");
        Instance* renamed = loadSourceFile(rt, "Workspace/Renamed.rbxm", "PHJvYmxveCB2ZXJzaW9uPSI0Ij48SXRlbSBjbGFzcz0iUGFydCIgcmVmZXJlbnQ9IkEiPjxQcm9wZXJ0aWVzPjxzdHJpbmcgbmFtZT0iTmFtZSI+T25lPC9zdHJpbmc+PC9Qcm9wZXJ0aWVzPjwvSXRlbT48L3JvYmxveD4=", &err);
        check(renamed && renamed->className() == "Part" && renamed->name() == "Renamed", "an XML model saved with the .rbxm extension loads as one");
        check(!loadSourceFile(rt, "Workspace/Bad.rbxm", "aGVsbG8gd29ybGQsIHRoaXMgaXMgbm90IGEgbW9kZWwgZmlsZSBhdCBhbGwh", &err) && err == "not a Roblox binary model (no <roblox! header)", "a file that is not a model is an error");
        std::string zstd = std::string("<roblox!\x89\xff\r\n\x1a\n", 14) + std::string(18, (char)0) + "INST" + std::string("\x08\x00\x00\x00\x40\x00\x00\x00", 8) + std::string(4, (char)0) + std::string("\x28\xb5\x2f\xfd\x00\x00\x00\x00", 8);
        static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string zb;
        for (size_t i = 0; i + 2 < zstd.size() + 2; i += 3) {
            unsigned v = (unsigned char)zstd[i] << 16;
            if (i + 1 < zstd.size()) v |= (unsigned char)zstd[i + 1] << 8;
            if (i + 2 < zstd.size()) v |= (unsigned char)zstd[i + 2];
            zb += tbl[(v >> 18) & 63]; zb += tbl[(v >> 12) & 63];
            zb += i + 1 < zstd.size() ? tbl[(v >> 6) & 63] : (char)61;
            zb += i + 2 < zstd.size() ? tbl[v & 63] : (char)61;
        }
        // The standalone harness installs no zstd decoder (Godot does, via setZstdDecoder),
        // so here a zstd chunk must say so and name the way round it.
        check(!loadSourceFile(rt, "Workspace/New.rbxm", zb, &err) && err == "a zstd-compressed INST chunk and no zstd here: save the model as .rbxmx instead", ("a zstd chunk says what to do when no zstd is installed: " + err).c_str());
        check(!loadSourceFile(rt, "Workspace/Cut.rbxm", "PHJvYmxveCGJ/w0KGgoAAAAAAAAAAAAAAAAAAAAAAABJTlNU", &err) && err == "truncated file", "a truncated file is an error");
    }

    section("R43. Lighting's children: post-processing effects, the Atmosphere, the Sky");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Fx.server.luau", R"(
            local L = game:GetService("Lighting")
            local bloom = Instance.new("BloomEffect")
            print("bloom", bloom.Enabled, bloom.Intensity, bloom.Size, bloom.Threshold, bloom:IsA("PostEffect"))
            bloom.Intensity = 1
            bloom.Parent = L
            local cc = Instance.new("ColorCorrectionEffect", L)
            print("cc", cc.Brightness, cc.Contrast, cc.Saturation, cc.TintColor)
            cc.Saturation = -1
            cc.TintColor = Color3.fromRGB(255, 200, 150)
            local dof = Instance.new("DepthOfFieldEffect", L)
            print("dof", dof.FarIntensity, dof.FocusDistance, dof.InFocusRadius, dof.NearIntensity)
            local blur = Instance.new("BlurEffect", workspace.CurrentCamera)
            print("blur", blur.Size, blur.Parent == workspace.CurrentCamera)
            local rays = Instance.new("SunRaysEffect", L)
            print("rays", rays.Intensity, rays.Spread)
            local atmo = Instance.new("Atmosphere", L)
            print("atmo", atmo.Density, atmo.Offset, atmo.Color, atmo.Decay, atmo.Glare, atmo.Haze)
            local sky = Instance.new("Sky", L)
            print("sky", sky.SunAngularSize, sky.MoonAngularSize, sky.StarCount, sky.CelestialBodiesShown, sky.SkyboxFt)
            sky.SkyboxFt = "rbxassetid://12345"
            print("children", #L:GetChildren(), L:FindFirstChildOfClass("Atmosphere") == atmo, L:FindFirstChildWhichIsA("PostEffect") == bloom)
            print("new", pcall(Instance.new, "PostEffect"))
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("bloom	true	0.4	24	0.95	true"), "BloomEffect: Studio's defaults, a PostEffect");
        check(log.has("cc	0	0	0	1, 1, 1"), "ColorCorrectionEffect: offsets of 0, a white tint");
        check(log.has("dof	0.75	0.05	10	0.75"), "DepthOfFieldEffect");
        check(log.has("blur	24	true") && log.has("rays	0.25	1"), "BlurEffect (under the camera too), SunRaysEffect");
        check(log.has("atmo	0.395	0	0.780392, 0.666667, 0.419608	0.415686, 0.439216, 0.490196	0	0"), "Atmosphere: Studio's haze");
        check(log.has("sky	21	11	3000	true	"), "Sky: the sun and moon's sizes, the star count, six skybox faces");
        check(log.has("children	6	true	true"), "they are Lighting's children");
        check(log.has("new	false	Unable to create an Instance of type \"PostEffect\""), "PostEffect itself is abstract");
        // the engine hears each as a Create with its class and every property change
        int creates = 0, props = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && (c.className == "BloomEffect" || c.className == "Atmosphere" || c.className == "Sky")) creates++;
            if (c.kind == Change::Property && (c.name == "Saturation" || c.name == "TintColor" || c.name == "SkyboxFt")) props++;
        }
        check(creates == 3 && props == 3, "the engine hears the effects' creation and their properties");
        check(log.errors.empty(), "no errors");
    }

    section("R44. Joints: Weld / Motor6D / WeldConstraint, Attachments, C0 / C1 as CFrames");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        const char* rig = R"XML(<roblox version="4">
  <Item class="Model" referent="RBX0">
    <Properties><string name="Name">Door</string></Properties>
    <Item class="Part" referent="RBX1">
      <Properties><string name="Name">Frame</string><bool name="Anchored">true</bool>
        <CoordinateFrame name="CFrame"><X>10</X><Y>5</Y><Z>0</Z></CoordinateFrame></Properties>
      <Item class="Motor6D" referent="RBX3">
        <Properties><string name="Name">Hinge</string><Ref name="Part0">RBX1</Ref><Ref name="Part1">RBX2</Ref>
          <CoordinateFrame name="C0"><X>2</X><Y>0</Y><Z>0</Z></CoordinateFrame>
          <CoordinateFrame name="C1"><X>-2</X><Y>0</Y><Z>0</Z><R00>0</R00><R01>0</R01><R02>1</R02><R10>0</R10><R11>1</R11><R12>0</R12><R20>-1</R20><R21>0</R21><R22>0</R22></CoordinateFrame>
          <float name="MaxVelocity">0.5</float></Properties>
      </Item>
    </Item>
    <Item class="Part" referent="RBX2">
      <Properties><string name="Name">Panel</string>
        <CoordinateFrame name="CFrame"><X>14</X><Y>5</Y><Z>0</Z></CoordinateFrame></Properties>
    </Item>
  </Item>
</roblox>)XML";
        check(loadSourceFile(rt, "Workspace/Door.rbxmx", rig, &err) != nullptr && err.empty(), "a Motor6D in an rbxmx loads (C0 / C1 as CoordinateFrames, Part0 / Part1 as Refs)");
        loadSourceFile(rt, "ServerScriptService/Joints.server.luau", R"(
            local w = Instance.new("Weld")
            print("weld", w.Part0, w.Part1, w.Enabled, w.C0, w.C1, w:IsA("JointInstance"), Instance.new("Snap"):IsA("JointInstance"), Instance.new("ManualWeld"):IsA("JointInstance"))
            local m = Instance.new("Motor6D")
            print("motor", m.CurrentAngle, m.DesiredAngle, m.MaxVelocity, m:IsA("JointInstance"))
            local wc = Instance.new("WeldConstraint")
            print("wc", wc.Part0, wc.Enabled, wc:IsA("JointInstance"), wc:IsA("Constraint"))
            local hinge = workspace.Door.Frame.Hinge
            print("hinge", hinge.Part0.Name, hinge.Part1.Name, hinge.C0.Position, hinge.C1.Position, hinge.C1.LookVector.X, hinge.MaxVelocity)
            hinge.C0 = CFrame.new(2, 1, 0) * CFrame.Angles(0, math.rad(90), 0)
            print("c0", hinge.C0.Position, hinge.C0.LookVector.X, hinge.C0Position)
            hinge:GetPropertyChangedSignal("C0"):Connect(function() print("c0changed", hinge.C0.X) end)
            hinge.DesiredAngle = 1
            task.wait(1 / 30)
            print("angle", hinge.CurrentAngle)
            task.wait(0.5)
            print("angle2", hinge.CurrentAngle)
            local a = Instance.new("Attachment")
            print("att", a.Position, a.Orientation, a.Visible, a.Axis, a.SecondaryAxis)
            a.Position = Vector3.new(0, 3, 0)
            a.Parent = workspace.Door.Frame
            print("world", a.WorldPosition, a.WorldCFrame.Position, a.WorldAxis)
            a.WorldPosition = Vector3.new(10, 5, -4)
            print("back", a.Position, a.WorldPosition)
            local tw = game:GetService("TweenService"):Create(hinge, TweenInfo.new(0.2, Enum.EasingStyle.Linear), {C0 = CFrame.new(4, 1, 0)})
            tw.Completed:Connect(function() print("tweened", hinge.C0.Position) end)
            tw:Play()
            print("bad", select(2, pcall(function() game:GetService("TweenService"):Create(hinge, TweenInfo.new(0.2), {C0 = 5}) end)):match("TweenService.*"))
        )", &err);
        rt.startScripts();
        for (int i = 0; i < 60; i++) rt.step(1.0 / 60);
        check(log.has("weld	nil	nil	true	0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1	0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1	true	true	true"), "Weld / Snap / ManualWeld: JointInstances with identity C0 / C1");
        check(log.has("motor	0	0	0.1	true"), "Motor6D: CurrentAngle, DesiredAngle, MaxVelocity");
        check(log.has("wc	nil	true	false	false"), "WeldConstraint: Part0 / Part1 / Enabled, no C0 (not a JointInstance)");
        check(log.has("hinge	Frame	Panel	2, 0, 0	-2, 0, 0	-1	0.5"), "the file's C0 / C1 / Part0 / Part1 / MaxVelocity");
        check(log.has("c0	2, 1, 0	-1	2, 1, 0"), "C0 is a CFrame: written whole, read whole, its Position also as C0Position");
        check(log.has("angle	0.5") && log.has("angle2	1"), "CurrentAngle chases DesiredAngle by MaxVelocity a frame, and stops there");
        check(log.has("att	0, 0, 0	0, 0, 0	false	1, 0, 0	0, 1, 0"), "Attachment: Position / Orientation / Axis / SecondaryAxis / Visible");
        check(log.has("world	10, 8, 0	10, 8, 0	1, 0, 0"), "WorldPosition / WorldCFrame / WorldAxis: through the parent part's CFrame");
        check(log.has("back	0, 0, -4	10, 5, -4"), "writing WorldPosition sets Position relative to the part");
        check(log.has("c0changed	4") && log.has("tweened	4, 1, 0"), "TweenService tweens C0 and GetPropertyChangedSignal(\"C0\") hears it");
        check(log.has("bad	TweenService:Create property 'C0' expects a CFrame"), "a non-CFrame C0 in a tween is an error");
        // the engine hears the joint's parts and frames
        int part0 = 0, c0 = 0, angle = 0, motor = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && c.className == "Motor6D") motor++;
            if (c.kind == Change::Property && c.name == "Part0" && c.value.type == Value::Ref) part0++;
            if (c.kind == Change::Property && c.name == "C0Position") c0++;
            if (c.kind == Change::Property && c.name == "CurrentAngle") angle++;
        }
        check(motor == 2 && part0 >= 1 && c0 >= 2 && angle >= 2, "the engine hears the Motor6D, its Part0 as a Ref, C0Position, CurrentAngle");
        check(log.errors.empty(), "no errors");
    }

    section("R45. Decal / Texture: an image on a Face of a part");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Decals.server.luau", R"(
            local part = Instance.new("Part", workspace)
            local d = Instance.new("Decal")
            print("decal", d.Face, d.Texture, d.Transparency, d.Color3, d.ZIndex, d:IsA("FaceInstance"))
            d.Texture = "images/sheet.png"
            d.Face = Enum.NormalId.Top
            d.Parent = part
            local t = Instance.new("Texture", part)
            print("texture", t.StudsPerTileU, t.StudsPerTileV, t.OffsetStudsU, t.OffsetStudsV, t:IsA("Decal"), t.Face)
            t.StudsPerTileU = 4
            t.Face = "Right"
            print("faces", d.Face == Enum.NormalId.Top, t.Face.Name, #part:GetChildren(), part:FindFirstChildOfClass("Decal") == d, part:FindFirstChildWhichIsA("Decal", true) == d)
            print("new", pcall(Instance.new, "FaceInstance"))
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("decal	Enum.NormalId.Front		0	1, 1, 1	1	true"), "Decal: Face Front, no Texture, opaque, white, a FaceInstance");
        check(log.has("texture	2	2	0	0	true	Enum.NormalId.Front"), "Texture: a Decal tiled every 2 studs");
        check(log.has("faces	true	Right	2	true	true"), "Face takes an enum or its name; both are the part's children");
        check(log.has("new	false	Unable to create an Instance of type \"FaceInstance\""), "FaceInstance itself is abstract");
        int creates = 0, props = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && (c.className == "Decal" || c.className == "Texture")) creates++;
            if (c.kind == Change::Property && (c.name == "Texture" || c.name == "Face" || c.name == "StudsPerTileU")) props++;
        }
        check(creates == 2 && props == 4, "the engine hears both, their Texture, Face and tiling");
        check(log.errors.empty(), "no errors");
    }

    section("R46. Particles: ParticleEmitter / Fire / Smoke / Sparkles, NumberRange / NumberSequence / ColorSequence");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        // Rojo's spellings of the datatypes in a model.json, and an rbxmx's
        check(loadSourceFile(rt, "Workspace/Torch.model.json", R"({"ClassName": "Part", "Name": "Torch", "Children": [
            {"ClassName": "ParticleEmitter", "Name": "Flame", "Properties": {
                "Lifetime": {"NumberRange": [0.5, 1.5]},
                "Size": {"NumberSequence": {"keypoints": [{"time": 0, "value": 2}, {"time": 0.5, "value": 1, "envelope": 0.25}, {"time": 1, "value": 0}]}},
                "Color": {"ColorSequence": {"keypoints": [{"time": 0, "color": [255, 128, 0]}, {"time": 1, "color": [0.2, 0.2, 0.2]}]}},
                "Transparency": {"NumberSequence": [0, 1]}}}]})", &err) != nullptr && err.empty(), "a model.json's NumberRange / NumberSequence / ColorSequence (Rojo keypoints, bare pairs, 0-255 colors) load");
        check(loadSourceFile(rt, "Workspace/Brazier.rbxmx", R"XML(<roblox version="4"><Item class="Part" referent="RBX0"><Properties><string name="Name">Brazier</string></Properties>
            <Item class="ParticleEmitter" referent="RBX1"><Properties><string name="Name">Embers</string>
              <NumberRange name="Lifetime">1 3 </NumberRange>
              <NumberSequence name="Size">0 0.5 0 1 2 0.5 </NumberSequence>
              <ColorSequence name="Color">0 1 0.5 0 0 1 0.2 0.2 0.2 0 </ColorSequence>
              <NumberSequence name="Transparency">0 0 0 0.5 0.25 0 1 1 0 </NumberSequence>
              <token name="EmissionDirection">5</token></Properties></Item></Item></roblox>)XML", &err) != nullptr && err.empty(), "an rbxmx's sequences load");
        loadSourceFile(rt, "ServerScriptService/Particles.server.luau", R"(
            local r = NumberRange.new(2, 5)
            print("range", r.Min, r.Max, NumberRange.new(3).Max, tostring(r), r == NumberRange.new(2, 5), typeof(r), select(2, pcall(NumberRange.new, 5, 2)):match("NumberRange.*"))
            local s = NumberSequence.new(1, 0)
            local kp = s.Keypoints
            print("seq", #kp, kp[1].Time, kp[1].Value, kp[2].Time, kp[2].Value, kp[2].Envelope, typeof(s), typeof(kp[1]), NumberSequence.new(3).Keypoints[2].Value)
            local s3 = NumberSequence.new({NumberSequenceKeypoint.new(0, 0), NumberSequenceKeypoint.new(0.5, 1, 0.25), NumberSequenceKeypoint.new(1, 0)})
            print("seq3", #s3.Keypoints, s3.Keypoints[2].Value, s3.Keypoints[2].Envelope, s3 == NumberSequence.new({NumberSequenceKeypoint.new(0, 0), NumberSequenceKeypoint.new(0.5, 1, 0.25), NumberSequenceKeypoint.new(1, 0)}), s3 == s)
            print("bad", select(2, pcall(NumberSequence.new, {NumberSequenceKeypoint.new(0, 0)})):match("NumberSequence.*"), select(2, pcall(NumberSequence.new, {NumberSequenceKeypoint.new(0.5, 0), NumberSequenceKeypoint.new(1, 0)})):match("NumberSequence.*"))
            local c = ColorSequence.new(Color3.new(1, 0, 0), Color3.new(0, 0, 1))
            print("cseq", #c.Keypoints, c.Keypoints[1].Value, c.Keypoints[2].Value, c.Keypoints[2].Time, typeof(c), typeof(c.Keypoints[1]), ColorSequence.new(Color3.new(0, 1, 0)).Keypoints[2].Value, ColorSequenceKeypoint.new(0.5, Color3.new(1, 1, 0)).Value)
            local e = Instance.new("ParticleEmitter")
            print("emitter", e.Enabled, e.Rate, e.Lifetime, e.Speed.Min, e.Color.Keypoints[1].Value, e.Size.Keypoints[1].Value, e.Transparency.Keypoints[2].Value, e.EmissionDirection, e.LightEmission, e.SpreadAngle, e.Shape, e.Orientation, e.LockedToPart)
            e.Lifetime = NumberRange.new(1, 2)
            e.Size = NumberSequence.new(2, 0)
            e.Color = ColorSequence.new(Color3.new(1, 0.5, 0))
            e.Transparency = s3
            e.Rate = 50
            e.Speed = 8                      -- a number: a range of one value
            e.Parent = workspace.Torch
            print("set", e.Lifetime.Min, e.Lifetime.Max, e.Size.Keypoints[1].Value, e.Color.Keypoints[2].Value, e.Transparency.Keypoints[2].Envelope, e.Speed.Min, e.Speed.Max)
            print("typed", select(2, pcall(function() e.Lifetime = "x" end)):match("Lifetime.*"), select(2, pcall(function() e.Color = Vector3.new() end)):match("Color.*"))
            e:GetPropertyChangedSignal("Size"):Connect(function() print("sizechanged", e.Size.Keypoints[2].Value) end)
            e.Size = NumberSequence.new(3, 1)
            e.Size = NumberSequence.new(3, 1)   -- the same again: no change
            e:Emit(25)
            e:Emit()
            e:Clear()
            local f = workspace.Torch.Flame
            print("json", f.Lifetime, #f.Size.Keypoints, f.Size.Keypoints[2].Envelope, f.Color.Keypoints[1].Value, f.Color.Keypoints[2].Value, f.Transparency.Keypoints[2].Value)
            local em = workspace.Brazier.Embers
            print("xml", em.Lifetime, em.Size.Keypoints[2].Value, em.Size.Keypoints[2].Envelope, em.Color.Keypoints[1].Value, #em.Transparency.Keypoints, em.Transparency.Keypoints[2].Value, em.EmissionDirection)
            local fire = Instance.new("Fire", workspace.Torch)
            local smoke = Instance.new("Smoke", workspace.Torch)
            local sp = Instance.new("Sparkles", workspace.Torch)
            print("fire", fire.Color, fire.SecondaryColor, fire.Heat, fire.Size, fire.Enabled, smoke.Color, smoke.Opacity, smoke.RiseVelocity, smoke.Size, sp.SparkleColor, sp.TimeScale)
            e:SetAttribute("Burst", NumberRange.new(1, 4))
            e:SetAttribute("Fade", NumberSequence.new(1, 0))
            print("attr", e:GetAttribute("Burst").Max, e:GetAttribute("Fade").Keypoints[2].Value)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("range	2	5	3	2 5	true	NumberRange	NumberRange.new(): invalid range"), "NumberRange: Min / Max, one value, equality, no backwards range");
        check(log.has("seq	2	0	1	1	0	0	NumberSequence	NumberSequenceKeypoint	3"), "NumberSequence.new(a, b) / (n): two keypoints at 0 and 1");
        check(log.has("seq3	3	1	0.25	true	false"), "NumberSequence.new({keypoints}) with envelopes; equal when the keypoints are");
        check(log.has("bad	NumberSequence.new(): requires at least 2 keypoints	NumberSequence.new(): first keypoint must be at time 0"), "Roblox's keypoint rules");
        check(log.has("cseq	2	1, 0, 0	0, 0, 1	1	ColorSequence	ColorSequenceKeypoint	0, 1, 0	1, 1, 0"), "ColorSequence.new(a, b) / (c), ColorSequenceKeypoint.new(t, color)");
        check(log.has("emitter	true	20	5 10	5	1, 1, 1	1	0	Enum.NormalId.Top	0	0, 0	Enum.ParticleEmitterShape.Box	Enum.ParticleOrientation.FacingCamera	false"), "ParticleEmitter: Studio's defaults");
        check(log.has("set	1	2	2	1, 0.5, 0	0.25	8	8"), "Lifetime / Size / Color / Transparency take the datatypes; a number is a one-value range");
        check(log.has("typed	Lifetime' (NumberRange expected, got string)	Color' (ColorSequence expected, got Vector3)"), "the wrong type is refused");
        check(log.count("sizechanged	1") == 1, "GetPropertyChangedSignal on a sequence: once, not for the same sequence again");
        check(log.has("json	0.5 1.5	3	0.25	1, 0.501961, 0	0.2, 0.2, 0.2	1"), "the model.json's values");
        check(log.has("xml	1 3	2	0.5	1, 0.5, 0	3	0.25	Enum.NormalId.Front"), "the rbxmx's values");
        check(log.has("fire	0.92549, 0.545098, 0.27451	0.545098, 0.313726, 0.0588235	9	5	true	0.698039, 0.698039, 0.698039	0.5	1	1	0.564706, 0.564706, 1	1"), "Fire / Smoke / Sparkles: Studio's defaults");
        check(log.has("attr	4	0"), "attributes hold the datatypes");
        int bursts = 0, seqs = 0; bool clear = false;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Property && c.name == "EmitBurst" && c.value.n > 0) bursts += (int)c.value.n;
            if (c.kind == Change::Property && c.name == "EmitBurst" && c.value.n < 0) clear = true;
            if (c.kind == Change::Property && (c.value.type == Value::NumberSequence || c.value.type == Value::ColorSequence || c.value.type == Value::NumberRange)) seqs++;
        }
        check(bursts == 41 && clear && seqs >= 6, "the engine hears Emit(25) + Emit() as bursts of 25 and 16, Clear(), and the sequences");
        check(log.errors.empty(), "no errors");
    }

    section("R47. Explosion: Hit within BlastRadius, joints break, a character there dies, removed once played");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Boom.server.luau", R"(
            local function part(name, x, anchored)
                local p = Instance.new("Part"); p.Name = name; p.Position = Vector3.new(x, 10, 0); p.Anchored = anchored; p.Parent = workspace; return p
            end
            local a, b, c, far = part("A", 0, true), part("B", 3, false), part("C", 5, false), part("Far", 30, false)
            local w = Instance.new("Weld"); w.Name = "AB"; w.Part0 = a; w.Part1 = b; w.Parent = a
            local w2 = Instance.new("WeldConstraint"); w2.Name = "CFar"; w2.Part0 = c; w2.Part1 = far; w2.Parent = c
            local w3 = Instance.new("Weld"); w3.Name = "Off"; w3.Part0 = far; w3.Part1 = far; w3.Parent = far
            local e = Instance.new("Explosion")
            print("defaults", e.BlastPressure, e.BlastRadius, e.DestroyJointRadiusPercent, e.ExplosionType, e.Position, e.TimeScale, e.Visible)
            e.Position = Vector3.new(0, 10, 0)
            e.BlastRadius = 6
            e.DestroyJointRadiusPercent = 0.6         -- joints break within 3.6 studs: A and B, not C
            e.Hit:Connect(function(part, dist) print("hit", part.Name, math.floor(dist * 100) / 100) end)
            e.Parent = workspace
            print("after", w.Parent, w2.Parent == c, w3.Parent == far, e.Parent == workspace, #workspace:GetChildren())
            e.Parent = workspace                     -- again: nothing more
            task.delay(2.5, function() print("gone", e.Parent, workspace:FindFirstChildOfClass("Explosion")) end)
            game.Players.PlayerAdded:Connect(function(p)
                p.CharacterAdded:Connect(function(ch)
                    local hum = ch:WaitForChild("Humanoid")
                    hum.Died:Connect(function() print("died", p.Name) end)
                    ch:WaitForChild("HumanoidRootPart").Position = Vector3.new(50, 5, 0)
                    task.wait(0.1)
                    local boom = Instance.new("Explosion")
                    boom.Position = Vector3.new(52, 5, 0)
                    boom.Parent = workspace
                    print("blown", hum.Health)
                    local dud = Instance.new("Explosion")   -- outside any radius of the character: it lives
                    dud.Position = Vector3.new(52, 5, 0)
                    dud.DestroyJointRadiusPercent = 0
                    dud.Parent = workspace
                    local quiet = Instance.new("Explosion")   -- not in the Workspace: nothing happens
                    quiet.Position = Vector3.new(52, 5, 0)
                    quiet.Hit:Connect(function() print("quiet hit") end)
                    quiet.Parent = game.ReplicatedStorage
                    task.wait(0.1)
                    print("still", quiet.Parent)
                end)
            end)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("defaults	500000	4	1	Enum.ExplosionType.Craters	0, 0, 0	1	true"), "Explosion: Studio's defaults");
        check(log.has("hit	A	0") && log.has("hit	B	3") && log.has("hit	C	5") && !log.has("hit	Far	30"), "Hit(part, distance) for the parts within BlastRadius");
        check(log.has("after\tnil\ttrue\ttrue\ttrue\t6"), "the Weld within DestroyJointRadiusPercent of the radius broke; the others and the parts stay (6 children = the parts plus Terrain)");
        check(log.count("hit	A	0") == 1, "parenting it again does not detonate it again");
        rt.addPlayer("ada", 7);
        for (int k = 0; k < 8; k++) rt.step(0.05);
        check(log.has("died	ada") && log.has("blown	0"), "a character within the joint radius dies");
        check(log.has("still	ReplicatedStorage") && !log.has("quiet hit"), "an Explosion outside the Workspace does nothing");
        for (int k = 0; k < 50; k++) rt.step(0.05);
        check(log.has("gone	nil	nil"), "removed from the tree once played");
        int creates = 0, positions = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && c.className == "Explosion") creates++;
            if (c.kind == Change::Property && c.name == "Position") positions++;
        }
        check(creates == 4 && positions >= 4, "the engine hears each Explosion and where");
        check(log.errors.empty(), "no errors");
    }

    section("R48. Highlight: an outline and tint over a Model or a part");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Glow.server.luau", R"(
            local m = Instance.new("Model", workspace)
            local p = Instance.new("Part", m)
            local h = Instance.new("Highlight")
            print("defaults", h.Adornee, h.DepthMode, h.Enabled, h.FillColor, h.FillTransparency, h.OutlineColor, h.OutlineTransparency)
            h.FillColor = Color3.new(0, 0, 1)
            h.FillTransparency = 0.25
            h.DepthMode = Enum.HighlightDepthMode.Occluded
            h.Parent = m
            local h2 = Instance.new("Highlight")
            h2.Adornee = p
            h2.Parent = game.ReplicatedStorage
            print("set", h.FillColor, h.DepthMode.Name, h2.Adornee == p, h2.Adornee.Name, pcall(function() h2.Adornee = "x" end))
            h2.Adornee = nil
            print("cleared", h2.Adornee)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("defaults	nil	Enum.HighlightDepthMode.AlwaysOnTop	true	1, 0, 0	0.5	1, 1, 1	0"), "Highlight: Studio's defaults");
        check(log.has("set	0, 0, 1	Occluded	true	Part	false	ServerScriptService.Glow:13: invalid argument #3 to 'Adornee' (Instance expected, got string)"), "the look, DepthMode, an Adornee elsewhere in the tree");
        check(log.has("cleared	nil"), "Adornee back to nil: the parent again");
        int creates = 0, props = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && c.className == "Highlight") creates++;
            if (c.kind == Change::Property && (c.name == "FillColor" || c.name == "Adornee" || c.name == "DepthMode")) props++;
        }
        check(creates == 2 && props >= 4, "the engine hears both, the colour, the mode and the Adornee");
        check(log.errors.empty(), "no errors");
    }

    section("R49. Seat / VehicleSeat: a touch sits the character, a jump stands it up");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Seats.server.luau", R"(
            local seat = Instance.new("Seat"); seat.Name = "Chair"; seat.Size = Vector3.new(2, 1, 2); seat.Position = Vector3.new(10, 0.5, 0); seat.Anchored = true; seat.Parent = workspace
            local car = Instance.new("VehicleSeat"); car.Name = "Driver"; car.Position = Vector3.new(20, 0.5, 0); car.Parent = workspace
            print("defaults", seat.Disabled, seat.Occupant, seat:IsA("Part"), car.MaxSpeed, car.Torque, car.TurnSpeed, car.Throttle, car.Steer, car.HeadsUpDisplay, car:IsA("Seat"))
            seat:GetPropertyChangedSignal("Occupant"):Connect(function() print("occupant", seat.Occupant and seat.Occupant.Parent.Name or "nil") end)
            game.Players.PlayerAdded:Connect(function(p)
                p.CharacterAdded:Connect(function(ch)
                    local hum = ch:WaitForChild("Humanoid")
                    print("hum", hum.Sit, hum.SeatPart)
                    hum.Seated:Connect(function(active, part) print("seated", p.Name, active, part and part.Name or "nil", hum.Sit, hum.SeatPart == part) end)
                end)
            end)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("defaults	false	nil	true	25	100	1	0	0	true	false"), "Seat / VehicleSeat: Studio's defaults; a VehicleSeat is not a Seat");
        Instance* p = rt.addPlayer("ada", 7);
        rt.step(0.05); rt.step(0.05);
        Instance* ch = dm.find(p->get("Character").ref);
        Instance* hum = ch ? ch->findFirstChildOfClass("Humanoid") : nullptr;
        Instance* leg = ch ? ch->findFirstChild("Left Leg") : nullptr;
        Instance* seat = dm.workspace()->findFirstChild("Chair");
        Instance* car = dm.workspace()->findFirstChild("Driver");
        check(hum && leg && seat && car && log.has("hum	false	nil"), "a character, standing");
        rt.fireEvent(*seat, "Touched", {Value::instance(leg->id())});
        rt.step(0.05);
        Instance* weld = seat->findFirstChild("SeatWeld");
        check(log.has("seated	ada	true	Chair	true	true") && seat->get("Occupant").ref == hum->id() && log.has("occupant	ada"), "a limb touching the Seat: Seated(true, seat), Sit, SeatPart, Occupant");
        check(weld && weld->isA("Weld") && weld->get("Part0").ref == seat->id() && weld->get("Part1").ref == ch->findFirstChild("HumanoidRootPart")->id() && weld->get("C0Position").v.y == 1.5f, "a SeatWeld from the seat to the HumanoidRootPart, C0 the seat's top");
        Instance* bob = rt.addPlayer("bob", 8);
        rt.step(0.05); rt.step(0.05);
        Instance* bobCh = dm.find(bob->get("Character").ref);
        rt.fireEvent(*seat, "Touched", {Value::instance(bobCh->findFirstChild("Right Leg")->id())});
        rt.step(0.05);
        check(seat->get("Occupant").ref == hum->id() && !log.has("seated	bob	true	Chair	true	true"), "a taken seat seats nobody else");
        rt.hostWrite(hum->id(), "Jump", Value::boolean(true));
        rt.step(0.05);
        check(log.has("seated	ada	false	nil	false	true") && seat->get("Occupant").ref == 0 && hum->get("SeatPart").ref == 0 && !seat->findFirstChild("SeatWeld") && log.has("occupant	nil"), "a jump stands it up: Seated(false), the SeatWeld gone, Occupant nil");
        rt.fireEvent(*seat, "Touched", {Value::instance(leg->id())});
        rt.step(0.05);
        check(seat->get("Occupant").ref == 0, "not straight back down: a moment's grace after standing");
        for (int k = 0; k < 24; k++) rt.step(0.05);
        rt.fireEvent(*seat, "Touched", {Value::instance(leg->id())});
        rt.step(0.05);
        check(seat->get("Occupant").ref == hum->id(), "then a touch seats it again");
        rt.runChunk("stand", R"(game.Players.ada.Character.Humanoid.Sit = false)");
        rt.step(0.05);
        check(seat->get("Occupant").ref == 0 && log.count("seated	ada	false	nil	false	true") == 2, "Humanoid.Sit = false stands it up too");
        rt.runChunk("disabled", R"(workspace.Chair.Disabled = true)");
        for (int k = 0; k < 24; k++) rt.step(0.05);
        rt.fireEvent(*seat, "Touched", {Value::instance(leg->id())});
        rt.step(0.05);
        check(seat->get("Occupant").ref == 0, "a Disabled seat seats nobody");
        rt.runChunk("sit", R"(workspace.Driver:Sit(game.Players.ada.Character.Humanoid); print("driving", workspace.Driver.Occupant == game.Players.ada.Character.Humanoid, game.Players.ada.Character.Humanoid.SeatPart.Name))");
        rt.step(0.05);
        check(log.has("driving	true	Driver"), "Seat:Sit(humanoid) sits it there now");
        rt.hostWrite(hum->id(), "MoveDirection", Value::vector3(0, 0, -1));
        rt.step(0.05);
        check(car->get("Throttle").n == 1 && car->get("Steer").n == 0 && car->get("ThrottleFloat").n == 1, "a VehicleSeat's Throttle follows the occupant's controls: forward along the seat");
        rt.hostWrite(hum->id(), "MoveDirection", Value::vector3(0.7f, 0, 0.7f));
        rt.step(0.05);
        check(car->get("Throttle").n == -1 && car->get("Steer").n == 1, "back and to the right");
        rt.runChunk("kill", R"(game.Players.ada.Character.Humanoid.Health = 0)");
        rt.step(0.05);
        check(car->get("Occupant").ref == 0 && car->get("Throttle").n == 0 && hum->get("SeatPart").ref == 0, "dying stands it up and lets go of the controls");
        rt.runChunk("gone", R"(
            local hum = game.Players.bob.Character.Humanoid
            workspace.Chair.Disabled = false
            workspace.Chair:Sit(hum)
            local was = workspace.Chair.Occupant == hum
            workspace.Chair:Destroy()
            print("destroyed", was, hum.Sit, hum.SeatPart)
        )");
        rt.step(0.05);
        check(log.has("destroyed	true	false	nil"), "destroying the seat stands its occupant up");
        check(log.errors.empty(), "no errors");
    }

    section("R50. HingeConstraint / BallSocketConstraint: two Attachments' parts joined through the engine");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Door.server.luau", R"(
            local post = Instance.new("Part"); post.Name = "Post"; post.Anchored = true; post.Position = Vector3.new(0, 4, 0); post.Parent = workspace
            local door = Instance.new("Part"); door.Name = "Door"; door.Size = Vector3.new(4, 7, 0.5); door.Position = Vector3.new(2.5, 4, 0); door.Parent = workspace
            local a0 = Instance.new("Attachment", post); a0.Name = "A0"; a0.Position = Vector3.new(0.5, 0, 0); a0.Orientation = Vector3.new(0, 0, 90)
            local a1 = Instance.new("Attachment", door); a1.Name = "A1"; a1.Position = Vector3.new(-2, 0, 0); a1.Orientation = Vector3.new(0, 0, 90)
            local h = Instance.new("HingeConstraint"); h.Name = "Hinge"; h.Attachment0 = a0; h.Attachment1 = a1; h.Parent = post
            print("defaults", h.ActuatorType, h.AngularVelocity, h.MotorMaxTorque, h.LimitsEnabled, h.LowerAngle, h.UpperAngle, h.CurrentAngle, h.Enabled, h.Visible, h:IsA("Constraint"))
            h.ActuatorType = Enum.ActuatorType.Motor; h.AngularVelocity = 2; h.MotorMaxTorque = 1000; h.LimitsEnabled = true; h.UpperAngle = 90
            print("motor", h.ActuatorType, h.Attachment0.WorldPosition, h.Attachment1.WorldPosition, h.Attachment0.WorldAxis.Y)
            print("readonly", pcall(function() h.CurrentAngle = 5 end))
            print("badref", pcall(function() h.Attachment1 = 5 end))
            h:GetPropertyChangedSignal("CurrentAngle"):Connect(function() print("angle", h.CurrentAngle) end)
            local bs = Instance.new("BallSocketConstraint"); bs.Attachment0 = a0; bs.Attachment1 = a1; bs.Parent = post
            print("socket", bs.LimitsEnabled, bs.UpperAngle, bs.TwistLimitsEnabled, bs.TwistLowerAngle, bs.TwistUpperAngle, bs.MaxFrictionTorque)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("defaults	Enum.ActuatorType.None	0	0	false	-45	45	0	true	false	true"), "HingeConstraint: Studio's defaults, a Constraint");
        check(log.has("motor	Enum.ActuatorType.Motor	0.5, 4, 0	0.5, 4, 0	1"), "a Motor; the attachments meet in the world, the axis up");
        check(log.has("readonly	false	ServerScriptService.Door:10: Unable to assign property CurrentAngle. Property is read only"), "CurrentAngle is the engine's to write");
        check(log.has("badref	false	ServerScriptService.Door:11: invalid argument #3 to 'Attachment1' (Instance expected, got number)"), "Attachment1 wants an Instance");
        check(log.has("socket	false	45	false	-45	45	0"), "BallSocketConstraint: Studio's defaults");
        int creates = 0, props = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && (c.className == "HingeConstraint" || c.className == "BallSocketConstraint")) creates++;
            if (c.kind == Change::Property && (c.name == "Attachment0" || c.name == "Attachment1" || c.name == "AngularVelocity" || c.name == "ActuatorType" || c.name == "UpperAngle")) props++;
        }
        check(creates == 2 && props >= 7, "the engine hears both, the attachments, the actuator and the motor");
        Instance* h = dm.workspace()->findFirstChild("Post")->findFirstChild("Hinge");
        rt.hostWrite(h->id(), "CurrentAngle", Value::number(37.5));
        rt.step(0.05);
        check(log.has("angle	37.5"), "the engine's CurrentAngle reaches the script, and its changed signal");
        check(log.errors.empty(), "no errors");
    }

    section("R51. RopeConstraint / RodConstraint / SpringConstraint: a distance kept by the engine");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Crane.server.luau", R"(
            local crane = Instance.new("Part"); crane.Name = "Crane"; crane.Anchored = true; crane.Position = Vector3.new(0, 14, 0); crane.Parent = workspace
            local load = Instance.new("Part"); load.Name = "Load"; load.Position = Vector3.new(0, 6, 0); load.Parent = workspace
            local a0 = Instance.new("Attachment", crane); local a1 = Instance.new("Attachment", load); a1.Position = Vector3.new(0, 0.5, 0)
            local rope = Instance.new("RopeConstraint"); rope.Name = "Rope"; rope.Attachment0 = a0; rope.Attachment1 = a1; rope.Parent = crane
            print("rope", rope.Length, rope.Restitution, rope.Thickness, rope.WinchEnabled, rope.WinchTarget, rope.WinchSpeed, rope.CurrentDistance, rope:IsA("Constraint"))
            local rod = Instance.new("RodConstraint"); rod.Attachment0 = a0; rod.Attachment1 = a1; rod.Parent = crane
            print("rod", rod.Length, rod.Thickness, rod.LimitsEnabled, rod.LimitAngle0, rod.LimitAngle1, rod.CurrentDistance)
            local spring = Instance.new("SpringConstraint"); spring.Attachment0 = a0; spring.Attachment1 = a1; spring.Parent = crane
            print("spring", spring.Stiffness, spring.Damping, spring.FreeLength, spring.MinLength, spring.LimitsEnabled, spring.Coils, spring.Radius, spring.CurrentLength)
            spring.Stiffness = 200; spring.Damping = 20; spring.FreeLength = 4
            rope.WinchEnabled = true; rope.WinchTarget = 3; rope.WinchSpeed = 2
            print("readonly", pcall(function() rope.CurrentDistance = 1 end))
            rope:GetPropertyChangedSignal("CurrentDistance"):Connect(function() print("distance", rope.CurrentDistance) end)
            rope:GetPropertyChangedSignal("Length"):Connect(function() print("length", rope.Length) end)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("rope	5	0	0.1	false	0	0	0	true"), "RopeConstraint: Studio's defaults, a Constraint");
        check(log.has("rod	5	0.1	false	45	45	0"), "RodConstraint: Studio's defaults");
        check(log.has("spring	100	0	5	0	false	3	0.5	0"), "SpringConstraint: Studio's defaults");
        check(log.has("readonly	false	ServerScriptService.Crane:13: Unable to assign property CurrentDistance. Property is read only"), "CurrentDistance is the engine's to write");
        int creates = 0, props = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && (c.className == "RopeConstraint" || c.className == "RodConstraint" || c.className == "SpringConstraint")) creates++;
            if (c.kind == Change::Property && (c.name == "Stiffness" || c.name == "Damping" || c.name == "FreeLength" || c.name == "WinchEnabled" || c.name == "WinchTarget" || c.name == "WinchSpeed")) props++;
        }
        check(creates == 3 && props == 6, "the engine hears the three, the spring's numbers and the winch");
        Instance* rope = dm.workspace()->findFirstChild("Crane")->findFirstChild("Rope");
        rt.hostWrite(rope->id(), "CurrentDistance", Value::number(4.25));
        rt.hostWrite(rope->id(), "Length", Value::number(4.9));   // the winch reeling it in
        rt.step(0.05);
        check(log.has("distance	4.25") && log.has("length	4.9"), "the engine's CurrentDistance and the winch's Length reach the script");
        check(log.errors.empty(), "no errors");
    }

    section("R52. VectorForce / Torque / LinearVelocity / AngularVelocity / AlignPosition / AlignOrientation: the movers");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Hover.server.luau", R"(
            local p = Instance.new("Part"); p.Name = "Hover"; p.Size = Vector3.new(4, 1, 2); p.Position = Vector3.new(0, 6, 0); p.Parent = workspace
            local a0 = Instance.new("Attachment", p)
            local vf = Instance.new("VectorForce"); vf.Attachment0 = a0; vf.Parent = p
            print("force", vf.Force, vf.RelativeTo, vf.ApplyAtCenterOfMass, vf:IsA("Constraint"))
            vf.Force = Vector3.new(0, p:GetMass() * workspace.Gravity, 0); vf.RelativeTo = Enum.ActuatorRelativeTo.World
            print("hover", vf.Force, vf.RelativeTo, p:GetMass())
            local tq = Instance.new("Torque"); tq.Attachment0 = a0; tq.Parent = p
            print("torque", tq.Torque, tq.RelativeTo)
            local lv = Instance.new("LinearVelocity"); lv.Attachment0 = a0; lv.Parent = p
            print("linear", lv.VectorVelocity, lv.MaxForce, lv.RelativeTo, lv.VelocityConstraintMode, lv.LineDirection, lv.LineVelocity, lv.PlaneVelocity)
            local av = Instance.new("AngularVelocity"); av.Attachment0 = a0; av.Parent = p
            print("angular", av.AngularVelocity, av.MaxTorque, av.RelativeTo)
            local ap = Instance.new("AlignPosition"); ap.Attachment0 = a0; ap.Parent = p
            print("alignpos", ap.Mode, ap.Position, ap.MaxForce, ap.MaxVelocity, ap.Responsiveness, ap.RigidityEnabled, ap.ApplyAtCenterOfMass)
            local ao = Instance.new("AlignOrientation"); ao.Attachment0 = a0; ao.Parent = p
            print("alignori", ao.Mode, ao.CFrame == CFrame.new(), ao.MaxTorque, ao.Responsiveness, ao.RigidityEnabled, ao.PrimaryAxisOnly)
            ao.Mode = Enum.OrientationAlignmentMode.OneAttachment; ao.CFrame = CFrame.new(1, 2, 3) * CFrame.Angles(0, math.rad(90), 0)
            local _, y = ao.CFrame:ToEulerAnglesYXZ()
            print("goal", ao.CFrame.Position, math.floor(math.deg(y) + 0.5), ao.CFrameOrientation)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("force	0, 0, 0	Enum.ActuatorRelativeTo.Attachment0	false	true"), "VectorForce: Studio's defaults, a Constraint");
        check(log.has("hover	0, 1098.72, 0	Enum.ActuatorRelativeTo.World	5.6"), "GetMass() * Gravity: the force that holds a 4x1x2 part up");
        check(log.has("torque	0, 0, 0	Enum.ActuatorRelativeTo.Attachment0"), "Torque: Studio's defaults");
        check(log.has("linear	0, 0, 0	0	Enum.ActuatorRelativeTo.World	Enum.VelocityConstraintMode.Vector	1, 0, 0	0	0, 0"), "LinearVelocity: Studio's defaults");
        check(log.has("angular	0, 0, 0	0	Enum.ActuatorRelativeTo.World"), "AngularVelocity: Studio's defaults");
        check(log.has("alignpos	Enum.PositionAlignmentMode.TwoAttachment	0, 0, 0	10000	1000000	10	false	false"), "AlignPosition: Studio's defaults");
        check(log.has("alignori	Enum.OrientationAlignmentMode.TwoAttachment	true	10000	10	false	false"), "AlignOrientation: Studio's defaults, CFrame the identity");
        check(log.has("goal	1, 2, 3	90	0, 90, 0"), "AlignOrientation.CFrame: a CFrame, kept as a position and an orientation");
        int creates = 0, props = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && (c.className == "VectorForce" || c.className == "Torque" || c.className == "LinearVelocity" || c.className == "AngularVelocity" || c.className == "AlignPosition" || c.className == "AlignOrientation")) creates++;
            if (c.kind == Change::Property && (c.name == "Force" || c.name == "RelativeTo" || c.name == "Mode" || c.name == "CFramePosition" || c.name == "CFrameOrientation")) props++;
        }
        check(creates == 6 && props >= 5, "the engine hears all six, the force, its frame, the mode and the goal");
        check(log.errors.empty(), "no errors");
    }

    section("R53. Materials weigh and rub: densities, friction, elasticity, CustomPhysicalProperties");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Anvil.server.luau", R"(
            local function r(x, d) return math.floor(x * d + 0.5) / d end
            local p = Instance.new("Part"); p.Name = "Anvil"; p.Size = Vector3.new(4, 1, 2); p.Parent = workspace
            print("plastic", p.Material, r(p:GetMass(), 100), p.CustomPhysicalProperties, p.Massless)
            p.Material = Enum.Material.Metal
            print("metal", r(p:GetMass(), 100))
            local ice = PhysicalProperties.new(Enum.Material.Ice)
            print("ice", r(ice.Density, 1000), r(ice.Friction, 1000), r(ice.Elasticity, 1000), ice.FrictionWeight, ice.ElasticityWeight)
            print("tostring", tostring(PhysicalProperties.new(2, 0.5, 0.3)), typeof(ice))
            p.CustomPhysicalProperties = PhysicalProperties.new(2, 0.5, 0.3)
            print("custom", p.CustomPhysicalProperties.Density, r(p:GetMass(), 100), p.CustomPhysicalProperties == PhysicalProperties.new(2, 0.5, 0.3))
            p.CustomPhysicalProperties = nil
            print("back", p.CustomPhysicalProperties, r(p:GetMass(), 100))
            local ok, e = pcall(function() p.CustomPhysicalProperties = 3 end)
            print("bad", ok, string.find(e, "PhysicalProperties expected, got number") ~= nil)
            local ok2, e2 = pcall(function() PhysicalProperties.new(0, 0.5, 0.3) end)
            print("dense", ok2, string.find(e2, "density must be between 0.01 and 100") ~= nil)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("plastic	Enum.Material.Plastic	5.6	nil	false"), "a part is Plastic: 0.7 a stud cubed, and no CustomPhysicalProperties");
        check(log.has("metal	62.8"), "Metal weighs 7.85: the same 8 studs are 62.8");
        check(log.has("ice	0.92	0.02	0.15	1	1"), "PhysicalProperties.new(Enum.Material.Ice): what Ice is made of");
        check(log.has("tostring	2, 0.5, 0.3, 1, 1	PhysicalProperties"), "PhysicalProperties.new(d, f, e) fills the weights in; typeof knows it");
        check(log.has("custom	2	16	true"), "CustomPhysicalProperties overrides the density, and two of them compare equal");
        check(log.has("back	nil	62.8"), "nil hands the part back to its material");
        check(log.has("bad	false	true"), "anything else is a type error");
        check(log.has("dense	false	true"), "a density outside 0.01 .. 100 is refused, as Studio refuses it");
        int mat = 0, custom = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Property && c.name == "Material") mat++;
            if (c.kind == Change::Property && c.name == "CustomPhysicalProperties") custom++;
        }
        check(mat == 1 && custom == 2, "the engine hears the material and both sides of the override");
        check(log.errors.empty(), "no errors");
    }

    section("R54. PrismaticConstraint / CylindricalConstraint: a slide along the axis, and a turn about it");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Rail.server.luau", R"(
            local rail = Instance.new("Part"); rail.Name = "Rail"; rail.Anchored = true; rail.Size = Vector3.new(8, 1, 2); rail.Parent = workspace
            local a0 = Instance.new("Attachment", rail)
            local car = Instance.new("Part"); car.Name = "Car"; car.Size = Vector3.new(2, 1, 2); car.Parent = workspace
            local a1 = Instance.new("Attachment", car)
            local pc = Instance.new("PrismaticConstraint"); pc.Name = "Slide"; pc.Attachment0 = a0; pc.Attachment1 = a1; pc.Parent = rail
            print("prismatic", pc.ActuatorType, pc.Velocity, pc.MotorMaxForce, pc.Speed, pc.ServoMaxForce, pc.TargetPosition, pc.LimitsEnabled, pc.LowerLimit, pc.UpperLimit, pc.CurrentPosition, pc.Size)
            print("isa", pc:IsA("SlidingBallConstraint"), pc:IsA("Constraint"), pc:IsA("Instance"))
            pc.ActuatorType = Enum.ActuatorType.Servo; pc.TargetPosition = 3; pc.Speed = 2; pc.ServoMaxForce = 5000
            print("servo", pc.ActuatorType, pc.TargetPosition, pc.Speed, pc.ServoMaxForce)
            local cc = Instance.new("CylindricalConstraint"); cc.Name = "Barrel"; cc.Attachment0 = a0; cc.Attachment1 = a1; cc.Parent = rail
            print("cylindrical", cc.AngularActuatorType, cc.AngularVelocity, cc.MotorMaxTorque, cc.AngularSpeed, cc.ServoMaxTorque, cc.TargetAngle, cc.AngularLimitsEnabled, cc.LowerAngle, cc.UpperAngle, cc.CurrentAngle, cc.InclinationAngle)
            print("both", cc.LowerLimit, cc.UpperLimit, cc:IsA("SlidingBallConstraint"))
            local ok, e = pcall(function() pc.CurrentPosition = 2 end)
            print("readonly", ok, string.find(e, "Unable to assign property CurrentPosition. Property is read only") ~= nil)
            pc:GetPropertyChangedSignal("CurrentPosition"):Connect(function() print("at", pc.CurrentPosition) end)
            cc:GetPropertyChangedSignal("CurrentAngle"):Connect(function() print("turned", cc.CurrentAngle) end)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        check(log.has("prismatic	Enum.ActuatorType.None	0	0	0	0	0	false	-5	5	0	0.15"), "PrismaticConstraint: Studio's defaults");
        check(log.has("isa	true	true	true"), "a SlidingBallConstraint, as in Studio's tree");
        check(log.has("servo	Enum.ActuatorType.Servo	3	2	5000"), "a Servo to TargetPosition at Speed under ServoMaxForce");
        check(log.has("cylindrical	Enum.ActuatorType.None	0	0	0	0	0	false	-45	45	0	0"), "CylindricalConstraint: the turning half of Studio's defaults");
        check(log.has("both	-5	5	true"), "and the sliding half with it");
        check(log.has("readonly	false	true"), "CurrentPosition is the engine's to write");
        int creates = 0, props = 0;
        for (const Change& c : rt.takeChanges()) {
            if (c.kind == Change::Create && (c.className == "PrismaticConstraint" || c.className == "CylindricalConstraint")) creates++;
            if (c.kind == Change::Property && (c.name == "ActuatorType" || c.name == "TargetPosition" || c.name == "Speed" || c.name == "ServoMaxForce")) props++;
        }
        check(creates == 2 && props == 4, "the engine hears both, the actuator and its numbers");
        Instance* rail = dm.workspace()->findFirstChild("Rail");
        rt.hostWrite(rail->findFirstChild("Slide")->id(), "CurrentPosition", Value::number(2.5));
        rt.hostWrite(rail->findFirstChild("Barrel")->id(), "CurrentAngle", Value::number(-30));
        rt.step(0.05);
        check(log.has("at	2.5") && log.has("turned	-30"), "the engine's CurrentPosition and CurrentAngle reach the script");
        check(log.errors.empty(), "no errors");
    }

    section("R55. Accessory: what a character wears");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Wardrobe.server.luau", R"(
            local function hat(name, att)
                local a = Instance.new("Accessory"); a.Name = name; a.AccessoryType = Enum.AccessoryType.Hat
                local h = Instance.new("Part"); h.Name = "Handle"; h.Size = Vector3.new(2, 1, 2); h.CanCollide = false; h.Parent = a
                local p = Instance.new("Attachment"); p.Name = att; p.Position = Vector3.new(0, -0.5, 0); p.Parent = h
                return a
            end
            print("accessory", hat("Cap", "HatAttachment").AccessoryType, hat("Cap", "HatAttachment"):IsA("Accoutrement"), hat("Cap", "HatAttachment").AttachmentPos)
            game.Players.PlayerAdded:Connect(function(player)
                player.CharacterAdded:Connect(function(char)
                    local hum = char:WaitForChild("Humanoid")
                    print("points", char.Head:FindFirstChild("HatAttachment") ~= nil, char.Torso:FindFirstChild("NeckAttachment") ~= nil,
                          char["Right Arm"]:FindFirstChild("RightGripAttachment").Position, #hum:GetAccessories())
                    hum:AddAccessory(hat("Cap", "HatAttachment"))
                    local worn = hum:GetAccessories()
                    print("worn", #worn, worn[1].Name, worn[1].Parent == char, worn[1].Handle.Name)
                    hum:RemoveAccessories()
                    print("bare", #hum:GetAccessories(), char:FindFirstChild("Cap") == nil)
                end)
            end)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        Instance* player = rt.addPlayer("Ada", 1);
        rt.step(0.05);
        rt.step(0.05);
        check(log.has("accessory	Enum.AccessoryType.Hat	true	0, 0, 0"), "an Accessory is an Accoutrement with an AccessoryType");
        check(log.has("points	true	true	0, -1, 0	0"), "an R6 rig wears Roblox's attachment points, and nothing else yet");
        check(log.has("worn	1	Cap	true	Handle"), "Humanoid:AddAccessory puts it on the character; GetAccessories lists it");
        check(log.has("bare	0	true"), "RemoveAccessories takes them off");
        check(player != nullptr, "the player joined");
        check(log.errors.empty(), "no errors");
    }

    section("R56. Animation: a KeyframeSequence on the Animator");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        const char* wave = R"XML(<?xml version="1.0" encoding="utf-8"?>
<roblox version="4">
  <Item class="KeyframeSequence" referent="RBX0">
    <Properties><string name="Name">Wave</string><bool name="Loop">true</bool></Properties>
    <Item class="Keyframe" referent="RBX1">
      <Properties><string name="Name">Start</string><float name="Time">0</float></Properties>
      <Item class="Pose" referent="RBX2">
        <Properties>
          <string name="Name">Right Arm</string>
          <CoordinateFrame name="CFrame"><X>0</X><Y>0</Y><Z>0</Z><R00>1</R00><R01>0</R01><R02>0</R02><R10>0</R10><R11>1</R11><R12>0</R12><R20>0</R20><R21>0</R21><R22>1</R22></CoordinateFrame>
        </Properties>
      </Item>
    </Item>
    <Item class="Keyframe" referent="RBX3">
      <Properties><string name="Name">Up</string><float name="Time">0.75</float></Properties>
      <Item class="Pose" referent="RBX4">
        <Properties>
          <string name="Name">Right Arm</string>
          <float name="Weight">0.5</float>
          <CoordinateFrame name="CFrame"><X>0</X><Y>1</Y><Z>0</Z><R00>1</R00><R01>0</R01><R02>0</R02><R10>0</R10><R11>0</R11><R12>-1</R12><R20>0</R20><R21>1</R21><R22>0</R22></CoordinateFrame>
        </Properties>
      </Item>
    </Item>
  </Item>
</roblox>)XML";
        loadSourceFile(rt, "ReplicatedStorage/animations/Wave.rbxmx", wave, &err);
        loadSourceFile(rt, "ServerScriptService/Wave.server.luau", R"(
            local seq = game.ReplicatedStorage.animations.Wave
            local pose = seq.Up:FindFirstChild("Right Arm")
            print("sequence", seq.Loop, #seq:GetChildren(), seq.Up.Time, pose.CFrame.Position, math.floor(math.deg((pose.CFrame:ToEulerAnglesXYZ())) + 0.5), pose.Weight)
            local anim = Instance.new("Animation"); anim.Name = "Wave"; anim.AnimationId = "ReplicatedStorage.animations.Wave"; anim.Parent = workspace
            game.Players.PlayerAdded:Connect(function(player)
                player.CharacterAdded:Connect(function(char)
                    local hum = char:WaitForChild("Humanoid")
                    local track = hum.Animator:LoadAnimation(anim)
                    print("track", track.Length, track.Looped, track.IsPlaying, track.Speed, track.Animation == anim, track:IsA("AnimationTrack"))
                    print("keytime", track:GetTimeOfKeyframe("Up"), #hum:GetPlayingAnimationTracks())
                    track.Stopped:Connect(function() print("stopped", track.TimePosition) end)
                    track:Play(0.1, 1, 2)
                    print("playing", track.IsPlaying, track.Speed, #hum.Animator:GetPlayingAnimationTracks())
                    track:AdjustSpeed(0.5)
                    track:Stop(0)   -- no fade: it ends here and now (a fade is the engine's to run)
                    print("after", track.IsPlaying, track.Speed, #hum:GetPlayingAnimationTracks())
                end)
            end)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        rt.addPlayer("Ada", 1);
        rt.step(0.05);
        rt.step(0.05);
        check(log.has("sequence	true	2	0.75	0, 1, 0	90	0.5"), "a KeyframeSequence loads from a .rbxmx: Keyframes at a Time, Poses with a CFrame and a Weight");
        check(log.has("track	0.75	true	false	1	true	true"), "LoadAnimation reads the sequence: its Length, its Loop, the Animation it came from");
        check(log.has("keytime	0.75	0"), "GetTimeOfKeyframe finds a Keyframe by name; nothing is playing yet");
        check(log.has("playing	true	2	1"), "Play(fade, weight, speed) starts it: IsPlaying, Speed, and the Animator lists it");
        check(log.has("after	false	0.5	0") && log.has("stopped	0"), "AdjustSpeed and Stop(0), and Stopped fires");
        check(log.errors.empty(), "no errors");
    }

    section("R57. DataStore versions and listings");
    {
        std::string path = "harness_versions.json";
        std::remove(path.c_str());
        Runtime::Options o = serverOpts();
        o.dataStorePath = path;
        Log log;
        {
            Runtime rt(o, callbacks(log));
            rt.runChunk("dsv", R"(
                local DSS = game:GetService("DataStoreService")
                local ds = DSS:GetDataStore("PlayerData")
                local first = ds:SetAsync("ada", {coins = 1})
                local second = ds:SetAsync("ada", {coins = 2})
                ds:SetAsync("bob", {coins = 9})
                ds:SetAsync("other", 1)
                print("versions", first, second, first ~= second)
                local value, info = ds:GetAsync("ada")
                print("info", value.coins, info.Version, info.CreatedTime <= info.UpdatedTime, info.ClassName)
                local old, oldInfo = ds:GetVersionAsync("ada", first)
                print("old", old.coins, oldInfo.Version)
                local pages = ds:ListVersionsAsync("ada")
                local list = pages:GetCurrentPage()
                print("list", #list, list[1].Version, list[1].ClassName, list[2].IsDeleted, pages.IsFinished)
                local keys = ds:ListKeysAsync("")
                local names = {}
                for _, k in keys:GetCurrentPage() do table.insert(names, k.KeyName) end
                table.sort(names)
                print("keys", #names, table.concat(names, ","), keys:GetCurrentPage()[1].ClassName)
                local mine = ds:ListKeysAsync("a"):GetCurrentPage()
                print("prefix", #mine, mine[1].KeyName)
                ds:RemoveAsync("bob")
                local gone = ds:ListVersionsAsync("bob"):GetCurrentPage()
                print("gone", #gone, gone[#gone].IsDeleted, (ds:GetVersionAsync("bob", gone[#gone].Version)))
                local stores = DSS:ListDataStoresAsync():GetCurrentPage()
                print("stores", #stores, stores[1].DataStoreName, stores[1].ClassName, stores[1].UpdatedTime >= stores[1].CreatedTime)
                ds:RemoveVersionAsync("ada", first)
                print("pruned", #ds:ListVersionsAsync("ada"):GetCurrentPage(), select(2, pcall(function() ds:GetVersionAsync("ada", first) end)))
            )");
            check(log.has("versions	1	2	true"), "SetAsync hands back the version it wrote");
            check(log.has("info	2	2	true	DataStoreKeyInfo"), "GetAsync hands back a DataStoreKeyInfo with the value");
            check(log.has("old	1	1"), "GetVersionAsync reads an older write back");
            check(log.has("list	2	1	DataStoreObjectVersionInfo	false	true"), "ListVersionsAsync lists them oldest first");
            check(log.has("keys	3	ada,bob,other	DataStoreKey"), "ListKeysAsync lists the keys as DataStoreKeys");
            check(log.has("prefix	1	ada"), "and takes a prefix");
            check(log.has("gone	2	true	nil"), "a removal is a version of its own, marked deleted");
            check(log.has("stores	1	PlayerData	DataStoreInfo	true"), "ListDataStoresAsync lists the stores, with when they were written");
            check(log.has("pruned	1	dsv:29: No version 1 for key ada"), "RemoveVersionAsync drops one");
            check(log.errors.empty(), "no errors");
            rt.step(0.016);
        }
        Log log2;
        {
            Runtime rt(o, callbacks(log2));
            rt.runChunk("dsv2", R"(
                local ds = game:GetService("DataStoreService"):GetDataStore("PlayerData")
                local list = ds:ListVersionsAsync("ada"):GetCurrentPage()
                print("kept", #list, list[1].Version, (ds:GetVersionAsync("ada", list[1].Version)).coins)
            )");
            check(log2.has("kept	1	2	2"), "the versions outlive the runtime, in the same file as the values");
            check(log2.errors.empty(), "no errors reading it back");
        }
        std::remove(path.c_str());
    }

    section("R58. R15: the fifteen-part rig StarterPlayer asks for");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        std::string err;
        loadSourceFile(rt, "ServerScriptService/Rig.server.luau", R"(
            game:GetService("StarterPlayer").CharacterRigType = Enum.HumanoidRigType.R15
            game.Players.PlayerAdded:Connect(function(player)
                player.CharacterAdded:Connect(function(char)
                    local hum = char:WaitForChild("Humanoid")
                    print("rig", hum.RigType, #char:GetChildren())
                    print("parts", char.UpperTorso.Size, char.LowerTorso.Size, char.RightUpperArm.Size, char.RightHand.Size, char.LeftFoot.Size)
                    local function r(x) return math.floor(x * 100 + 0.5) / 100 end
                    print("where", r(char.Head.Position.Y - char.HumanoidRootPart.Position.Y),
                          r(char.RightUpperLeg.Position.Y - char.HumanoidRootPart.Position.Y), r(char.RightHand.Position.X - char.HumanoidRootPart.Position.X))
                    print("points", char.Head:FindFirstChild("HatAttachment") ~= nil, char.RightHand:FindFirstChild("RightGripAttachment").Position,
                          char.UpperTorso:FindFirstChild("NeckRigAttachment") ~= nil, char:FindFirstChild("Torso") == nil)
                    local ok, e = pcall(function() hum.RigType = Enum.HumanoidRigType.R6 end)
                    print("fixed", ok, tostring(hum.RigType))
                end)
            end)
        )", &err);
        rt.startScripts();
        rt.step(0.05);
        rt.addPlayer("Ada", 1);
        rt.step(0.05);
        rt.step(0.05);
        check(log.has("rig	Enum.HumanoidRigType.R15	17"), "an R15 character: fifteen parts and a Humanoid");
        check(log.has("parts	2, 1.6, 1	2, 0.4, 1	1, 1.2, 1	1, 0.4, 1	1, 0.2, 1"), "Roblox's R15 sizes");
        check(log.has("where	2.3	-0.9	1.5"), "laid out around the HumanoidRootPart as Studio lays them");
        check(log.has("points	true	0, -0.2, 0	true	true"), "the R15 attachment points, and no R6 Torso");
        check(log.has("fixed	true	Enum.HumanoidRigType.R6"), "Humanoid.RigType is writable: a custom rig's Humanoid says which rig it is");
        check(log.errors.empty(), "no errors");
    }

    section("R59. plugins: the host's record, not the tree");
    {
        Log log;
        Runtime rt(serverOpts(), callbacks(log));
        DataModel& dm = rt.dataModel();
        std::string err;
        // What a replicated tree or a Parent write leaves: a Script under the service the host never put there.
        Instance* svc = dm.getService("PluginDebugService");
        Instance::Ptr planted = dm.create("Script", svc);
        planted->setName("Planted");
        planted->set("Source", Value::string("print('PLANTED RAN')"));
        rt.startScripts();
        rt.step(0.05);
        check(!rt.impl().isPluginScript(*planted) && !log.has("PLANTED RAN"), "a Script merely sitting under PluginDebugService is not a plugin and does not run");
        // A place file addressed there is refused by the loader.
        check(!loadSourceFile(rt, "PluginDebugService/Evil.server.luau", "print('EVIL RAN')", &err) && err == "PluginDebugService is the Studio's: a place file cannot go there",
              ("a place file cannot be placed under PluginDebugService: " + err).c_str());
        check(!loadSourceFile(rt, "PluginDebugService/init.meta.json", "{}", &err) && err == "PluginDebugService is the Studio's: a place file cannot go there", "nor its meta");
        // The host's own two ways in are what make a plugin.
        rt.addPlugin("Real", "print('REAL', plugin.Name)");
        rt.addPluginFile("Filed.server.luau", "print('FILED', plugin.Name)");
        rt.step(0.05);
        Instance* real = svc->findFirstChild("Real");
        Instance* filed = svc->findFirstChild("Filed");
        check(real && rt.impl().isPluginScript(*real) && log.has("REAL	Real"), "addPlugin: a plugin, with its `plugin`");
        check(filed && rt.impl().isPluginScript(*filed) && log.has("FILED	Filed"), "addPluginFile: a plugin too, placed by the loader");
        Instance::Ptr under = dm.create("Script", real);
        check(rt.impl().isPluginScript(*under), "what a plugin puts under itself is the plugin's");
        rt.unloadPlugins();
        check(!svc->findFirstChild("Real") && !rt.impl().isPluginScript(*planted), "unloadPlugins clears the record");
        check(log.errors.empty(), "no errors");
    }
}
