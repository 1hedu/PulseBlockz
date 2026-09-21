// SPDX-License-Identifier: LicenseRef-PulseBlockz
pragma solidity ^0.8.24;

import {ERC2771Forwarder} from "@openzeppelin/contracts/metatx/ERC2771Forwarder.sol";

/// @title Forwarder — EIP-712 meta-transaction entry point (audited OZ impl).
/// @dev Domain: name "PulseBlockzForwarder", version "1". The relayer calls
///      execute(); abuse control (rate limits, target allowlist) lives in the
///      relayer, not here — keep the on-chain surface minimal.
contract Forwarder is ERC2771Forwarder {
/// @dev The name below is not this project's name: it is the deployed contract's EIP-712
///      domain, sitting in the bytecode at the address in addresses.943.json, and every meta
///      transaction ever signed was signed against it. It changes on a redeploy and not before.
    constructor() ERC2771Forwarder("PulseBlockzForwarder") {}
}
