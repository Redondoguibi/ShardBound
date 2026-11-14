---
name: blockbench
description: Inspect and author Generic Models or complete ShardBound Studio projects in Blockbench 5.2.2 through MCP, including native group animation, particles/VFX, synchronized audio, skins, painting, materials, MTools and complete package export.
---

Use the real tools exposed by the `shardbound_blockbench` MCP. Start with
`status`, `read_project` and `capture_view`. Show the captured image before
editing; use the returned `project_id` and `expected_revision` for edits.
Supported formats are Generic Model (`free`) and ShardBound Studio
(`shardbound_studio`). Studio requires Studio 1.1+ and Bridge 0.5+ loaded together.
Check `status.studio` and `bridge_version`; never infer compatibility from a
plugin attachment alone. Read [Studio workflow](references/studio.md) for Studio.

This private account plugin requires its registered ShardBound MCP connection.
ChatGPT reaches the Windows stdio server through the OpenAI Secure MCP Tunnel.
The owner runs Install-ChatGPT-Desktop-Plugin.ps1 or Connect-ChatGPT.ps1 locally.
The account plugin and a local Codex MCP configuration are different integrations.

If tools are absent, state that this session did not expose the connection's tools.
Do not infer a Node spawn failure from that alone. Have the owner inspect the
account connection and Status-ChatGPT-Desktop-Plugin.ps1 diagnostics.
If `status` reports `connected: false`, ask the user to open
Blockbench, load the paired `.local/shardbound_bridge.js`, and open a supported
project. For Studio also load `studio/dist/shardbound_studio.js` and open/create
a Studio project. Never ask for pairing tokens, API keys, OAuth tokens, or passwords.

Read `mtools_list` before selecting a MTools action. Only entries with
`automated: true` are implemented. Set exact mesh/component selection with
`select_elements`; do not invent action names.

Mesh vertex coordinates are local to the element origin. Faces use vertex keys;
UV coordinates use texture pixels. Use `read_project(geometry: true)` to obtain
real keys before patching. Create groups first, then use their UUIDs as parents.

Verify edits with geometry reads and `capture_view`. After a timeout or
disconnect, inspect the project before repeating a mutation.
Re-read status after each mutation, selection change or Undo; revisions are
opaque monotonic counters, not a predictable increment. Studio commands require
both project_id and expected_revision. Native timeline/painting modes work with Studio.

Use `list_model_files` and `open_project_file` for sources under `assets/`.
`save_project` saves `.sbstudio` for Studio and `.bbmodel` for Generic there;
the extension must match the active format. Overwriting requires explicit
`overwrite: true`. Exports use unique files in `outputs/` unless a destination
under `assets/` is provided.
