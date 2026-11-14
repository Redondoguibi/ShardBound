# ShardBound Studio over MCP

Use real tools supplied by the registered connection, regardless of whether
the host prefixes the namespace as shardbound_blockbench or shardbound_mcp.
Do not replace a missing tool with invented JavaScript or claims of completed work.

1. status → read_project(geometry:true) → capture_view; show the real capture.
2. If format is shardbound_studio, call studio_read. Its state lists model roots,
   bindings, the reference player, all authored effects and audio metadata.
   clip UUID + offset/limit returns native transform keyframes. Avoid dumping
   an entire large geometry or keyframe list when a page suffices.
3. Plan from actual UUIDs. Create geometry under the selected model's declared
   roots. Use studio_model to add/bind/manage independent models. Internal
   folders are not separate exported objects. Do not include neutral player
   geometry in an equipment model. Use reference body folder UUIDs for full-body
   equipment animations. studio_locator makes actual attachment markers.
4. Use studio_clip and studio_keyframes for native group position/rotation/scale.
   Rotations are degrees; values are numeric, not Molang/scripts. Interpolation
   supports linear, step, Catmull-Rom and Bezier handles. Read back IDs after edits.
   Animation length is at most 1000s; native snapping is 1–120 FPS.
5. studio_effect edits every supported emitter, sprite atlas, life curve, trail,
   flash and glow setting. A texture is a real project texture UUID; create/import
   it with put_texture and paint pixels with paint_texture. Read get_texture to
   verify actual pixels. studio_material controls MASK/OPAQUE and sidedness.
6. studio_event adds/edits/removes particle/trail/flash/glow/sound events in the
   same native timeline. Use seconds for time, the composite event ID from
   studio_read, actual attachment UUIDs, explicit instance names for start/stop,
   and follow/detach where appropriate. Removing resources/targets with references
   is rejected: update/remove those events first, after inspecting them.
7. studio_sound imports existing WAV/OGG/MP3 under assets/. The editor decodes
   actual bytes before accepting it. Do not call a fabricated audio placeholder
   a finished sound. studio_skin imports a real PNG 64x64 or project texture;
   choose wide/slim when creating the player. Layers may be toggled independently.
8. studio_timeline seeks/plays/pauses the real clip with FX/audio. Scrubbing does
   not replay sounds. Check preview.error and capture_view at a useful frame.
9. studio_validate → save_project(path:"assets/...sbstudio") → studio_export.
   studio_export produces the complete independent animated GLBs plus four
   versioned companions, original audio and sprites. Generic export_project GLB
   is deliberately rejected for Studio because it would lose reference filtering
   and synchronized authoring data. bbmodel/sbstudio remain editable snapshots.

New Studio project: studio_create_project(type,id,name,variant). Existing tabs
stay open. A Generic project is not automatically converted to Studio. Inspect
and explicitly copy/import needed geometry instead of silently changing formats.

The account connection remains the existing OpenAI Secure MCP Tunnel; no API
key or OAuth reset is required by this update. Local commands:
Update-Studio-Integration.ps1 -RestartConnection (one-time update), then
Connect-ChatGPT.ps1 after restarting Windows. Reload both native plugin files,
open a new normal Chat/Desktop or Codex conversation and select this plugin.

Limits: Studio project import 32 MiB through MCP; sound import 24 MiB, PNG import
4 MiB; package transfer 256 MiB, chunks 1 MiB, expires after 5 minutes idle.
The complete package is written on the Blockbench PC, not a remote download URL.
Game runtime integration and identical in-game rendering remain phase B.
Native Blockbench lighting and extreme sound pitch have the documented editor
limitations. Never claim a local test proves tool availability in normal Chat.
