// Deploys the Forwarder and writes its address into addresses.943.json.
//
//   node scripts/compile.js && node scripts/deploy-forwarder.js [--redeploy]
//
// The ERC-2771 entry point the relayer calls. Its EIP-712 domain name is fixed at deployment and
// every meta transaction is signed against it, so a rename of the project means a redeploy. Kept
// out of deploy.js: rerunning that would move the AssetStore and UGC addresses that assets and
// items already published name.
const { ethers } = require("ethers");
const fs = require("fs"), path = require("path");
const { applyFeeOverrides } = require("../bridge/src/fees");

const ROOT = path.join(__dirname, "..");
const ADDR_FILE = path.join(ROOT, "addresses.943.json");

async function main() {
  const env = Object.fromEntries(fs.readFileSync(path.join(ROOT, ".env.testnet"), "utf8").trim().split("\n").map((l) => l.split("=")));
  const provider = applyFeeOverrides(new ethers.JsonRpcProvider(process.env.RPC_URL || "https://rpc.v4.testnet.pulsechain.com", 943, { staticNetwork: true }));
  const deployer = new ethers.NonceManager(new ethers.Wallet(env.DEPLOYER_KEY, provider));
  const addrs = JSON.parse(fs.readFileSync(ADDR_FILE, "utf8"));

  if (addrs.Forwarder && !process.argv.includes("--redeploy")) {
    console.log("already deployed at", addrs.Forwarder, "(--redeploy to replace)");
    return;
  }
  const was = addrs.Forwarder;
  const art = JSON.parse(fs.readFileSync(path.join(ROOT, "artifacts", "Forwarder.json"), "utf8"));
  const c = await new ethers.ContractFactory(art.abi, art.bytecode, deployer).deploy();
  await c.waitForDeployment();
  addrs.Forwarder = await c.getAddress();
  fs.writeFileSync(ADDR_FILE, JSON.stringify(addrs, null, 2) + "\n");
  if (was) console.log("Forwarder was", was);
  console.log("Forwarder", addrs.Forwarder);

  // The domain read back off the contract: one that differs from what this repo signs against is
  // a relayer that refuses every meta transaction.
  const domain = await new ethers.Contract(addrs.Forwarder, [
    "function eip712Domain() view returns (bytes1,string,string,uint256,address,bytes32,uint256[])",
  ], provider).eip712Domain();
  console.log("domain name on chain                  :", domain[1]);
  console.log("version                               :", domain[2]);
}

main().catch((e) => { console.error(e.shortMessage || e.message || e); process.exit(1); });
