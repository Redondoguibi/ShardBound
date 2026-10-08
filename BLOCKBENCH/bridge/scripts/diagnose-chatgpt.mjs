import { readFile, readdir, mkdir, writeFile } from "node:fs/promises";
import path from "node:path";
import os from "node:os";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";
import { loadConfig, root } from "../mcp/config.mjs";
import { tools } from "../shared/tools.mjs";

export async function probeMcp(
  command = process.execPath,
  server = path.join(root, "mcp/server.mjs"),
) {
  const client = new Client({
    name: "shardbound-diagnostic",
    version: "1.0.0",
  });
  const transport = new StdioClientTransport({
    command,
    args: [server],
    cwd: root,
    stderr: "pipe",
  });
  transport.stderr?.on("data", () => {});
  try {
    await client.connect(transport);
    const discovered = (await client.listTools()).tools.map(
      (tool) => tool.name,
    );
    const missing = Object.keys(tools).filter(
      (name) => !discovered.includes(name),
    );
    if (missing.length)
      throw new Error(`tools/list omitted: ${missing.join(", ")}`);
    const response = await client.callTool({ name: "status", arguments: {} });
    if (response.isError)
      throw new Error(
        response.content?.find((item) => item.type === "text")?.text ||
          "status failed",
      );
    const status = JSON.parse(
      response.content.find((item) => item.type === "text").text,
    );
    return {
      pid: transport.pid,
      server: client.getServerVersion(),
      handshake: true,
      tools: discovered,
      status,
      origin: "independent diagnostic client; not proof of Chat tool exposure",
    };
  } finally {
    await client.close();
  }
}

async function json(filename) {
  try {
    return JSON.parse(
      (await readFile(filename, "utf8")).replace(/^\uFEFF/, ""),
    );
  } catch (error) {
    if (error.code === "ENOENT") return null;
    return { readError: `${filename}: ${error.code ?? "invalid JSON"}` };
  }
}

export async function diagnose() {
  const checks = [];
  const add = (layer, state, detail) => checks.push({ layer, state, detail });
  const portable = await json(path.join(root, "plugin.json"));
  const compatibility = await json(
    path.join(root, ".codex-plugin/plugin.json"),
  );
  const app = await json(path.join(root, ".app.json"));
  const binding = app?.apps?.shardbound_blockbench;
  const manifestOk =
    portable?.extensions?.["com.openai"]?.apps === "./.app.json" &&
    compatibility?.apps === "./.app.json" &&
    /^asdk_app_[a-z0-9]{32}$/.test(binding?.id ?? "") &&
    binding.required === true;
  add("account_plugin_manifest", manifestOk ? "pass" : "fail", {
    version: portable?.version,
    appId: binding?.id,
    explanation:
      "A registered account MCP app is required for this Chat configuration.",
  });
  const published = await json(
    path.join(root, ".local/published-plugin-verification.json"),
  );
  const connection = await json(
    path.join(root, ".local/account-connection-verification.json"),
  );
  const matchingPublication =
    published?.version === portable?.version &&
    published?.appId === binding?.id;
  const matchingConnection =
    connection?.appId === binding?.id && connection?.connected === true;
  add(
    "account_published_release",
    matchingPublication ? "observed" : "pending",
    matchingPublication
      ? published
      : {
          explanation:
            "No matching read-back of the published account release. Files in this repository do not prove publication.",
        },
  );
  add(
    "account_connection_registration",
    matchingConnection ? "observed" : "pending",
    matchingConnection
      ? connection
      : {
          explanation:
            "No matching UI observation of the registered account connection. Historical observations are not a live account check.",
        },
  );
  const marketplace = await json(
    path.join(os.homedir(), ".agents/plugins/marketplace.json"),
  );
  add("legacy_local_marketplace", "info", {
    registered: !!marketplace?.plugins?.some(
      (p) => p.name === "shardbound-blockbench-local",
    ),
    explanation:
      "Local marketplace registration does not install a plugin or register an account MCP connection.",
  });
  const cached = [];
  const cacheRoot = path.join(os.homedir(), ".codex/plugins/cache");
  for (const market of await readdir(cacheRoot).catch(() => [])) {
    for (const name of [
      "shardbound-blockbench",
      "shardbound-blockbench-local",
    ]) {
      const location = path.join(cacheRoot, market, name);
      for (const version of await readdir(location).catch(() => [])) {
        const cachedManifest = await json(
          path.join(location, version, "plugin.json"),
        );
        if (cachedManifest)
          cached.push({
            marketplace: market,
            name,
            version: cachedManifest.version,
            appMapping: cachedManifest.extensions?.["com.openai"]?.apps ?? null,
            path: path.join(location, version),
          });
      }
    }
  }
  add("desktop_plugin_cache", "info", {
    entries: cached,
    explanation:
      "Cache presence proves downloaded files only, not which plugin a normal Chat loaded.",
  });
  add(
    "desktop_installation_logs_processes",
    "info",
    await json(path.join(root, ".local/desktop-installation-safe.json")),
  );
  let config;
  try {
    config = loadConfig();
    add("pairing_config", "pass", {
      file: config.filename,
      port: config.bridgePort,
      legacyPublicUrl: !!config.publicUrl,
    });
  } catch (error) {
    add("pairing_config", "fail", error.message);
  }
  try {
    const local = await probeMcp();
    add("node_stdio_handshake_tools_status", "pass", local);
    add(
      "blockbench_controller",
      local.status.connected === false || !local.status.blockbench
        ? "fail"
        : "pass",
      local.status,
    );
  } catch (error) {
    add("node_stdio_handshake_tools_status", "fail", error.message);
  }
  if (config) {
    try {
      const response = await fetch(
        `http://127.0.0.1:${config.bridgePort}/health`,
        {
          headers: { Authorization: `Bearer ${config.token}` },
          signal: AbortSignal.timeout(3000),
        },
      );
      if (!response.ok) throw new Error(`health HTTP ${response.status}`);
      const health = await response.json();
      if (health.service !== "shardbound-blockbench")
        throw new Error("Unexpected daemon on bridge port");
      add("loopback_daemon", "pass", {
        service: health.service,
        version: health.version,
        connected: health.connected,
        legacyPublicUrl: !!health.publicUrl,
      });
    } catch (error) {
      add("loopback_daemon", "fail", error.message);
    }
  }
  const tunnel = await json(path.join(root, ".local/tunnel-status-safe.json"));
  const current = tunnel && Date.now() - Date.parse(tunnel.checkedAt) < 60000;
  add(
    "openai_tunnel_runtime",
    current &&
      tunnel.process_running &&
      tunnel.healthy &&
      tunnel.ready &&
      tunnel.control_plane?.status === "ok"
      ? "pass"
      : "fail",
    tunnel ||
      "Run Status-ChatGPT-Desktop-Plugin.ps1 to collect current tunnel state.",
  );
  add(
    "openai_tunnel_tools_catalog",
    current && tunnel.mcp?.details?.tools_list?.ok ? "observed" : "pending",
    tunnel?.mcp?.details?.tools_list ??
      "Discovery through the tunnel has not been observed for this runtime.",
  );
  const events = (
    await readFile(path.join(root, ".local/mcp-audit.jsonl"), "utf8").catch(
      () => "",
    )
  )
    .trim()
    .split("\n")
    .filter(Boolean)
    .flatMap((line) => {
      try {
        return [JSON.parse(line)];
      } catch {
        return [];
      }
    });
  const forwarded = events
    .filter(
      (event) => event.parentPid === tunnel?.pid && event.event === "response",
    )
    .slice(-20);
  add(
    "tunnel_protocol_observation",
    forwarded.length ? "observed" : "pending",
    {
      events: forwarded,
      explanation:
        "Forwarded requests establish tunnel traffic; discovery is not a normal Chat tool call.",
    },
  );
  const hostProof = await json(
    path.join(root, ".local/chatgpt-host-verification.json"),
  );
  const validProof =
    !!hostProof &&
    hostProof.pluginVersion === portable?.version &&
    hostProof.appId === binding?.id;
  add(
    "normal_chat_host",
    validProof ? "observed" : "pending",
    validProof
      ? hostProof
      : {
          explanation:
            "No matching observation of tools and status in normal Chat. Local tests cannot prove this layer.",
        },
  );
  const required = [
    "account_plugin_manifest",
    "pairing_config",
    "node_stdio_handshake_tools_status",
    "blockbench_controller",
    "loopback_daemon",
    "openai_tunnel_runtime",
  ];
  return {
    checkedAt: new Date().toISOString(),
    target: "ChatGPT normal Desktop Windows",
    node: process.execPath,
    checks,
    localReady: required.every((layer) =>
      checks.some((c) => c.layer === layer && c.state === "pass"),
    ),
    desktopChatVerified:
      validProof &&
      hostProof.surface === "ChatGPT Desktop Windows normal Chat" &&
      hostProof.statusSucceeded === true,
  };
}

if (
  process.argv[1] &&
  path.resolve(process.argv[1]) ===
    path.join(root, "scripts/diagnose-chatgpt.mjs")
) {
  const report = await diagnose();
  await mkdir(path.join(root, ".local"), { recursive: true });
  await writeFile(
    path.join(root, ".local/chatgpt-diagnostic.json"),
    JSON.stringify(report, null, 2),
  );
  if (process.argv.includes("--json"))
    console.log(JSON.stringify(report, null, 2));
  else {
    for (const check of report.checks)
      console.log(
        `[${check.state.toUpperCase()}] ${check.layer}: ${JSON.stringify(check.detail)}`,
      );
    console.log("Report: " + path.join(root, ".local/chatgpt-diagnostic.json"));
  }
  process.exitCode = !report.localReady
    ? 1
    : process.argv.includes("--require-chat-host") &&
        !report.desktopChatVerified
      ? 2
      : 0;
}
