// SPDX-License-Identifier: LicenseRef-PulseBlockz
pragma solidity ^0.8.20;

/// @title  Inventory — who has what, on chain, and nothing else.
/// @notice Not a token. Not an NFT. No ERC standard, no ids to mint, no marketplace,
///         no royalties. Just a record: this address holds this many of this thing.
///
///         An item is identified by the **content hash of its own data** — the same
///         hash a `pblockz://` URI carries. So there is no registry to look an item up
///         in and no authority that decides what exists: if you can name the bytes,
///         you can name the item, and anybody can fetch those bytes and check that
///         they hash to it. The asset layer is the uploader; this is the save file.
///
/// @dev    Stated plainly because it is the design rather than an oversight: this is
///         not scarcity. Anyone standing high enough on the ladder who knows a hash can
///         record that they hold that thing too. Copying data costs nothing, and
///         pretending otherwise would need a gatekeeper — the thing this project does
///         not have. What the ladder buys is a floor on who may claim what, not a
///         ceiling on how many exist. Real scarcity belongs in a different contract.
///
///         Holdings gate what you may claim, and the chain enforces it: `add` checks
///         `msg.sender.balance` against a fixed ladder. Doing that in a client would be
///         theatre -- anyone running their own would skip it.
///
///         Ownerless: no admin, no pause, no upgrade, no privileged address. The constructor
///         credits a founder with a stated list, once, before the contract can be called by
///         anybody -- and leaves no function behind that could do it again.
contract Inventory {
    /// @notice How many of `item` `who` holds.
    mapping(address => mapping(bytes32 => uint32)) public held;

    /// Every item an address has ever held, so a client can enumerate without an
    /// indexer. Entries are never removed -- a count of zero stays listed, which costs
    /// one slot and saves the client from needing logs to find what you used to have.
    mapping(address => bytes32[]) private _seen;
    mapping(address => mapping(bytes32 => bool)) private _known;

    /// @notice The data an item is made of. Because an item's id folds in the tier it
    ///         was claimed at, the id alone is not enough to go and fetch the bytes;
    ///         this is what turns one back into the other. Written once, by whoever
    ///         claims a thing first, and never changed.
    mapping(bytes32 => bytes32) public dataOf;

    event Changed(address indexed who, bytes32 indexed item, uint32 count);

    error NotEnough();
    error ZeroAmount();
    error NoSuchTier();
    error NotBigEnough();
    error TooManySlots();
    error StillHeld();
    error NotListed();

    /// A list has a ceiling, because `give` writes into somebody else's storage and an
    /// unbounded array there is a griefing tool: claim junk at tier zero, hand it to a
    /// stranger, repeat, and their list eventually costs more to read than any client
    /// will spend. Bounded, and readable a page at a time.
    uint256 public constant MAX_SLOTS = 1024;

    /// Of which this many may be opened by *other people's* gifts. Kept separate so
    /// that filling somebody's list with rubbish can never stop them adding their own
    /// things -- the worst it does is use up the space reserved for presents.
    uint256 public constant MAX_GIFT_SLOTS = 128;

    mapping(address => uint256) public giftSlots;

    /// Holdings, in wei, for each rung. Set once at construction and never again:
    /// there is nobody who could change them.
    uint256[] public thresholds;

    /// @param rungs      Holdings, in wei, for each rung. Fixed forever at this moment.
    /// @param founder    Who the town belongs to, credited with `seedItems` below.
    /// @param seedItems  Data hashes to credit the founder with, one of each.
    /// @param seedTiers  The tier each of them is claimed at, in the same order.
    ///
    /// @dev The seed is the one privilege in this contract and it lasts one transaction.
    ///      There is no function that does this -- not a restricted one, not an ownable one,
    ///      not one behind a flag. It happens inside the constructor, before an address
    ///      exists for anybody to call, and after it returns there is no path to `_credit`
    ///      that does not go through `add` and its balance check, or `give` and somebody
    ///      actually parting with a thing they hold.
    ///
    ///      That is a different claim from "trust the deployer". Anyone can read the
    ///      deployment transaction and see exactly what was seeded and to whom, and anyone
    ///      can read this constructor and see there is no second helping. A demo whose author
    ///      cannot show the town wearing its own clothes is a poor demo; a contract with a
    ///      mint function for the author is a poor contract. This is neither.
    ///
    ///      The tier is folded into the id as it is everywhere else, so a seeded item is the
    ///      SAME id as one somebody earns by standing on that rung -- not a special edition,
    ///      not a variant. The founder is holding the thing you can hold.
    constructor(
        uint256[] memory rungs,
        address founder,
        bytes32[] memory seedItems,
        uint8[] memory seedTiers
    ) {
        thresholds = rungs;
        if (seedItems.length != seedTiers.length) revert NotEnough();
        for (uint256 i = 0; i < seedItems.length; i++) {
            if (seedTiers[i] >= rungs.length) revert NoSuchTier();
            bytes32 item = itemId(seedItems[i], seedTiers[i]);
            if (dataOf[item] == bytes32(0)) dataOf[item] = seedItems[i];
            _credit(founder, item, 1);
        }
    }

    /// @notice Record that you hold one more of the thing described by `dataHash`,
    ///         claimed at `tier`.
    ///
    ///         The tier is part of the item's identity, not a field beside it:
    ///         `item = keccak256(dataHash, tier)`. That is what makes the gate real.
    ///         A modified client can call this with any tier it likes, but claiming a
    ///         lower one to duck the threshold produces a *different* id, which is not
    ///         the id the game knows that thing by -- so it buys you a record of
    ///         something nobody recognises. There is no honest way round it and no
    ///         gatekeeper deciding: the chain checks your balance itself.
    uint8 public constant RECORD_TIER = 255;   // `keep`: a record, on no rung of the ladder

    function add(bytes32 dataHash, uint8 tier) external {
        if (tier >= thresholds.length) revert NoSuchTier();
        // msg.sender.balance is native PLS, read by the chain at the moment of the
        // call. Nothing is taken and nothing is locked -- spend it and you fall back
        // down the ladder, which is the point of measuring holdings rather than spend.
        if (msg.sender.balance < thresholds[tier]) revert NotBigEnough();
        bytes32 item = itemId(dataHash, tier);
        if (dataOf[item] == bytes32(0)) dataOf[item] = dataHash;
        _credit(msg.sender, item, 1);
    }

    /// @notice Keep a record of your own -- an outfit, say: a list of things you already hold.
    ///         Nothing is claimed, so nothing is gated: the ladder is what standing on a rung
    ///         buys, and a note about what you wear is not bought. Kept at RECORD_TIER, which
    ///         `add` can never produce (it is past every rung), so a kept hash is never the id
    ///         of anything on a shelf and keeping one cannot be a way round the gate.
    function keep(bytes32 dataHash) external {
        bytes32 item = itemId(dataHash, RECORD_TIER);
        if (dataOf[item] == bytes32(0)) dataOf[item] = dataHash;
        _credit(msg.sender, item, 1);
    }

    /// @notice The id a piece of data has when claimed at a given tier.
    function itemId(bytes32 dataHash, uint8 tier) public pure returns (bytes32) {
        return keccak256(abi.encodePacked(dataHash, tier));
    }

    /// @notice What each rung of the ladder costs to stand on, in wei of native PLS.
    function ladder() external view returns (uint256[] memory) {
        return thresholds;
    }

    /// @notice Give some of yours to someone else. Yours goes down, theirs goes up;
    ///         nothing is created or destroyed.
    function give(address to, bytes32 item, uint32 amount) external {
        _debit(msg.sender, item, amount);
        if (to != msg.sender && !_known[to][item]) {
            if (giftSlots[to] >= MAX_GIFT_SLOTS) revert TooManySlots();
            giftSlots[to]++;
        }
        _credit(to, item, amount);
    }

    /// @notice Drop an entry you no longer hold out of your own list, so a list filled
    ///         with somebody else's rubbish can be cleaned up. Only you can call this,
    ///         and only for something whose count is already zero.
    function forget(bytes32 item) external {
        if (held[msg.sender][item] != 0) revert StillHeld();
        if (!_known[msg.sender][item]) revert NotListed();
        bytes32[] storage list = _seen[msg.sender];
        for (uint256 i = 0; i < list.length; i++) {
            if (list[i] == item) {
                list[i] = list[list.length - 1];
                list.pop();
                break;
            }
        }
        _known[msg.sender][item] = false;
        if (giftSlots[msg.sender] > 0) giftSlots[msg.sender]--;
        emit Changed(msg.sender, item, 0);
    }

    /// @notice Throw some away. There is no way for anyone else to do this to you.
    function drop(bytes32 item, uint32 amount) external {
        _debit(msg.sender, item, amount);
    }

    // ---- views ----------------------------------------------------------------
    /// @notice Everything `who` has ever held, and how many of each they hold now.
    ///         Counts of zero are included; the caller decides whether to show them.
    /// @param offset where to start, @param limit how many at most. A page, because a
    ///        list somebody else can grow should never be a single unbounded return.
    function inventoryOf(address who, uint256 offset, uint256 limit)
        external view returns (bytes32[] memory items, uint32[] memory counts, bytes32[] memory data)
    {
        bytes32[] storage all = _seen[who];
        uint256 n = offset >= all.length ? 0 : all.length - offset;
        if (n > limit) n = limit;
        items = new bytes32[](n);
        counts = new uint32[](n);
        data = new bytes32[](n);
        for (uint256 i = 0; i < n; i++) {
            items[i] = all[offset + i];
            counts[i] = held[who][items[i]];
            data[i] = dataOf[items[i]];
        }
    }

    /// @notice How many distinct items `who` has ever held.
    function slotsOf(address who) external view returns (uint256) {
        return _seen[who].length;
    }

    // ---- internals ------------------------------------------------------------
    function _credit(address who, bytes32 item, uint32 amount) internal {
        if (amount == 0) revert ZeroAmount();
        if (!_known[who][item]) {
            if (_seen[who].length >= MAX_SLOTS) revert TooManySlots();
            _known[who][item] = true;
            _seen[who].push(item);
        }
        uint32 next = held[who][item] + amount;   // 0.8 reverts on overflow
        held[who][item] = next;
        emit Changed(who, item, next);
    }

    function _debit(address who, bytes32 item, uint32 amount) internal {
        if (amount == 0) revert ZeroAmount();
        uint32 have = held[who][item];
        if (have < amount) revert NotEnough();
        uint32 next = have - amount;
        held[who][item] = next;
        emit Changed(who, item, next);
    }
}
