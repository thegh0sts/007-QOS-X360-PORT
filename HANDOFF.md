# 007: Quantum of Solace (Xbox 360) - handoff

ReXGlue recompilation of *007: Quantum of Solace* (Treyarch, 2008, Call of Duty 4 engine). Project name `qos`.
Local only: `%USERPROFILE%\source\repos\007qol` (no GitHub repository yet). Started 2026-11-11, paused 13-14 Nov,
resumed 2026-11-19. **This file is how work resumes after a context reset:** update it (here and the project
copy `007/HANDOFF.md`) whenever something is found, decided or changed, not only at the end of a session, and
keep "Resume here" current.

## State (2026-11-20)

- Builds and runs with **Xenos** (`--gpu_plugin xenos`, the project's default): fine.
- **Native renderer** (`--gpu_plugin native`, run with `BOTS_GFX_NESTED_DRAWS=1`): the menus now render, text
  included (confirmed 20 Nov). Not yet played through further.
- **Leaving a level for the main menu crashed** (Xenos and native alike: sub_82299400 reading 0x70172634). Fix
  added 20 Nov, **not yet tested**: `setjmp_address`/`longjmp_address` in `default_functions.toml` (see below).
- Run from the build folder with `--game_data_root=%USERPROFILE%\source\repos\007qol\game`.
- Fixes found while porting QoS went into the shared SDK patches (Bourne's `tools\sdk-patches`): jump tables (0001,
  0002), XMA decoder input checks (0003), input system thread safety (0005, a 0xC0000374 heap crash).
- `src\fps_unlock.cpp`: `--frame_rate` sets com_maxfps's default (Dvar_RegisterInt, sub_822E4188). Untested above 30.
- Code: `default_functions.toml` (asset-type switch cases called through vtables; setjmp/longjmp),
  `default_functions_scan.toml`.

## Findings

- **setjmp/longjmp:** MSVC CRT setjmp 0x825ABAE0, longjmp 0x825AB7C0. Without them in the codegen config, the
  game's error recovery (longjmp back to the frame loop) restored the guest registers but returned into the caller,
  which then used the frame loop's r31 as its own pointer.
- **Native, missing menu text and images - the SDK's SPIR-V translator** took a texture fetch's exponent bias
  (exp_adjust) from bits 13:18 of fetch constant word 4 (inside the LOD bias) instead of word 3, where the DXBC
  translator reads it. QoS's menu textures have LOD bias -0.5, which read as exp_adjust -8: every sample 1/256 as
  bright. Fixed by SDK patch `tools\sdk-patches\0013-texture-exp-adjust-word.patch` (OWN_PATCHES own13). Pushed
  to Bourne (as its 0012, tag tbc12, `2d5e104`) and Eat Lead (0013, own13, `02bc95d`) on 2026-11-20.
- QoS's vertex shaders have their vertex fetch constants patched by D3D between batches: native patch 0017 (copy
  shader at load) is needed, otherwise the text reads vertices through slot 0 (a texture constant).
- Point lists (24 single-point draws a frame, vertex shader 1F794AB4, no pixel shader): native patch 0016.
- **Xenia Canary game patch "Unlock FPS"** (xenia-canary/game-patches, `415607FF - 007 Quantum of Solace.patch.toml`,
  by Sowa_95; the instructions at its addresses match our executable): nops two branches in the frame limiter
  sub_8229B7C0 (0x8229B808 `bne`, 0x8229B86C `bge`: the com_maxfps clamp) and changes the immediate of a
  `li r11,N` at 0x820F2280 in sub_820F2188 (the D3D presentation interval: 1 = 60 FPS, 0 = unlimited). Byte
  patches can't apply to recompiled code; the same effect needs hooks (as Eat Lead's `frame_pacing.cpp`), with
  `src\fps_unlock.cpp`. Its warning: above 30 FPS some missions can softlock.
- Debugging leftovers you can delete: the `spirv` folder and *-trace.txt / *-dump.txt / texture-*.bin files in the
  build folder.

## Native renderer work

- `native-plugin\patches`: the Eat Lead set, plus 0016 point lists, 0017 copy shaders at load, and two debug
  patches: 0018 (`BOTS_GFX_VFETCH_TRACE=1`, `BOTS_GFX_VFETCH_ANY_TYPE=1`, not needed) and 0019
  (`BOTS_GFX_NO_BLEND=1`, `BOTS_GFX_BASE_LEVEL_ONLY=1`).
- Build with `tools\build_native.bat` from this folder, then rebuild qos (its build copies the plugin). After an SDK
  translator change, delete `%USERPROFILE%\Documents\qos\cache\native` (cached pipelines keep old shaders).
- The SDK folder `C:\rexglue-src` is shared: whichever project's build ran last decides the patches. Run that
  project's `build_native` before rebuilding it.
- Existing renderer switches worth knowing: `BOTS_DRAW_TRACE=1`, `BOTS_DUMP_SPIRV=<dir>`, `BOTS_NULL_TEXTURE_WHITE=1`,
  `BOTS_GFX_DECODE_BC=1`.

## Resume here

- **Last thing done (20 Nov):** text fix confirmed; setjmp/longjmp fix written but not built or tested.
- **Shared SDK folder:** last built by 007 (SDK own13 + native 0001-0019). Eat Lead has the same SDK set (own13,
  native 0001-0015); Bourne's is tbc12. Each project's `build_native` resets it to its own set.
- **Patch overview for all games:** project doc `claude/patches.md`.
- **Patch sources:** the PC copies in `tools\sdk-patches` and `native-plugin\patches` are the masters. Claude's
  scratch copies in the cloud workspace don't survive between sessions: to change a patch, read it from the PC,
  apply it to a clean SDK/renderer checkout, edit there, and write the regenerated patch back.
- **Writing files to the PC:** after writing, check the file's size on the PC. On 19 Nov a rewrite of the same
  path silently kept the old version; writing it from a new staging folder fixed it.
- **Full rebuild and run** (Developer Command Prompt):

```bat
cd %USERPROFILE%\source\repos\007qol
tools\build_native.bat
cmake --build out\build\win-amd64-relwithdebinfo
cd out\build\win-amd64-relwithdebinfo
set BOTS_GFX_NESTED_DRAWS=1
qos.exe --game_data_root=%USERPROFILE%\source\repos\007qol\game --gpu_plugin native
```

- **Logs and crashes:** `out\build\win-amd64-relwithdebinfo\logs\qos_NNN.log`, `crash-PID.txt/.dmp`. Generated
  code: `generated\default\qos_recomp.N.cpp` (`codegen.partition.json` maps a function's address to its file N).

## Next

1. Rebuild (codegen reruns for the toml change) and test leaving a level for the main menu.
2. Play further on native; compare with Xenos.
3. Then: frame rate above 30, launcher (from Eat Lead's), repository.
