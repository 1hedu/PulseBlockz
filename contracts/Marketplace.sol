// SPDX-License-Identifier: LicenseRef-PulseBlockz
pragma solidity ^0.8.24;

import {ReentrancyGuard} from "@openzeppelin/contracts/utils/ReentrancyGuard.sol";
import {ERC2771Context} from "@openzeppelin/contracts/metatx/ERC2771Context.sol";
import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {IERC20Permit} from "@openzeppelin/contracts/token/ERC20/extensions/IERC20Permit.sol";
import {SafeERC20} from "@openzeppelin/contracts/token/ERC20/utils/SafeERC20.sol";
import {UGC1155} from "./UGC1155.sol";

/// @title Marketplace — primary sales of UGC, paid in whatever tokens the creator accepts.
/// @notice Nobody owns this contract. There is no admin, no pause, no upgrade path,
///         and the fee is fixed forever at deployment: either zero, or a percentage
///         sent to an address the deployer chose (a burn address makes it provably
///         unspendable). It deploys its own UGC1155 and is the only minter of it.
///
///         A creator sets, per asset, a price in each ERC-20 they accept; a buyer
///         picks one and pays it. The payment goes to the creator (and the fee
///         address, if any) in the same transaction and a copy is minted. Nothing
///         is ever held here.
///
///         Meta-tx aware: setPrice, buy and buyWithPermit can be relayed gaslessly
///         by any relayer; none of them require one. Plain ERC-20s need one real
///         approve from the buyer per token; tokens with EIP-2612 permit need none.
///         Secondary trading is out of scope for v1 — ERC-2981 royalties are set on
///         UGC1155 so any standard NFT market honors them.
contract Marketplace is ReentrancyGuard, ERC2771Context {
    using SafeERC20 for IERC20;

    UGC1155 public immutable ugc;
    uint16 public immutable feeBps;        // 0 = no fee
    address public immutable feeRecipient; // e.g. 0x000000000000000000000000000000000000dEaD

    /// @notice price[id][token] per copy in the token's base units; 0 = not for sale in that token
    mapping(uint256 => mapping(address => uint256)) public price;
    mapping(uint256 => address[]) private _tokensFor; // tokens with a non-zero price, for UIs

    event PriceSet(uint256 indexed id, address indexed token, uint256 price);
    event Purchased(address indexed buyer, uint256 indexed id, uint256 qty, address token, uint256 paid, uint256 fee);

    error NotForSale();
    error NotCreator();
    error BadQty();
    error FeeTooHigh();
    error FeeRecipientRequired();

    constructor(address trustedForwarder, uint16 feeBps_, address feeRecipient_) ERC2771Context(trustedForwarder) {
        if (feeBps_ > 10_000) revert FeeTooHigh();
        if (feeBps_ > 0 && feeRecipient_ == address(0)) revert FeeRecipientRequired();
        feeBps = feeBps_;
        feeRecipient = feeRecipient_;
        ugc = new UGC1155(address(this), trustedForwarder);
    }

    // ---- creators ------------------------------------------------------------
    /// @notice Accept `token` for asset `id` at `pricePerCopy` (0 to stop accepting it).
    ///         A creator's "whitelist" is simply the set of tokens they have priced.
    function setPrice(uint256 id, IERC20 token, uint256 pricePerCopy) external {
        if (ugc.creatorOf(id) != _msgSender()) revert NotCreator();
        uint256 old = price[id][address(token)];
        price[id][address(token)] = pricePerCopy;
        if (old == 0 && pricePerCopy != 0) {
            _tokensFor[id].push(address(token));
        } else if (old != 0 && pricePerCopy == 0) {
            address[] storage arr = _tokensFor[id];
            for (uint256 i = 0; i < arr.length; i++) {
                if (arr[i] == address(token)) { arr[i] = arr[arr.length - 1]; arr.pop(); break; }
            }
        }
        emit PriceSet(id, address(token), pricePerCopy);
    }

    // ---- buyers --------------------------------------------------------------
    /// @notice Buy `qty` copies paying in `token`. Buyer must have approved `token` to this contract.
    function buy(uint256 id, IERC20 token, uint256 qty) external nonReentrant {
        _buy(id, token, qty);
    }

    /// @notice Buy with an EIP-2612 permit for `token`: no prior approve, so the
    ///         whole purchase is one gasless signature for the buyer.
    function buyWithPermit(uint256 id, IERC20 token, uint256 qty, uint256 deadline, uint8 v, bytes32 r, bytes32 s)
        external nonReentrant
    {
        uint256 p = price[id][address(token)];
        if (p == 0) revert NotForSale();
        // A front-run permit would revert here; tolerate it, the allowance check in _buy is what matters.
        try IERC20Permit(address(token)).permit(_msgSender(), address(this), p * qty, deadline, v, r, s) {} catch {}
        _buy(id, token, qty);
    }

    function _buy(uint256 id, IERC20 token, uint256 qty) internal {
        if (qty == 0) revert BadQty();
        uint256 p = price[id][address(token)];
        if (p == 0) revert NotForSale();

        address buyer = _msgSender();
        uint256 total = p * qty;
        uint256 fee = (total * feeBps) / 10_000;

        if (fee > 0) token.safeTransferFrom(buyer, feeRecipient, fee);
        token.safeTransferFrom(buyer, ugc.creatorOf(id), total - fee);
        ugc.mint(buyer, id, qty); // reverts if supply exceeded

        emit Purchased(buyer, id, qty, address(token), total, fee);
    }

    // ---- views ---------------------------------------------------------------
    /// @notice Tokens asset `id` can currently be bought with.
    function tokensFor(uint256 id) external view returns (address[] memory) { return _tokensFor[id]; }
}
