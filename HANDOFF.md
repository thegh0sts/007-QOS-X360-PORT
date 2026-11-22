# 007: Quantum of Solace (Xbox 360) - handoff

ReXGlue recompilation of *007: Quantum of Solace* (Treyarch, 2008, Call of Duty 4 engine). Project name `qos`.
Folder `%USERPROFILE%\source\repos\007qol`, repository thegh0sts/007-QOS-X360-PORT (first push 2026-11-20). Started 2026-11-11, paused 13-14 Nov,
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
- **Holding B (1 s) quits from the Press Start screen** (`src/menu_quit.cpp`, written 2026-11-20, hold added the same
  day at the user's request, **not yet tested**): a "B QUIT (HOLD)"
  hint (our own art, `hints/`, made by `tools/make_quit_hint.py` to match the game's "A SELECT" prompt; PS circle
  version with a PlayStation pad) at 1080p (354,922), where the main menu has "A SELECT". Drawn by the native
  renderer only (patch 0013); B quits on Xenos too. `--menu_b_quit=false` turns it off.
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
- **Title screen detection:** dvar `ui_reachedIIS` ("Has the player reached the PRESS START IIS? 0 not yet, 1-2 there
  now (normal=1/brightness=2), 3 past it"; on the PC the Press Start screen showed with 2, so 1 and 2 both count), registered by Dvar_RegisterInt (pointer also stored at 0x83725B30),
  value = big-endian int at dvar+12. Attract-mode demo (com_attractmode on, 90 s idle): byte 0x827CDD8C is 1 while
  it plays (set in sub_821AF8C8, cleared in sub_821AF7B8). ui_reachedIIS is already 2 during the intro logo videos,
  so videos are ruled out too: cinematicGlob at 0x841E56A0, +256 current video name (cleared by StopPlayback
  sub_825667C8), +777 finished (sub_82566BA8). Name set and not finished = playing, except the menus' looping
  background videos (bink_bkg_loop_0/1, bink_ig_bkg_loop_1): the title screen plays bink_bkg_loop_1 behind it
  (log, 20 Nov), so those don't count. Intro logo video: atvi_treyarch. XInputGetState is sub_82425198 (r3 user, r4 state); the
  engine polls all four pads every frame (sub_82304048), so polling patterns can't tell screens apart. Decrypt the
  XEX with Eat Lead's `tools/xexdump.py` to search its strings.
- **PlayStation prompts (20 Nov):** every button picture is a texture. Menus: Scaleform's platform
  library `scaleform/CmSharedPlatform.gfx` (and `cm_ig_textures.gfx` in game) uses external images btn_accept (A),
  btn_back (B), btn_y, btn_refresh, btn_left/right, bumper_left/right, trigger_left/right, stick_left/right,
  button_back, btn_nav_up/down/navigate_*, and `controller` (the controls screen's picture), loaded from ui.ff.
  In game: trigger_use hint icons HINT_BUTTON_A/B/X/Y/GREY (materials). Text: @PLATFORM_PRESS_START, the controls
  screen's "A BUTTON"... labels (localized strings in the fastfiles). The .gfx files are zlib SWF ("CFX": 8-byte
  header, then zlib). The engine also has PS3 code (ui_circleBtnIsEnter).
  - **Found by texture dump** (patch 0020, `BOTS_GFX_DUMP_TEXTURES=<dir>`; decoder `texdec.py` in the cloud
    workspace, not kept): menu A = `FFC0169EA80A8F57`, menu B = `09784A4CD20751E1`, both 64x64 DXT5 (format 20),
    a 40 px disc centred in the texture (texels 12..52). The dump runs so far only reached the title and main menu:
    no X/Y/bumper/trigger/stick textures seen yet. To find more: run with the dump on, visit the controls screen,
    pause menu and an in-game hint, then send the new files.
  - **Implemented (20 Nov), not built or tested yet:** native patch `0021-replace-textures.patch` adds
    `replace <name> <w> <h> <hash> <image> [bgr]` to prompts/playstation.txt: a texture of any format whose base
    level hashes to that (same hash as the dump names) is shown as prompts/<image>.png while pad_prompts_active
    is 1, reloaded when it changes. playstation.txt is also looked for in prompts/ beside the exe. `src/
    pad_prompts.cpp` (as Eat Lead's, no text swaps yet) sets pad_prompts_active from ConnectedPadStyle
    (--pad_prompts auto/xbox/playstation); called from menu_quit.cpp's XInputGetState hook. Art: `tools/
    make_ps_buttons.py` -> `prompts/ps_cross|circle|square|triangle.png` (cross = A's look turned blue, circle =
    B's maroon). CMake copies `prompts/` beside the exe. If the colours come out with red/blue swapped, add `bgr`
    to the line in playstation.txt.
- Debugging leftovers you can delete: the `spirv` folder and *-trace.txt / *-dump.txt / texture-*.bin files in the
  build folder.

## Native renderer work

- `native-plugin\patches`: the Eat Lead set (0013 changed: hint images up to 7680 pixels and 256 wide, not 160x48, for
  the 172x31 "QUIT (HOLD)" hint), plus 0016 point lists, 0017 copy shaders at load, and two debug
  patches: 0018 (`BOTS_GFX_VFETCH_TRACE=1`, `BOTS_GFX_VFETCH_ANY_TYPE=1`, not needed) and 0019
  (`BOTS_GFX_NO_BLEND=1`, `BOTS_GFX_BASE_LEVEL_ONLY=1`).
- Build with `tools\build_native.bat` from this folder, then rebuild qos (its build copies the plugin). After an SDK
  translator change, delete `%USERPROFILE%\Documents\qos\cache\native` (cached pipelines keep old shaders).
- **007 has its own SDK folder `C:\rexglue-src-007` (since 22 Nov 16:30)**: build_sdk/build_native and
  build_installer default to it; Bourne and Eat Lead keep `C:\rexglue-src`. Before that the folder was shared and
  whichever project's build ran last decided the renderer (the 14 MB DLL mix-ups of 22 Nov).
- Existing renderer switches worth knowing: `BOTS_DRAW_TRACE=1`, `BOTS_DUMP_SPIRV=<dir>`, `BOTS_NULL_TEXTURE_WHITE=1`,
  `BOTS_GFX_DECODE_BC=1`.

## Resume here

- **Last thing done (20 Nov):** PlayStation menu buttons (A/B -> cross/circle) written: native patch 0021, src/
  pad_prompts.cpp, prompts/ (see Findings). Needs `tools\build_native.bat` then the game build; check the log for
  "pad prompts: ... 2 replaced textures" and "btn_accept replaced". Still untested: the setjmp/longjmp fix, B QUIT
  (HOLD) with the menu-background fix, render scales 1.5x/2x/3x.
- **PS prompts, test 1 (20 Nov, 19:14):** menu A/B swap works (user confirmed). In-game prompts still Xbox.
- **In-game prompts found (20 Nov, 19:35):** a dump with `BOTS_GFX_DUMP_MAX=2048` (texdump2; delete the video frames
  `*_f2_t0_*` first, they number thousands) showed the in-game button glyphs live in the HUD font atlas
  `C1DB20162D971011`, 512x1024 DXT5: 28 px A B X Y at (103,192) (137,192) (171,192) (205,192), LB RB LS RS LT at
  y 190-222, RT at (5,211); 14 px set around y 411-440 (exact cells in prompts/playstation.txt). New native patch
  **0022-dxt5-pad-sheets** (`texture ... dxt5` in playstation.txt: decodes the tiled DXT5 base level, verified
  against texdec.py, then paints like the ARGB sheets). Art `prompts/g_*.png` (make_ps_buttons.py): cross blue,
  circle red, square pink, triangle green, L1/R1/L2/R2/L3/R3 grey pills. START/BACK/D-pad left as they are.
  **Not built/tested yet.**
- **PS prompts, test 2 (20 Nov, 19:40):** 0022 works: HUD glyphs (Dash - Hold, Cover...) show the PS buttons.
  Still Xbox: the tutorial box ("Press [LB] to switch weapons", "Hold [LT] to aim", "Press [BACK] to access your
  phone": dark rounded glyphs, a different texture, probably Scaleform cm_ig_textures images bumper_left,
  trigger_left, button_back...) and the takedown QTE (a big X disc with grey dots in a diamond). Patch 0020 changed to
  dump tiled textures only (no video frames). Asked for a dump run (texdump3, BOTS_GFX_DUMP_MAX=2048) showing a
  tutorial hint and a takedown.
- **Tutorial/takedown pictures found (20 Nov, 19:50, texdump3):** 64x64 DXT5, swapped whole with `replace`:
  X disc 82C5D2A286CB4ABC (40 px) -> t_square, takedown X CE0004494F1602F5 (50 px) -> t_square_big, LB
  FFE4445869FD3106 -> t_l1, LT DC83034FD7B78B60 -> t_l2, LS 748B7606A1F93C3C -> t_l3, RS 9849DBEDAA951D82 -> t_r3
  (dark-grey pills, make_ps_buttons.py). The grey QTE dots (32F578B188679B72) stay. Not seen yet: these pictures'
  A/B/Y/RB/RT/START/BACK versions. Only playstation.txt and PNGs changed: a rebuild of the game copies them, no
  build_native needed. **Not tested yet.**
- **Test 3 (20 Nov, 19:54):** user wants LS/RS kept as the game's stick glyphs: the L3/R3 draws (HUD font) and
  replaces (tutorial) removed from playstation.txt (the g_l3/g_r3/t_l3/t_r3 PNGs are unused leftovers). Tutorial/
  takedown swaps hadn't shown because the build folder still had the old playstation.txt: the copy was a POST_BUILD
  step, which runs only when qos.exe relinks. CMakeLists now has a `qos_assets` target (ALL) copying hints/ and
  prompts/ on every build. **Check after a build:** `out\build\...\prompts\playstation.txt` matches the source.
- **Test 4 (20 Nov, 20:02):** takedown left (X -> square) works. The diamond's other three are the plain grey
  QTE dot (32F578B188679B72), not Xbox-specific: left as is (coloured A/B/Y takedown discs not captured yet). The
  phone hint's BACK is the HUD-font glyph: added START -> OPTIONS (three lines) and BACK -> touch pad (rounded bar)
  grey discs, g_options/g_touchpad, at font cells (308,190)-(334,214), (342,190)-(368,214) and small (432,424),
  (450,424). The START/BACK labels under the big glyphs are left. **Not tested yet.**
- **texdump4 (20 Nov, 20:24, user got all takedown/tutorial cases):** found A DD0679586F9D6F4A, B
  EB0894A556ADBDD3, Y 99FAE00AC5BB2840 (50 px discs, as the large X) -> t_cross_big/t_circle_big/t_triangle_big;
  RT 30044CB9F07E3F81 -> t_r2; the tutorial's dark BACK disc 92F008A6E5D310A6 -> t_touchpad (dark). D-pad picture
  23A5DEDB971F4898 left. RB and START pictures still not seen. **Not tested yet.** To find new pictures: list the
  dump, take the 64x64 f19/f20 files not seen before (cloud texdec.py, or ask Claude).
- **Grenade tutorial RB (20 Nov, 20:45, texdump5):** found RB FE735E11E62340EA (dark pill, as LB/LT/RT) ->
  `t_r1` (`replace tutorial_rb` in playstation.txt, written to the PC). texdump5 had no other new 64x64 picture:
  START tutorial picture still not seen. **Not tested yet** (game rebuild only; no build_native).
- **Render scale 1.5 (20 Nov, 20:53, qos_091.log):** applied (log "gfx native: render scale 1.5", frontbuffer
  1920x1080), but the user's display is 3840x2160, so the presenter stretches 1080p to 4K (bilinear): looks soft.
  The 3D view is also probably below 720p, as COD4 on the 360 (1024x600, 2xAA; same engine), so 1.5x would give
  about 1536x900 before the game's own upscale. Not confirmed from a trace yet. Next: try 3x (720p x3 = 3840x2160,
  the display's size).
- **Render scale 3 (20 Nov, 20:57, qos_092.log):** "render scale 3", frontbuffer 3840x2160, but the 3D still looks
  blocky. Measured on the user's 4K screenshots (1.5x and 3x): geometry edges step every ~3.75 screen px, i.e. the 3D
  scene is ~1024 px wide (guest size, unscaled), while the HUD text is sharp (scaled). So the scene is drawn or
  sampled at 1x somewhere: suspect the scene's resolve texture is read back from guest memory ("tex", ~1 texture
  reload after CPU writes per frame in the log) instead of the scaled host image ("rt"). Asked for a filtered
  BOTS_DRAW_TRACE at scale 2 (resolves, transfers, draws sampling "rt" or 720/600/576-high textures) ->
  scale-trace.txt.
- **Resolve dump at 1.5x (20 Nov, 21:00, resdump/, 17 resolves a frame):** the resolves ARE scaled: shadow maps
  768x768/768x3072/2304/1536, scene 1F148000 and 1EC58000 1536x912 (guest 1024x608), 1E978000 1536x1080 (guest
  1024x720: the 3D view after the game's own passes), 1F3A8000 and 1D4A8000 1920x1080 (frontbuffer, with HUD).
  But the 1920x1080 frontbuffer has a strong 15-pixel column period (1920/1024 x 8): the final pass reads the
  3D view as a 1024-wide image, not the 1536-wide host one. So the scaled host image is lost when the frontbuffer
  pass samples it (probably read from guest memory as "tex" instead of "rt"). Next: filtered draw trace to see the
  final pass's texture source.
- **Draw trace at 2x (20 Nov, 21:06, scale-trace.txt):** every scene pass is scaled and samples host images ("rt"):
  scene 2080x2432 (1024x608 2x MSAA), 1F148000 -> 1E978000 (draw: 608 -> 720 rows), then ONE final draw into the
  1280x720 front buffer (rt0 base 0x260) samples 1E978000 (rt 1024x720, host 2048x1440), opaque (blend
  00010001), no depth, 6 vertices; the HUD follows. Checked against the 1.5x dump: the front buffer matches
  "1E978000 shrunk to 1024 then stretched" (column periods 3/6/15), not "stretched directly" (period 5), and its
  colours equal the source's (no colour change). So the game's stretch shader reads whole guest texels (a custom
  filter), which caps the 3D at 1024 columns whatever the scale.
- **Fix written (20 Nov, 21:15), not built/tested:** native patch `0023-plain-upscale.patch`: at render scale != 1,
  a draw with one colour target, no depth, opaque full write, one pixel texture that is a resolve destination
  (>= 640 guest wide), <= 6 vertices, and a viewport wider than the source is followed by a linear vkCmdBlitImage of
  the source host image over the viewport (same format only). Log: "gfx native: plain upscale AxB -> CxD ..." (first
  4). `BOTS_GFX_PLAIN_UPSCALE=0` turns it off. Needs `tools\build_native.bat` + game build.
- **0023 test 1 (20 Nov, 21:20, qos_095.log):** built (renderer DLL 6,496,256 bytes, 21:17) but no "plain upscale"
  line: the check never matched (likely the viewport: a screen-space draw has none, or the texture-binding count).
  **0023 v2 written (21:25):** area = scissor (bounded by the viewport if any), the widest resolve read taken, and
  every candidate logged (first 8) with count, reads, textures, rt1, depth, mask, blend, formats and "done|skipped".
  Needs build_native + game build again.
- **0023 v2 test (20 Nov, 21:29, qos_096.log):** the check found the final stretch at 3x: "skipped: 3072x2160 ->
  0,0-3840,2160 of 3840x4320 (count 6, reads 2, textures 0+2, rt1 0, depth 0, mask F, blend 00010001, formats 37 ->
  37)". Only "reads 2" stopped it: the pixel shader binds the source twice (two texture bindings, probably the
  signed/unsigned pair of one fetch). **0023 v3 (21:33):** accepts several reads when all are the same image
  (log says "of one image" / "of different images"). Needs build_native + game build again.
- **0023 v3 works (20 Nov, 22:25, user's 4K screenshot at 3x):** "better but still looks pixelated". The 15-px
  column period is gone (3D now at full scaled resolution); geometry edges step per screen pixel. What is left is
  aliasing (the game's 2x MSAA is drawn as 2 rows per pixel by native 0002, so near-vertical edges get no
  smoothing) and texture minification streaks. Suggested `BOTS_GFX_FXAA=1` (2 = strong) and
  `BOTS_GFX_FORCE_ANISO=16`; not tried yet. Possible later: real host MSAA / horizontal supersampling.
- **FXAA + aniso test (20 Nov, 23:16, qos_098.log):** all on ("FXAA", "anisotropic filtering at least 16x",
  "plain upscale done ... reads 2 of one image"), user saw no difference. Row analysis of the 22:24 screenshot: a
  strong 3.55-row period (2160/608) -> the 3D is still only 608 rows: the game's 608 -> 720 rows pass (draw sampling
  1F148000 into 1E978000, surface pitch 1040) also reads whole guest texels. **0023 v4 (23:25):** a candidate is
  any opaque single-image draw into an area >= the source both ways and larger one way (so the 608 -> 720 pass too);
  a side within 1/32 of the source's keeps the source's size (pitch padding); 12 log lines. Needs build_native +
  game build. Check: "done" lines for both 3072x1824 -> ...x2160 and 3072x2160 -> 3840x2160.
- **DX12 version of the native renderer? (20 Nov, 23:40, plan only, nothing written):** possible but a real
  port. The renderer is Skullburglar's `src\gfx\vk` (in `C:\rexglue-src\skullburglar`): ~5,800 Vulkan-touching
  lines (draw.cpp 2177, renderer.cpp/.h 2417, texture 784, present 754, transfer 364, occlusion 178; ~70 distinct
  vk calls). hybrid_fill.cpp (UE3 research spike) dropped; native.cpp, pm4, manifest are API-neutral. Shaders: use
  the SDK's DXBC translator (as the Xenos D3D12 backend; alternative SPIR-V -> DXIL at runtime). 4 GLSL built-in
  shaders need HLSL; vkCmdBlitImage (0023) becomes a draw; SDK 0014's render-scale texture weights need the same
  change in the DXBC translator. Plan agreed so far (step-by-step in chat, 20 Nov 23:40):
  - Phase 0: pick a frozen baseline (Vulkan renderer still changing daily); own folder/repo for the renderer, one
    DLL for all three games (007's patch set as the superset, each game-specific patch off unless triggered);
    offline test harness: Skullburglar's frame capture (`capture.cpp`, hooks at Skullburglar's D3D addresses in
    `d3d_entries.inc`, so per-game addresses or a PM4-level capture needed) + `bots-replay` (renders a capture,
    PSNR vs reference, `image_diff.h`) extended to DX12; golden frames and saves per game; Bourne's state needed.
  - Test order: Bourne (UE3, the renderer's home, fewest patches), Eat Lead (deferred, 4 MRT, tiled resolves,
    render-scale vPos; Xenos is blank there), 007 last (nested draws, points, shader rewrites, most patches).
  - Port steps: skeleton+present, resources, shaders/pipelines, draws, textures, resolves/transfers, occlusion,
    patches; debug layer/GBV/DRED switch from the start; PIX or RenderDoc for wrong frames.
- **0023 v4 works (20 Nov, 23:21, qos_099.log):** "much better". Both stretches replaced ("done 3072x1824 ->
  3072x2160" and "3072x2160 -> 3840x2160"). Left: aliasing on the jaw/nose and a hard, stair-stepped shadow line
  down the nose (screenshot 23:21). The log also lists skipped candidates: passes on the MSAA scene surface that read
  the scene depth 1EC58000 (formats 100 -> 37, blend 00010706 / 010B0706: lighting/shadow passes).
- **Root cause found (23:30):** the SDK's SPIR-V translator only puts texture weights (getWeights, used by shaders
  that filter by hand) and fetch offsets on the host texel grid for Xenia's integer draw_resolution_scale, and only
  for textures flagged in the textures_resolution_scaled system constant, which the native renderer never set. So
  every hand-filtered read of a resolve destination (007's 3D stretches, and probably its depth-reading shadow/light
  passes) worked on whole guest texels. **Written, not built:** SDK patch `0014-render-scale-texture-weights.patch`
  (scale = Features::render_scale when draw_resolution_scale is 1) + `OWN_PATCHES=own14` in build_sdk.bat, and
  native patch `0024-textures-resolution-scaled.patch` (sets sc.textures_resolution_scaled for fetch constants bound
  to resolve destinations at scale != 1). Needs build_native, the game build, and deleting
  `%USERPROFILE%\Documents\qos\cache\native` (translated shaders are cached). A/B: `BOTS_GFX_PLAIN_UPSCALE=0`
  turns 0023 off; if the picture stays sharp then, 0014/0024 fixed the stretches properly (0023 could go).
- **SDK 0014 + native 0024 test (20 Nov, 23:31-23:35, qos_100/101.log):** built (renderer DLL 6,497,280, 23:30),
  shader cache cleared. Run 100 with 0023 on, run 101 with `BOTS_GFX_PLAIN_UPSCALE=0` (no upscale lines): the user
  sees no difference between them, i.e. with 0014/0024 the game's own stretches now keep the scaled resolution, so
  0023 is no longer needed (could be dropped or defaulted off; screenshot of a =0 run would confirm). The face is
  unchanged: the hard line down the nose / jaw is not a resolution problem. Next: compare the same spot with Xenos
  (`--gpu_plugin xenos`, 1x) to see whether the original game draws that shadow line too.
- **Xenos comparison (20 Nov, 23:37, user screenshot, same checkpoint):** the native renderer lights the scene
  very differently. Xenos: darker, hazier (more fog), pillars and Bond's face mostly in shadow, soft shading, no
  line down the nose. Native (23:21 shot): pillars and face brightly lit from the side, hard self-shadow on the face.
  Looks like native misses shadowing from the world (pillar) and/or fog: suspect the shadow-map path (the game
  resolves its shadow-map depth into 8888 textures, fmt 6 "depth src 4 -> fmt 6", and decodes depth in the shader;
  the renderer's raw 8888 depth encoding or the scaled shadow maps may not match). Next: native at render scale 1
  at the same spot, to split "scale bug" from "native renderer bug".
- **Native at 1x (20 Nov, 23:39, user screenshot):** same over-bright lighting as at 3x -> not a scale bug; the
  native renderer lights 007 wrongly at any scale. The shadow maps themselves render (1.5x resdump: 1DCE0000
  512x512, 1D8E0000 512x2048, 1DAE0000 512x1024 hold perspective depth from spot lights, values 0..0.38;
  1D9E0000 empty). So suspect the lighting passes' shadow test (how the depth resolves are sampled: depth value
  image via depth_texture_at_ for k_24_8 fetches, or the raw 8888 materialized from it). Asked for a draw trace at
  1x filtered on "resolve ", "(depth " -> shadow-trace.txt.
- **Shadow trace (20 Nov, 23:47, shadow-trace.txt, 1x, 120 MB):** cause found. The lighting draws sample the
  shadow atlas `1D8E0000` as k_24_8 512x2048 (t0..t5, ~300k reads) and `1DCE0000` 512x512. Each frame the game
  resolves four 512x512 shadow maps into that atlas at 1D8E/1D9E/1DAE/1DBE0000 (1 MB apart = 512 rows), each copy's
  destination height set to the rest of the atlas (2048, 1536, 1024, 512). TiledResolveBase (native 0012) only
  matched same-size textures, so copies 2-4 went into separate textures and the atlas held only the first map: the
  other lights read empty shadow maps and lit everything (bright pillars and faces, only odd self-shadow edges).
  Matches the 1.5x dump (1D8E0000 content only in its top quarter). **Native patch
  `0025-shadow-atlas-resolve.patch` (23:55), not built:** a copy also lands inside a taller texture of the same width
  and format when it fits (lowest base wins), and the depth and colour resolve paths use the enclosing texture's
  size. Needs build_native + game build (no SDK or shader-cache change). Check: same checkpoint vs Xenos.
- **0025 works (20 Nov, 23:54, two 4K screenshots at 3x):** "a vast improvement": pillars, faces and walls now in
  shadow, close to Xenos. Left: shadow edges very blocky (stair steps on the walls and Bond's cheek). Likely cause:
  0024 also flags the shadow atlas (a depth value image) as resolution-scaled, so SDK 0014 shrinks the game's
  shadow-filter fetch offsets and weights to host texels: the soft edge (a kernel sized in guest texels) becomes 3x
  narrower at 3x. **Native patch `0026-shadow-guest-weights.patch` (23:58), not built:** depth value images
  (R32_SFLOAT) keep guest-texel weights and offsets; `BOTS_GFX_SCALED_DEPTH_WEIGHTS=1` brings back the old way (A/B).
  Needs build_native + game build (no translator change, so no need to clear the shader cache). If still
  blocky: compare with native at 1x and with Xenos at the same spot.
- **0026 works (21 Nov, 07:56):** user: shadows in the level are softer now. Both 0025 and 0026 are general
  renderer rules (any copy into a taller texture; any shadow/depth texture), not tied to a level. Still to check on
  other levels: outdoor/sun shadows (may use a different map layout) and the hard line down the nose.
- **Grenade tutorial RB works (21 Nov, 08:03):** confirmed with a PS pad (an earlier run had only the Xbox pad
  connected, so Xbox prompts were correct). All tutorial/takedown swaps found so far work; only START not seen.
- **Level pop-in on first visit (21 Nov, 08:04-08:13):** a level's first start skips draws whose shaders are still
  being built (qos_109: ~2,000 at the first level; qos_110: ~77,000 at the next new level). Gone on later starts: the
  cache (Documents\qos\cache\native: 415607FF.manifest.bin 0.9 MB = shaders + pipeline records, the shippable part;
  415607FF.pipelines.bin 53 MB = driver cache, this PC only) is built at startup. User chose to ship shader refs.
- **Shader refs for 007 (21 Nov, 08:30), not built/tested:**
  - 007's fastfile: 7 big-endian dwords (0x1D6, zone size, ...), then one zlib stream at byte 28 (verified on
    code_post_gfx/common/ui/whites_estate/siena/operahouse). Shaders are stored raw in the zone; vertex shaders
    differ in their vfetch dwords (the XDK patches them from the vertex declaration): 2-21 dwords, kept as patches.
  - `tools/ff_shader_refs.py index MANIFEST pipelines/415607FF.refs --game game` (needs numpy; ~70 s for six fastfiles)
    and `check IN.refs --game game --manifest MANIFEST`. On the 08:10 manifest (800 shaders, 804 pipelines): 787
    referenced (631 exact, 156 patched; 7.5 KB of patch dwords), all rebuilt equal; 13 not in those six fastfiles
    (incl. 1E6883FCCDE1F688, a startup clear shader: probably in the XEX image or other levels).
  - `pipelines/415607FF.refs` (184 KB) on the PC; CMake's qos_assets copies pipelines/ beside the exe.
  - Native patch `0027-fastfile-shader-refs.patch`: the refs reader reads `.ff` sources (whole zone inflated with
    stb_image's zlib, ~1 s for a 220 MB level zone in a test; other language folders tried), zone freed after
    loading; shaders rebuilt from refs are appended to `415607FF.master.bin` in the cache folder, so later starts read
    no fastfile.
  - **Test:** build_native + game build; rename `Documents\qos\cache\native` (a fresh machine); start: log
    "shader refs ...: N pipelines, M shaders rebuilt", "kept in ...master.bin"; first level without pop-in. Second
    start: no "shader refs: ... read" lines. Then put the old folder back (or keep the new one).
  - To keep it complete: after playing new levels, rerun the tool on the newest manifest (needs the levels' .ff).
- **Frame rate and FOV (21 Nov, 09:05), not built/tested:** from the generated code and the decrypted XEX (cloud
  `/tmp/xex/xexdec.py`: XEX2 retail AES key + basic compression, image at 0x82000000; not kept between sessions):
  - Com_Frame = sub_8229B7C0: waits until 1000/com_maxfps ms have passed (dvar ptr at 0x83375314; registered 30,
    range 0-1000); with r_vsync (dvar ptr 0x84055C2C) on and timescale 1 also at least one refresh (1000 / the float
    at 0x841ED138). D3D swap packet sub_820F2188 maps device+13596 (presentation interval) 0/1 -> 1, 2 -> 2, 4 -> 3,
    0x80000000 -> 0; the Canary be8 at 0x820F2283 is the "1" case's immediate. (Earlier note "0x8229B808 bne /
    0x8229B86C bge" was wrong for this XEX: the branches are at 0x8229B820 and 0x8229B8A4; the Canary SP file
    couldn't be fetched, the MP file has 0x8225E8E0/0x8225E924 + 0x820F2283.)
  - `src/fps_unlock.cpp` rewritten: `--frame_rate` 20-60 (default 30) sets com_maxfps's default and its current value
    at every Com_Frame (configs could reset it); interval 2 passed as 1 above 30; logs "frame rate: ..." and the
    device's interval once. Game speed: the engine times frames in real ms.
  - New `src/fov.cpp` (in CMakeLists): `--fov` 65-110 (default 65): cg_fov (Dvar_RegisterFloat sub_822E41D0, name
    0x82016FF8, default 65, range 1-160, dvar ptr 0x827700C0) default = --fov; each frame a current value of exactly 65
    is set back to --fov (other values the game sets are left). Logs "fov: ...".
  - **Test:** game build only (codegen not needed); `qos.exe ... --frame_rate=60 --fov=90`; check log lines, the FPS
    counter, ADS zoom, cover camera, cutscenes; Canary warns some missions can softlock above 30.
- **60 FPS works (21 Nov, 09:08, qos_114):** log "com_maxfps was 30, set to 60" when the level loaded (its config
  resets it), interval 1, "fov: cg_fov default 90" + "back at 65, set to 90". User: fps a bit inconsistent in spots.
  Heavy scenes (~5,500 draws, 420 render passes, 1,150 barriers a frame) take ~16 ms of renderer work (12 ms draws,
  2.8 textures), so frames miss the vblank and take 33 ms (frame times bimodal 17/33, up to 400 of 600 at 33). Asked
  to try `--vsync=false` (misses cost ~20 ms, not 33) and `BOTS_GFX_PASS_MERGE=1` (renderer's pass merging, off by
  default), one at a time. If not enough: renderer per-draw CPU cost is the real fix.
- **End-of-mission video black on native (21 Nov, 09:18, qos_115):** whites_estate ends with video `open_credits`:
  audio plays, screen black. Xenos shows it (user, 09:25) -> native bug. While it plays the game sends exactly 1
  draw a frame (600 draws / 600 frames, 2 resolves, 2 transfers) and the renderer loads/reloads 0 textures, i.e. the
  video texture (CPU-written Bink frames) is never picked up. whites_estate_load and the menu background videos do
  play. Asked for a BOTS_DRAW_TRACE run (stderr -> video-trace.txt) from a checkpoint near the end.
- **21 Nov, 13:15 (qos_117), after 007's build_native (renderer 6,545,920 bytes, 0001-0027):** shader refs work on a fresh cache: "804 pipelines, 787 shaders rebuilt from the disc in 2150 ms", kept in 415607FF.master.bin, warm-up 1.3 s (17 missing a shader). **open_credits now plays** on native (no change aimed at it; the earlier black run may have had another renderer). **Achievement popup showed twice** (the renderer's own over the runtime's ImGui one): qos_app.h now returns no runtime popup with --gpu_plugin native, as Bourne does. Game build only. Pop-in on the first level with the fresh cache: ask the user.
- **Launcher (21 Nov, 13:40), written, not built or run:** `installer/` (qos_launcher.py, qos_installer.py, build_installer.bat, README.md; from Eat Lead's, look from the design canvas "007 Quantum of Solace launcher mockup"). Tabs Game files (language English/French, shader cache Clear and rebuild), Display (render scale, FXAA, frame rate 30/40/50/60, V-sync, FOV 65-110, FPS counter), Graphics, Controls (prompts, dead zone, vibration on/off), Saves, Achievements. Game side: qos_app.h now has show_fps/show_fps_detail/brightness cvars, sets BOTS_GFX_NESTED_DRAWS=1 itself with native, logs the version (src/version_log.cpp, QOS_RELEASE 0.1.0 in CMakeLists) and exports the achievement list (src/achievement_list.cpp). SDK bundle name qos1. Defaults: render scale 1x, AA off, game's filtering (untested in 007). --user_language untested.
- **Renderer CPU per draw (21 Nov, 14:15):** launcher install (its folder on another drive), game.log there. Busy scenes: ~11,500 draws a frame, 23-29 ms frames, renderer thread busy all of it (GPU 6-10 ms). BOTS_DRAW_PROFILE=1 per frame: textures 5.2 (2.8 of it reloads), setup 3.9, indices 3.6, descriptors 2.8, tracker 2.6, pass 2.0, constants 1.7, rest 3.5. BOTS_GFX_PASS_MERGE is already on by default. **Native patch 0028-draw-cpu (written, not built):** ProcessInvalidations in one pass over textures for all ranges and index uploads found by 64 KB block (index_blocks_) instead of a scan of all uploads per range per draw; shaders kept by ShaderLoad::serial (LoadShader keeps the serial for identical microcode) instead of XXH3 of both shaders every draw; each stage's last texture set reused without the hash lookup. Checked: applies in order (0001-0027, 0028, 0090), compiles apart from the cloud SDK's known mismatches. Next if needed: per-slot texture view/sampler memo, two-thread recording.
- **0028 measured (21 Nov 15:11, CPU not shared):** 11,800 draws in 17.4-17.9 ms = 1.47-1.51 us/draw (was 1.65-1.71); setup 3.9 -> 1.9. An earlier run looked worse because another compile shared the CPU (every phase and menus 2-3x). Left: textures 4.8, indices 3.6-4.0 (~4,200 index buffers converted per frame, ~7,600 reused), tracker 2.5, descriptors 2.4, pass 2.0. Frames still 26-27 ms (~37 FPS) at 4K, vsync off.
- **Native patch 0029-texture-memo-index-heap (written 21 Nov 15:21, not built):** (1) per-binding texture memo (texture_memo_[stage][binding]): same six fetch words + signedness + dimension and an unchanged texture_memo_epoch_ reuse the last view/swizzle/signs without texture_at_/guest_textures_/swizzled_views lookups; guest textures only (not resolve destinations, crops, raw depth, prompt atlases or pad sheets). Epoch bumped by DropGuestTexture (eviction, reload), a texture marked dirty or a texture_at_ entry erased in ProcessInvalidations, a resolve destination registered, DestroyFrameState. (2) Index heap (64 MB host-visible, BOTS_GFX_INDEX_HEAP=0 turns it off): direct DMA index buffers are requested from the tracker in the tracker phase (pages watched) and converted once into the heap, kept across submissions (index_static_, by 64 KB block); a CPU write erases the entry and marks the base dynamic (index_dynamic_, ring from then on). Heap full -> next Flush waits for all submissions and empties it (index_heap_generation_ guards a lookup across that flush). Profile line now adds "kept from earlier frames", heap resets, "texture bindings remembered". Checked: applies 0001-0028, 0029, 0090 identically to the cloud tree; syntax-checks against the linuxbuild SDK. Watch for: wrong/missing geometry (index heap: test with BOTS_GFX_INDEX_HEAP=0), wrong textures after loads (memo).
- **0029 first run (21 Nov 20:06):** indices 4.0 -> 0.5 ms (7,600-8,300 of ~11,000 index buffers kept from earlier frames), textures 4.9 -> 3.9 (~43k bindings remembered), but tracker 2.5 -> 9.4-11.2 ms: the game streams dynamic indices at a new address each frame, so each new base was requested from the tracker (lock + page protect + upload, ~16 us each, ~550 a frame). Fixed (patch rewritten 20:09): index_dynamic_ now holds 64 KB blocks, not bases, so a block with rewritten indices stops being requested. Not built yet.
- **0029 A/B (21 Nov 20:22-20:35):** with at most 32 heap tracker requests per submission (IsCurrent first) the run showed 0 heap requests but tracker still 6.8-7.0 ms (indices 0.5). With BOTS_GFX_INDEX_HEAP=0: tracker 2.63, indices 3.68, textures 3.43 (memo: ~43.5k bindings remembered), 11,497 draws in 15.56 ms = **1.35 us/draw** (0028: 1.47-1.51), 24.5 ms frames (~41 FPS). So the heap's per-draw lookups (index_static_ find, index_dynamic_, IsCurrent) cost ~4.4 ms vs 3.2 saved. Patch now has the heap **off by default** (BOTS_GFX_INDEX_HEAP=1 turns it on); texture memo stays. Next: two-thread recording, or a cheaper index cache (per-draw-slot memo instead of the big map).
- **CP thread split (patch 0091-cp-profile, 21 Nov 21:10, 4K render, 11,200 draws):** wall 24.9-25.1 ms = command buffers 24.9 (swap 0.6 is inside them): IssueDraw 21.7, shader loads 0.2 (4,748 loads, 1,232 copied), PM4 parsing/register writes ~2.4 (71k register writes, 265k constant registers). **"ms in draws" (draw_us) undercounts**: ScopedMicros truncates each draw to whole microseconds; the phase sum (20.6 ms) matches IssueDraw. So draws are ~87% of the thread, ~1.9 us each with profiling on (13 clock reads a draw add ~2-3 ms). A PM4-parse thread would save <=2.4 ms; splitting Vulkan recording (descriptors, pipeline, pass, record ~5.8 ms profiled) <=~5 ms, big refactor. Real frame time without BOTS_DRAW_PROFILE not measured yet.
- **Without profiler (21 Nov 21:15):** 21.7-21.8 ms frames (~46 FPS), 11,311 draws, max 25 ms; profiler costs ~3.3 ms.
- **Native patches 0092-draw-memo and 0093-record-thread (written 21 Nov 21:42, not built):** 0092 (on by default, BOTS_GFX_DRAW_MEMO=0 off): per-slot sampler memo (fetch words + binding filters -> VkSampler), texture-set reuse by raw handle compare (no set_key_ string), sets 2-3 not rebound while layout/sets unchanged and dynamic_valid_; draw_us now summed in ns (ScopedNanos). 0093 (off by default, BOTS_GFX_RECORD_THREAD=1 on): DXVK-style worker (vk/recorder.h, SPSC byte ring, 4 KB batches, std::atomic wait): cmd_ renamed cmd_raw_, every direct use is Cmd() (drains the worker first); draws hand one DrawPacket (binds, changed dynamic state, index buffer, draw) via Rec(), plus Rec for queries, timestamps, barriers, Begin/EndRendering, guest-memory uploads, crop copies and descriptor updates. Profile CP line adds "recorder N packets, M waits". Recorder unit-tested (4M packets in order, TSan clean); ~30 ns a push on the 2-core cloud VM. Test: 4 runs without profiler (memo off/on x thread off/on).
- **0092/0093 results (21 Nov 21:45-21:53, 4K render, ~11,250 draws, no profiler):** A only 21.7-22.1 ms (~46 FPS), B only 20.0-20.1 ms (~50), A+B 19.7-19.8 ms (~51, 62% of frames under 20 ms). No errors, user saw no visual problems. Baseline (both off) not run yet. Launcher: Graphics tab "Second render thread" (render_thread, default on) sets BOTS_GFX_RECORD_THREAD=1/0 in game_env (overrides the cmd environment). Needs the launcher rebuilt; a release needs the SDK bundle repacked with the new renderer DLL.
- **To test later: are late frames rounded to 60 Hz? (22 Nov, 10:05, from the DX12 chat; not tested)** In
  qos_117 (21 Nov 13:18, busy stretch, before 0028) frame times were "over 20 ms 412, over 25 ms 412, over 33 ms
  173": not one frame between 20 and 25 ms, so late frames look pushed to the next 16.7 ms tick, not just slow.
  The run's --vsync value isn't logged. Two game-side waits could do it even with the SDK's vsync off: the game's own
  r_vsync wait in Com_Frame (dvar ptr 0x84055C2C, see Frame rate and FOV above) and D3D presentation interval 1
  ("every vblank", set by fps_unlock.cpp), which waits on the emulated vblank. **Test:** busy spot, current renderer
  (0092 + 0093 on), `--vsync=false --frame_rate=60`, no BOTS_DRAW_PROFILE; read the "frame times over 600 frames"
  lines (game.log in the launcher install, or logs\qos_NNN.log): frames now between 20 and 25 ms -> no rounding,
  what's left is renderer cost; still a gap at 20-25 ms -> fix in `src/fps_unlock.cpp`: game r_vsync 0 and
  presentation interval 0 (the Canary "Unlock FPS" patch's "0 = unlimited"), com_maxfps 60 stays the cap. Also
  Task Manager -> Performance -> CPU -> graph "Logical processors" (or Process Explorer, qos.exe threads): expect
  one or two busy cores (the render thread, and the recorder with 0093), which is why total CPU use looks low.
- **Result (22 Nov, 10:21-10:23, qos_118, build folder, --vsync=false --frame_rate=60, BOTS_GFX_RECORD_THREAD=1):
  no rounding.** Busy windows: 7,408 draws -> >20 ms 352, >25 ms 132 (220 frames at 20-25 ms); 7,833 -> 318/144;
  9,559 -> 492/71; 12,048 draws -> 600/397 (203 at 20-25), 21-26 ms wall, 16.6 ms in draws (~1.38 us/draw), max
  31 ms, none over 33. So no fps_unlock.cpp change needed: what's left is renderer CPU cost (~40-50 FPS in busy
  scenes). qos_117's gap was probably vsync on and/or the pre-0028 renderer. Many "ResolvePath(scaleform\) failed"
  errors in the log (game probing a folder; no visible effect noted).
- **Build folder's renderer is old (22 Nov, 10:35):** `out\build\...\rexgpu-nativerd.dll` is 6,545,920 bytes from 21
  Nov 13:12 (patches 0001-0027): qos_118 and qos_119 ran without 0028/0029/0092/0093 (their log lines "recorder",
  "kept from earlier frames", "texture bindings remembered" are missing). Last night's 0028-0093 runs were from the
  launcher install. The shared SDK source was reset at 10:06 today by another project's build_native. So the
  rounding result above holds, but its frame times are the old renderer's. qos_119 (profiler on, old renderer,
  ~11,000 draws): 26.9 ms wall, 18.0 in draws: setup 3.28, shaders 0.65, tracker 2.36, textures 5.55, ring 0.20,
  targets 0.65, indices 3.21, constants 1.57, descriptors 2.59, pipeline 0.81, pass 1.58, record 1.00; 6,994 index
  buffers reused, 3,952 converted, 107 invalidations a frame.
- **Native patch 0094-index-table (written 22 Nov 10:40, not built):** index_uploads_ + index_blocks_ (an
  unordered_map node and a block-vector entry per converted index buffer, ~4,000 a frame, all freed every
  submission) replaced by a flat open-addressed table (16,384 slots, emptied by a generation bump, at most 3/4
  used); CPU writes (ProcessInvalidations) stamp their 4 KB pages, an entry counts only while none of its pages has
  a newer stamp (no erasing). `BOTS_GFX_INDEX_FLAT=0` brings back the maps (A/B). Also: the 16-bit-list-with-reset
  path reuses a scratch vector; the profile line adds "converted (N KB), M rebuilt on the CPU" (index bytes
  converted; quad/fan/other draws whose indices are rebuilt in std::vectors). Checked in the cloud: 0001-0094 apply
  to Skullburglar 67aacb28 (base rebuilt from the PC's .git pack); the table's logic tested standalone against the
  map semantics (10M random lookups with writes and resets: no wrong hits); map bookkeeping cost there ~0.35-0.5 ms
  a frame vs ~0.1 flat, so expect ~0.3-0.5 ms (more if the Windows heap is slower); the rest of "indices" is the
  conversion itself, which the new KB counter will size. Not compiled against the SDK (headers not in the cloud).
  **Test:** `tools\build_native.bat` (resets the shared SDK to 007's set) + game build; check the DLL's date in the
  build folder; profiled run (BOTS_DRAW_PROFILE=1, BOTS_GFX_RECORD_THREAD=1, --vsync=false --frame_rate=60) at a
  busy spot, then the same with BOTS_GFX_INDEX_FLAT=0.
- **0094 works (22 Nov, 10:44, qos_120, renderer DLL 6,588,416 bytes 10:42, 0001-0094, profiler on, recorder on):**
  ~11,400 draws: 20.3 ms wall, 16.7 in draws (1.46 us/draw profiled); **indices 0.99 ms** (3.2-3.7 before): the
  maps were nearly all of it (only ~2.2 MB of indices converted a frame, ~4,100 buffers; 25 rebuilt on the CPU).
  Frames >20 ms 327/600, >25 ms 2. Phases now: textures 3.94, tracker 2.08, descriptors 1.88, setup 1.75,
  constants 1.63, record 0.99, indices 0.99, pipeline 0.83, shaders 0.65, targets 0.61, pass 0.41, ring 0.22.
  No errors.
- **0094 A/B and real FPS (22 Nov, 10:55-11:00):** qos_121 (BOTS_GFX_INDEX_FLAT=0, profiler on): indices 3.1-3.35
  ms, wall 21.9-23.7 ms at 10,500-11,900 draws -> the table saves ~2.2-2.4 ms a frame. qos_122 (table, no
  profiler, recorder on): wall a flat **16.6-16.7 ms = held at the 60 FPS cap** in the busy spot (11,100-11,200
  draws in 12.5-12.9 ms, ~1.13 us/draw); 9-22 of 600 frames over 20 ms, max 23-27 ms. User: run 2 felt better.
  Headroom left ~4 ms a frame at that spot; the late frames are the next thing (hitches, not average cost).
- **Hitch log (22 Nov, 11:07-11:12, qos_123, BOTS_HITCH_MS=20; logs frames > 20 ms and > 1.5x the recent
  average):** gameplay windows: >20 ms 1-11 of 600, mostly 0 over 25, max 20-23 ms. Logged slow frames: menus/
  videos (CP idle 20-64 ms: the game itself; a 1280x720 untiled fmt 2 video frame rewritten every other frame),
  level load (89 ms, 349 new textures, 67 MB), one 112 ms frame with 1 new pipeline started (draws 104.8 ms), a few
  30-37 ms with CP idle 17-24 ms (game side). In gameplay: **exactly 600 texture reloads per 600 frames**; the
  hitch lines name a 256x256x4 format 6 (8888) tiled texture, 1.4-2.8 ms per reload (far too slow for 1 MB: likely
  3D, which the loader copied block by block).
- **Native patch 0095-texture-reload-hash (written 22 Nov 11:25, not built):** (1) every loaded texture keeps an
  XXH3 of its guest ranges (not prompt atlases/pad sheets); a dirty texture whose bytes hash the same is kept (write
  watch re-armed, no reload), counted in the textures log line "(N more kept: written but unchanged)";
  `BOTS_GFX_RELOAD_HASH=0` off. (2) 3D tiled textures copied in 16-byte runs like 2D, only for shapes where a one-time
  check of every run found it contiguous (log "3D tiled ... contiguous, copied in runs" / "not contiguous");
  `BOTS_GFX_3D_RUNS=0` off. Logs the first 8 real reloads ("reloaded after a CPU write (contents changed)", with
  address, size, dim, format) and the first 8 kept ones. Applies after 0001-0094 (checked); not compiled here.
- **0095 works (22 Nov, 11:23-11:28, qos_124, renderer 6,598,144 bytes 11:20):** "3D tiled 256x256x4 blocks of 4
  bytes: runs of 4 contiguous, copied in runs": that per-frame texture's reload now 0.4-0.8 ms (was 1.4-2.8). It
  really changes every frame (~540-600 reloads per 600 frames remain); the hash keeps 0-133 per 600 (mostly menu
  video frames 1C3B9000/1C16F000, 921,600 bytes, written unchanged). Gameplay windows: >20 ms 0-8 of 600, >25 ms
  0-1, max 20-22 ms. Left: one 48.5 ms frame with 12 new pipelines (19,122 draws: a new area; cached for next time),
  one window with 30 frames >25 ms around it, and game-side frames (CP idle 18-41 ms: videos, menus, loads). No
  visual problems reported.
- **Shader refs refreshed (22 Nov, 11:55):** `tools/ff_shader_refs.py` has `--keep OLD.refs` (an earlier refs
  file's shaders and pipelines missing from the manifest stay in). Run in the cloud on the manifest copied to
  `dumps\415607FF.manifest.bin` (11:20, 1.8 MB) + the English common and all single-player .ff (no mp_, ~8.5 min):
  `pipelines\415607FF.refs` now 365,894 bytes: **1,437 shaders (1,224 exact, 209 patched, 4 kept from the old
  file), 1,655 pipelines** (was 787 / 804); `check` against the manifest: 1437/1437 rebuilt and equal. 14 shaders
  not in any SP fastfile (1E6883FCCDE1F688 and 13 more; the old file had 13 missing too: XEX image or mp). Takes
  effect after a game build (qos_assets copies pipelines/). A player's first start rebuilds them into
  415607FF.master.bin (expect a few seconds; was 2.2 s for 787).
- **Wrong renderer again (22 Nov, 11:58, qos_125 + crash-6600):** low FPS then a crash (strlen null in
  ParsedTextureFetchInstruction::Disassemble <- Shader::AnalyzeUcode <- ShaderCache::Get <- IssueDraw). Cause: another
  project's build_native rebuilt the shared SDK at 11:55 (skullburglar sources rewritten, no recorder.h; renderer DLL
  14,602,752 bytes, 11:56), and 007's game build at 11:57 copied it: log shows "unsupported prims" (no 0016) and no
  recorder lines. Not caused by 0094/0095 or the refs. The refs themselves loaded ("1437 known", 1655 pipelines
  warmed). The cache folder wasn't renamed in that run (manifest 1646 pipelines read). Fix: 007's
  `tools\build_native.bat` again, then the game build. While two sessions build the shared SDK, check the DLL size
  before testing (007's with 0001-0095: 6,598,144 bytes).
- **Fresh-cache test of the new refs (22 Nov, 12:17, qos_127, right renderer; the old cache renamed to
  `Documents\qos\cache\native-keep`; `native-old` from 21 Nov also still there):** "1655 pipelines, 1437 shaders
  rebuilt from the disc in 7681 ms", kept in 415607FF.master.bin; warm-up 1.7 s: 1636 built, **19 missing a shader**
  (the 14 shaders not in any SP fastfile). During play 27 built (20 of them those 19-ish, 7 other state), 74 draws
  skipped for pipelines in total: no visible pop-in reported. Gameplay windows >20 ms 1-10 of 600, max 20-22 ms.
  **Cost: 7.7 s at the first start** (13 fastfiles fully inflated, ~600 ms per 200 MB level zone). Possible later:
  pick refs by set cover (each shader from whichever fastfile covers the most others) to read ~5 files instead of
  13, and find the 14 missing shaders (XEX image / mp fastfiles) or ship them another way.
- **Refs by set cover (22 Nov, 12:35):** `ff_shader_refs.py index ... --cover` searches every fastfile for every
  shader (~10 min in the cloud), then picks sources greedily by shaders newly covered per MB of zone. Result:
  **6 sources, 801 MB of zones** (was 13, ~2.4 GB): siena 729 shaders, operahouse 343, english/common 146,
  whites_estate 125, english/code_post_gfx 63, siena_rooftops 31. Same 1,437 shaders (1,210 exact, 227 patched),
  1,655 pipelines; `check`: 1437/1437 rebuilt and equal. Written to `pipelines\415607FF.refs` (365,892 bytes;
  the first write silently kept the old file, rewritten from a new staging folder, size checked). Expected first
  start ~2.5 s instead of 7.7 s. **Tested 13:15 (qos_131, renderer 6,598,144 bytes, fresh cache):** 6 fastfiles
  read, "1655 pipelines, 1437 shaders rebuilt from the disc in **2736 ms**" (was 7681), warm-up 1.7 s (19 missing a
  shader, as before), 37 draws skipped for pipelines up to the main menu. Done.
  Command used: `ff_shader_refs.py index dumps/415607FF.manifest.bin OUT --game game --keep
  pipelines/415607FF.refs --cover`.
- **Shared SDK, 13:11:** the renderer DLL was 14 MB again: Bourne is getting renderer changes and its builds replace
  the shared SDK. Before every 007 test: `tools\build_native.bat`, game build, then check `rexgpu-nativerd.dll` is
  6,598,144 bytes.
- **Bourne port of the speed-ups (parked until 007 is done, user 13:14):** 0094/0095 are game-agnostic but sit on
  0028/0029/0091-0093, which Bourne lacks. Cloud test on Skullburglar 67aacb2 + 0001-0012 (Bourne's renderer set):
  0028 applies after 007's 0017 (copy shader at load, harmless elsewhere); 0029 conflicts in draw.cpp (hand
  merge); 0091-0095 not tried. Check first whether Bourne is draw-CPU bound (a BOTS_DRAW_PROFILE run). The refs
  set-cover idea fits Bourne's own refs tool; ff_shader_refs.py itself is 007-only (fastfile format).
- **Own SDK folder (22 Nov, 16:30):** to stop Bourne's renderer builds replacing 007's, `tools\build_sdk.bat` (and so
  `build_native.bat`) now default to `C:\rexglue-src-007`, and `installer\build_installer.bat` packs the SDK from
  there. Needed once: `tools\build_native.bat` (fresh clone and full SDK build into the new folder), then reconfigure
  the game against it: `cmake --preset win-amd64-relwithdebinfo
  -DCMAKE_PREFIX_PATH=C:/rexglue-src-007/rexglue-sdk/out/install/win-amd64` (if CMake keeps the old rexglue_DIR,
  add `-Drexglue_DIR=C:/rexglue-src-007/rexglue-sdk/out/install/win-amd64/lib/cmake/rexglue` or delete
  out\build\win-amd64-relwithdebinfo\CMakeCache.txt first), then `cmake --build`. Check: the build folder's
  rexgpu-nativerd.dll is 6,598,144 bytes. Launcher builds bundle the SDK as qos2 (renderer 0001-0095).
- **Next on PS prompts:** START tutorial picture if it exists; menus' other icons (btn_y, bumpers...) when seen in a dump; START/
  BACK glyphs (OPTIONS/touchpad); then the text (Press Start, controls labels).
- **Shared SDK folder (history; 007 moved to `C:\rexglue-src-007` on 22 Nov):** last built by 007 (SDK own14 + native 0001-0024 since 23:30; native 0025 + 0026 work; 0027 written 21 Nov, not built). Eat Lead has the same SDK set (own13,
  native 0001-0015); Bourne's is tbc12. Each project's `build_native` resets it to its own set.
- **Check the renderer before testing:** 007's build copies whatever renderer the shared SDK folder last built. On
  20 Nov another project's build_native (16:53) replaced it: text gone and "unsupported draw (primitive): prim 1"
  in the log. Fix: run 007's `tools\build_native.bat` first. 007's renderer DLL was 6,473,728 bytes (19 Nov).
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

## Planned: keyboard and mouse (user's binds, 21 Nov 09:30; not started)

Wanted later, from the PC version's layout. Takedowns to work like the Bourne port's (its keyboard takedown/QTE
handling: see the Bourne project's handoff before starting).

- Movement & cover: move/strafe `WASD`; jump/mantle `Space`; take cover / dash to cover `E`.
- Weapons & combat: aim/look mouse; fire `Mouse 1`; reload `R`; throw grenade `G` (hold to cook); change grenade
  type `3`; toggle silencer / fire mode `4` / `X`; take down (melee) `Q` near an enemy, then the prompt's keys.
- Menus & extras: phone / objectives `Tab`.
- **Key pictures made (21 Nov, 09:45), not wired up:** `tools/make_kbm_keys.py` (own art, not Bourne's) ->
  `prompts/kbm/` (54 PNGs): `k_*` HUD-font style (light grey cap, edge 150, label 131,129,131 like the LB glyph),
  `kt_*` tutorial/takedown style (dark cap 88, black edge, light label, shadow like t_l1), `km_enter`/`km_esc` menu
  keys in light grey with a shadow (user didn't want green/maroon, 09:44; dark ones are kt_enter/kt_esc). Keys: W A S D E R G Q X F C V 1-4, WASD cluster, Space, Tab, Enter, Shift, Esc;
  mouse left/right/middle/move. Wide keys are wider PNGs ("fit" keeps the shape). Previews sent in chat (09:45).
  **Isometric alternative (09:50):** `*_iso_*` (k_iso_, kt_iso_, km_iso_; 52 more PNGs, same sizes): top face
  sheared and squashed (SHEAR 0.32, SQUASH 0.78) with the label on it, sides extruded down-right (DEPTH 7). User
  liked the grey menu keys; flat vs isometric still to choose.
  **Tilted-back version (09:55, user: "take the top view and tilt back"):** `*_tilt_*` (k_tilt_, kt_tilt_,
  km_tilt_; 52 PNGs): the flat face in perspective, narrower at the top (TILT_TOP 0.84, TILT_SQUASH 0.80), front edge
  below (TILT_DEPTH 6), labels upright. Previews preview_tilt.png in chat. The `_iso_` set can be deleted if unused.
  **Decision (09:53): flat keys** (`k_*`, `kt_*`, `km_*`): the game's own prompts are all flat, so the tilted and
  isometric sets don't fit. Their PNGs (`*_iso_*`, `*_tilt_*`) can be deleted; the tool still has both functions.
  **10:01:** user would like 3D keys for the takedown diamond (Bourne's): keep the
  `_iso_`/`_tilt_` sets. Diamond only is 3D (10:03); the game animates it, so pressed frames were added:
  `k_tilt_<key>_pressed` / `kt_tilt_<key>_pressed` for every single-character key (32 PNGs): face dropped by 4.5 of
  the 6-texel front edge, 14% darker, tighter shadow. Assumed the tilted set (user's last 3D pick); iso pressed not made.
  **Correction (10:05): the 3D diamond keys are for Bourne's takedown diamond, not 007's.** 007 stays all flat (its
  diamond scales up and fades one picture, user). The `_tilt_` up/pressed PNGs here are a starting point for the
  Bourne project: redo them there in Bourne's prompt colours, and check how its diamond animates (frame swap or not).
  **Bourne WASD diamond made (21 Nov, 10:10):** Bourne's originals are in `tbc-test/assets/` (ButtonPresses*.dds,
  128x512 DXT5, rows A B X Y; the diamond already drawn tilted back as ellipses). New `tbc-test/assets/kbm-test/`:
  make_kbm_diamond.py + the three .dds (same layout, positions measured from the originals): Y top -> W, X left -> A,
  A bottom -> S, B right -> D as tilted-back key caps; lit key in the original button colour with the black ring,
  unlit keys dark grey with a light rim and faint letters. Comparison: kbm_diamond_compare.png. Not wired up in
  Bourne (its ps-test set shows how replacements are loaded).
  Light grey version (10:12): `kbm-test/grey/` (make_kbm_diamond.py --grey): lit key light grey (216,218,216) with a
  dark grey letter, same ring and unlit keys. **Decision (10:14): QTE diamond in colour; every other key glyph flat
  light grey.** Bourne's `tbc-test/assets/keys/*.png` (read by its src/key_glyphs.cpp: scaled into the button font's
  40 px cells, 80 px when wide) replaced (10:16) by own flat light grey caps (make_key_glyphs.py in kbm-test; 128x128,
  wide keys 256x128): a c d e f g q r s v w x 1-4, space shift enter ctrl tab alt escape backspace_icon, mouse
  left/right/scroll. The old Kenney-style ones are in tbc-test's git history. Not test-run in Bourne yet.
  **3D diamond wired into Bourne (10:22), not built:** tbc-test `src/key_glyphs.cpp` (HudTexture.whole): the three QTE
  textures get `keys/qte/<texture>.png` (assets/keys/qte, colour version; grey PNGs in kbm-test/grey) drawn over the
  whole texture while their binds are the default W A S D (--qte_wasd, on by default); otherwise the old per-badge
  drawing. Log "key_glyphs: ... replaced by its 3D picture" / "QTE picture <name>: 128x512". Needs Bourne's own
  build (its build_native first: the shared SDK folder was last built by 007).
  To use them later: a second prompt list (e.g. prompts/keyboard.txt) chosen when keyboard/mouse is the last device.
- Notes for then: the SDK already has mouse-to-stick and keyboard binds (SDK patches 0004, 0008, 0009, used by Bourne
  and Eat Lead); keyboard prompts would need glyph swaps like the PlayStation ones (prompts/, native 0021/0022).

## Next

1. Rebuild (codegen reruns for the toml change); test leaving a level for the main menu, and B on Press Start
   (hint shows after 0.4 s; B quits; B on the main menu still goes back; idle 90 s for the demo: B ends it, no quit).
2. Play further on native; compare with Xenos.
3. Then: frame rate above 30 (see the Xenia patch note), launcher (from Eat Lead's), leak-scan git hooks.
