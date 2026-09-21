// Both of Roblox's chat APIs over one remote: the Players service instance.
// TextChatService routes a TextChatMessage through channels and callbacks,
// LegacyChatService sends the text alone; both land a line in `chatLines`.
#include "rbx_internal.h"

#include <cstdio>
#include <cstring>

namespace pulseblockz::rbx {

// 200 characters is Roblox's chat limit
std::string chatText(const std::string& text) {
    size_t a = text.find_first_not_of(" \t\r\n"), b = text.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    std::string s = text.substr(a, b - a + 1);
    if (s.size() > 200) s.resize(200);
    return s;
}

// Roblox's ChatScript name hash, so a player gets the same colour in every place
Col3 nameColor(const std::string& name) {
    static const Col3 palette[8] = {
        {253 / 255.f, 41 / 255.f, 67 / 255.f},    // Bright red
        {1 / 255.f, 162 / 255.f, 255 / 255.f},    // Bright blue
        {2 / 255.f, 184 / 255.f, 87 / 255.f},     // Earth green
        {107 / 255.f, 50 / 255.f, 124 / 255.f},   // Bright violet
        {218 / 255.f, 133 / 255.f, 65 / 255.f},   // Bright orange
        {245 / 255.f, 205 / 255.f, 48 / 255.f},   // Bright yellow
        {232 / 255.f, 186 / 255.f, 200 / 255.f},  // Light reddish violet
        {215 / 255.f, 197 / 255.f, 154 / 255.f},  // Brick yellow
    };
    int value = 0, n = (int)name.size();
    for (int i = 0; i < n; i++) {
        int c = (unsigned char)name[i];
        int reverse = n - i;                     // 1-based index from the end
        if (n % 2 == 1) reverse--;
        if (reverse % 4 >= 2) c = -c;
        value += c;
    }
    return palette[((value % 8) + 8) % 8];
}

// Chat lines are Roblox rich text, so typed text is escaped rather than parsed as markup
std::string escapeRich(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;"; else if (c == '>') out += "&gt;"; else out += c;
    }
    return out;
}

static std::string hex(Col3 c) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02X%02X%02X", (int)(c.r * 255 + 0.5f), (int)(c.g * 255 + 0.5f), (int)(c.b * 255 + 0.5f));
    return buf;
}

static std::string namePrefix(const std::string& name) {
    return "<font color=\"" + hex(nameColor(name)) + "\">" + escapeRich(name) + "</font>: ";
}

static Value statusValue(int status) {
    const EnumItem* item = findEnum("TextChatMessageStatus")->findValue(status);
    return Value::enumItem(item->name, item->value);
}

static std::string displayName(Instance& player) {
    std::string name = player.get("DisplayName").s;
    return name.empty() ? player.name() : name;
}

// ---- callbacks ------------------------------------------------------------------------
const Callback* Runtime::Impl::callback(Instance& i, const char* name) {
    if (!i.binding) return nullptr;
    auto& cbs = binding(i).callbacks;
    auto it = cbs.find(name);
    return it == cbs.end() ? nullptr : &it->second;
}

// Results reach `take` only if the callback finishes now; a yield, an error or a
// frame-budget deferral leaves it to run later with nobody listening.
void Runtime::Impl::callSync(const Callback& cb, const std::function<int(lua_State*)>& pushArgs, const std::function<void(lua_State*, int)>& take) {
    Task* t = newTask(nullptr, cb.ctx);
    lua_getref(t->co, cb.ref);
    int n = pushArgs(t->co);
    t->sink = &take;
    resume(t, n);
}

// ---- TextChatService --------------------------------------------------------------------
Instance* Runtime::Impl::textChannels() {
    Instance* tcs = dm.getService("TextChatService");
    return tcs->findFirstChild("TextChannels");
}

Instance* Runtime::Impl::textSourceOf(Instance& channel, int64_t userId) {
    for (auto& c : channel.children()) if (c->isA("TextSource") && (int64_t)c->get("UserId").n == userId) return c.get();
    return nullptr;
}

Instance* Runtime::Impl::playerOfSource(Instance* source) {
    if (!source) return nullptr;
    int64_t userId = (int64_t)source->get("UserId").n;
    for (auto& p : dm.getService("Players")->children()) if (p->isA("Player") && (int64_t)p->get("UserId").n == userId) return p.get();
    return nullptr;
}

// Backs TextChannel:AddUserAsync
Instance* Runtime::Impl::addTextSource(Instance& channel, Instance& player) {
    int64_t userId = (int64_t)player.get("UserId").n;
    if (Instance* have = textSourceOf(channel, userId)) return have;
    Instance::Ptr src = dm.createInternal("TextSource");
    src->setName(player.name());
    src->setInternal("UserId", Value::number((double)userId));
    src->setParent(&channel);
    return src.get();
}

// The auto-join channels, named rather than matched on the "RBX" prefix: RBXWhisper is
// Roblox's name for a two-person channel, and a prefix match would join every joiner to it.
static bool isDefaultChannel(const std::string& name) {
    return name == "RBXGeneral" || name == "RBXSystem";
}

static bool isWhisperChannel(const std::string& name) {
    return name.rfind("RBXWhisper", 0) == 0;
}

// The server's TextChannels, built at start-up before any player joins
void Runtime::Impl::makeDefaultChannels() {
    Instance* tcs = dm.getService("TextChatService");
    if (tcs->findFirstChild("TextChannels")) return;
    Instance::Ptr channels = dm.createInternal("Folder");
    channels->setName("TextChannels");
    channels->setParent(tcs);
    for (const char* name : {"RBXGeneral", "RBXSystem"}) {
        Instance::Ptr ch = dm.createInternal("TextChannel");
        ch->setName(name);
        ch->setParent(channels.get());
    }
    Instance::Ptr commands = dm.createInternal("Folder");
    commands->setName("TextChatCommands");
    commands->setParent(tcs);
}

// Server-side; a leaving player loses its TextSource in every channel, not just the defaults
void Runtime::Impl::playerChannels(Instance& player, bool joining) {
    Instance* channels = textChannels();
    if (!channels) return;
    if (joining) {
        for (auto& ch : channels->children()) if (ch->isA("TextChannel") && isDefaultChannel(ch->name())) addTextSource(*ch, player);
        return;
    }
    int64_t userId = (int64_t)player.get("UserId").n;
    for (auto& ch : channels->children()) if (ch->isA("TextChannel")) if (Instance* s = textSourceOf(*ch, userId)) s->destroy();
}

Instance::Ptr Runtime::Impl::newMessage(Instance& channel, const std::string& text, Instance* source, const std::string& meta, int status) {
    Instance::Ptr msg = dm.createInternal("TextChatMessage");
    msg->setInternal("Text", Value::string(text));
    msg->setInternal("Metadata", Value::string(meta));
    msg->setInternal("TextChannel", Value::instance(channel.id()));
    msg->setInternal("Timestamp", Value::number(now));
    if (source) {
        msg->setInternal("TextSource", Value::instance(source->id()));
        if (Instance* p = playerOfSource(source)) msg->setInternal("PrefixText", Value::string(namePrefix(displayName(*p))));
    }
    char id[24]; std::snprintf(id, sizeof id, "%s%d", dm.isServer() ? "s" : "c", nextMessageId++);
    msg->setInternal("MessageId", Value::string(id));
    msg->setInternal("Status", statusValue(status));
    return msg;
}

// TextChannel:SendAsync (client): nothing is displayed here, the server's copy
// comes back through chatRemote and is shown then.
Instance::Ptr Runtime::Impl::sendAsync(Instance& channel, const std::string& raw, const std::string& meta) {
    Instance* source = localPlayer ? textSourceOf(channel, (int64_t)localPlayer->get("UserId").n) : nullptr;
    std::string text = chatText(raw);
    Instance::Ptr msg = newMessage(channel, text, source, meta, 3 /* Sending */);
    if (!source || !source->get("CanSend").b || text.empty()) {
        msg->setInternal("Status", statusValue(!source || !source->get("CanSend").b ? 7 /* InvalidTextChannelPermissions */ : 1 /* Unknown */));
        return msg;
    }
    fireValues(*dm.getService("TextChatService"), "SendingMessage", {Value::instance(msg->id())});
    // A TextChatCommand fires Triggered on both sides and never becomes a message
    if (!text.empty() && text[0] == '/') {
        std::string word = text.substr(0, text.find(' '));
        for (Instance* d : dm.getService("TextChatService")->getDescendants()) {
            if (!d->isA("TextChatCommand") || !d->get("Enabled").b) continue;
            if (d->get("PrimaryAlias").s != word && d->get("SecondaryAlias").s != word) continue;
            fireValues(*d, "Triggered", {Value::instance(source->id()), Value::string(text)});
            RemoteMsg up; up.kind = RemoteMsg::Event; up.remote = dm.getService("Players")->id();
            up.args = {NetValue::string("Command"), NetValue::instance(d->id()), NetValue::string(text)};
            outRemotes.push_back(std::move(up));
            msg->setInternal("Status", statusValue(2 /* Success */));
            return msg;
        }
    }
    sentMessages[msg->get("MessageId").s] = msg;
    RemoteMsg up; up.kind = RemoteMsg::Event; up.remote = dm.getService("Players")->id();
    up.args = {NetValue::string("Send"), NetValue::instance(channel.id()), NetValue::string(msg->get("MessageId").s), NetValue::string(text), NetValue::string(meta)};
    outRemotes.push_back(std::move(up));
    return msg;
}

// The client's receive path, in Roblox's order: the channel's callback and event
// run before the service's.
void Runtime::Impl::incoming(Instance& msg) {
    Instance* channel = dm.findRef(msg.get("TextChannel").ref);
    Instance* tcs = dm.getService("TextChatService");
    auto override = [&](Instance* on) {
        const Callback* cb = on ? callback(*on, "OnIncomingMessage") : nullptr;
        if (!cb) return;
        callSync(*cb, [&](lua_State* co) { pushInstance(co, &msg); return 1; }, [&](lua_State* co, int n) {
            if (n < 1) return;
            Instance* props = toInstance(co, -n);
            if (!props || !props->isA("TextChatMessageProperties")) return;
            if (!props->get("PrefixText").s.empty()) msg.setInternal("PrefixText", props->get("PrefixText"));
            if (!props->get("Text").s.empty()) msg.setInternal("Text", props->get("Text"));
        });
    };
    override(channel);
    override(tcs);
    if (channel) fireValues(*channel, "MessageReceived", {Value::instance(msg.id())});
    fireValues(*tcs, "MessageReceived", {Value::instance(msg.id())});
    Instance* source = dm.findRef(msg.get("TextSource").ref);
    Instance* player = playerOfSource(source && source->isA("TextSource") ? source : nullptr);
    Runtime::ChatLine line;
    line.text = msg.get("Text").s;
    line.rich = msg.get("PrefixText").s + escapeRich(line.text);
    if (player) { line.speaker = player->id(); line.name = displayName(*player); line.color = nameColor(line.name); }
    else line.system = true;
    // A bubble is visible to everyone in sight of the speaker, which would leak the whisper
    if (channel && isWhisperChannel(channel->name())) line.bubble = false;
    chatLines.push_back(std::move(line));
    if (player) fireValues(*player, "Chatted", {Value::string(msg.get("Text").s), Value::nil()});
}

// TextChannel:DisplaySystemMessage: no TextSource, and never leaves this client
Instance::Ptr Runtime::Impl::systemMessage(Instance& channel, const std::string& text, const std::string& meta) {
    Instance::Ptr msg = newMessage(channel, text, nullptr, meta, 2 /* Success */);
    incoming(*msg);
    return msg;
}

// ---- the wire ---------------------------------------------------------------------------
// What Runtime::chat, the engine's chat box, sends. ChatVersion defaults to TextChatService
// (rbx_instance.cpp); the Players remote is for a place that opted back into LegacyChatService.
void Runtime::Impl::chatSend(const std::string& text) {
    if (dm.isServer() || !localPlayer) return;
    std::string s = chatText(text);
    if (s.empty()) return;
    Instance* tcs = dm.getService("TextChatService");
    Instance* channels = textChannels();
    Instance* general = channels ? channels->findFirstChild("RBXGeneral") : nullptr;
    if (tcs->get("ChatVersion").s == "TextChatService" && general && general->isA("TextChannel")) { sendAsync(*general, s, ""); return; }
    RemoteMsg msg; msg.kind = RemoteMsg::Event; msg.remote = dm.getService("Players")->id();
    msg.args = {NetValue::string("Chat"), NetValue::string(s)};
    outRemotes.push_back(std::move(msg));
}

// deliverRemote's Players branch. args[0] tags the wire: "Chat" (legacy, both ways),
// "Send"/"Msg" (TextChatService up/down), "Command" (up), "Bubble" (Chat:Chat, down).
void Runtime::Impl::chatRemote(const RemoteMsg& m, Instance* sender) {
    bool server = dm.isServer();
    if (m.args.empty() || m.args[0].type != NetValue::String) return;
    const std::string& what = m.args[0].s;
    Instance* players = dm.getService("Players");
    if (server && sender && what == "Chat" && m.args.size() == 2 && m.args[1].type == NetValue::String) {
        std::string s = chatText(m.args[1].s);
        if (s.empty()) return;
        fireValues(*sender, "Chatted", {Value::string(s), Value::nil()});
        RemoteMsg down; down.kind = RemoteMsg::Event; down.remote = players->id();
        down.args = {NetValue::string("Chat"), NetValue::instance(sender->id()), NetValue::string(displayName(*sender)), NetValue::string(s)};
        outRemotes.push_back(std::move(down));
    } else if (!server && what == "Chat" && m.args.size() == 4 && m.args[1].type == NetValue::Ref) {
        Runtime::ChatLine line; line.speaker = m.args[1].ref; line.name = m.args[2].s; line.text = m.args[3].s;
        line.color = nameColor(line.name);
        line.rich = namePrefix(line.name) + escapeRich(line.text);
        chatLines.push_back(line);
        if (Instance* who = dm.find(line.speaker); who && who->isA("Player")) fireValues(*who, "Chatted", {Value::string(line.text), Value::nil()});
    } else if (!server && what == "Bubble" && m.args.size() == 4 && m.args[1].type == NetValue::Ref) {
        Runtime::ChatLine line; line.part = m.args[1].ref; line.text = m.args[2].s; line.color = m.args[3].c;
        chatLines.push_back(line);
    } else if (server && sender && what == "Send" && m.args.size() == 5 && m.args[1].type == NetValue::Ref) {
        Instance* channel = dm.findRef(m.args[1].ref);
        if (!channel || !channel->isA("TextChannel")) return;
        Instance* source = textSourceOf(*channel, (int64_t)sender->get("UserId").n);
        if (!source || !source->get("CanSend").b) return;
        std::string s = chatText(m.args[3].s);
        if (s.empty()) return;
        Instance::Ptr msg = newMessage(*channel, s, source, m.args[4].s, 2 /* Success */);
        fireValues(*sender, "Chatted", {Value::string(s), Value::nil()});
        fireValues(*channel, "MessageReceived", {Value::instance(msg->id())});
        fireValues(*dm.getService("TextChatService"), "MessageReceived", {Value::instance(msg->id())});
        const Callback* should = callback(*channel, "ShouldDeliverCallback");
        for (auto& c : channel->children()) {
            if (!c->isA("TextSource")) continue;
            Instance* to = playerOfSource(c.get());
            if (!to) continue;
            bool deliver = true;
            if (should) callSync(*should, [&](lua_State* co) { pushInstance(co, msg.get()); pushInstance(co, c.get()); return 2; },
                                 [&](lua_State* co, int n) { if (n >= 1) deliver = lua_toboolean(co, -n); });
            if (!deliver) continue;
            RemoteMsg down; down.kind = RemoteMsg::Event; down.remote = players->id(); down.player = to->id();
            down.args = {NetValue::string("Msg"), NetValue::instance(channel->id()), NetValue::string(msg->get("MessageId").s), NetValue::instance(source->id()),
                         NetValue::string(s), NetValue::string(m.args[4].s), NetValue::string(to == sender ? m.args[2].s : "")};
            outRemotes.push_back(std::move(down));
        }
    } else if (!server && what == "Msg" && m.args.size() == 7 && m.args[1].type == NetValue::Ref) {
        Instance* channel = dm.findRef(m.args[1].ref);
        Instance* source = dm.findRef(m.args[3].ref);
        if (!channel || !channel->isA("TextChannel")) return;
        // args[6] is the sender's own MessageId, echoed back to it alone, so the sender
        // gets the same Instance SendAsync returned rather than a second one
        Instance::Ptr msg;
        if (auto it = sentMessages.find(m.args[6].s); it != sentMessages.end()) { msg = it->second; sentMessages.erase(it); }
        if (!msg) msg = newMessage(*channel, m.args[4].s, source && source->isA("TextSource") ? source : nullptr, m.args[5].s, 2);
        else { msg->setInternal("Status", statusValue(2 /* Success */)); msg->setInternal("Text", Value::string(m.args[4].s)); }
        msg->setInternal("MessageId", Value::string(m.args[2].s));
        incoming(*msg);
    } else if (server && sender && what == "Command" && m.args.size() == 3 && m.args[1].type == NetValue::Ref) {
        Instance* cmd = dm.findRef(m.args[1].ref);
        if (!cmd || !cmd->isA("TextChatCommand") || !cmd->get("Enabled").b) return;
        Instance* channels = textChannels();
        Instance* general = channels ? channels->findFirstChild("RBXGeneral") : nullptr;
        Instance* source = general ? textSourceOf(*general, (int64_t)sender->get("UserId").n) : nullptr;
        fireValues(*cmd, "Triggered", {source ? Value::instance(source->id()) : Value::nil(), Value::string(m.args[2].s)});
    }
}

} // namespace pulseblockz::rbx
