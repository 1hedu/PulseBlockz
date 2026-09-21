// SPDX-License-Identifier: LicenseRef-PulseBlockz
pragma solidity ^0.8.24;

import {ERC20} from "@openzeppelin/contracts/token/ERC20/ERC20.sol";
import {ERC20Permit} from "@openzeppelin/contracts/token/ERC20/extensions/ERC20Permit.sol";

/// @title MockERC20 — payment-token stand-in for local/anvil runs ONLY.
/// @dev Anyone can mint. Supports EIP-2612 permit so buyWithPermit can be tested.
///      deploy.js deploys two of these on the local chain for the e2e script.
contract MockERC20 is ERC20, ERC20Permit {
    uint8 private immutable _dec;
    constructor(string memory name_, string memory symbol_, uint8 decimals_)
        ERC20(name_, symbol_) ERC20Permit(name_)
    { _dec = decimals_; }
    function decimals() public view override returns (uint8) { return _dec; }
    function mint(address to, uint256 amount) external { _mint(to, amount); }
}
