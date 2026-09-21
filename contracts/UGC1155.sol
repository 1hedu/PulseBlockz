// SPDX-License-Identifier: LicenseRef-PulseBlockz
pragma solidity ^0.8.24;

import {ERC1155} from "@openzeppelin/contracts/token/ERC1155/ERC1155.sol";
import {ERC1155Supply} from "@openzeppelin/contracts/token/ERC1155/extensions/ERC1155Supply.sol";
import {ERC2981} from "@openzeppelin/contracts/token/common/ERC2981.sol";
import {ERC2771Context} from "@openzeppelin/contracts/metatx/ERC2771Context.sol";
import {Context} from "@openzeppelin/contracts/utils/Context.sol";

/// @title UGC1155 — PulseBlockz user-generated content ownership (PRC-1155)
/// @notice One token ID = one published asset *type* (hat, model, script pack).
///         Balance > 0 = the player owns a copy. The token stores a metadata URI,
///         not the binary; the creator hosts that wherever they like (IPFS, their
///         own server) and can re-point it until they freeze it.
///
///         Nobody administers this contract. Any creator registers their own
///         assets. There is no publisher role, no delist switch, no admin. The
///         only privileged address is the Marketplace, fixed at construction,
///         which is the only thing allowed to mint copies (it does so on sale).
///         Curation, if any, is the job of clients and the servers creators run.
contract UGC1155 is ERC1155, ERC1155Supply, ERC2981, ERC2771Context {
    struct Asset {
        address creator;
        uint32 maxSupply;      // 0 = unlimited
        bool frozen;           // creator locked the metadata (immutable from here on)
        string uri;
    }

    address public immutable minter; // the Marketplace
    uint256 public nextId = 1;
    mapping(uint256 => Asset) private _assets;

    event AssetRegistered(uint256 indexed id, address indexed creator, string uri, uint32 maxSupply);
    event AssetUriUpdated(uint256 indexed id, string uri);
    event AssetFrozen(uint256 indexed id);

    error UnknownAsset();
    error Frozen();
    error SupplyExceeded();
    error NotCreator();
    error NotMinter();

    constructor(address minter_, address trustedForwarder)
        ERC1155("")
        ERC2771Context(trustedForwarder)
    {
        minter = minter_;
    }

    // ---- publishing (any creator, for themselves) ------------------------------
    /// @notice Register an asset. The caller is its creator. Gasless via forwarder.
    /// @param royaltyBps creator royalty on secondary sales (ERC-2981), e.g. 500 = 5%
    function register(string calldata uri_, uint32 maxSupply, uint96 royaltyBps) external returns (uint256 id) {
        address creator = _msgSender();
        id = nextId++;
        _assets[id] = Asset({creator: creator, maxSupply: maxSupply, frozen: false, uri: uri_});
        if (royaltyBps > 0) _setTokenRoyalty(id, creator, royaltyBps);
        emit AssetRegistered(id, creator, uri_, maxSupply);
        emit URI(uri_, id);
    }

    /// @notice Creator publishes a new version of the metadata.
    function setUri(uint256 id, string calldata uri_) external {
        Asset storage a = _assets[id];
        if (a.creator == address(0)) revert UnknownAsset();
        if (a.frozen) revert Frozen();
        if (_msgSender() != a.creator) revert NotCreator();
        a.uri = uri_;
        emit AssetUriUpdated(id, uri_);
        emit URI(uri_, id);
    }

    /// @notice Creator makes the metadata immutable. Holders can trust it never changes.
    function freeze(uint256 id) external {
        Asset storage a = _assets[id];
        if (_msgSender() != a.creator) revert NotCreator();
        a.frozen = true;
        emit AssetFrozen(id);
    }

    // ---- minting (marketplace only) --------------------------------------------
    function mint(address to, uint256 id, uint256 amount) external {
        if (msg.sender != minter) revert NotMinter();
        Asset storage a = _assets[id];
        if (a.creator == address(0)) revert UnknownAsset();
        if (a.maxSupply != 0 && totalSupply(id) + amount > a.maxSupply) revert SupplyExceeded();
        _mint(to, id, amount, "");
    }

    // ---- views ---------------------------------------------------------------------
    function asset(uint256 id) external view returns (Asset memory) { return _assets[id]; }
    function creatorOf(uint256 id) external view returns (address) { return _assets[id].creator; }
    function uri(uint256 id) public view override returns (string memory) { return _assets[id].uri; }

    // ---- boilerplate ---------------------------------------------------------------
    function _update(address from, address to, uint256[] memory ids, uint256[] memory values)
        internal override(ERC1155, ERC1155Supply)
    { super._update(from, to, ids, values); }

    function supportsInterface(bytes4 iid) public view override(ERC1155, ERC2981) returns (bool)
    { return super.supportsInterface(iid); }

    function _msgSender() internal view override(Context, ERC2771Context) returns (address) {
        return ERC2771Context._msgSender();
    }
    function _msgData() internal view override(Context, ERC2771Context) returns (bytes calldata) {
        return ERC2771Context._msgData();
    }
    function _contextSuffixLength() internal view override(Context, ERC2771Context) returns (uint256) {
        return ERC2771Context._contextSuffixLength();
    }
}
