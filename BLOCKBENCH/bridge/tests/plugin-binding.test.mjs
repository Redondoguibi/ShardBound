import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";
const root = new URL("../", import.meta.url);
const json = async (file) =>
  JSON.parse(await readFile(new URL(file, root), "utf8"));
test("account plugin binds one required app and avoids duplicate local/remote MCP", async () => {
  const portable = await json("plugin.json"),
    compat = await json(".codex-plugin/plugin.json"),
    app = await json(".app.json");
  assert.match(app.apps.shardbound_blockbench.id, /^asdk_app_[a-f0-9]{32}$/);
  assert.equal(
    app.apps.shardbound_blockbench.id,
    "asdk_app_6ac1a125e01c81918b4b37cc32bf346b",
  );
  assert.equal(app.apps.shardbound_blockbench.required, true);
  assert.equal(portable.extensions["com.openai"].apps, "./.app.json");
  assert.equal(compat.apps, "./.app.json");
  assert.equal(compat.version, portable.version);
  assert.equal(compat.name, portable.name);
  assert.equal(
    compat.interface.defaultPrompt,
    portable.extensions["com.openai"].interface.defaultPrompt,
  );
  assert.deepEqual((await json("mcp.json")).mcpServers, {});
  assert.deepEqual((await json(".mcp.json")).mcpServers, {});
});
test(
  "private ZIP actually contains the app binding and excludes runtime secrets",
  { skip: process.platform !== "win32" },
  () => {
    const output = execFileSync(
      "powershell",
      [
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        fileURLToPath(new URL("scripts/package.ps1", root)),
      ],
      { encoding: "utf8", windowsHide: true },
    );
    assert.match(output, /Source package verified:/);
    // The packaging command opens its completed ZIP and checks members and private-file exclusion.
  },
);
