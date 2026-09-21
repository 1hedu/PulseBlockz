// Sane EIP-1559 fees for PulseChain.
//
// PulseChain's nodes answer eth_gasPrice / eth_maxPriorityFeePerGas with a fixed
// 10,000 gwei (1e13 wei) while the base fee sits at 7 wei and blocks actually
// include transactions tipping far less. Taking the node's suggestion literally
// makes a 5M-gas deployment cost ~50 PLS instead of a few thousandths, which on a
// testnet means burning the whole faucet grant on one contract.
//
// So: bid a priority fee we choose and a max fee that covers a few base-fee doublings.
//
// The tip is the smallest the chain's validators actually mine, with room -- and "actually
// mine" moves, so it is measured rather than assumed.
//
// Measured again on 2026-09-17, zero-value self-transfers, an empty chain (blocks 1% full,
// base fee 7 wei), timed from send to receipt:
//
//   0.002 gwei   78s      0.05 gwei   40s      1 gwei   6s      10 gwei   10s
//
// So one gwei is the knee: next block, every time, for 0.000021 PLS on a transfer and about
// two ten-thousandths of a PLS on a real call. Below it the wait is not congestion -- there is
// none -- it is waiting for a validator that will take the tip, and it reads as the chain being
// slow. The 0.002 here was measured on 2026-09-15 and had stopped being true.
//
// PRIORITY_FEE_GWEI overrides; FEE_MODE=node defers to the node's suggestion.
const { ethers } = require("ethers");

/// The tip, in gwei, for a chain id.
const PRIORITY_GWEI_BY_CHAIN = { 943: "1" };
const FALLBACK_PRIORITY_GWEI = "1";

function priorityGweiFor(chainId) {
  return process.env.PRIORITY_FEE_GWEI || PRIORITY_GWEI_BY_CHAIN[Number(chainId)] || FALLBACK_PRIORITY_GWEI;
}

/** Wrap provider.getFeeData so every signer built on it bids sensibly. Returns the provider. */
function applyFeeOverrides(provider, { priorityGwei } = {}) {
  if (process.env.FEE_MODE === "node" || provider.__pulseblockzFees) return provider;
  const original = provider.getFeeData.bind(provider);
  provider.getFeeData = async () => {
    const fd = await original();
    if (fd.maxFeePerGas == null) return fd;            // pre-1559 chain: leave it alone
    const { chainId } = await provider.getNetwork();
    const priority = ethers.parseUnits(String(priorityGwei || priorityGweiFor(chainId)), "gwei");
    const block = await provider.getBlock("latest");
    const base = block?.baseFeePerGas ?? 0n;
    // Room for the base fee to climb (it can rise 12.5% per block) without overpaying:
    // we only ever pay base + priority, maxFeePerGas is just the ceiling.
    const maxFee = base * 8n + priority;
    return new ethers.FeeData(fd.gasPrice, maxFee, priority);
  };
  provider.__pulseblockzFees = true;
  return provider;
}

module.exports = { applyFeeOverrides, priorityGweiFor, PRIORITY_GWEI_BY_CHAIN };
