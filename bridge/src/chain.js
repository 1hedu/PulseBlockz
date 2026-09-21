// Contract handles + typed-data helpers shared by server and client SDK.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const cfg = require("./config");
const { applyFeeOverrides } = require("./fees");

const abi = (n) => JSON.parse(fs.readFileSync(path.join(__dirname, "..", "..", "artifacts", n + ".json"))).abi;
const provider = applyFeeOverrides(new ethers.JsonRpcProvider(cfg.rpcUrl, cfg.chainId, { staticNetwork: true }));
const relayerWallet = new ethers.Wallet(cfg.relayerKey, provider);
const relayer = new ethers.NonceManager(relayerWallet); relayer.address = relayerWallet.address;

const A = cfg.addresses;
const contracts = {
  forwarder:   new ethers.Contract(A.Forwarder,   abi("Forwarder"),   relayer),
  ugc:         new ethers.Contract(A.UGC1155,     abi("UGC1155"),     provider),
  marketplace: new ethers.Contract(A.Marketplace, abi("Marketplace"), provider),
  assetStore:  new ethers.Contract(A.AssetStore,  abi("AssetStore"),  provider),
};
// Any ERC-20 (with optional EIP-2612 permit). The MockERC20 ABI is a superset of what we call.
const erc20 = (address, signer = provider) => new ethers.Contract(address, abi("MockERC20"), signer);
// Payment tokens by symbol, as recorded by deploy.js (e.g. { mUSD, mWPLS } locally).
const tokens = Object.fromEntries(Object.entries(A.Tokens || {}).map(([sym, addr]) => [sym, erc20(addr)]));

// EIP-712 domain/types for OpenZeppelin ERC2771Forwarder (v5).
const forwarderDomain = { name: "PulseBlockzForwarder", version: "1", chainId: cfg.chainId, verifyingContract: A.Forwarder };
const forwardTypes = { ForwardRequest: [
  { name: "from", type: "address" }, { name: "to", type: "address" }, { name: "value", type: "uint256" },
  { name: "gas", type: "uint256" }, { name: "nonce", type: "uint256" }, { name: "deadline", type: "uint48" },
  { name: "data", type: "bytes" },
] };

/** Build an unsigned ForwardRequest for `signer` calling `contract.fn(args)`. */
async function buildForwardRequest(from, contract, fn, args, gas = 300_000n, ttlSec = 300) {
  const nonce = await contracts.forwarder.nonces(from);
  return {
    from, to: await contract.getAddress(), value: 0n, gas, nonce,
    deadline: Math.floor(Date.now() / 1000) + ttlSec,
    data: contract.interface.encodeFunctionData(fn, args),
  };
}

/** Client side: sign a ForwardRequest with an ethers Signer (wallet popup in prod). */
async function signForwardRequest(signer, req) {
  const signature = await signer.signTypedData(forwarderDomain, forwardTypes, req);
  return { ...req, signature };
}

/** Client side: sign an EIP-2612 permit so Marketplace.buyWithPermit needs no prior approve. */
async function signPermit(signer, token, spender, value, ttlSec = 300) {
  const owner = await signer.getAddress();
  const [name, nonce] = await Promise.all([token.name(), token.nonces(owner)]);
  const deadline = Math.floor(Date.now() / 1000) + ttlSec;
  const domain = { name, version: "1", chainId: cfg.chainId, verifyingContract: await token.getAddress() };
  const types = { Permit: [
    { name: "owner", type: "address" }, { name: "spender", type: "address" }, { name: "value", type: "uint256" },
    { name: "nonce", type: "uint256" }, { name: "deadline", type: "uint256" },
  ] };
  const sig = ethers.Signature.from(await signer.signTypedData(domain, types, { owner, spender, value, nonce, deadline }));
  return { deadline, v: sig.v, r: sig.r, s: sig.s };
}

module.exports = { provider, relayer, contracts, erc20, tokens, forwarderDomain, forwardTypes, buildForwardRequest, signForwardRequest, signPermit };
