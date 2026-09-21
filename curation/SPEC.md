# PulseBlockz curation lists — format v1

A curation list is a signed, off-chain document that tells a client what **not** to show
(mode `block`) or what **only** to show (mode `allow`). Lists are the whole moderation model:
the chain has no delist switch, so every client ships with the ability to subscribe to lists,
and anyone can publish one. A list is identified by its maintainer's address, so it can be
served from any URL or mirror and still be trusted if the signature checks.

This is the DNS/ad-blocker model: an asset existing in the registry does not make anyone
display it.

## Document

```json
{
  "version": 1,
  "mode": "block",
  "chainId": 369,
  "registry": "0xUGC1155…",
  "maintainer": "0xMaintainer…",
  "name": "PulseBlockz community blocklist",
  "description": "Copyright, scams, abuse. Report at https://…/report",
  "report": "https://example.org/report",
  "updated": 1757000000,
  "ttl": 86400,
  "entries": [
    { "kind": "asset",   "id": "123",            "reason": "copyright", "note": "ripped model" },
    { "kind": "creator", "address": "0xabc…",    "reason": "scam" },
    { "kind": "server",  "endpoint": "wss://bad.example", "reason": "malware" },
    { "kind": "uri",     "prefix": "ipfs://bafy…", "reason": "abuse" }
  ],
  "signature": "0x…"
}
```

| Field | Meaning |
|---|---|
| `version` | always `1` |
| `mode` | `block` (hide listed things) or `allow` (show only listed things; for curated/kids-mode clients) |
| `chainId`, `registry` | which UGC1155 deployment asset ids refer to; a list never applies to another registry |
| `maintainer` | address that signed the list; the list's identity |
| `name`, `description`, `report` | human-facing; `report` is where a client sends reports (maintainer's choice of endpoint, may be absent) |
| `updated` | unix seconds; a client replaces a cached copy only with a newer `updated` from the same maintainer |
| `ttl` | seconds after `updated` during which the list is considered fresh; a stale list is still applied, but the client should refetch |
| `entries[]` | see kinds below |
| `signature` | EIP-712 signature by `maintainer` (below) |

### Entry kinds

| kind | keys | matches |
|---|---|---|
| `asset` | `id` (decimal string) | one token id in `registry` |
| `creator` | `address` | every asset whose `creatorOf(id)` is this address, and their servers |
| `server` | `endpoint` | a server endpoint (exact, case-insensitive) |
| `uri` | `prefix` | any asset whose metadata URI starts with this (a content hash, a host) |

`reason` is one of `abuse`, `copyright`, `scam`, `malware`, `spam`, `other`. `note` is free text.

### Signature

EIP-712, so a maintainer signs it with an ordinary wallet and sees what they are signing:

```
domain  = { name: "PulseBlockzCuration", version: "1" }          // no chainId: the list carries its own
types   = { CurationList: [
            { name: "mode",        type: "string"  },
            { name: "chainId",     type: "uint256" },
            { name: "registry",    type: "address" },
            { name: "name",        type: "string"  },
            { name: "updated",     type: "uint64"  },
            { name: "ttl",         type: "uint64"  },
            { name: "entriesHash", type: "bytes32" } ] }
entriesHash = keccak256(utf8(canonicalJSON(entries)))
```

`canonicalJSON` = JSON with object keys sorted, no whitespace, entries in document order.
A client MUST reject a list whose recovered signer is not `maintainer`.

## Client behaviour

1. The client has a set of **subscriptions**: `{ maintainer, url }`. A default client ships with
   at least one and lets the user add, remove and disable any of them.
2. On start and every `ttl`, fetch each url, verify, keep the newest per maintainer.
3. **Merge**: a thing is hidden if any enabled `block` list matches it. If any enabled `allow`
   list exists, a thing is shown only if some `allow` list matches it (and no block list does).
4. Apply to: store/listing views, inventory (hidden items are not equipped or rendered),
   the server browser, and asset URIs before fetching them. Hidden ≠ deleted: the user's
   wallet still holds the token; the client just does not render it.
5. **Report**: a "report" action on any asset/creator/server posts `{ kind, id|address|endpoint,
   reason, note, reporter? }` to each subscribed list's `report` URL that has one. Lists decide
   what to do with reports; the client never waits on them.

Nothing here touches the chain, so it costs nothing in decentralization. It is what lets any
client maintainer meet app-store user-content rules (filter, report, block) and answer
takedown requests for their client, without anyone holding a key over the registry.
