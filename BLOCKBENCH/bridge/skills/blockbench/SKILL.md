---
name: blockbench
description: Inspect or edit the user's open Blockbench Generic Model through the local ShardBound MCP, including adapted MTools operations, mesh geometry, UVs, textures and exports.
---

Use the real tools exposed by the `shardbound_blockbench` MCP. Start with
`status`, `read_project` and `capture_view`. Show the captured image before
editing; use the returned `project_id` and `expected_revision` for edits.
Only Generic Model (`free`) is supported.

This private account plugin requires its registered ShardBound MCP connection.
ChatGPT reaches the Windows stdio server through the OpenAI Secure MCP Tunnel.
The owner runs Install-ChatGPT-Desktop-Plugin.ps1 or Connect-ChatGPT.ps1 locally.
The account plugin and a local Codex MCP configuration are different integrations.

If tools are absent, state that this session did not expose the connection's tools.
Do not infer a Node spawn failure from that alone. Have the owner inspect the
account connection and Status-ChatGPT-Desktop-Plugin.ps1 diagnostics.
If `status` reports `connected: false`, ask the user to open
Blockbench, load the paired `.local/shardbound_bridge.js`, and open a Generic
Model. Never ask for pairing tokens, API keys, OAuth tokens, or passwords.

Read `mtools_list` before selecting a MTools action. Only entries with
`automated: true` are implemented. Set exact mesh/component selection with
`select_elements`; do not invent action names.

Mesh vertex coordinates are local to the element origin. Faces use vertex keys;
UV coordinates use texture pixels. Use `read_project(geometry: true)` to obtain
real keys before patching. Create groups first, then use their UUIDs as parents.

Verify edits with geometry reads and `capture_view`. After a timeout or
disconnect, inspect the project before repeating a mutation.

Use `list_model_files` and `open_project_file` for sources under `assets/`.
`save_project` saves editable sources there; overwriting requires explicit
`overwrite: true`. Exports use unique files in `outputs/` unless a destination
under `assets/` is provided.
