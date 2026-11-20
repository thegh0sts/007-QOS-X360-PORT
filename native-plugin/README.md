# Native renderer patches for 007: Quantum of Solace

`tools\build_native.bat` (copied from the Eat Lead project) builds the shared SDK in `C:\rexglue-src` with these
applied to Skullburglar's renderer:

- 0001-0015: the Eat Lead set (Bourne's 0001-0011, tiled resolves, menu hint, pad prompts, slope depth bias).
- 0016 point lists (from `native-patches\0006`): point sprites expanded in the vertex shader. QoS draws single points
  from nested command lists (the log's "unsupported draw (primitive): prim 1").
- 0017 shader microcode copied when it's loaded (from `native-patches\0017`): QoS's menus rewrite their vertex shaders
  between batches, so reading them at the draw got a later batch's version. `BOTS_GFX_SHADER_AT_DRAW=1` reverts.

The SDK folder is shared with Bourne and Eat Lead: the project whose `build_native.bat` ran last decides the
renderer. 0016 and 0017 should be harmless for the other two but haven't been tested there.

`native-patches\` keeps the original debugging patches (0007-0015: traces and switches).
