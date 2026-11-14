# Native SDK v1

Engine extension API 1 uses additional JSON operations through this same ABI:
`engine.info`, `engine.configuration`, `engine.configure`, `engine.register`,
`engine.default`, `engine.enabled`, and `engine.remove`. Binary struct layouts
remain unchanged. See the [engine extension contract](../../docs/modding/12-ENGINE-EXTENSIONS.md)
for phase contexts, limits, replacement ownership, and the native hook workflow.

Include `shardbound.h`, build a Windows x64 DLL, and export
`int SB_CALL sb_mod_init(const SbHost*, SbPlugin*)` with C linkage. Fill the plugin
descriptor and return `SB_OK` only after initialization succeeds. Keep no borrowed
JSON buffers. Do not throw exceptions across the ABI. Calls use the runtime thread.

The full lifecycle and compatibility contract is in
[21-EXTENSIBILITY-V1](../../docs/21-EXTENSIBILITY-V1.md).
An executable example is [sample.cpp](../../examples/native/sample.cpp), built as
the `native_sample` CMake target.

`host->call` takes one JSON request and returns JSON in the caller's output buffer.
`required` includes NUL. Mutations require at least 32 bytes before any effect;
all mutation responses fit in that size. Reads may return `SB_BUFFER_TOO_SMALL`:
allocate `required` bytes and repeat. `component.get` can perform a registered
migration on first access. Errors return `SB_ERROR` and produce a runtime log.

| Operation | Request | Response |
|---|---|---|
| `log` | `{"message":"hello"}` | `null` |
| `entity.find` | `{"name":"actor"}` | `"123"`, or `"0"` if absent |
| `entity.create` | `{"name":"actor"}` | New runtime ID as string; owner is calling mod |
| `entity.destroy` | `{"entity":"123"}` | `null` |
| `component.define` | `{"type":"my_mod:counter","version":1,"defaults":{"count":0}}` | `null` |
| `component.add` | `{"entity":"123","type":"my_mod:counter"}` | `null` |
| `component.get` | Same as add | Component data object |
| `component.set` | `{"entity":"123","type":"my_mod:counter","data":{"count":2}}` | `null` |
| `component.remove` | Same as add | Boolean |
| `state.get` | `null` | Calling mod's saved state, or null |
| `state.set` | Any serializable JSON data | `null` |

Entity creation adds identity/name/active, not a visual model or physical body.
Use script definitions and the gameplay API for visuals, physics and player
control. Native systems can attach data and subscribe to runtime events/hooks.
Pointers and runtime IDs must not be retained across a world load: re-resolve
entities on `on_loaded`. Durable script references use `entity_key`.

`subscribe(context,name,callback,user,hook,&token)` returns a removable token.
For events, callback input is an array and output is ignored. For hooks, input
is a single JSON value and output must be the replacement JSON, NUL terminated.
The host provides a 1 MiB output buffer. Entity references in nested payloads use
`{"$entity":"123"}`; do not forge or persist these transient handles.

Only `native` declared in mod.json is loaded, and only with `--allow-native`.
The loader validates size/ABI, removes callbacks before unload and rejects
incompatible modules. Native code is in-process and trusted, not sandboxed.
