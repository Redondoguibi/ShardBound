import test from "node:test";
import assert from "node:assert/strict";
import { readFile, mkdtemp, readdir } from "node:fs/promises";
import { execFileSync } from "node:child_process";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { InMemoryTransport } from "@modelcontextprotocol/sdk/inMemory.js";
import { createMcp } from "../mcp/server-tools.mjs";
import { auditTransport } from "../mcp/audit.mjs";
import { tools, vec3 } from "../shared/tools.mjs";
import os from "node:os";
import path from "node:path";

test("fixed-length vectors retain validation and publish Chat-compatible array schemas", async () => {
  assert.deepEqual(vec3.parse([1, 2, 3]), [1, 2, 3]);
  for (const value of [
    [1, 2],
    [1, 2, 3, 4],
    [1, 2, "3"],
    [1, 2, Infinity],
  ])
    assert.equal(vec3.safeParse(value).success, false);
  const server = createMcp(async () => ({ connected: false }), {});
  const [clientTransport, serverTransport] =
    InMemoryTransport.createLinkedPair();
  const client = new Client({ name: "schema-test", version: "1" });
  try {
    await server.connect(serverTransport);
    await client.connect(clientTransport);
    const discovered = await client.listTools();
    assert.deepEqual(
      discovered.tools.map((t) => t.name),
      Object.keys(tools),
    );
    const walk = (v) => {
      if (!v || typeof v !== "object") return;
      if (v.type === "array") assert.equal(Array.isArray(v.items), false);
      for (const child of Object.values(v)) walk(child);
    };
    discovered.tools.forEach((t) => {
      assert.ok(t.title);
      walk(t.inputSchema);
    });
  } finally {
    await client.close();
    await server.close();
  }
});

test("protocol audit records successful requests without arguments or results", async () => {
  const directory = await mkdtemp(path.join(os.tmpdir(), "shardbound-audit-"));
  const filename = path.join(directory, "events.jsonl");
  const server = createMcp(
    async () => ({ project_id: "PRIVATE_RESULT", revision: 2 }),
    {},
  );
  const [ct, st] = InMemoryTransport.createLinkedPair();
  const attach = auditTransport(st, filename);
  const client = new Client({ name: "audit-test", version: "1" });
  try {
    await server.connect(st);
    attach();
    await client.connect(ct);
    await client.listTools();
    await client.callTool({
      name: "read_project",
      arguments: { geometry: true },
    });
    const text = await readFile(filename, "utf8");
    const events = text.trim().split("\n").map(JSON.parse);
    assert.ok(events.some((e) => e.method === "initialize" && e.success));
    assert.ok(
      events.some((e) => e.method === "tools/list" && e.tools.length === 18),
    );
    assert.ok(events.some((e) => e.tool === "read_project" && e.success));
    assert.doesNotMatch(text, /PRIVATE_RESULT|geometry|arguments|content/);
  } finally {
    await client.close();
    await server.close();
  }
});
