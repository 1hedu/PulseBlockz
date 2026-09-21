// Bridge configuration. Every value can be overridden by env.
const fs = require("fs"), path = require("path");
// ADDRESSES_FILE=addresses.943.json to point the bridge at a testnet deployment.
const addrFile = process.env.ADDRESSES_FILE ? path.resolve(process.env.ADDRESSES_FILE) : path.join(__dirname, "..", "..", "addresses.json");
const addresses = fs.existsSync(addrFile) ? JSON.parse(fs.readFileSync(addrFile)) : {};

const CHAINS = {
  local:   { chainId: 31337, rpc: "http://127.0.0.1:8545" },
  testnet: { chainId: 943,   rpc: "https://rpc.v4.testnet.pulsechain.com" }, // PulseChain testnet v4
  mainnet: { chainId: 369,   rpc: "https://rpc.pulsechain.com" },
};
const chain = CHAINS[process.env.CHAIN || "local"];

module.exports = {
  port: Number(process.env.PORT || 8787),
  chainId: chain.chainId,
  rpcUrl: process.env.RPC_URL || chain.rpc,
  relayerKey: process.env.RELAYER_KEY || "0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d", // anvil #1 — dev only
  addresses,
  // Relayer abuse controls
  relay: {
    maxGasPerCall: 500_000n,
    perWalletPerMinute: 10,
    // Only these (target, selector) pairs may be relayed. Everything else is rejected.
    // Payment tokens are third-party ERC-20s and are not relayable: a buyer either
    // sends one real approve per token or signs a permit (buyWithPermit).
    allow: {
      Marketplace: ["setPrice(uint256,address,uint256)", "buy(uint256,address,uint256)",
                    "buyWithPermit(uint256,address,uint256,uint256,uint8,bytes32,bytes32)"],
      UGC1155:     ["register(string,uint32,uint96)", "setUri(uint256,string)", "freeze(uint256)"],
    },
  },
  sessionTtlSec: 60 * 60 * 24,
};
