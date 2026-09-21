// Socket form of LocalSession (rbx_net.h): packets in place of queues, little-endian with
// varint lengths. Network ownership, as Roblox: the server takes a client's writes and touches
// only for that client's own character.
#pragma once
#include "rbx_host.h"
#include <cstdint>
#include <string>
#include <vector>

namespace pulseblockz::rbx {

// 5: the version whose Welcome carries place; a Hello naming any other number is disconnected
constexpr uint32_t kWireProtocol = 5;

/// Team Create: an edit a joined Studio asks the host to apply and replicate back.
/// op: prop / attr / tag / parent / create / destroy / model / chunk / file / unfile.
struct NetEdit {
    std::string op, name, text, path;
    int64_t id = 0, parent = 0;
    bool flag = false;
    Value value;
};

struct NetPacket {
    // c->s: Hello, Proof, ClientFrame.  s->c: Welcome, ServerFrame.
    enum Type : uint8_t { Hello = 1, Welcome = 2, ServerFrame = 3, ClientFrame = 4, Proof = 5 };
    Type type = Hello;
    uint32_t protocol = kWireProtocol;     // Hello
    std::string name;                      // Hello
    int64_t userId = 0;                    // Hello
    int64_t playerId = 0;                  // Welcome
    std::string nonce;                     // Welcome: what to sign to prove a wallet
    // Welcome: a pblockz:// uri the client fetches from the chain, hash-checking every file
    // before it runs; empty means unpublished scripts, which a client cannot check and will not run.
    // The only path code takes to a client: no packet here carries script Source, changes included
    std::string place;
    // Proof: EIP-191 over the nonce and the server dialled (sign_in_digest in
    // pulseblockz_world.cpp). The client names the address it claims and the server recovers the
    // signer from the signature: a mismatch disconnects, a match sets Player.WalletAddress.
    // No Proof at all, or one with both fields empty, plays as a guest
    std::string address, signature;        // the wallet claimed, and its 65-byte signature as hex
    std::string server;                    // the host:port the wallet signed for
    std::vector<Change> changes;           // Welcome: snapshot; ServerFrame: replication delta
    std::vector<RemoteMsg> remotes;        // ServerFrame / ClientFrame
    // Whitelist, checked on receipt (pulseblockz_world.cpp): writes are Position / Orientation /
    // AssemblyLinearVelocity on this client's own root and MoveDirection / Jump / StateName on its
    // Humanoid; events are Touched / TouchEnded / MoveToFinished. Anything else is dropped
    std::vector<HostWrite> writes;         // ClientFrame
    std::vector<HostEvent> events;         // ClientFrame
    std::vector<NetEdit> edits;            // ClientFrame: Team Create edits, taken only by an edit world
};

std::string encodePacket(const NetPacket& p);
/// False on malformed or truncated bytes (a packet from a client is untrusted input);
/// `out` is then unspecified.
bool decodePacket(const std::string& bytes, NetPacket& out);

} // namespace pulseblockz::rbx
