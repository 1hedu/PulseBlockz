// Deploys the ledger. Usage:
//   RPC_URL=... DEPLOYER_KEY=0x... [FEE_BPS=0 FEE_RECIPIENT=0x...] node scripts/deploy.js
// The deployer keeps no power afterwards: the contracts have no owner, no admin roles and no
// upgrade path. The fee and its recipient are fixed here for good; 0x0…dEaD burns the fee.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");

const RPC = process.env.RPC_URL || "http://127.0.0.1:8545";   // PulseChain's own endpoints: bridge/src/config.js
const KEY = process.env.DEPLOYER_KEY || "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"; // anvil #0
const FEE_BPS = Number(process.env.FEE_BPS || 0);
const FEE_RECIPIENT = process.env.FEE_RECIPIENT || ethers.ZeroAddress;

const art = (n) => JSON.parse(fs.readFileSync(path.join(__dirname, "..", "artifacts", n + ".json")));

async function main() {
  if (FEE_BPS > 0 && FEE_RECIPIENT === ethers.ZeroAddress) throw new Error("FEE_BPS > 0 needs FEE_RECIPIENT (0x…dEaD to burn)");
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(RPC));
  const deployer = new ethers.NonceManager(new ethers.Wallet(KEY, provider));
  const deployerAddr = await deployer.getAddress();
  const chainId = Number((await provider.getNetwork()).chainId);
  const relayer = process.env.RELAYER_ADDRESS || deployerAddr;   // whoever runs the reference bridge
  const dep = async (name, ...args) => {
    const f = new ethers.ContractFactory(art(name).abi, art(name).bytecode, deployer);
    const c = await f.deploy(...args); await c.waitForDeployment();
    console.log(`${name.padEnd(12)} ${await c.getAddress()}`);
    return c;
  };

  // Test payment tokens: the e2e needs ERC-20s it can mint and permit.
  const tokens = {};
  if (chainId === 31337 || process.env.DEPLOY_MOCKS === "1") {
    tokens.mUSD = await (await dep("MockERC20", "Mock USD", "mUSD", 6)).getAddress();
    tokens.mWPLS = await (await dep("MockERC20", "Mock Wrapped PLS", "mWPLS", 18)).getAddress();
  }

  const forwarder = await dep("Forwarder");
  const fwd = await forwarder.getAddress();
  const market = await dep("Marketplace", fwd, FEE_BPS, FEE_RECIPIENT);
  const ugcAddr = await market.ugc();
  const assets = await dep("AssetStore");
  console.log(`${"UGC1155".padEnd(12)} ${ugcAddr}  (deployed by Marketplace, which is its only minter)`);
  console.log(`fee: ${FEE_BPS / 100}%${FEE_BPS ? " -> " + FEE_RECIPIENT : ""}; nothing left to administer`);

  const out = { chainId, Forwarder: fwd, UGC1155: ugcAddr, Marketplace: await market.getAddress(), AssetStore: await assets.getAddress(), Tokens: tokens, relayer };
  fs.writeFileSync(path.join(__dirname, "..", "addresses.json"), JSON.stringify(out, null, 2));
  fs.writeFileSync(path.join(__dirname, "..", `addresses.${chainId}.json`), JSON.stringify(out, null, 2));
  console.log(`wrote addresses.json and addresses.${chainId}.json`);
}
main().catch((e) => { console.error(e); process.exit(1); });
