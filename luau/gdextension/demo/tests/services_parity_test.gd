# The services family: the memory stores, MessagingService, the data stores' options, versions
# at a time and cursors, LogService and ScriptContext's events, CollectionService's tag events,
# LocalizationTables and Translators, TweenService:SmoothDamp and a Tween's Instance, the declined
# purchase and invite prompts, TeleportAsync's refusal, VRService's answers, Stats' counts, the
# session-kept badges and teleport settings, and the recorded-only members.
#
#   godot --headless --path . -s res://tests/services_parity_test.gd
#
# A Play Solo world with one joined player; a server script does the work.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var ok := 0
var bad := 0
var said: Array[String] = []
var lines: Array[String] = []

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("SVCPAR %-62s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	# The chat configurations Roblox keeps under TextChatService; a place file carries them here.
	for cfg in ["BubbleChatConfiguration", "ChatWindowConfiguration", "ChatInputBarConfiguration"]:
		world.load_file("TextChatService/%s.model.json" % cfg, '{"ClassName": "%s"}' % cfg)
	world.load_file("ServerScriptService/Services.server.luau", """
local Players = game:GetService("Players")
local p = Players:GetPlayers()[1] or Players.PlayerAdded:Wait()
task.wait(0.5)
local function b(v) return v and "t" or "f" end

-- the newcomers answer GetService, and the absent classes are declared
local svcs = ""
for _, n in ipairs({"MemoryStoreService", "MessagingService", "Stats", "UserService", "SharedTableRegistry", "AdService", "AnalyticsService",
                    "AvatarCreationService", "AvatarEditorService", "ExperienceNotificationService"}) do
	svcs = svcs .. b(pcall(function() return game:GetService(n).ClassName == n end) and game:GetService(n).ClassName == n)
end
print("SERVICES " .. svcs)
local made = ""
for _, n in ipairs({"DataStoreOptions", "DataStoreGetOptions", "DataStoreSetOptions", "DataStoreIncrementOptions", "TeleportOptions",
                    "BubbleChatMessageProperties", "ChatWindowMessageProperties"}) do
	local okc, i = pcall(Instance.new, n)
	made = made .. b(okc and i.ClassName == n)
end
print("CLASSES " .. made)

-- Tween: TweenBase in the chain, Instance and TweenInfo from the running tween
local part = Instance.new("Part")
part.Anchored = true
part.Parent = workspace
local info = TweenInfo.new(0.25, Enum.EasingStyle.Sine)
local tw = game:GetService("TweenService"):Create(part, info, {Transparency = 1})
print("TWEEN " .. b(tw:IsA("TweenBase")) .. " " .. b(tw.Instance == part) .. " " .. tostring(tw.TweenInfo.Time) .. " " .. tostring(tw.TweenInfo.EasingStyle) .. " " .. b(not pcall(function() tw.Instance = nil end)))

-- SmoothDamp: a number and a Vector3 move toward the target with a positive velocity
local v, vel = game:GetService("TweenService"):SmoothDamp(0, 10, 0, 0.5, nil, 1 / 30)
local v3, vel3 = game:GetService("TweenService"):SmoothDamp(Vector3.new(0, 0, 0), Vector3.new(0, 10, 0), Vector3.new(), 0.5, nil, 1 / 30)
print("SMOOTH " .. b(v > 0 and v < 10) .. " " .. b(vel > 0) .. " " .. b(v3.Y > 0 and v3.Y < 10) .. " " .. b(vel3.Y > 0) .. " " .. b(v3.X == 0))

-- HttpService: HttpEnabled is the host's option, a game script cannot set it; UrlEncode
local http = game:GetService("HttpService")
print("HTTP " .. tostring(http.HttpEnabled) .. " " .. b(not pcall(function() http.HttpEnabled = true end)) .. " " .. http:UrlEncode("a b.c/d_e-f"))

-- DataStore: the class, the global store, user ids and metadata with a write, a version at a time, cursors
local DSS = game:GetService("DataStoreService")
local ds = DSS:GetDataStore("ParityStore")
local gds = DSS:GetGlobalDataStore()
print("DSCLASS " .. ds.ClassName .. " " .. b(ds:IsA("GlobalDataStore")) .. " " .. gds.ClassName .. " " .. b(gds ~= ds))
local so = Instance.new("DataStoreSetOptions")
so:SetMetadata({tag = "meta"})
ds:SetAsync("alpha", {n = 1}, {123, 456}, so)
ds:SetAsync("beta", 2)
local val, ki = ds:GetAsync("alpha")
print("DSMETA " .. tostring(val.n) .. " " .. tostring(ki:GetUserIds()[2]) .. " " .. tostring(ki:GetMetadata().tag) .. " " .. tostring(so:GetMetadata().tag))
local atNow = ds:GetVersionAtTimeAsync("alpha", ki.UpdatedTime)
local atZero = ds:GetVersionAtTimeAsync("alpha", 0)
print("DSATTIME " .. tostring(atNow and atNow.n) .. " " .. tostring(atZero))
local _, ki2 = ds:UpdateAsync("beta", function(old) return old + 1, {7}, {k = "v"} end)
print("DSUPDATE " .. tostring(ds:GetAsync("beta")) .. " " .. tostring(ki2:GetUserIds()[1]) .. " " .. tostring(ki2:GetMetadata().k))
local pages = ds:ListKeysAsync("", 1)
local c0 = pages.Cursor
pages:AdvanceToNextPageAsync()
local c1 = pages.Cursor
local resumed = ds:ListKeysAsync("", 1, c1)
print("DSCURSOR [" .. c0 .. "] [" .. c1 .. "] " .. resumed:GetCurrentPage()[1].KeyName .. " " .. b(resumed.IsFinished))
local dopt = Instance.new("DataStoreOptions")
local gopt = Instance.new("DataStoreGetOptions")
dopt:SetExperimentalFeatures({v2 = true})
print("DSOPTS " .. tostring(dopt.AllScopes) .. " " .. tostring(gopt.UseCache))
dopt.AllScopes = true; gopt.UseCache = false
print("DSOPTSW " .. tostring(dopt.AllScopes) .. " " .. tostring(gopt.UseCache))

-- MemoryStoreService: a hash map, a sorted map with ranges, a queue that waits
local MSS = game:GetService("MemoryStoreService")
local hm = MSS:GetHashMap("ParityMap")
hm:SetAsync("a", {x = 1}, 60)
hm:SetAsync("b", "two", 60)
local upd = hm:UpdateAsync("a", function(old) old.x = old.x + 1 return old end, 60)
local listed = hm:ListItemsAsync(10):GetCurrentPage()
hm:RemoveAsync("b")
print("HASHMAP " .. tostring(hm:GetAsync("a").x) .. " " .. tostring(upd.x) .. " " .. tostring(#listed) .. " " .. listed[1].key .. " " .. tostring(hm:GetAsync("b")) .. " " .. hm.ClassName)
local sm = MSS:GetSortedMap("ParitySorted")
sm:SetAsync("p1", "low", 60, 10)
sm:SetAsync("p2", "high", 60, 30)
sm:SetAsync("p3", "mid", 60, 20)
local asc = sm:GetRangeAsync(Enum.SortDirection.Ascending, 10)
local desc = sm:GetRangeAsync(Enum.SortDirection.Descending, 2)
local above = sm:GetRangeAsync(Enum.SortDirection.Ascending, 10, {key = "p1", sortKey = 10})
local gv, gs = sm:GetAsync("p3")
print("SORTED " .. asc[1].key .. asc[2].key .. asc[3].key .. " " .. desc[1].key .. desc[2].key .. " " .. tostring(#above) .. above[1].key .. " " .. tostring(gv) .. tostring(gs) .. " " .. tostring(sm:GetSizeAsync()))
local q = MSS:GetQueue("ParityQueue", 1)
q:AddAsync("first", 60)
q:AddAsync("urgent", 60, 5)
local items, id = q:ReadAsync(2)
local sizeAll, sizeVisible = q:GetSizeAsync(), q:GetSizeAsync(true)
q:RemoveAsync(id)
local empty, noId = q:ReadAsync(1, false, 0.2)
print("QUEUE " .. items[1] .. items[2] .. " " .. b(type(id) == "string") .. " " .. tostring(sizeAll) .. tostring(sizeVisible) .. " " .. tostring(q:GetSizeAsync()) .. " " .. tostring(#empty) .. tostring(noId))
task.delay(0.3, function() q:AddAsync("late", 60) end)
local t0 = os.clock()
local waited = q:ReadAsync(1, false, 3)
print("QUEUEWAIT " .. tostring(waited[1]) .. " " .. b(os.clock() - t0 >= 0.25))

-- MessagingService: a subscriber hears a publish on this server
local MS = game:GetService("MessagingService")
local heardMsg = nil
local conn = MS:SubscribeAsync("parity", function(m) heardMsg = m end)
MS:PublishAsync("parity", {hello = "world"})
task.wait(0.3)
print("MESSAGING " .. tostring(heardMsg and heardMsg.Data.hello) .. " " .. b(heardMsg and type(heardMsg.Sent) == "number") .. " " .. typeof(conn))
conn:Disconnect()
heardMsg = nil
MS:PublishAsync("parity", 1)
task.wait(0.3)
print("MESSAGINGOFF " .. tostring(heardMsg))

-- LogService: MessageOut and the history hear a print and a warn
local LS = game:GetService("LogService")
local heardLog = {}
local lc = LS.MessageOut:Connect(function(msg, kind) if msg == "LOGTEST" or msg == "WARNTEST" then table.insert(heardLog, msg .. ":" .. tostring(kind)) end end)
print("LOGTEST")
warn("WARNTEST")
lc:Disconnect()
local hist = LS:GetLogHistory()
local last = hist[#hist]
print("LOG " .. tostring(heardLog[1]) .. " " .. tostring(heardLog[2]) .. " " .. tostring(last.message) .. " " .. tostring(last.messageType) .. " " .. b(type(last.timestamp) == "number"))

-- ScriptContext.Error hears an uncaught error
local SC = game:GetService("ScriptContext")
local heardErr = nil
local ec = SC.Error:Connect(function(msg, trace, src) heardErr = {msg = msg, trace = trace, src = src} end)
task.spawn(function() error("parityboom") end)
task.wait(0.1)
ec:Disconnect()
print("SCRIPTERR " .. b(heardErr ~= nil and string.find(heardErr.msg, "parityboom") ~= nil) .. " " .. b(heardErr and heardErr.trace == "") .. " " .. b(heardErr and heardErr.src == script))

-- CollectionService: TagAdded once for the first instance, TagRemoved once for the last; GetAllTags
local CS = game:GetService("CollectionService")
local added, removed = 0, 0
local ac = CS.TagAdded:Connect(function(tag) if tag == "ParityTag" then added += 1 end end)
local rc = CS.TagRemoved:Connect(function(tag) if tag == "ParityTag" then removed += 1 end end)
local p2 = Instance.new("Part")
p2.Parent = workspace
CS:AddTag(part, "ParityTag")
CS:AddTag(p2, "ParityTag")
local all = CS:GetAllTags()
local hasTag = false
for _, tg in ipairs(all) do if tg == "ParityTag" then hasTag = true end end
CS:RemoveTag(part, "ParityTag")
local midway = removed
CS:RemoveTag(p2, "ParityTag")
ac:Disconnect(); rc:Disconnect()
print("TAGS " .. tostring(added) .. " " .. tostring(midway) .. " " .. tostring(removed) .. " " .. b(hasTag))

-- LocalizationTable and Translator
local LOC = game:GetService("LocalizationService")
local lt = Instance.new("LocalizationTable")
lt.Parent = LOC
lt:SetEntryValue("greet", "Hello", "", "es", "Hola")
lt:SetEntryValue("greet", "Hello", "", "en-us", "Howdy")
lt:SetEntryValue("count", "You have {n} coins", "", "es", "Tienes {n} monedas")
lt:SetEntryExample("greet", "Hello", "", "a greeting")
local entries = lt:GetEntries()
local es = lt:GetTranslator("es")
local mx = LOC:GetTranslatorForLocaleAsync("es-mx")
local forPlayer = LOC:GetTranslatorForPlayerAsync(p)
print("LOCALE " .. LOC.RobloxLocaleId .. " " .. LOC.SystemLocaleId .. " " .. forPlayer.LocaleId .. " " .. es.ClassName)
print("ENTRIES " .. tostring(#entries) .. " " .. entries[1].Key .. " " .. entries[1].Example .. " " .. entries[1].Values.es .. " " .. tostring(#LOC:GetTableEntries(lt)))
print("TRANSLATE " .. es:Translate(nil, "Hello") .. " " .. mx:Translate(nil, "Hello") .. " " .. forPlayer:Translate(nil, "Hello") .. " [" .. es:Translate(nil, "Missing") .. "] " .. es:FormatByKey("count", {n = 5}) .. " " .. b(not pcall(es.FormatByKey, es, "nokey")))
lt:RemoveEntryValue("greet", "Hello", "", "es")
local afterRemove = es:Translate(nil, "Hello")
lt:RemoveTargetLocale("es")
local afterLocale = es:Translate(nil, "You have {n} coins")
lt:SetEntryKey("count", "", "", "coins")
lt:RemoveEntry("greet", "", "")
local left = lt:GetEntries()
print("LOCEDIT [" .. afterRemove .. "] [" .. afterLocale .. "] " .. tostring(#left) .. " " .. left[1].Key)
lt:SetEntries({{Key = "k", Source = "s", Context = "", Example = "", Values = {fr = "f"}}})
print("SETENTRIES " .. lt:GetTranslator("fr"):Translate(nil, "s") .. " " .. tostring(#lt:GetEntries()))

-- MarketplaceService: every prompt declines, on the next pass; nobody owns anything
local MP = game:GetService("MarketplaceService")
local fin = {}
MP.PromptPurchaseFinished:Connect(function(pl, id, bought) fin.p = tostring(pl == p) .. tostring(id) .. tostring(bought) end)
MP.PromptGamePassPurchaseFinished:Connect(function(pl, id, bought) fin.g = tostring(pl == p) .. tostring(id) .. tostring(bought) end)
MP.PromptProductPurchaseFinished:Connect(function(uid, id, bought) fin.d = tostring(uid == p.UserId) .. tostring(id) .. tostring(bought) end)
MP:PromptPurchase(p, 11)
MP:PromptGamePassPurchase(p, 22)
MP:PromptProductPurchase(p, 33)
local before = fin.p
task.wait(0.2)
local sub = MP:GetUserSubscriptionStatusAsync(p, "EXP-1")
print("MARKET " .. tostring(before) .. " " .. fin.p .. " " .. fin.g .. " " .. fin.d .. " " .. b(not MP:PlayerOwnsAssetAsync(p, 1)) .. b(not MP:UserOwnsGamePassAsync(p.UserId, 1)) .. b(not MP:PlayerOwnsBundleAsync(p, 1)) .. " " .. tostring(sub.IsSubscribed) .. " " .. tostring(#MP:GetDeveloperProductsAsync():GetCurrentPage()) .. " " .. b(not pcall(MP.GetProductInfoAsync, MP, 1)))

-- BadgeService: an award is kept for the session
local BS = game:GetService("BadgeService")
local hadBefore = BS:UserHasBadgeAsync(p.UserId, 99)
BS:AwardBadgeAsync(p.UserId, 99)
local checked = BS:CheckUserBadgesAsync(p.UserId, {98, 99, 100})
print("BADGE " .. b(not hadBefore) .. " " .. b(BS:UserHasBadgeAsync(p.UserId, 99)) .. " " .. tostring(#checked) .. tostring(checked[1]) .. " " .. b(not pcall(BS.GetBadgeInfoAsync, BS, 99)))

-- TeleportService: a teleport is refused, the settings and options keep their data
local TS = game:GetService("TeleportService")
local failed = nil
TS.TeleportInitFailed:Connect(function(pl, result, msg, placeId, opts) failed = tostring(pl == p) .. " " .. tostring(result) .. " " .. tostring(placeId) .. " " .. tostring(opts ~= nil) end)
local topts = Instance.new("TeleportOptions")
topts:SetTeleportData({spawn = "B"})
local okTp, err = pcall(TS.TeleportAsync, TS, 1234, {p}, topts)
TS:SetTeleportSetting("volume", {level = 3})
print("TELEPORT " .. b(not okTp) .. " " .. tostring(failed) .. " " .. tostring(topts:GetTeleportData().spawn) .. " " .. tostring(TS:GetTeleportSetting("volume").level) .. " " .. tostring(TS:GetTeleportSetting("none")) .. " " .. tostring(TS:GetArrivingTeleportGui()) .. " " .. tostring(TS:GetLocalPlayerTeleportData()))
print("TELEOPTS " .. topts.ReservedServerAccessCode .. "|" .. topts.ServerInstanceId .. "|" .. tostring(topts.ShouldReserveServer))
topts.ReservedServerAccessCode = "code"; topts.ServerInstanceId = "job"; topts.ShouldReserveServer = true
print("TELEOPTSW " .. topts.ReservedServerAccessCode .. "|" .. topts.ServerInstanceId .. "|" .. tostring(topts.ShouldReserveServer))

-- VRService: no headset; the touchpad modes and a navigation request
local VR = game:GetService("VRService")
local padChanged, navReq = nil, nil
VR.TouchpadModeChanged:Connect(function(pad, mode) padChanged = tostring(pad) .. " " .. tostring(mode) end)
VR.NavigationRequested:Connect(function(cf, which) navReq = tostring(cf.Position) .. " " .. tostring(which) end)
local modeBefore = VR:GetTouchpadMode(Enum.VRTouchpad.Left)
VR:SetTouchpadMode(Enum.VRTouchpad.Left, Enum.VRTouchpadMode.ABXY)
VR:RequestNavigation(CFrame.new(1, 2, 3), Enum.UserCFrame.RightHand)
VR:RecenterUserHeadCFrame()
print("VR " .. tostring(VR.VREnabled) .. " " .. tostring(VR:GetUserCFrame(Enum.UserCFrame.Head) == CFrame.new()) .. " " .. tostring(VR:GetUserCFrameEnabled(Enum.UserCFrame.Head)) .. " " .. tostring(modeBefore) .. " " .. tostring(VR:GetTouchpadMode(Enum.VRTouchpad.Left)) .. " " .. tostring(padChanged) .. " | " .. tostring(navReq))
print("VRPROPS " .. tostring(VR.AutomaticScaling) .. " " .. tostring(VR.AvatarGestures) .. " " .. tostring(VR.ControllerModels) .. " " .. tostring(VR.FadeOutViewOnCollision) .. " " .. tostring(VR.GuiInputUserCFrame) .. " " .. tostring(VR.LaserPointer) .. " " .. tostring(VR.ThirdPersonFollowCamEnabled))
VR.AutomaticScaling = Enum.VRScaling.Off; VR.AvatarGestures = true; VR.ControllerModels = Enum.VRControllerModelMode.Transparent; VR.FadeOutViewOnCollision = false
VR.GuiInputUserCFrame = Enum.UserCFrame.LeftHand; VR.LaserPointer = Enum.VRLaserPointerMode.Pointer; VR.ThirdPersonFollowCamEnabled = true
print("VRPROPSW " .. tostring(VR.AutomaticScaling) .. " " .. tostring(VR.AvatarGestures) .. " " .. tostring(VR.ControllerModels) .. " " .. tostring(VR.FadeOutViewOnCollision) .. " " .. tostring(VR.GuiInputUserCFrame) .. " " .. tostring(VR.LaserPointer) .. " " .. tostring(VR.ThirdPersonFollowCamEnabled) .. " " .. b(not pcall(function() VR.VREnabled = true end)))

-- Stats: the counts come from the tree, the memory from the sandbox, the rest read zero
local ST = game:GetService("Stats")
local partsBefore = ST.PrimitivesCount
local p3 = Instance.new("Part")
p3.Parent = workspace
print("STATS " .. b(ST.InstanceCount > 10) .. " " .. tostring(ST.PrimitivesCount - partsBefore) .. " " .. b(ST:GetTotalMemoryUsageMb() > 0) .. " " .. b(ST:GetMemoryUsageMbForTag(Enum.DeveloperMemoryTag.LuaHeap) > 0) .. " " .. tostring(ST:GetMemoryUsageMbForTag(Enum.DeveloperMemoryTag.Internal)) .. " " .. tostring(ST.FrameTime) .. tostring(ST.ContactsCount) .. tostring(ST.SceneTriangleCount) .. " " .. b(not pcall(function() ST.FrameTime = 1 end)) .. " " .. tostring(ST.MemoryTrackingEnabled))
ST.MemoryTrackingEnabled = true
print("STATSW " .. tostring(ST.MemoryTrackingEnabled))

-- the social, group, policy, user and voice answers
local SS = game:GetService("SocialService")
local inviteClosed = nil
SS.GameInvitePromptClosed:Connect(function(pl, ids) inviteClosed = tostring(pl == p) .. tostring(#ids) end)
SS:PromptGameInvite(p)
SS:HideSelfView(); SS:ShowSelfView()
local EN = game:GetService("ExperienceNotificationService")
local optClosed = false
EN.OptInPromptClosed:Connect(function() optClosed = true end)
EN:PromptOptIn()
task.wait(0.2)
local pol = game:GetService("PolicyService"):GetPolicyInfoForPlayerAsync(p)
local infos = game:GetService("UserService"):GetUserInfosByUserIdsAsync({p.UserId, 424242})
local GS = game:GetService("GroupService")
print("SOCIAL " .. tostring(inviteClosed) .. " " .. b(not SS:CanSendGameInviteAsync(p)) .. b(not SS:CanSendCallInviteAsync(p)) .. " " .. tostring(#SS:GetPlayersByPartyId("x")) .. " " .. b(optClosed) .. b(not EN:CanPromptOptInAsync()))
print("POLICY " .. tostring(pol.IsSubjectToChinaPolicies) .. " " .. tostring(pol.ArePaidRandomItemsRestricted) .. " " .. tostring(#pol.AllowedExternalLinkReferences > 0) .. " " .. b(not game:GetService("PolicyService"):CanViewBrandProjectAsync(p, 1)))
print("USERS " .. tostring(#infos) .. " " .. b(infos[1].Username == p.Name) .. " " .. b(infos[1].Id == p.UserId) .. " " .. b(infos[1].DisplayName == p.DisplayName))
print("GROUPS " .. tostring(#GS:GetGroupsAsync(p.UserId)) .. " " .. tostring(GS:GetAlliesAsync(1).IsFinished) .. " " .. tostring(#GS:GetEnemiesAsync(1):GetCurrentPage()) .. " " .. b(not pcall(GS.GetGroupInfoAsync, GS, 1)) .. b(not pcall(GS.GetRolesInGroupAsync, GS, 1)))
print("VOICE " .. tostring(game:GetService("VoiceChatService"):IsVoiceEnabledForUserIdAsync(p.UserId)) .. " " .. tostring(game:GetService("RunService"):IsResimulating()))

-- TestService: the checks count
local TSV = game:GetService("TestService")
print("TESTDEF " .. tostring(TSV.AutoRuns) .. " " .. tostring(TSV.Timeout) .. " " .. tostring(TSV.NumberOfPlayers) .. " " .. tostring(TSV.IsSleepAllowed) .. " " .. tostring(TSV.ThrottlePhysicsToRealtime) .. " " .. tostring(TSV.ExecuteWithStudioRun) .. " " .. tostring(TSV.SimulateSecondsLag) .. " [" .. TSV.Description .. "] " .. tostring(TSV.TestCount))
TSV:Check(true, "fine")
TSV:Check(false, "parity check failed")
TSV:Warn(false, "parity warning")
TSV:Message("parity message")
TSV:Checkpoint("parity checkpoint")
TSV.Timeout = 20; TSV.Description = "d"; TSV.NumberOfPlayers = 2
print("TESTSVC " .. tostring(TSV.ErrorCount) .. " " .. tostring(TSV.WarnCount) .. " " .. tostring(TSV:isFeatureEnabled("x")) .. " " .. tostring(TSV.Timeout) .. " " .. TSV.Description .. " " .. tostring(TSV.NumberOfPlayers))

-- the chat configurations, the message properties and a channel's requester
local TCS = game:GetService("TextChatService")
local bubble = TCS:WaitForChild("BubbleChatConfiguration")
local win = TCS:WaitForChild("ChatWindowConfiguration")
local bar = TCS:WaitForChild("ChatInputBarConfiguration")
print("CHATCFG " .. tostring(bubble.Font) .. " " .. tostring(win.AbsolutePosition) .. " " .. tostring(bar.AbsoluteSize) .. " " .. tostring(bar.IsFocused) .. " " .. tostring(bar.TextBox) .. " " .. b(not pcall(function() win.AbsoluteSize = Vector2.new(1, 1) end)))
bubble.Font = Enum.Font.GothamBold
local bmp = Instance.new("BubbleChatMessageProperties")
local derived = win:DeriveNewMessageProperties()
print("CHATCFGW " .. tostring(bubble.FontFace.Weight) .. " " .. tostring(bmp.TailVisible) .. " " .. tostring(bmp.TextSize) .. " " .. tostring(bmp.BackgroundTransparency) .. " " .. derived.ClassName .. " " .. tostring(derived.TextSize) .. " " .. b(derived:IsA("TextChatMessageProperties")))
bmp.TailVisible = false; bmp.TextSize = 20; derived.TextStrokeTransparency = 0
print("CHATMPW " .. tostring(bmp.TailVisible) .. " " .. tostring(bmp.TextSize) .. " " .. tostring(derived.TextStrokeTransparency))
local general = TCS:WaitForChild("TextChannels"):WaitForChild("RBXGeneral")
local src = general:FindFirstChildOfClass("TextSource")
general:SetDirectChatRequester(src)
local direct = TCS:CanUsersDirectChatAsync(p.UserId, {1, 2})
print("CHANNEL " .. b(general.DirectChatRequester == src and src ~= nil) .. " " .. tostring(#direct) .. " " .. b(not pcall(function() return TCS.ChatTranslationEnabled end)))

-- the recorded-only members read their defaults and take a write
local SND = game:GetService("SoundService")
print("SOUNDREC " .. tostring(SND.AmbientReverb) .. " " .. tostring(SND.DistanceFactor) .. " " .. tostring(SND.DopplerScale) .. " " .. tostring(SND.RolloffScale) .. " " .. tostring(SND.VolumetricAudio) .. " " .. b(not pcall(function() SND.CharacterSoundsUseNewApi = true end)))
SND.AmbientReverb = Enum.ReverbType.Cave; SND.DistanceFactor = 10; SND.DopplerScale = 2; SND.RolloffScale = 3; SND.VolumetricAudio = Enum.VolumetricAudio.Enabled
print("SOUNDRECW " .. tostring(SND.AmbientReverb) .. " " .. tostring(SND.DistanceFactor) .. " " .. tostring(SND.DopplerScale) .. " " .. tostring(SND.RolloffScale) .. " " .. tostring(SND.VolumetricAudio))
local PPS = game:GetService("ProximityPromptService")
local SSS = game:GetService("ServerScriptService")
local CP = game:GetService("ContentProvider")
local VC = game:GetService("VoiceChatService")
print("MISCREC " .. tostring(PPS.MaxIndicatorsVisible) .. " " .. tostring(SSS.LoadStringEnabled) .. " [" .. CP.BaseUrl .. "] " .. tostring(CP:GetAssetFetchStatus("rbxassetid://1")) .. " " .. typeof(CP:GetAssetFetchStatusChangedSignal("rbxassetid://1")) .. " " .. b(not pcall(function() VC.EnableDefaultVoice = false end)) .. " " .. b(not pcall(function() return VC.EnableVoiceVolumeControls end)))
PPS.MaxIndicatorsVisible = 3; SSS.LoadStringEnabled = true
print("MISCRECW " .. tostring(PPS.MaxIndicatorsVisible) .. " " .. tostring(SSS.LoadStringEnabled) .. " " .. b(not pcall(function() CP.BaseUrl = "x" end)))

-- the enums and the pure mappings
local AES = game:GetService("AvatarEditorService")
print("ENUMS " .. tostring(Enum.TeleportResult.Failure.Value) .. " " .. tostring(Enum.VRTouchpadMode.ABXY.Value) .. " " .. tostring(Enum.SortDirection.Descending.Value) .. " " .. tostring(Enum.AvatarAssetType.HairAccessory.Value) .. " " .. tostring(AES:GetAccessoryType(Enum.AvatarAssetType.HairAccessory)) .. " " .. tostring(AES:GetAccessoryType(Enum.AvatarAssetType.Hat)) .. " " .. tostring(AES:GetAccessoryType(Enum.AvatarAssetType.Image)))
game:GetService("AnalyticsService"):LogCustomEvent(p, "parity", 1)
game:GetService("ReplicatedFirst"):RemoveDefaultLoadingScreen()
print("EVENTS " .. typeof(MP.PromptBulkPurchaseFinished) .. " " .. typeof(SS.ShareSheetClosed) .. " " .. typeof(TS.LocalPlayerArrivedFromTeleport) .. " " .. typeof(PPS.IndicatorShown) .. " " .. typeof(game:GetService("RunService").Misprediction) .. " " .. typeof(CP.AssetFetchFailed) .. " " .. typeof(TCS.BubbleDisplayed) .. " " .. typeof(AES.PromptSaveAvatarCompleted) .. " " .. typeof(TSV.ServerCollectResult))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	if t < 25.0 and not heard.call("DONE"):
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("the new services answer GetService", heard.call("SERVICES tttttttttt"), true)
	check("the new creatable classes are declared", heard.call("CLASSES ttttttt"), true)
	check("TweenBase sits in the chain; Instance and TweenInfo read from the tween", heard.call("TWEEN t t 0.25 Enum.EasingStyle.Sine t"), true)
	check("SmoothDamp moves a number and a Vector3 toward the target", heard.call("SMOOTH t t t t t"), true)
	check("HttpEnabled is the host's option; a game script cannot set it; UrlEncode", heard.call("HTTP false t a%20b%2Ec%2Fd_e-f"), true)
	check("GetDataStore is a DataStore, GetGlobalDataStore a GlobalDataStore apart", heard.call("DSCLASS DataStore t GlobalDataStore t"), true)
	check("SetAsync keeps the user ids and the options' metadata on the key info", heard.call("DSMETA 1 456 meta meta"), true)
	check("GetVersionAtTimeAsync finds the version at the time, none before", heard.call("DSATTIME 1 nil"), true)
	check("UpdateAsync's transform hands back user ids and metadata too", heard.call("DSUPDATE 3 7 v"), true)
	check("a key listing's Cursor resumes it", heard.call("DSCURSOR [] [1] beta t"), true)
	check("the option objects read their defaults", heard.call("DSOPTS false true"), true)
	check("and take a write", heard.call("DSOPTSW true false"), true)
	check("a hash map sets, updates, lists and removes", heard.call("HASHMAP 2 2 2 a nil MemoryStoreHashMap"), true)
	check("a sorted map ranges by sort key, both ways, above a bound", heard.call("SORTED p1p3p2 p2p3 2p3 mid20 3"), true)
	check("a queue reads by priority, hides what was read, removes by id", heard.call("QUEUE urgentfirst t 20 0 0nil"), true)
	check("and a read waits for an item to arrive", heard.call("QUEUEWAIT late t"), true)
	check("MessagingService delivers a publish to this server's subscriber", heard.call("MESSAGING world t RBXScriptConnection"), true)
	check("and not after a disconnect", heard.call("MESSAGINGOFF nil"), true)
	check("LogService.MessageOut and the history hear a print and a warn", heard.call("LOG LOGTEST:Enum.MessageType.MessageOutput WARNTEST:Enum.MessageType.MessageWarning WARNTEST Enum.MessageType.MessageWarning t"), true)
	check("ScriptContext.Error hears an uncaught error, with the script", heard.call("SCRIPTERR t t t"), true)
	check("TagAdded once for the first instance, TagRemoved once for the last; GetAllTags", heard.call("TAGS 1 0 1 t"), true)
	check("the locale is en-us; a Translator is made", heard.call("LOCALE en-us en-us en-us Translator"), true)
	check("GetEntries reads what SetEntryValue and SetEntryExample wrote", heard.call("ENTRIES 2 greet a greeting Hola 2"), true)
	check("Translate by locale, es-mx falling back to es, the player's en-us; FormatByKey fills and errors on a missing key", heard.call("TRANSLATE Hola Hola Howdy [] Tienes 5 monedas t"), true)
	check("RemoveEntryValue, RemoveTargetLocale, SetEntryKey and RemoveEntry", heard.call("LOCEDIT [] [] 1 coins"), true)
	check("SetEntries replaces the table", heard.call("SETENTRIES f 1"), true)
	check("the purchase prompts decline a pass later, the product one by user id", heard.call("MARKET nil true11false true22false true33false ttt false 0 t"), true)
	check("a badge award is kept for the session", heard.call("BADGE t t 199 t"), true)
	check("TeleportAsync is refused with TeleportInitFailed; settings and options keep data", heard.call("TELEPORT t true Enum.TeleportResult.Failure 1234 true B 3 nil nil nil"), true)
	check("TeleportOptions' recorded members read their defaults", heard.call("TELEOPTS ||false"), true)
	check("and take a write", heard.call("TELEOPTSW code|job|true"), true)
	check("VRService: no headset, the touchpad modes and a navigation request", heard.call("VR false true false Enum.VRTouchpadMode.Touch Enum.VRTouchpadMode.ABXY Enum.VRTouchpad.Left Enum.VRTouchpadMode.ABXY | 1, 2, 3 Enum.UserCFrame.RightHand"), true)
	check("VRService's recorded members read their defaults", heard.call("VRPROPS Enum.VRScaling.World false Enum.VRControllerModelMode.Disabled true Enum.UserCFrame.Head Enum.VRLaserPointerMode.Disabled false"), true)
	check("and take a write; VREnabled is read only", heard.call("VRPROPSW Enum.VRScaling.Off true Enum.VRControllerModelMode.Transparent false Enum.UserCFrame.LeftHand Enum.VRLaserPointerMode.Pointer true t"), true)
	check("Stats counts instances and parts, measures the Lua heap, the rest read zero", heard.call("STATS t 1 t t 0 000 t false"), true)
	check("MemoryTrackingEnabled takes a write", heard.call("STATSW true"), true)
	check("the invite prompt closes, nobody can invite or call, the opt-in closes", heard.call("SOCIAL true0 tt 0 tt"), true)
	check("the policy is an unrestricted account's", heard.call("POLICY false false true t"), true)
	check("UserService knows the players in the game, and leaves unknown ids out", heard.call("USERS 1 t t t"), true)
	check("GroupService: no groups, empty allies and enemies, the info calls error", heard.call("GROUPS 0 true 0 tt"), true)
	check("no voice, no resimulation", heard.call("VOICE false false"), true)
	check("TestService's members read their defaults", heard.call("TESTDEF true 10 0 true true false 0 [] 0"), true)
	check("its checks count, and the writable ones take a write", heard.call("TESTSVC 1 1 false 20 d 2"), true)
	check("the chat configurations' new members read their zeros; the bubble Font is GothamMedium", heard.call("CHATCFG Enum.Font.GothamMedium 0, 0 0, 0 false nil t"), true)
	check("Font follows into FontFace; the message properties read their defaults", heard.call("CHATCFGW Enum.FontWeight.Bold true 14 0.3 ChatWindowMessageProperties 14 t"), true)
	check("and take a write", heard.call("CHATMPW false 20 0"), true)
	check("SetDirectChatRequester keeps its source; CanUsersDirectChatAsync answers the ids; the engine's flags are unreadable", heard.call("CHANNEL t 2 t"), true)
	check("SoundService's recorded members read their defaults", heard.call("SOUNDREC Enum.ReverbType.NoReverb 3.33 1 1 Enum.VolumetricAudio.Automatic t"), true)
	check("and take a write", heard.call("SOUNDRECW Enum.ReverbType.Cave 10 2 3 Enum.VolumetricAudio.Enabled"), true)
	check("the other recorded members read their defaults; fetch status None; the plugin-only ones refuse", heard.call("MISCREC 0 false [] Enum.AssetFetchStatus.None RBXScriptSignal t t"), true)
	check("and take a write; BaseUrl is read only", heard.call("MISCRECW 3 true t"), true)
	check("the enums resolve and GetAccessoryType maps", heard.call("ENUMS 1 2 1 41 Enum.AccessoryType.Hair Enum.AccessoryType.Hat Enum.AccessoryType.Unknown"), true)
	check("the declared-only events are signals", heard.call("EVENTS RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal"), true)
	var stray := 0
	for l in said:
		if l.find("ERROR") != -1 and l.find("parityboom") == -1 and l.find("parity check failed") == -1:
			stray += 1
	check("no script error beyond the two the script raises on purpose", stray, 0)
	for l in lines:
		print(l)
	if bad > 0:
		for l in said:
			print("SVCPAR said: " + l)
	print("SVCPAR %d passed, %d failed" % [ok, bad])
	print("services parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
