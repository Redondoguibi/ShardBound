import { readFile, writeFile, copyFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
async function read(filename) {
  try {
    return await readFile(filename, "utf8");
  } catch (error) {
    if (error.code === "ENOENT") return null;
    throw error;
  }
}
const suffix =
  ".shardbound-backup-" + new Date().toISOString().replaceAll(":", "-");
const marketplace = path.join(os.homedir(), ".agents/plugins/marketplace.json");
const raw = await read(marketplace);
if (raw) {
  const data = JSON.parse(raw.replace(/^\uFEFF/, ""));
  if (data.plugins?.some((p) => p.name === "shardbound-blockbench-local")) {
    data.plugins = data.plugins.filter(
      (p) => p.name !== "shardbound-blockbench-local",
    );
    await copyFile(marketplace, marketplace + suffix);
    await writeFile(marketplace, JSON.stringify(data, null, 2) + "\n");
    console.log(
      "Retired the old ShardBound local marketplace entry; other entries preserved and backup saved.",
    );
  }
}
const config = path.join(os.homedir(), ".codex/config.toml");
const toml = await read(config);
if (toml) {
  const old =
    /^\[mcp_servers\.shardbound_blockbench-2\][^\r\n]*\r?\n[\s\S]*?(?=^\[|$(?![\s\S]))/m;
  const section = toml.match(old)?.[0];
  if (
    section &&
    /url\s*=\s*["'][^"']*\.trycloudflare\.com\//.test(section) &&
    !/^enabled\s*=\s*false\s*$/m.test(section)
  ) {
    const disabled = /^enabled\s*=.*$/m.test(section)
      ? section.replace(/^enabled\s*=.*$/m, "enabled = false")
      : section.replace(/\r?\n/, "\nenabled = false\n");
    await copyFile(config, config + suffix);
    await writeFile(
      config,
      toml.replace(old, () => disabled),
    );
    console.log(
      "Disabled only the obsolete ShardBound temporary-URL connection; local stdio and other servers preserved.",
    );
  }
}
