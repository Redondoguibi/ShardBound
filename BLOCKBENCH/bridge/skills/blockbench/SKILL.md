---
name: blockbench
description: Inspect or edit the user's open Blockbench Generic Model through the local ShardBound MCP, including adapted MTools operations, mesh geometry, UVs, textures and exports.
---

Use the real tools exposed by the `shardbound_blockbench` MCP. Start with
`status`, `read_project` and `capture_view`. Show the captured image before
editing; use the returned `project_id` and `expected_revision` for edits.
Only Generic Model (`free`) is supported.

For this private Desktop plugin, the MCP is local stdio on the owner's Windows
machine. It does not require Cloudflare, a remote MCP URL, OAuth, or Secure MCP
Tunnel for normal ChatGPT Desktop use.

If tools are absent, report that the Desktop host did not start/discover the
local MCP. If `status` reports `connected: false`, ask the user to open
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
