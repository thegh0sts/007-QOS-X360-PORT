# Native renderer patches for 007's menu investigation (not applied)

Moved here from the Bourne repo (`native-plugin\patches-wip`) so Bourne releases build with the renderer as tested.
They apply to Skullburglar's renderer after the Bourne repo's `native-plugin\patches\0001-0005`:

- 0006 point lists (point sprites) expanded in the vertex shader.
- 0007-0016 debugging switches and traces (`BOTS_GFX_ALWAYS_DEPTH`, `BOTS_GFX_BASE_LEVEL_ONLY`, `BOTS_GFX_NO_BLEND`,
  `BOTS_DUMP_TEXTURE`, `BOTS_GFX_ONE_LEVEL`, `BOTS_GFX_NO_TEXTURES`, raw fetch constants and vertices in the trace).
- 0017 shader microcode copied when loaded rather than at the draw.

To use them, copy them into the Bourne checkout's `native-plugin\patches\` (e.g. `tbc-test`) and run
`tools\build_native.bat`; the 007 build then picks the plugin up from the SDK.
