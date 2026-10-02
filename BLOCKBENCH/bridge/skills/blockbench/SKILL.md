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

Use the connected `shardbound_blockbench` tools. Start with `status` and
`read_project`; use the returned `project_id` and `expected_revision` for edits.
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

Exports create new files in `outputs/`, on the local machine. `.bbmodel` retains
the editable project and bitmaps; `.gltf` embeds assets and exports static
geometry; `.obj` is geometry only. A local filesystem path is not a downloadable
ChatGPT URL. This bridge does not expose arbitrary JavaScript or shell execution.

If a real status call reports disconnected, ask the user to open Blockbench and load the paired
`.local/shardbound_bridge.js`. Run setup from the installed package folder before
first use. The local stdio package requires a host capable of starting processes;
registering it only in Codex does not install it in ordinary ChatGPT Chat.
Local MCP availability varies by Chat/Work/Codex host. This stdio integration
has been tested with a local MCP client; it has not been verified in ordinary
ChatGPT Chat. Ordinary Chat may require a registered remote HTTPS MCP or Secure
MCP Tunnel. Never promise that installing this package exposes stdio tools in
every Desktop conversation. See CHATGPT-DESKTOP.md.
