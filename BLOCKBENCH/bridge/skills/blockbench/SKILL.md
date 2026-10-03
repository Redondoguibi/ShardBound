---
name: blockbench
description: Inspect or edit the user's open Blockbench Generic Model through the ShardBound MCP, including adapted MTools operations, mesh geometry, UVs, textures and exports.
---

First check whether real MCP tools from `shardbound_blockbench` are available
in the current conversation. Reading this skill or selecting the plugin does
not prove tool discovery. If tools are absent, report that limitation; do not
claim to have called status or ask a user already on Desktop to open Desktop.
An absent tool requires host discovery/connection troubleshooting. A real
status result with connected:false requires Blockbench pairing instead.

Use the real tools exposed by the ShardBound Blockbench connection, including
when the host assigns a different namespace from `shardbound_blockbench`.
Start with `status`, `read_project` and `capture_view`. Show the captured image
before editing; use the returned `project_id` and `expected_revision` for edits.
Only Generic Model (`free`) is supported. Existing project tabs stay open.

Read `mtools_list` before selecting a MTools action. Only entries with
`automated: true` are implemented; the result includes their parameter schemas.
Set exact mesh/component selection with `select_elements`. Select a complete
connected component for MTools 2.1.0 Laplacian Smooth. Do not invent action names.

Mesh vertex coordinates are local to the element origin. Faces use vertex keys;
UV coordinates use texture pixels. Use `read_project(geometry: true)` to obtain
real keys before patching. Create groups first, then use their UUIDs as parents.

Verify edits with geometry reads and `capture_view`. Use `undo_redo` carefully:
native history may also contain the user's manual changes. After a timeout or
disconnection inspect the project before repeating a mutation.

Use `list_model_files` and `open_project_file` for sources under `assets/`.
`save_project` saves the editable source there; overwriting requires explicit
`overwrite: true`. Exports use unique files in `outputs/` unless a destination
under `assets/` is provided. `.bbmodel` retains the project and bitmaps;
`.gltf` and `.glb` embed assets and export static geometry; `.obj` is geometry only.
A local filesystem path is not a downloadable
ChatGPT URL. This bridge does not expose arbitrary JavaScript or shell execution.

If a real status call reports disconnected, ask the user to open Blockbench and load the paired
`.local/shardbound_bridge.js`. Run setup from the installed package folder before
first use. The local stdio package requires a host capable of starting processes;
registering it only in Codex does not install it in ordinary ChatGPT Chat.
The repository keeps a stdio configuration for Codex; the private account
release 0.2.0 uses a remote HTTPS MCP endpoint. Both transports have been tested
with real MCP clients. Tool availability in ordinary ChatGPT Chat has not yet
been verified. HTTPS requires OAuth authorization and a running tunnel.
An installed package or manually configured Desktop MCP does not by itself
prove the connection is registered for ordinary Chat. Inspect the actual host
connection and discovered tools; a registered app may need a verified
`plugin_asdk_app...` mapping. Do not invent IDs or infer an account restriction
from absent tools. Never ask the user to paste pairing tokens, OAuth tokens or
the consent password into chat. See CHATGPT-DESKTOP.md.
