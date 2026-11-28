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

## Multiplayer recompilation (24 Nov): mp/, written, not built yet

The user asked whether default_mp.xex can run; step 1 is recompiling it. `mp/` is a second ReXGlue project
(qosmp_manifest.toml, game_root `../game`, entrypoint `../game/default_mp.xex`, output `mp/generated/default`),
with its own codegen target `qosmp_codegen` and exe `qosmp` (the SDK drops underscores from project names, so not qos_mp). It shares single player's crash handler
(src/crash_win.cpp), prompts/ and pipelines/, and the SDK. MP's CRT: setjmp 0x8250B6C0, longjmp 0x8250B3A0
(mp/mp_functions.toml; matched against SP's 0x825ABAE0/0x825AB7C0). No game hooks: SP's hooks use default.xex
addresses. Next: run codegen, add the "Unresolved" function starts it reports to mp/mp_functions.toml, build, run to
the MP menu; then a way to start it (the SP menu's MULTIPLAYER item runs default_mp.xex), System Link networking
(Skullburglar 0018-netplay-system-link) and MP versions of the hooks.

Commands (Windows, from the repo):
    cd mp
    cmake --preset win-amd64-relwithdebinfo -DCMAKE_PREFIX_PATH=C:/rexglue-src-007/rexglue-sdk/out/install/win-amd64
    cmake --build out\build\win-amd64-relwithdebinfo --target qosmp_codegen
    cmake --preset win-amd64-relwithdebinfo
    cmake --build out\build\win-amd64-relwithdebinfo

**First boot to the MP menu (24 Nov, 22:24):** after four fixes qosmp.exe reaches the multiplayer main menu (Xbox
LIVE / SYSTEM LINK / OPTIONS / CUSTOMIZE EQUIPMENT), native renderer, pad. Fixes: (1) the runtime loads
game:\\default.xex unless OnLoadXexImage says otherwise (qosmp_app.h); (2) 0x82100460 function start (this-adjust
thunk); (3) the asset-type size switch sub_821E1168 (bctr 0x821E117C, table 0x82547D80, 39 entries; entry 16 points at
a far shared "return 4" 0x822F49D0, so the analyzer stopped at 16): full table as [[switch_tables]] in
mp_functions.toml; (4) 78 pointer-reached function starts (mp/mp_functions_scan.toml, made by
tools/mp_function_scan.py IMAGE GENERATED_DIR OUT, as single player's scan; first crash was the audio callback
0x8211E898). Import stubs start at 0x8252516C. Note: git pull once silently didn't happen on the PC: check the
files arrived before rebuilding. Next: walk the menus (options, customize equipment, System Link create game), try
a System Link match alone (party/host on one PC; addtestclient bots if reachable), then networking (Skullburglar
0018-netplay-system-link), SP<->MP switching, and MP versions of the hooks (KBM, FPS, prompts).

**System Link needs networking (24 Nov, 22:27):** every MP mode (Xbox LIVE, System Link, Customize Equipment) said
"No network connection detected": the stock SDK's XNetGetEthernetLinkStatus returns 0 and XNet is stubbed. Added SDK
patch `tools/sdk-patches/0020-netplay-system-link.patch` (needs Skullburglar's 0020-xam-local-users, added as own 0019: user_profile(index), ResolveGameUserContentPath(profile); the first build without it failed) (Skullburglar's 0018 rebased onto 007's stack, only the
XSocket::Initialize hunk redone; checked: applies on v0.10.1 + Skullburglar subset + own 0001-0018; not compiled in
the cloud, simde headers missing), OWN_PATCHES own20. Cvars: netplay_slot (-1 auto; guest port P = host P + 10000 +
16*slot, so several instances per PC), netplay_address, netplay_peers (hosts for VPNs that drop broadcast),
netplay_trace, user_signin_live (Live menus), user_xuid / user_gamertag (a second instance needs its own XUID).
Needs tools\build_native.bat (rebuilds the SDK and renderer), then both games rebuilt.

**First System Link match (24 Nov, 22:40, qosmp_010):** netplay up ("netplay: slot 0 at 192.168.1.74"), a System Link
game loaded and played (one player), ended -> score screen -> process ended ("Execution complete", no crash): XGI
0x000B0013 (XSessionLeave, 20-byte buffer) was unimplemented -> X_E_FAIL. SDK patch 0021-xgi-session-leave answers it
(own21). Also: `--user_signin_live=true` makes the game quit 2 s after start on XLIVEBASE 0x00058035 (unimplemented);
not needed for System Link, left off. Frame times in the match ~31-33 ms wall with CP idle ~29 ms: the game runs at its
30 FPS cap (no fps_unlock for MP yet).

**Sys_Error relaunch (24 Nov, 22:51, qosmp_012/013):** multiplayer's Sys_Error (sub_822E14E0) puts the message in a
984-byte block at 0x84E8D7D0 (+4 text), sets it as launch data (stub 0x8252516C) and relaunches default_mp.xex
(sub_82372B60(name, flags); sub_821C5598 uses it with "default.xex" to go back to single player). No relaunch in
ReXGlue: the process ends. mp/src/relaunch_log.cpp logs "game relaunch: ...; Sys_Error message: ...". Both runs
(leaving a match; opening System Link's join menu): "Failed to recvfrom() socket, error WSAECONNRESET" = Windows
reporting ICMP port-unreachable on UDP. SDK patch 0022-udp-no-connreset (own22): SIO_UDP_CONNRESET/NETRESET off on
every datagram socket. **Works (user, 22:59):** a match ends back at the menu; join menu opens. Later: make the relaunch real (restart the process, or reset and pass the launch data) so
Sys_Error shows its message as on the console, and single player <-> multiplayer switching.

**Launcher Multiplayer tab (24 Nov, 23:05), not run yet:** "Gamertag" entry (clean_gamertag: letters/digits/spaces,
15 max, default "Player") and "Play multiplayer" (starts qosmp.exe beside qos.exe with mp_args = game_args minus
SP_ONLY_ARGS + --user_gamertag + --user_xuid; log game_mp.log). XUID: "E000" + 12 random hex made once in
load_settings (two PCs must differ). Not yet: the installer building/copying mp/ (qosmp.exe), so the button says
multiplayer isn't installed.

**MP frame rate and FOV (24 Nov, 23:15), not built:** `mp/src/frame_rate_fov.cpp` = single player's fps_unlock.cpp +
fov.cpp at MP addresses: Com_Frame sub_8225E8B8 (com_maxfps dvar ptr 0x83A678D0, registered 30 at 0x8225E0E0),
Dvar_RegisterInt sub_822BBEB8, Dvar_RegisterFloat sub_822BBF00 (cg_fov at 0x82184F20), D3D swap sub_820F2188 (same
as SP). Launcher: mp_args now passes --frame_rate/--fov too (SP_ONLY_ARGS: achievement_popups, pad_prompts,
mouse_sensitivity, mouse_invert); renderer env (render scale, AA, filtering, render threads) was already shared;
Clear and rebuild clears Documents\\qosmp\\cache\\native too.

**System Link join list empty (24 Nov, 23:10-23:15, qosmp_018-021, two instances on one PC):** the exchange works
(Guest broadcasts a 15-byte "getinfo" to port 1001, Host answers 322 bytes, Guest receives it), but the list fills only
after XNetQosLookup completes for the server (result handler sub_822E0180 -> sub_821D1FE8 checks rtt against
minping/maxping and bandwidth > 0; "Throwing out server ... ping" otherwise). The SDK had XNetQosLookup as a stub.
SDK patch 0023-xnet-qos-lookup (own23): answers at once, 1 ms / 100 Mbit/s, no data. Also new:
mp/src/console_log.cpp logs the game's console (Com_PrintMessage sub_8225CE50) as "game[channel]: ..."
(--game_console_log, default on).

**Join list still empty after 0023 (23:21, qosmp_022/023):** XNetQosLookup is never called, so Guest drops the reply
before that. infoResponse handler = sub_821CB0E0 (from CL_ConnectionlessPacket sub_821C8690): silent exits on
"protocol" (info key) != debug_protocol dvar or 48, on a flag at 0x82C34CC8+3720, or a duplicate xnaddr in the
16-entry list (208 bytes each at +392). Suspect: Host replies from its VDP socket (proto 254, port 1000) and the
322 bytes may start with VDP's 2-byte game-data length, so Guest's UDP socket (1001) sees no FF FF FF FF
connectionless prefix. SDK patch 0024-netplay-trace-bytes (own24) logs the first 24 bytes of each traced packet to
check.

**VDP header confirmed (23:29, qosmp_024/025):** getinfo = "FF FF FF FF getinfo xxx" (UDP 1001 broadcast); the answer
from Host's VDP socket is "01 40 FF FF FF FF infoResponse\\n..." (VDP game-data length 0x140 first), received as-is
by Guest's UDP socket. SDK patch 0025-netplay-vdp-to-udp (own25): VDP sends set kViaVdp (0x40) in the prefix's
version byte; a non-VDP socket receiving one strips the 2-byte length and cuts to the game data. **Works (user,
23:39):** two instances on one PC, System Link: Host listed in Guest's join list and the join worked.

**Installer builds multiplayer (25 Nov, 09:05), not run yet:** build_installer.bat copies mp\ (manifest, function
tomls, CMake files, src, generated\rexglue.cmake) into payload\project\mp; the installer points qosmp_manifest.toml at
../../game, runs codegen + configure + build in project\mp after single player (progress 0.80-0.98; failure logged,
install continues), copies the renderer DLLs if missing, and writes "mp_exe" into launcher.json; the launcher's Play
multiplayer uses it. Existing installs: Game files -> Reinstall.

**Keyboard and mouse = no controller (25 Nov, 16:50):** the pad still worked with "Keyboard and mouse" picked: the SDK
always loads SDL (or XInput). SDK patch 0026-input-backend-none (own26): input_backend "none" loads no controller
driver; the launcher passes --input_backend=none for Keyboard and mouse, sdl otherwise. SDK bundle qos7.
Also: the launcher's XUID is now plain text (the read-only Entry drew nothing).

**Single player <-> multiplayer switching (25 Nov, 17:10), written, not built:** `src/title_switch.h` (header-only,
both programs): Start() finds the other exe (beside this one, else project\out\build\<preset>\qos.exe /
project\mp\out\build\<preset>\qosmp.exe) and CreateProcessW's it with this process's command line (unknown
--name=value are ignored by SDK cvar parsing, so both take the same settings): to MP adds --mp_extra_args's text
(the launcher puts the gamertag and XUID there) and game.log -> game_mp.log; to SP drops --user_gamertag/--user_xuid
(the SDK names the profile folder after the gamertag) and game_mp.log -> game.log. SP: `src/multiplayer_launch.cpp`
hooks sub_82423D78 (title launch; "default_mp.xex" from startMultiplayer), cvar mp_extra_args. MP:
relaunch_log.cpp's sub_82372B60 hook: "default.xex" -> qos.exe, "default_mp.xex" (Sys_Error) -> restart qosmp.exe
once (env QOS_RELAUNCHED stops loops; the error message in the launch data isn't passed on yet). Launcher: SP gets
game_args + --mp_extra_args; MP gets game_args + --user_gamertag/--user_xuid + --mp_extra_args (SP_ONLY_ARGS filter
removed). Mockup of the launcher's Play SP / Play MP buttons: design canvas "QoS launcher: Play SP / Play MP".

**Launcher Play SP / Play MP (25 Nov, 17:15) - reverted 18:17 (user): one Play again; multiplayer only from the game's
MULTIPLAYER item, since leaving MP starts SP anyway. Was:** the bottom bar's Play is now "Play SP" and "Play MP"
side by side (as the mockup); the Multiplayer tab's Play multiplayer link removed. For the next update (user, 17:07),
together with the SP<->MP switching.

**MP keyboard and mouse, part 1 (25 Nov, 17:35), written, not built:** `mp/src/mouse_look.cpp` = single player's at MP
addresses: CL_MouseMove sub_821BF478(localClientNum, cmd), CL_GetMouseMovement sub_821BF340; cl[n] = 0x82C3A1D8 +
n * 0x1FAE8C, mouseDx +0x252EC, mouseDy +0x252F4, mouseIndex +0x252FC (dvar ptrs: sensitivity 0x82C34C90, m_yaw
0x82C34C9C, m_pitch 0x82C34CB4, cl_mouseAccel 0x82C3A164, cl_freelook 0x82C3A1C8, m_filter 0x82C3A1B4; found by which
function reads them). `src/kbm_binds.cpp` built into MP too (no game addresses; pch include guarded), called from
qosmp_app.h's OnPreSetup. MP's pad file buttons_default_mp.cfg isn't in english/code_post_gfx_mp.ff or common_mp.ff
(default_mp.cfg execs it, then BACK togglescores, START togglemenu); keys follow pad positions, so SP's layout
carries over (Tab = scoreboard, Esc = menu). **Esc back in MP menus (works, user 17:31):** mp/src/menu_keys.cpp, called from
the Com_Frame hook: key catchers of local client 0 at 0x82C3A1DC (cl block +4; from Key_IsCatcherActive sub_821C2FC8);
KEYCATCH_UI 0x10 -> SetStartKeysAsBack. User had hit it: Esc in a match's settings closed the menu (START = togglemenu).
**MP prompts (17:40, not built):** src/pad_prompts.cpp built into MP too (pch include guarded), Update() from the MP
Com_Frame hook, PhoneUp() stub false in mp/src/menu_keys.cpp. Same prompts/*.txt lists (copied by qosmp_assets).
**MP dump (mpdump, 17:45, two instances):** 52 64x64 textures, no new button pictures (MP uses SP's menu A/B FFC0.../0978...
and the X disc 82C5..., already listed); MP's HUD font atlas is 2714820410303606 (512x1024 DXT5), glyph cells as SP's
C1DB...: keyboard.txt and playstation.txt each get a second `texture hud_font_mp` block with the same draws. Decoder
for dumps: cloud /home/claude/mpscan/texdec.py (tiled DXT1/3/5, contact sheet), not kept in the repo.
**PS prompts in MP work (user, 18:04).** Keyboard prompts in MP work and the installer builds multiplayer (user, 25 Nov 19:28).

**Prompts follow the pad in use (25 Nov, 18:35), not built:** with a PS and an Xbox pad both connected, Auto stayed on
the first pad's pictures: the SDK published the pad style only when a pad connected (the first one). SDK patch
0027-pad-style-last-used (own27): a button press, or a stick/trigger past 20000, publishes that pad's style. Bundle qos8.
**0027 works for prompts (user, 25 Nov 18:42), but Xbox -> PS -> Xbox left the Xbox pad dead:** the SDK's
SlotAssignment made each pad its own guest user (pad 2 = user 1) and the game followed one. SDK patch
0028-shared-pads (own28, bundle qos9): `--shared_pads` (default on) uses SharedAssignment, every pad merged into
user 0 (as the keyboard/mouse device already was); off = pad N is player N. Not built.

## Multiplayer to-do (user, 24 Nov 23:46), in the order the parts depend on each other

1. (needed by everything that starts qosmp.exe) Installer builds/copies qosmp.exe (needed by the launcher's Play multiplayer).
2. (needs qosmp.exe installed beside qos.exe) **Single player <-> multiplayer switching:** SP's MULTIPLAYER item (startMultiplayer, sub_821BCD08 -> title
   launch sub_82423D78("default_mp.xex")) starts qosmp.exe; MP's way back (sub_821C5598 -> sub_82372B60("default.xex"))
   starts qos.exe; a launch of its own name (Sys_Error, relaunch_log.cpp) restarts qosmp.exe so the error shows.
   Pass the launch data (MP's 984-byte block at 0x84E8D7D0) along.
3. (needed before players get instructions; independent of the rest) **Keyboard/mouse and button prompts in multiplayer:** single player's kbm_binds, mouse_look, pad_prompts,
   menu_keys, takedown/tutorial hooks at MP addresses (mouse look: CL_MouseMove etc. are CoD4 code, likely in MP
   too); MP's own binds file is buttons_default_mp.cfg; prompts/ textures may differ in ui_mp.ff.
4. (the instructions depend on it) **DLC support:** the Camille Map Pack (Canals, Bags, Dam, Cisterns; Xbox Live 30 Jun 2009). Every player needs the
   maps (each game loads them from its own files; the lobby tracks owned packs with party_mappacks). Find how the
   SDK installs content packages (STFS) for the title and whether qosmp's content enumeration finds them; test a
   System Link match on a DLC map with both instances. Launcher (user, 25 Nov 00:27): in the Multiplayer tab,
   a "Map packs" section with a file browser ("Add map pack...") to pick the pack's content file (an Xbox 360 STFS
   package, e.g. from a console's Content\\0000000000000000\\415607FF\\00000002\\ folder); the launcher checks it is a
   QoS package (title ID 415607FF in its header), copies it where the SDK looks for DLC, and lists installed packs
   (name from the package header) with Remove.
   **Done (25 Nov, untested; user asked for a user-selectable folder instead):** launcher Multiplayer tab
   "Downloadable content": Choose folder... / Clear, saved as `dlc_folder`, passed as `--dlc_folder=` on both
   programs' command lines (single player ignores it; title_switch passes it on). `mp/src/dlc_folder.cpp`
   (OnPostSetup, after the XEX load): walks the folder 5 deep for STFS files ("LIVE"/"PIRS"/"CON "), skips any whose
   header isn't title 415607FF + type 00000002, and installs each one without an installed header with the SDK's
   `ContentManager::InstallContent` (extracts to Documents\qosmp\content\0000000000000000\415607FF\00000002\<file>\
   and writes Headers\00000002\<file>.header, which XamContentCreateEnumerator lists). Log lines start "dlc:".
   Not done: removing a pack (delete its folder + header by hand); checking the maps appear (party_mappacks,
   map list) and a System Link match on one.
   **Test (25 Nov 18:56):** installed and mounted ("Mounting Camille Map Pack as drive 'mappack1'"), maps listed, but
   names showed as CONTENT_MP_BAGS and no picture ("Could not load material mapimg_mp_bags"). The pack holds mp_*.ff,
   mp_*_load.ff, jbmaps.arena (longname "@CONTENT_MP_*"), mappack1.id ("mp_bags,18,4" ...) and <language>\content.ff
   (CONTENT_MP_* strings incl. unused MANOR/VENICE, mapimg_mp_* materials). Nothing in default_mp.xex asks for a zone
   "content" (no such string; probably the title update did). Zone path builder sub_821F0EA8(name, ext, size, buf,
   lang): non-localized "%s\\%s%s" with base from sub_822D83F0(name) (mappack drive for a pack's map), localized
   "D:\\<lang>..." only. DB_LoadXAssets = sub_821F2A98(XZoneInfo{name, alloc, free}[], count, r5, r6); startup: common_mp
   {17, 0}, ui_mp {34, 0}; ui reload {ui_mp, 2, 2}. Pack mount = sub_822D7D58(n) (returns 1 when mounted).
   **mp/src/dlc_content.cpp (19:25, not built):** after a mount, the next Com_Frame loads zone "content" {alloc 1, free
   0}; the sub_821F0EA8 hook gives it "mappack1:\\<language>\\content.ff" (sub_82296340 = language folder; languages
   the pack lacks read english). --dlc_content=false off. Log "dlc: loading ...", "dlc: zone content read from ...".
   A second pack (mappack2) isn't handled (only one pack exists).
   **Works (user, 25 Nov 19:19).** Launcher now lists the packs found in the DLC folder under its path ("Found: Camille
   Map Pack", from each package header: type 0x344, title 0x360, display name 0x411), dlc_packages() in qos_launcher.py.
- **Auto input = controllers + keyboard/mouse (user, 25 Nov 19:19), launcher only, not run:** Auto now passes
  --mnk_mode/--mnk_mouse=true with --input_backend=sdl (plus mouse sensitivity/invert); pad_prompts auto already
  shows keys while keyboard/mouse was used last (SDK 0016), Xbox/PS by the pad (0027), all pads player 1 (0028).
  Controls shows both the controller and the keyboard settings for Auto.
  **Reverted (user, 25 Nov 19:25):** switching input while a tutorial hint was up left wrong glyphs in it, so Auto
  is controllers only again (Xbox/PS switching kept). To revisit: the tutorial hints pick their picture once when
  shown (tutorial_keys.cpp swaps names only in keyboard mode), so a hint up during a switch keeps the old device's
  pictures; would need re-rendering the hint on a device change.
- **Sys_Error message after a restart (25 Nov 19:40; works, user 19:37):** the game reads its launch data at start-up
  (sub_822E1738: XamLoaderGetLaunchData into 0x84E8D7D0; version 5 at +0, message at +4; Com_Init sub_8225F210 checks
  +4 / +904). relaunch_log.cpp now writes the SDK's launch data (xam loader_data) to %TEMP%\qosmp_launch_data.bin
  before a restart, path in env QOS_LAUNCH_DATA (inherited); the new copy's OnPostSetup restores it as its launch
  data and deletes the file. Test: --test_sys_error=TEXT raises Sys_Error(TEXT) after 600 frames (not in a restarted
  copy). Logs "launch data ... passed on", "launch data from the last run restored (... message ...)". Unknown yet:
  whether the game then shows the message on screen. SP <-> MP switching still passes no launch data.
  **Works (user, 25 Nov 19:37):** the restarted copy shows the error message.
- **Release 0.2.0 prepared (25 Nov 19:45; first as 0.1.3, renamed 0.2.0 at the user's request, 19:43):** QOS_RELEASE,
  launcher VERSION and GAME_VERSION 0.2.0 (installs rebuild), SDK bundle qos11 (SDK patches to 0028, native to 0100; qos10 had 0101 and installs that got it kept it on reinstall, the bundle name being the same: 1440p pixelated, user 21:20), notes
  docs/releases/0.2.0.md. The user
  builds the installer (after build_native) and publishes with gh.
- **See-through letterbox bars (25 Nov 19:52, native only; Xenos solid black):** Bolivia intro ("Oruro Province",
  a Bink video with an alpha plane: ps 7CD27CF8D7613081, t3 = alpha 1C3B9000/1C16F000). The bars are overlay rects
  drawn after the video; with BOTS_GFX_NO_BLEND=1 they come out solid black (user screenshot 19:59), so they're drawn
  with alpha ~0. Suspects in bars-trace.txt: draws with ps 2DA1A34264D18532 (alpha = (r0.w*c2.w+c3.w) * r1.w: a
  second interpolant r1 from vs D437CDDFB1D56DD6, count 30) or ps 3A92D78FE55C7B82 (vertex colour, vs
  103B6AAEE41C86E1). Next: print those vertex shaders and their vertex fetch formats (extend the 0097 trace to dump
  vs microcode + vfetch), check what feeds r1.w.
  **Solved (25 Nov 20:25):** not the shaders. The bars are the video's alpha 0 over a black made by an XDK depth-only
  clear (z 0x3F7F0001 -> D24S8 0xFF000100 = colour 00 01 00 FF) moved into colour by Own()'s transfer; fine at render
  scale 1 (manual runs), missing at 2 (launcher 1440p), whatever the render threads. BOTS_GFX_PLAIN_UPSCALE=0 at scale
  2: black bars. Native patch 0101 made plain upscale (0023) opt-in (reverted, see below). SDK bundle qos10. Also new: native 0100 (trace prints vertex shaders). Side notes: the
  1-thread launcher run crashed once (game: read of 0x30, as crash-15880 on 24 Nov); a manual 1440p run without
  BOTS_GFX_RECORD_THREAD=1 crackled (CPU-bound, launcher runs set it).
  **0101 reverted (25 Nov 20:50):** with plain upscale off the 3D is pixelated again at 1440p (user screenshot
  20:48, render scale 2, FXAA): SDK 0014 + native 0024 don't keep the game's stretches sharp after all, 0023 is still
  needed. Bars back to a known issue (0.2.0 notes). Next: find which plain-upscale candidate covers the bars (a
  1440p trace with plain upscale on, at "Oruro Province", + the "plain upscale done/skipped" lines, which only log
  the first 12: log every one during a trace-now window) and limit the blit to the draw's own rectangle (vertex
  bounds) instead of the whole scissor/viewport. Audio crackle the same evening went away after a PC restart (0
  underruns in the logs; leftover builds/test runs).
- **0.2.1 (25 Nov 22:50):** desktop shortcut fix (APP_NAME's colon made "007" + an NTFS stream; now
  "007 - Quantum of Solace.lnk" on SHGetFolderPath's desktop, leftover removed; works, user 22:49). Launcher VERSION
  0.2.1, GAME_VERSION and QOS_RELEASE stay 0.2.0 (game unchanged, no rebuild prompt), SDK bundle qos11. Notes
  docs/releases/0.2.1.md. Idea for later: the launcher could repair a missing/broken shortcut at start, so players
  needn't reinstall for it.
- **To do, 26 Nov (user, 25 Nov 21:44), in this order:**
  1. ~~Launcher: fixed window size, scroll bar~~ done 26 Nov (works, user 18:17): window not resizable, settings tabs
     in a canvas whose scroll bar shows only when needed, the wheel scrolls the shown tab, the wheel over a closed
     drop-down scrolls instead of changing it. Every on/off setting is now a switch (_switch/_toggle) with
     Enabled/Disabled beside it (user: tick boxes weren't clear); "Hide achievement pop-ups" became "Achievement
     pop-ups" (inverted switch, same saved key).
  2. ~~Depth of field off~~ done 26 Nov (SP works, user 18:03): src/dof.cpp, shared by both programs, holds r_dof_enable
     at 0 (SP dvar ptr 0x84055E40, MP 0x8518C0E0); the renderer's R_SetDof (SP 0x82561A58) zeroes the view's DoF when
     it's 0, cutscenes and aiming alike. On: set back to 1 once (it's a saved dvar). --depth_of_field, Graphics tab.
  3. ~~Hit markers off~~ done 26 Nov (SP works, user 18:03): SP's is a HUD owner-draw (sub_82183188, fade
     hud_fade_enemyhit), skipped; MP's is _damagefeedback.gsc's hud element: the script source is patched as it's read
     (sub_82274EE0) so updateDamageFeedback sets alpha 0 (the sound stays). --hit_markers, Graphics tab. MP untested:
     the first MP test died earlier, see the DLC memory fix below.
  - **DLC zone out of memory (26 Nov 18:01, fixed, not yet tested):** with a DLC folder, starting a match failed
    ("Need 47906831 more bytes of ram", Sys_Error Out of memory, mp_barge): dlc_content.cpp's "content" zone was
    loaded with allocFlags 1 after ui_mp and never freed, and zone memory is a stack, so unloading ui_mp for the map
    freed nothing usable. Now it takes ui_mp's flags (34 at start, 2 from the front end) and every later ui_mp load
    gets "content" appended (DB_LoadXAssets hook). 0.2.1 has the bug: release 0.2.2 once it's confirmed.
  - **Testing note:** the launcher's Play runs the installed build (launcher.json), not the repo's; test new game code
    with the repo's qos.exe directly.
  4. ~~LOD distance option~~ written 26 Nov 18:30, not tested: src/lod.cpp (both programs) writes r_lodScale every
     frame (SP dvar ptr 0x84055C78, MP 0x8518BF18; registered 1, range 1-4, so written raw). The renderer
     (SP sub_82563E30) picks LODs by distance * r_lodScale + r_lodBias, scaled by FOV: smaller = detailed further,
     0 = always LOD 0 (the devs' minimap script uses r_lodscale 0). --lod_scale 0-1; launcher Graphics "Detail
     distance" Original 1 / Far (2x) 0.5 / Very far (4x) 0.25 / Maximum 0. r_lodBias untouched (takedown camera
     uses it). To check: frame times and draw counts at a big outdoor spot per setting.
  5. ~~Shadow quality option~~ written 26 Nov 18:40, not tested: src/shadows.cpp (both programs). sm_sunSampleSizeNear
     (float, registered 0.381875, range 0.0625-32; SP ptr 0x84055E5C, MP 0x8518C0FC) = world units per sun shadow
     texel: bigger reaches further, coarser. Levels set it themselves (whites_estate 0.25/0.569/1.208, eco_hotel
     0.25/0.7, shantytown 0.38; sub_8223F750 resets it at level start), so --sun_shadow_range (1-4) multiplies the
     game's last value each frame (a value not written by us = the game's). Launcher Graphics "Sun shadow range"
     Original / Longer (2x) / Longest (4x). Test switch --sun_shadow_fast=false holds sm_fastSunShadow (SP 0x84055D98,
     MP 0x8518C038) at 0, effect unknown. sm_maxLights is already 4 = its max. The native renderer draws shadow maps
     at the render scale already; shadow maps above the render scale would need a renderer patch. Dvar registration
     reader for these: cloud /home/claude/xex/dvars.py NAME... (MP=1 for multiplayer).
  6. **Weapon keeps its own FOV: parked 26 Nov 19:15 (user), two tries failed, code removed.** Findings:
     (a) the view angle comes from sub_82198A30 (cg_fov blended to the weapon's zoom while aiming; returns f1; may
     update a smoothed float at 0x827A4A24); calling it a second time with cg_fov 65 gives the original angle fine.
     (b) The render backend's projection for the CoD4 "depth hack" pass (source state + 4784 == 2, set by
     sub_825993D0 from the sort key's 0x2 bit; matrices from sub_82599668 by index, 4 projection, 16 view-proj via
     sub_82599490, 20 world-view-proj, 24 world; view copy at state + 3920, projection + 64): scaling it changed
     nothing, and a run with the weapon on screen had no mode-2 requests at all (gun_fov_debug=3 squashing that pass
     to a point left the gun visible). So 007 draws the gun in the normal pass with the world's matrices.
     (c) cg_gun_x/y/z (SP ptrs 0x8266EF38/0x8266C6F4/0x8266EEE4, MP 0x82603810/0x825F04AC/0x82603754; read in
     sub_821A87F8, CoD4's CG_AddViewWeapon) set to -6 and -20: no visible change, so 007 places its weapon elsewhere.
     Next try, if ever: find 007's own weapon placement (which DObj is the view model; where its pose/bone matrices
     are built) and squash it along the view direction by tan(W/2)/tan(G/2); or identify the gun's draws in a
     renderer trace and work back from their constants.
  7. ~~Video cutscenes' see-through bars above 720p~~ fixed 28 Nov (native 0119: plain upscale compared in pixels,
     not MSAA samples; see the 28 Nov entry).
  8. Launcher: an optional "Host addresses" field in the Multiplayer tab -> --netplay_peers (comma-separated), passed to
     both programs like the DLC folder. Only for System Link where broadcasts don't arrive: VPNs that drop them
     (Tailscale), or home networks that block them (guest Wi-Fi, client isolation, separate segments). Not needed on
     one PC, a normal home LAN or ZeroTier-style virtual LANs: the field's note says so. Then update docs/multiplayer.md
     (it says the launcher doesn't offer it yet).
  - **Launcher layout (27 Nov, user, committed):** every settings tab is a grid of two columns 2:1 (two_columns):
    left the setting's name with its note under it in one cell (setting_name + note, which joins the cell of row - 1),
    wrapping at the column width (wrapping()); right the control, filling the column and centred vertically.
    Controls: toggle button groups (_buttons: on/off settings, Game language, resolutions, AA, upscaling, frame
    rate, FPS counter, texture filtering, render threads, detail distance, sun shadow range; short labels, saved
    values unchanged), drop-downs drawn in the same style (qos_installer.dropdown, with its own full-width list window;
    Input device, both launcher-language pickers), sliders and the Gamertag field full width, buttons in the same style
    (_button/_button_stack) on Game files (Install location, Help, SP/MP shader cache) and Multiplayer (DLC folder).
    "Close launcher" keeps the slide switch (_slide_switch). New Credits tab (CREDITS list, Website buttons, trademark
    note; the Skullburglar line left the Display tab). Tab list level with the page. New strings in launcher_text.py.
  9. ~~Launcher: repair a missing or broken desktop shortcut at start~~ done (user, 25 Nov 23:34: fixed by 0.2.1).
  10. **Upload hoisting done 26 Nov 19:35 (native patch 0102-upload-hoisting, works, no glitches):** an upload no
      GPU work recorded so far in the submission reads (per-4 KB-page stamps set in MemorySync::Request) goes into a
      second command buffer (own pool per submission: the recorder thread records the main one) submitted before the
      main one; barrier at its start and end. BOTS_GFX_HOIST_UPLOADS=0 off. A/B at the same busy spot, render scale
      2, recorder on: ~5,000 draws 296-365 passes / 600-740 barriers (on) vs 702 / 1,440 (off); heaviest ~7,300 draws
      1,140 vs 1,253 passes, 17.7 vs 18.2 ms. GPU wait 0.00 ms in both: the GPU isn't the limit on the user's 4080, the
      renderer thread is (13-14.5 ms in draws at 7,300+). 40-65% of uploads move ahead (80-95% in light scenes); the
      rest are pages already read earlier in the submission. Next (user, 19:52): upload copies on a worker thread (CPU
      half of the uploads, ~1-1.5 ms), then PM4 parsing on its own thread (~2.4 ms), then per-draw trims (setup from
      changed registers, constants by changed ranges, descriptor reuse, tracker skip).
      **Upload worker done 26 Nov 20:45 (native 0103-upload-worker + SDK 0029-cp-before-guest-write, OWN_PATCHES
      own29; works, no glitches):** BOTS_GFX_UPLOAD_WORKER=1 starts a lock-free SPSC thread (upload_worker.h) that does
      the memcpy of uploads >= BOTS_GFX_UPLOAD_WORKER_MIN_KB (default 4) into the staging ring; drained in Flush and
      in NativeCommandProcessor::OnBeforeGuestWrite (SDK 0029: a virtual called before every CP guest-memory write -
      read pointer / scratch / event write-backs, MEM_WRITE, REG_TO_MEM, COND_WRITE, ZPD - and before interrupts), so
      the game never sees a fence with a copy pending. A/B same session: 3-4% less renderer time per draw at 16 KB;
      4 KB and 1 KB no further gain (noise): the copies were cheap, the rest of "tracker" is bookkeeping. Not yet in
      the launcher (env var only). Heavy-stretch finding: ~7,400 draws at 1.85-1.96 us/draw with ~1,150 render passes
      vs 8,500 draws at 1.29-1.31 us with ~470 elsewhere: the extra pass splits (uploads in place) cost ~2-3 ms there.
      **Tried and parked (21:10, user): native 0104-hoist-past-reads** (an upload may go ahead past reads of its pages
      in this submission if no OnBeforeGuestWrite since the first of them; per-page first-read epoch). Safe (no
      glitches) but useless: 70-230 guest writes a frame (one every ~35 draws) keep the window shut, only 2-22 extra
      uploads a frame moved (1.7 in the heaviest stretch). Not kept (patch deleted; in this session's chat). Might pay
      off if most guest writes are the ring read-pointer write-back (which frees command-ring space, not vertex data):
      would need SDK 0029 to pass the write's kind, a count by kind, then an epoch that ignores the read pointer.
      **Tracker cost (native 0104-tracker-read-bits, 26 Nov 22:00, works, no glitches):** the 21:28 profile had
      tracker at 0.40 us/draw (0.23 on 24 Nov) - 0102's NoteSharedRead wrote one stamp per 4 KB page (256 for a 1 MB
      vertex buffer) per draw. Now a bitmap (64 pages a word, per-word generation) plus a 256-entry memo in
      MemorySync::Request keyed on (start, length) and the invalidation count (counted after the valid bits are
      cleared, release/acquire); same read stamp skips the marking too. BOTS_GFX_TRACKER_MEMO=0 turns the memo off;
      hitch lines and the 10M-request line count memo hits (54% of requests). Profiled: tracker 0.26-0.33 us/draw
      (7,500 draws: 2.2 ms vs 3.1), heavy stretch 0.38-0.52 vs 0.45-0.65. Unprofiled: 1.22-1.43 us/draw in draws vs
      1.32-1.51 (same route, 21:00 run), heavy stretch 1.58-1.80 vs 1.70-1.90; ~0.5-1 ms a busy frame.
      **PM4 packet profile (26 Nov 22:20, SDK 0030-packet-profile + native 0105-cp-packet-profile, own30; built,
      qos_194):** per-packet-type CP time (exclusive), ~7,600 draws: 3.7 ms total incl. ~0.5 ms of its own clock
      reads: INDIRECT_BUFFER 0.9 (= header reads/dispatch of ~60k packets a frame, attributed to the parent IB),
      type-0 registers 0.8 (32k packets, 203k dwords), WAIT_REG_MEM 0.7 (140 a frame: waiting on the game, a
      thread can't help), IM_LOAD 0.37 (0.21 shader loads), type-2 padding 0.26 (29k packets: ~all profiler cost),
      DRAW_INDX 0.12, SET_CONSTANT 0.1, LOAD_ALU_CONSTANT 0.09. So ~2.2 ms is movable parsing, half of it per-packet
      overhead; a parsing thread would also stall at ~200 guest writes a frame. Decision (user, 22:28): fast path
      first, thread maybe later (~1 ms more at best).
      **Native 0106-pm4-fast-path (works, 27 Nov 07:08-07:25, qos_195 on / qos_196 off, same route):** NativeCommandProcessor runs
      indirect buffers with its own loop (headers read straight, type-2 skipped, type-0/1 writes to registers with no
      side effects stored directly: all but scratch 0-7, COHER_STATUS_HOST, DC_LUT_RW_INDEX/SEQ_COLOR/PWL_DATA/
      30_COLOR and the hybrid marker 0x30; type-3 via ExecutePacketType3 on a RingBuffer positioned at the packet).
      BOTS_GFX_FAST_PM4=0 off; packet profile now needs BOTS_CP_PACKETS=1 too (and runs the SDK loop). Test: A/B
      BOTS_DRAW_PROFILE=1 with BOTS_GFX_FAST_PM4 unset vs =0 at the heavy spot; compare "command buffers" minus
      IssueDraw and shader loads in the CP thread line. Result: CP time outside draws 2.69 -> 2.15-2.22 ms at ~7,200
      draws, 2.0 -> 1.66 at ~4,100 (0.35-0.5 ms a frame, ~18%; less than the ~1 ms hoped). No glitches.
      **Explosions (user, 27 Nov 07:18):** FPS drops at an explosion: 5,400 draws, ~560-580 render passes (210-350
      around it), 1,275 barriers, ~600 uploads in place, tracker 3.1 ms, 2.2 us/draw, 221 of 600 frames > 20 ms;
      same in both runs. Particle vertices written into pages earlier draws of the submission already read, so
      hoisting can't move them. Next: a diagnostic logging in-place uploads (address, size, which part of the page
      was read) to choose between finer read tracking and checking the read bytes are unchanged. Note: manual runs
      had BOTS_GFX_RECORD_THREAD unset ("recorder 0 packets"); the launcher sets 1: set it in test runs.
      **0107 diagnostic result (27 Nov 11:10, qos_197, BOTS_GFX_HOIST_DIAG=1):** of ~4 million in-place uploads in
      the run, >99.9% were over pages read this submission whose read bytes were unchanged (~800 changed, ~2,000 with
      a read over 256 KB). Reads are small (84 B - 5 KB, e.g. 1F7xxxxx+84, 09CFxxxx+576) in pages the game appends to.
      **Native 0108-hoist-past-unchanged-reads (11:18, written, on the PC, NOT built or tested; 0107 also on the PC,
      uncommitted):** hot pages (once uploaded in place; persistent bitmap) get their reads kept with an XXH3
      (reads <= 64 KB); an upload over read pages moves ahead (CopyAhead into pre_cmd) when every kept read of its
      pages hashes the same, no read of them went unkept this submission (unchecked_reads_) and none of them was
      uploaded in place this submission (in_place_pages_). BOTS_GFX_HOIST_UNCHANGED=0 off. Stats line: "uploads moved
      ahead (N past unchanged reads)". Test: build_native + game build; at the explosion, RECORD_THREAD=1,
      UPLOAD_WORKER=1, no diag/profile: run 1 default, run 2 BOTS_GFX_HOIST_UNCHANGED=0; compare render passes,
      barriers, frames > 20 ms; watch particles/smoke for flicker.
      **0108 result: dropped** (three versions; as many pass splits gone, no frame time saved; kept on the renderer
      tree's branch try-0108). Pass splits cost ~0.3 ms CPU on the test PC: not the explosion's cost.
      **Native 0109-memory-sync-profile (committed 27 Nov):** BOTS_DRAW_PROFILE=1 logs "memory sync per frame" (SDK
      lock + page scan, valid bits + write protection, upload copies, page runs/pages, CPU write faults and pages).
      Result at explosions: ~700-1,350 single-page uploads a frame, write protection ~75% of the slow path (up to
      3.15 ms at the peak, ~2.3 us per protect), ~100-230 write faults a frame on the game thread; draws request whole
      vertex buffers (6-7 MB fetch-constant ranges). Tried and dropped (branch try-0110-0111): 0110 streaming pages
      (stop protecting pages rewritten every frame: no net gain) and 0111 narrowing vertex requests (memo misses,
      tracker 0.26-0.33 -> 0.47-0.81 us/draw). Real fix, deferred: streamed vertex data in its own GPU buffer.
      **Native 0110-texture-index (27 Nov, committed):** "setup" (1.4-3 ms/frame at explosions,
      2.99 ms at 8,495 draws in qos_208) is mostly ProcessInvalidations, not register work: every non-empty batch of
      CPU writes walked all kept guest textures (thousands). Textures are now listed by 64 KB block
      (texture_blocks_, IndexTexture/UnindexTexture: indexed at the end of TextureView, unlisted in DropGuestTexture)
      and MemorySync::TakeInvalidated skips its lock when nothing is queued (pending_). BOTS_GFX_TEXTURE_INDEX=0: the
      old walk (A/B). Profile line "CPU-write checks per frame". Test: build_native + game build, explosion spot,
      BOTS_DRAW_PROFILE=1 + all three render threads; run 1 default, run 2 BOTS_GFX_TEXTURE_INDEX=0; compare setup ms.
      **0110 works (27 Nov 19:19-19:43, qos_209 index on / qos_210 off, same route incl. the explosion):** CPU-write
      checks 0.02-0.08 ms a frame (1 texture checked) vs 0.2-1.94 ms (24k-254k textures checked; ~1,000-1,650 kept);
      setup 0.54 ms at 7,239 draws vs 2.39 at 7,436 (~0.07 vs ~0.32 us/draw). Peak windows: 12.3 ms in draws, 22 of
      600 frames > 20 ms (max 22) vs 13.9 ms, 145-224 > 20 ms. Whole runs: 589 vs 1,027 frames > 20 ms. Left at the
      peak: tracker 3.6-4.1 ms (memory sync: write protection), the next target. User (19:46): run 1 felt like it recovered
      faster and had smaller dips.
      **SDK 0031-invalidation-pages (27 Nov, own31, default unchanged = 64 pages):** BOTS_GFX_INVALIDATION_PAGES
      narrows how far a CPU write invalidates. qos_211 (=1): ~800-1,075 faults a frame, re-protects 10+ us each (lock
      contention with the faults), tracker 8-13 ms, 8,745 frames > 20 ms: unusable. qos_212 (=16): faults ~280 vs
      ~200, pages invalidated ~4,500 vs ~12,500, pages uploaded ~1,400 vs ~2,000, but requests that re-protect the same
      (~740 at 7,500 draws) and tracker the same (2.5 ms; 4.64 at 8,065 draws); frames > 20 ms 391 vs 404 (qos_209).
      So the re-protect count follows the game's write -> draw cycles, not the widening. Left as a switch, default 64.
      Options next: re-protect on a worker (copy now, protect then compare the page with the copy; a page waiting for
      its protect is copied again rather than trusted), or the streamed-vertex buffer.
      **Native 0111-async-protect + SDK 0032-async-protect (own32), 27 Nov 20:30, DROPPED (branches try-0111-async-protect / try-0032 in the cloud trees):**
      BOTS_GFX_ASYNC_PROTECT=1: UploadRanges hashes each page (XXH3, before the copy), copies, queues; a worker thread
      protects (WatchRangeForWrites) and, under the global critical region, marks valid the pages still hashing the
      same with their copy still newest (MakeRangeValidIf; pending_serial_ per page, cleared by OnInvalidated). A
      pending page is re-hashed by the next request and copied only if changed; changes found that way or by the
      worker go to invalidated_ (QueueChanged) for the texture/index caches. Profile line "write protection worker
      per frame". Test: run 1 =1, run 2 unset, explosion route; watch particles/smoke, textures.
      **Result (20:22-20:33, qos_213 on / qos_214 off):** worse. No glitches; the worker kept up (0 queued, 1.6-3.2 ms
      of its own, ~1% of pages changed before their protection), but the CP thread's requests rose (1,365 vs 877 at
      ~7,200 draws: pending pages take the slow path) and each still cost ~2.3 us, now waiting for the global critical
      region, which EnableAccessCallbacks holds across the OS protection call. Tracker 4.3 vs 2.8 ms; frames > 20 ms
      806 vs 229; user: run 2 (off) felt better. Lesson: the protection cost is the global lock, not the thread it runs
      on. Left: the streamed-vertex buffer (no protection for pages rewritten every frame), deferred.
      **Streamed-vertex plan (user asked, 20:38):** (1) diagnostic; (2) hot pages (re-uploaded 3+ frames running)
      stop being protected; (3) each draw on them copies its vertex span (index min/max x stride) into a scratch area
      of the shared-memory buffer the game never uses, hoisted ahead of the pass, and its fetch constant (renderer's
      copy) points there; (4) index buffers / textures on hot pages re-checked; (5) fallbacks + switch; (6) A/B.
      **Native 0111-stream-diagnostic (20:50, written, not built):** BOTS_GFX_STREAM_DIAG=1, lines "hot pages per
      frame" and "streamed-vertex diagnostic per frame" (requested vs spanned MB, KB a draw, index scan ms).
      **Result (20:49-20:54, qos_215):** hot pages (uploaded 3+ frames running) are ~400-800 at once and cause
      57-76% of page uploads and 88-98% of upload runs (= re-protects). 3,250-5,000 draws a frame read them (nearly all
      indexed), but their requests are already tight: 1.6-2.1 KB a draw, requested = spanned (5-9 MB a frame in
      total, no draw over 256 KB), so no index scan is needed (the scan would cost ~1-1.4 ms). Per-draw copies would be
      ~4,000-7,000 small copies a frame vs ~550-940 protected runs now: not clearly cheaper. Better variant: hot pages
      stay unprotected and are checked by hash (per page, what the GPU copy holds) at each request; changed -> copied
      in place, no protect, no global lock; changes reported to the caches (QueueChanged); a hot page unchanged for N
      frames goes back to protection. No fetch-constant redirect or scratch area. Est. ~1-1.5 ms off the peak.
      **Native 0112-hot-pages (21:05; works, 21:18):** BOTS_GFX_HOT_PAGES=1. RequestVertex (draw.cpp's vertex
      fetch requests) marks vertex_request_; UploadRanges counts per-page upload streaks (HotNoteUpload), 3 -> hot.
      Request's slow path sends hot ranges to HotRequest (non-hot sub-ranges via SlowRequest = the old slow path; hot
      runs via HotCheck: XXH3 per page vs hot_hash_, changed -> QueueChanged + UploadGuestMemory in place). Plain
      Request over hot pages demotes them; 120 frames unchanged demotes. FindIndexUpload skips Unwatched ranges.
      Profile line "hot vertex pages per frame". Test: run 1 =1, run 2 unset (both BOTS_DRAW_PROFILE=1).
      **Result (21:06-21:17, qos_216 on / qos_217 off; user: run 1 better, no glitches mentioned):** write faults
      ~10 a frame vs ~120-220; protected-path requests 300-730 vs 500-960 a frame, ~0.15-0.4 ms vs 1.2-2.8 ms in
      them; tracker 1.1-2.0 ms vs 1.4-3.2; draws 1.32 vs 1.48-1.52 us each at >= 5,000 draws (~12% less render-thread
      time). Hash checks 600-4,500 pages a frame (0.15-0.8 ms) of which only ~7-15% had changed: most re-uploads were
      the 256 KB invalidation widening, not real writes. ~300-1,000 pages hot. Frames > 20 ms per busy window about
      the same (13-19 vs 13 per 600; different walks). Next: default on (BOTS_GFX_HOT_PAGES=0 off) once confirmed
      glitch-free; maybe a per-page "checked this draw batch" skip to trim repeated hashes.
      **Default on (21:22, user: no new glitches):** 0112 now on unless BOTS_GFX_HOT_PAGES=0; log "hot vertex pages
      kept current by hash". **Known, older issue (user, 21:20): some z-fighting**, seen since well before this work;
      not caused by 0112. To look at later (where it shows: level/surfaces; compare Xenos; depth format/bias).
      **Launcher render threads (21:35, written, not run):** "Render threads" is now Auto / 1 / 2 / 3 / 4 (4 = plus the
      upload worker, BOTS_GFX_UPLOAD_WORKER). Auto (default): 4 with >= 12 logical processors, 3 with >= 8, 2 with
      >= 4, else 1 (auto_render_threads); the note says what Auto picks on this PC. Saved "2" (the old default) moves
      to Auto once (render_threads_auto marker). Hot pages (0112) need no launcher setting: on by default.
      **Z-fighting parked (user, 21:33).** **Next: per-draw trims.** qos_216 peak (7,629 draws, 9.75 ms in draws):
      tracker 1.75, textures 1.22, descriptors 1.20, constants 1.14, indices 0.69, record 0.68, pipeline 0.59, setup
      0.52, shaders 0.44, targets 0.42, pass 0.38, ring 0.16. Frames 20-25 ms in busy windows aren't hitch-logged
      (need > 1.5x the average): they're total per-frame cost, so trims matter there.
      **Native 0113-draw-subprofile (21:40, built, works):** with BOTS_DRAW_PROFILE=1, "draw sub-phases per
      frame": constants (viewport / system / floats / bool+fetch ms, uploads vs reuses per buffer) and descriptors
      (layout / texture sets / updates ms; sets none / as last draw / found / new). One run at the busy route.
      **Result (21:42-21:47, qos_218; the extra clocks add ~0.4 ms to constants):** at ~7,000 draws: viewport 0.23 ms,
      system constants 0.61 (built and compared every draw, uploaded for only ~7%), floats 0.28 (vertex floats
      uploaded for ~84% of draws, pixel ~17%), bool/loop + fetch 0.22 (fetch uploaded ~59%); pipeline layout lookup
      0.37 (a std::map per draw), texture sets 0.76 (per frame ~11,000 stages without textures, ~1,900 as the last
      draw, ~300 found, ~620 new: sets don't outlive a submission), descriptor updates 0.20. Spread thin: no single
      big piece left.
      **Native 0114-draw-trims (21:55; built, works):** last pipeline layout reused when the counts match;
      hot pages already checked for the same draw (draw_serial) not hashed again. Expect ~0.3-0.5 ms at the peak.
      **Result (qos_219 vs qos_218, >= 5,000-draw windows, profiler on):** 1.393 vs 1.407 us/draw (~1%, ~0.1 ms at
      7,000 draws): layout 0.038 vs 0.054 us/draw (most of what's left is the sub-profile's own clock reads), hot-page
      hashing 0.127 vs 0.140 us/draw (~990 second hashes a frame skipped). Kept (harmless, small). Per-draw trims are
      now at diminishing returns.
      **Detail distance / Sun shadow range (user, 22:04):** left to player feedback rather than our own testing
      (hard to judge where to look): ask for it in the next release's notes (what changed visibly, any FPS cost, where).
      **Uncapped frame rate (22:03, src/fps_unlock.cpp, written in the cloud repo; the write to the PC was stopped
      mid-call, so the PC copy may be old or new):** --frame_rate 20-1000, 0 = com_maxfps 1000; above 60 r_vsync held
      at 0 and swaps passed as D3DPRESENT_INTERVAL_IMMEDIATE (0x80000000 -> swap interval 0). Not built or tested.
      **Test (22:09, user):** smoother at 120 / uncapped, but the balance beam breaks with a controller (wobble runs
      away at high FPS). **Fix (22:15, not built):** while balancing the cap drops to 60. Found from SetBalanceForce
      sub_82262C60 (string 0x82037078): g_entities 0x82B5CC58 (stride 0x298), gentity +316 = gclient; client pm_flags
      (+12) & 0x8 = in a traversal, traversal type (+2260) 1 = balance; balance force stored at client +2324. Balance
      dvars bal_* (bal_wobble_accel/decel/sensitivity/levels, bal_move_level/speed, bal_force_*). Log "frame rate: on a
      balance beam, capped at 60" / "off the balance beam". Multiplayer (mp/src/frame_rate_fov.cpp) still 20-60.
      **Replaced (22:25) by a real fix, src/balance_fix.cpp (--balance_fix, on; works, user 22:23: "fixed it nicely, very doable with controller" at uncapped FPS):** the balance update is
      sub_8215F070 (r3 pmove, r4 pml; pml msec at +40; balance float at traversal state = ps +2260 +60). Per update:
      drift (stick + forces) x msec x 0.001 x bal_wobble_sensitivity is added (frame-rate independent), but the balance
      is first multiplied by bal_wobble_decel 0.93 (zones 3-5, middle) or bal_wobble_accel 1.02 (zones <= 2 or >= 6,
      outer; zones from sub_82159330 and bal_wobble_levels 0.06/0.27/0.8) once per update regardless of its length:
      tuned at 30 FPS (x1.8/s and x0.11/s), at 60 x3.3 and x0.013, at 120 x10.8 and x0.0002. The hook raises both to
      the power msec/33.3 for each update (dvar values at +12; dvar table 0x826596A0: decel +24, accel +72; registered
      in sub_82158AE0), restored after: every rate behaves like the original 30. Note: this also changes 60 FPS (now
      as the original 30, easier than before at 60). The 60 cap in fps_unlock.cpp was removed. Log "balance: ...".
      Next for uncapped: other frame-rate-sensitive things (play more levels at 120+), then launcher choices above 60
      (e.g. 90 / 120 / 144 / Unlimited, marked experimental) and multiplayer's frame_rate_fov.cpp.
      **Launcher 120 (22:40, user asked; not run):** Frame rate gets "120" (FPS/FPS_RATE), note says experimental, busy
      scenes can drop below, multiplayer stays at 60. mp/src/frame_rate_fov.cpp: cvar range now 0-1000 (so SP's
      values passed on by the launcher / title switching parse), anything above 60 or 0 counts as 60.
      **User (22:34): dips into the 50s on level 2's rooftop** (qos_222 22:25:53-22:26:31: ~3,200 draws, 4.7 ms in
      draws but 10-12 ms frames, worst 23-24 ms: not drawing; suspect the game thread). Profiled run asked
      (BOTS_DRAW_PROFILE=1, BOTS_HITCH_MS=16). Busy scenes at 10-11k draws: ~85-95 FPS, renderer-bound (9.4-10 ms).
      **Rooftop result (22:38-22:42, qos_223, profiled):** game-bound. ~3,000 draws, 5.2-6.0 ms drawing, CP idle 5.7
      ms a frame on average; the slow frames (16-21 ms) are ~6 ms drawing + 9-14 ms CP idle (waiting for the game):
      the game thread needs 12-20 ms there. GPU 3.7 ms, no GPU wait. Later (user: "another time"): a Tracy capture
      (TRACY_ON_DEMAND is built in) at the rooftop to see which game functions cost it.
      **Z-fighting examples (user videos 27 Nov 21:45 and 21:52, 4K, 60 FPS):** both are decals flickering on and
      off frame to frame while the camera moves: the cracked-glass decal on a glass panel (museum level, upper floor
      by the ROTARY FLIGHT banners) and the warning/instruction stickers on the grey electrical panels (boiler room
      with pipes and a red valve): in consecutive frames a sticker is whole, missing, or partly drawn. So the decals
      and their surfaces get nearly the same depth and the depth bias doesn't separate them on the host (D32_SFLOAT;
      guest D24S8 or D24FS8). Existing knob (Eat Lead): BOTS_GFX_DEPTH_BIAS_SCALE=f multiplies the constant bias; test
      16 (if it stops: bias too small for the host format) and 0 (if nothing changes: the bias isn't used, the decals
      rely on equal depth and the host computes it differently).
      **Result (22:58, user): both 16 and 0 still flicker:** not the bias. Suspects: float24 depth rounding (wall's
      depth rounded via a depth transfer/reload, decal's not), or wall and decal vertex shaders computing depth
      differently on the host. **Next (parked for tonight):** a trace at the boiler-room panels: run with
      "2> zfight-trace.txt", stand still facing the stickers, "type nul > trace-now" in the build folder (0097 traces
      2 frames), quit; compare the sticker draws with the panel's (shaders, depth func/format, transfers between).
      Bigger ideas left: system constants memo by inputs (hard), texture sets kept across submissions (views are
      retired per submission: needs lifetime work), PM4 parsing thread.
      **Launcher after 922fbcf (committed 27 Nov):** SlimScrollbar (qos_installer.py) replaces ttk scroll bars;
      Multiplayer "Host addresses (optional)" (`--netplay_peers`, cleaned by clean_host_addresses); Input device is
      a button group (Auto/KBM/XBOX/PS); Saves tab in two columns, buttons on the right.
      Proposed, not answered: render threads on by default (3 on CPUs with >= 6 cores, else 2) and the upload worker
      under "Render threads".
      Committed 27 Nov.
      Earlier next (user, 21:10): PM4 parsing on its own thread (sized at 22:20, see above).
      Earlier order (user, 25 Nov 23:29): upload hoisting first (1-2 sessions: copy each upload before
      its render pass, so uploads stop splitting passes; ~300 passes a frame now), then copying uploads on a worker
      thread (needs exact copy timing, or the 24 Nov zero-copy glitch comes back), then the command stream (PM4)
      parsed on its own thread (up to ~2.4 ms), then faster level loads (native zlib stall) and cheaper texture
      checks. Comparison sheet: https://claude.ai/artifact/HCX3LGF4VVAyR2L5HYT6uW
  - **Release 0.2.2 prepared (27 Nov 23:10, user):** QOS_RELEASE, launcher VERSION and GAME_VERSION 0.2.2 (installs
    rebuild), SDK bundle qos12 (native renderer to 0114, SDK own31), notes docs/releases/0.2.2.md. The user builds
    (build_native, game, installer) and publishes with gh.
    **Released on GitHub (user, 27 Nov 23:29) as v0.2.2.**
  - **Sprint/walk keys swapped (28 Nov 00:10, user: the real PC version runs on Shift and walks on Ctrl; the manual
    page said the opposite):** kbm_binds.cpp lstick_press Shift, keybind_walk Control; prompts/keyboard.txt HUD LS
    glyphs k_shift, tutorial btn_left km_shift_sq; launcher keyboard map. Not built (game build; prompts copied by it).
    Goes out in 0.2.3 with the other fixes (user, 28 Nov 00:48).
  - **Xenia Canary balance patch (28 Nov 08:05, not tested):** canary/415607FF - 007 Quantum of Solace.patch.toml =
    Canary's game-patches file (hash A29428CB1432DD9E) plus "Balance beams at 60 FPS" / "at 120 FPS": f32 values
    1.02^(30/fps), 0.93^(30/fps) at 0x82000500.. (PE header padding), lis r4 at 0x82158EE0 -> 0x8200, lfs at
    0x82158F00 (accel, r4) and 0x82158F34 (decel, r10) pointed there. Our default.xex's Canary hash computed offline =
    A29428CB1432DD9E (code pages 0x820E0000-0x825E0000 from the page descriptors, 189 import thunks rewritten to
    sc 2 / blr / nop / nop as Canary's loader does): same build as Canary's file. Note: Canary's own "Unlock FPS"
    nops 0x8229B808 / 0x8229B86C, which in this build are loads (com_maxfps dvar pointer, a float), not the clamp
    branches (0x8229B820 / 0x8229B8A4).
    **Separate release plan (user, 28 Nov 08:11):** canary/ (patch file, comments trimmed, + README.md). Publish
    as its own GitHub release with --latest=false: the launcher's update check reads /releases/latest (tag compared
    with VERSION, QuantumOfSolace.exe asset), so a Canary-only release must not become "latest". Tag not v*, e.g.
    canary-balance-1. Test in Canary first; later a PR to xenia-canary/game-patches.
    **Released (user, 28 Nov 20:51)** on the user's own repo; not offered to xenia-canary/game-patches (a PR there
    is the only part left, optional).
  - **0.2.3 additions (28 Nov 08:30-09:05, written to the PC, not built):** src/motion_blur.cpp `--motion_blur`
    original/off/on (r_motionblur_enable: SP ptr 0x84055FC0, stored at 0x8255EFD4; MP 0x8518C260; bool default 0,
    flags 0x4000, not saved; takedown start sets 1 at 0x82386764, end restores at 0x82387838). Off keeps the byte at
    0 each Com_Frame, on keeps it at 1; logs the game's own changes (first 20). Launcher Graphics "Motion blur"
    Original/Off/On. Launcher frame rate adds 90 (note: "90 and 120 are experimental"). Launcher always opens on
    Game files (was the last tab).
  - **Damage blur and bloom switches (28 Nov 09:40, written to the PC, not built):** src/effects.cpp (both
    programs): `--damage_blur` holds cg_showHurtBlur / cg_showVeryHurtBlur (SP ptrs 0x8266F534 / 0x827700EC, bools
    default 1, read by the HUD sub_8217E110; MP has neither) at 0 each Com_Frame; `--bloom` sets the bloom intensity
    to 0 after the renderer fills the glow settings (src/glow_hook.cpp hooks sub_82561DC0, MP mp/src/glow_hook.cpp
    sub_824C6D98: 56 bytes at r3 + 22916 copied to 0x841EF1B4 / MP 0x8518FE64; +0 enabled, +4 cutoff, +8 desat, +12
    intensity, +16 radius, +20 ray expansion, +24 ray intensity; from the vision set or r_glowTweak* when
    r_glowUseTweaks; r_glow is registered but never read). Light rays left on. Launcher Graphics "Damage blur" and
    "Bloom" switches. **Lens flares switch dropped (user, 28 Nov 21:48: no flare seen that needs it).** Was: r_sunflare_* (SP ptrs 0x843E3858.., registered in sub_82589D20) have
    no readers by pointer or name, so the flares come from elsewhere (level effects?); needs a trace with a flare on
    screen.
  - **DXVK-style work (28 Nov 10:30-11:50):** SDK own patch 0032-native-device-extensions (OWN_PATCHES own33):
    VK_KHR_push_descriptor, VK_EXT_extended_dynamic_state 1/2/3, VK_KHR_pipeline_library and
    VK_EXT_graphics_pipeline_library enabled when offered (all their features on); nothing else changes. Native
    0115-push-descriptors: the pixel textures (set 3) pushed per draw (vkCmdPushDescriptorSetKHR) when they fit
    maxPushDescriptors (32); vertex textures keep sets; a push is skipped while the last one had the same layout and
    handles and nothing bound since; recorded at the binds. BOTS_GFX_PUSH_DESCRIPTORS=0 off. Switching it on/off
    changes pipeline layouts, so the driver rebuilds every pipeline once (warm-up ~6 s instead of ~1 s). Tested
    (qos_227-231): no visual problems; per-frame gain not measured A/B yet. Pipeline libraries: deprioritised
    (qos_223: 2 pipelines built during play, 16 draws skipped in 142,000 frames; the RTX 4080 SUPER supports fast
    linking). **Pipeline libraries skipped (user, 28 Nov 12:08):** almost nothing to fix (warm-up covers it) and
    costs (unoptimised fast-linked pipelines until the optimised one replaces them, every pipeline built twice,
    more memory, uneven AMD/Intel driver support, invasive change). Revisit only if players report first-visit
    pop-in or hitches, or a cold-cache run (cache\native deleted, a level not played since) shows many "pipeline
    took N ms" lines or skipped draws. SDK 0032 still enables the extensions (harmless).
    **Extended dynamic state (native 0117-extended-dynamic-state, 28 Nov 12:00, on the PC, not built or tested):**
    cull mode, front face, depth test/write/compare, stencil test/ops (VK_EXT_extended_dynamic_state) and primitive
    restart (VK_EXT_extended_dynamic_state2) set per draw; GetPipeline drops them from the key (StripDynamic), so
    draws differing only in them share a pipeline; changed states only (DrawPacket::dyn_mask bits 10-18), all after
    a rebind. BOTS_GFX_DYNAMIC_STATE=0 off; log "extended dynamic state on (with primitive restart)". Switching it
    rebuilds pipelines once; the manifest then records stripped keys (with it off later, those warm up unused
    pipelines: harmless). Test: build_native + game build, mission 10 busy spots, A/B with =0; watch for missing or
    inside-out faces, see-through objects, broken stencil effects.
  - **Mission 10 (train) 10 FPS stretches fixed (28 Nov 11:20, native 0116-hot-page-draw-limit):** qos_226 had ~70,000
    hot-page hashes and ~16 ms of tracker a frame, 121 of 600 frames over 50 ms (the game held at ~10 FPS for seconds):
    many draws read the same big block of hot pages. A hot page read by more than BOTS_GFX_HOT_MAX_DRAWS draws in a
    frame (default 16, 0 = no limit) goes back to protection and isn't made hot again for 600 frames. qos_228 (train +
    casino, 70 windows): at most 892 hashes and 1.9 ms a frame, 12 frames over 50 ms in the session. 0.2.2 shipped
    hot pages on, so 0.2.2 players hit this on the train: 0116 goes into 0.2.3.
  - **Train cutscene lamp flicker: intended (28 Nov 11:50).** The light pool and bottle shadow on Vesper's table
    flash on and off: lightning from the storm outside. Same on Xenos, at 30 FPS, in 6-year-old console footage and on
    the PC version. Not a bug. (Ruled out on the way: push descriptors, hot pages, occlusion queries - 0 in that run.)
  - **Closed (user, 28 Nov 22:00): the 0.2.3 release holds a solid 60 FPS on the train.** (A dev run that evening,
    qos_286, was slow only because a Command Prompt still had test switches set: BOTS_GFX_HOIST_UPLOADS=0,
    BOTS_GFX_HOT_PAGES=0, BOTS_GFX_TEXTURE_INDEX=0, plus the profiler. Test runs: use a fresh window.) Was:
  - **To do: mission 10 (train) performance (user, 28 Nov 11:52):** still feels slow in places after 0116. From qos_228: busy
    train/casino stretches at ~15,000-17,000 draws are renderer-bound at 18-20 ms frames (low 50s FPS at a 60 cap);
    one ~200 ms frame each time the phone opens (log: "phone true", video blood_barrel; likely the game's own phone
    video start). Next: a profiled run at the slow spots (BOTS_DRAW_PROFILE=1, note where), then the per-draw costs
    there (constants ~2.5-3.3 ms, descriptors ~1.6-1.9, record ~1.2-1.4 at 16k draws) or extended dynamic state.
  - **Train car gaps at 60 FPS fixed (28 Nov 14:04, src/traversal_fix.cpp, works, user):** mission 10's jumps
    between cars (wall move along the car's side, traversal type 5, then a mantle, type 3) dropped Bond onto the
    tracks above 30 FPS: the mantle ended at the same point every time just before the gap (qos_237, 60 FPS) where at
    30 it carried on across (qos_238). sub_82157A28 is Pmove (r3 pmove: +0 ps, +4 cmd serverTime; steps of up to 66
    ms through PmoveSingle sub_82156E88; also run by client prediction). --traversal_fix (on): during wall/mantle
    moves (ps pm_flags +12 & 8, type +2260 3 or 5) a call with under 33 ms to run moves nothing (serverTime set to
    commandTime for the call), so those moves run in 33 ms steps as at 30 FPS. --traversal_log (off) logs pm_flags /
    traversal / falling changes with origin (+32) and velocity (+44). Type names at 0x825E65D0 (none, balance,
    climb, mantle, transition, wall); sub-types (beam, ladder, ledge, pipe...) at 0x825E65EC. Not committed yet;
    the old src/traversal_log.cpp on the PC is unused (delete). Real cause inside the mantle code not found.
    **Holds at 90 FPS (user, 28 Nov 22:29):** the mission 10 mantle across the gap works at 90; 120 not reported.
  - **0117 A/B done (28 Nov 17:05, qos_247 on / qos_248 off, mission 10, profiler on): no gain, stays off.**
    Busy windows (>= 7,000 draws): 0.953 vs 0.930 us/draw in draws, pipeline 0.063 vs 0.058, record 0.098 vs
    0.095; pipelines warmed 4,401 vs 4,459 (58 fewer: the game rarely pairs the same shaders with different
    cull/depth/stencil). Code kept, off by default (BOTS_GFX_DYNAMIC_STATE=1). Closed.
  - **Phone hitch skipped (user, 17:11):** not noticed in play; left on the list, low priority.
  - **Mission 2 rooftop: can't reproduce (28 Nov 18:23).** Tracy (--tracy, src/qos_app.h: the SDK never started
    the profiler; viewer 0.13.1, run as admin for sampling): main thread mostly game frame code; hottest guest
    functions are CPU vertex skinning (sub_825A68F8 -> sub_825A65A0 -> sub_825A59A0/5B98/5DF8/60B0, bone blend +
    2-10-10-10 normal unpack/repack). src/skin_profile.cpp (--skin_profile): 1.2-4.6 ms a frame total, split evenly
    over three JQ Worker threads, so not the rooftop limit; native skinning dropped. qos_252 rooftop at 60: game
    ~6-7 ms a frame (renderer waits 9-10 ms), vs 12-20 ms on 27 Nov (qos_223); not shader builds. Original report was
    uncapped: a --frame_rate=0 check would settle it.
  - **Decal z-fighting (28 Nov 21:24, native 0120-decal-depth-slack; for 0.2.4, not committed):** both renderers
    flicker (Xenos too). Boiler-room stickers (zfight-trace.txt): quads on the wall's plane, depth GEQUAL, no depth
    write, no polygon offset (scm 00018006: offset bits 0x1800 off), so BOTS_GFX_DEPTH_BIAS_SCALE had nothing to
    scale. The guest depth is D24FS8 (20-bit mantissa): wall and sticker round to the same value and GEQUAL passes;
    host D32_SFLOAT keeps their tiny difference. 0120: D24FS8 draws with depth test on, write off, GEQUAL/GREATER
    (or LEQUAL/LESS) and no guest bias get a constant bias of 8 float32 steps (one float24 step) towards passing.
    BOTS_GFX_DECAL_DEPTH_SLACK=n (0 off). **Works on the boiler-room level (user).**
    **Not fixed, parked (user 21:41: "unless someone complains"):** coplanar level geometry that writes depth itself,
    e.g. the security room's sticky notes on shelves/desk (zfight-trace2.txt: dc E6, mode 4, no bias). Nearly all
    of 007's colour draws write depth with GEQUAL (no depth prepass); on the Xbox both round to the same float24
    value and the later draw wins; a bias can't help (both draws would shift). Real fix: round written depth to
    float24 in the pixel shader (SDK's kFloat24Rounding, which assumes Xenia's host [0, 0.5] depth range: needs an
    SDK patch for the native renderer's [0, 1]; depth-only draws need a pass-through pixel shader; costs early-Z, so a
    launcher option). Xenia/Xenos show the same flicker by default.
  - **See-through letterbox bars above 720p fixed (28 Nov 20:44, native 0119-plain-upscale-pixels, works, user;
    for 0.2.4, not committed):** with --cutscene_letterbox (launcher "Black bars in in-game cutscenes") at render
    scale 2 the Bolivia intro ("Oruro Province", an in-engine 3D intro, not a Bink video) and other letterboxed
    cutscenes showed only the two thin blue border lines, the scene running on behind them (Xbox and the Xenos
    renderer: solid black bars, checked against a YouTube capture). The game composes the letterbox in its 2x MSAA
    3D view surface (base 0x48, 1024x608 pixels): a black fill (1x1 texture 05E23000, opaque), the scene copied into
    rows 60.8-547.2 with texcoords 0.1-0.9 (vs B4A1C67978D074E5, ps 6E57A6E305D5D69A), then four 2-pixel border
    quads (1x1 texture 05E22000 times vertex colour). Plain upscale (0023) compared the copy's source (2048x1216 host)
    with the draw's area in samples (2048x2432: MSAA targets are drawn supersampled), took it for a stretch and
    blitted the whole view over the whole surface, wiping out the black fill. 0119 divides the area by the sample
    grid first; the 608 -> 720 and 1024 -> 1280 stretches (1x targets) are unchanged. The default (bars off) was
    never affected: letterbox.cpp keeps the view full height. Earlier notes blaming a depth-clear transfer and
    "fine at scale 1" were wrong. Native 0118-plain-upscale-trace (diagnostic, kept): trace-now extras: every plain
    upscale candidate, vertices/indices of screen quads and 2D batches, 1x1 texels, BOTS_TRACE_PROBE=x,y (a pixel
    read back after every draw and at present) and trace-present.ppm (the shown image). **Trace trap:** switching to
    the Command Prompt to type trace-now pauses videos and cutscenes (SKIP/RESUME screen over a dark world map), so
    every such trace catches the pause screen; trigger it with a delay instead (timeout /t 8 /nobreak >nul & type nul
    > "%USERPROFILE%\source\repos\007qol\out\build\win-amd64-relwithdebinfo\trace-now") and click back into
    the game.
  - **Shader refs from the full playthrough (28 Nov 18:00; committed 166a80e, 20:48, 937 KB; `check` not run):** ff_shader_refs.py index on the PC's
    manifest (~4,400 pipelines) with --keep and --cover -> pipelines/415607FF.refs for 0.2.4 (installer ships the
    folder; existing installs rebuild only new shaders). Needs numpy on the PC.
  - **0.2.3 released (user, 28 Nov ~15:10).** Queue for 0.2.4 below.
  - **Mission 11 cutscene camera with the bars off (28 Nov 15:56, fixed, works, user; for 0.2.4, not committed):**
    after the reception keypad the cutscene showed the wrong camera and Bond out of place: letterbox.cpp held
    r_pipMainMode at 0, but that cutscene frames its shot through the main PIP window. Now the window stays and only
    its crop goes: the script function animatePip (sub_82230440) sends "animatePip <time> <screen> <x> <y> <scalex>
    <scaley> <cropx> <cropy>" through SV_GameSendServerCommand (sub_822C1F50, r3 client, r4 text); while
    letterboxActive, a screen-1 command's scaley is rewritten to 1.000000 in place, and cg_pipmain_border (float,
    ptr 0x8276A078) is held at 0. Log "letterbox: cutscene view kept full height". Workaround in 0.2.3: launcher
    "Black bars in in-game cutscenes" on.
  - **Pistol LOD switching at wide FOV (28 Nov 15:05, fixed, works at 90, user):** the P99 view model switched
    between its detailed and simpler model frame to frame at --fov 90 (not at 65): the game scales LOD distances by
    the field of view and the gun sits at its first threshold. src/lod.cpp now multiplies r_lodScale by
    tan(32.5 deg) / tan(fov/2) (0.637 at 90), so every model keeps its FOV-65 detail. First blamed on 0117 (a dev
    run without --fov looked fine with BOTS_GFX_DYNAMIC_STATE=0): **0117 is innocent but went into 0.2.3 off by
    default (BOTS_GFX_DYNAMIC_STATE=1 on)**; to re-enable after an A/B at 60 FPS.
  - **Release 0.2.3 prepared (28 Nov 14:10, user):** QOS_RELEASE, launcher VERSION and GAME_VERSION 0.2.3 (installs
    rebuild), SDK bundle qos13 (native renderer to 0117, SDK own33), notes docs/releases/0.2.3.md. The user builds
    (build_native, game, installer) and publishes with gh.
  - **Playthrough complete (user, 28 Nov 16:47):** all 15 missions finished (11-15 on 0.2.3 at 60 FPS, FOV 90, with
    cutscene bars on after the mission 11 camera bug). Issues found on the way: train gaps (fixed 0.2.3), pistol LOD
    at wide FOV (fixed 0.2.3), mission 11 cutscene camera with bars off (fixed, for 0.2.4). qos_246 (missions 12-15):
    60 FPS held, ~200 ms hitch every time the phone opens with the blood_barrel video (9 in 40 min; phone opens
    without that video don't hitch): now the top performance item.
  - **Quick kills at high FPS (28 Nov 09:10):** the press windows are in ms (qk_diff_tweak_easy +500, norm +300,
    hard -100, xprt -150; picked by difficulty in sub_823866D0 around 0x82387030), so frame rate shouldn't matter.
    Not traced: ButtonMash, StickSwirl/StickSlap/StickToggle, lock picking (MechLock pins). Watch for these.
  - **Correction (28 Nov 09:23, user):** mission titles (location text) are not part of the load videos; the mission
    2 title is drawn by the game. The 25 Nov trace only showed a Bink video with alpha drawing at that moment.
    **Dropped (user, 28 Nov 22:16).** Was: curiosity, low priority: how the titles are drawn (script hud text or Scaleform) and whether their position can
    be moved; one trace while a title is on screen would tell.
  - **Playthrough (user, 28 Nov 09:25):** 7 of 15 levels played; the other 8 still to check (above-60 FPS breakage,
    keyboard prompts, z-fighting, see-through bars, slowdowns).
  12. **Rooftop slowdown, level 2 (user, 27 Nov 23:01):** dips into the 50s uncapped (and a few frames just under 60
      at 60) on the rooftop chase. Game-bound, not the renderer (qos_223: ~6 ms drawing, then 9-14 ms waiting for the
      game; the game thread needs 12-20 ms a frame there). Next: a Tracy capture there (TRACY_ON_DEMAND is built in;
      Tracy viewer on the PC) to see which game functions cost it (game logic, a hook of ours, or an emulated kernel
      call it waits on).
  11. (later) Xbox LIVE via Xenia Web Services (multiplayer to-do item 6 above: the SDK's Live calls - XSession create/
     search/join/leave, the XLIVEBASE messages such as 0x00058035 - talking to Xenia Web Services instead of the LAN
     broadcast; real QoS probes for Live; launcher server address setting; the one-active-XUID rule). Several
     sessions. **Later, not tomorrow (user, 25 Nov 22:28: "down the line")**; the groundwork below is done.
  Options go on the Graphics tab (Display for FOV), translated in launcher_text.py.
- **Xbox LIVE via Xenia Web Services, step 1 groundwork (25 Nov 22:05):**
  - QoS is among the titles the netplay fork's release notes name as working (AdrianCassar/xenia-canary releases:
    "Armoured Core 4, Blur, Call of Duty 4 ..., Quantum of Solace, Super Meat Boy"). The wiki compatibility page
    couldn't be fetched (GitHub client_error); check its QoS row (modes, notes) by hand.
  - Client code, branch netplay_canary_experimental (BSD, like Xenia): src/xenia/kernel/XLiveAPI.cpp/.h (2,685 lines:
    REST client over libcurl, async via overlapped tasks), xsession.cc/.h (1,588: XSession create/search/join/leave/
    migrate/arbitration/details/modify), xam/apps/xlivebase_app.cc (3,259: XLIVEBASE messages), xgi_app.cc (761),
    xam/xam_net.cc (2,673: XNet incl. QoS), upnp.cc (611), json/*_object_json (session, leaderboard, stats, http
    response), util/net_utils + network_adapter_manager_win/linux. About 12k lines to bring into SDK patches.
  - Server endpoints used (api_address + ...): whoami; players, players/find, findusers, presence, setpresence,
    getsettings/setsettings, deletemyprofiles; title/{id}/ports, servers, services; title/{id}/sessions (create),
    sessions/search, sessions/{sid}[/details|join|leave|migrate|modify|arbitration|prejoin|qos|properties|
    leaderboards]; leaderboards/enumerate|find|estimate-rank; DeleteSessions.
  - Why --user_signin_live=true quit at start (24 Nov): XLIVEBASE 0x00058035 is XStorageBuildServerPath (the fork
    answers it: X_ONLINE_E_LOGON_NOT_LOGGED_ON when not signed in to Live, else builds the server path from the
    XUID). Our SDK leaves it unimplemented. First thing to port.
  - Our SDK has no HTTP client (thirdparty: no libcurl). Either add libcurl (as the fork) or a small WinHTTP wrapper
    on Windows (no new dependency; Linux later via libcurl).
  - Local server for development: Xenia-WebServices (Node/NestJS + MongoDB; npm run start; Swagger UI) on the PC.
    The public server (xenia-netplay-2a0298c0e3f4.herokuapp.com) is third-party: ask its maintainer before pointing
    the port's players at it.
  - Compatibility (user's links, 25 Nov 22:03; Xenia-WebServices src/public/data/compatibility.json): "007: Quantum of
    Solace", public "working" (Xbox LIVE through the public server works in the netplay fork), systemlink "partial":
    "Patch required for systemlink. Systemlink requires a server." Patch patches/415607FF - Quantum of Solace.patch.toml
    (default_mp.xex hash BC3F3CCCF880F4EC), "Enable QoS Systemlink, host must enable this": nops the two bne at
    0x8228FA5C / 0x8228FA68 in sub_8228F950 (the server's connectionless packet handler, "getinfo" branch): with them,
    a getinfo whose from-address bytes +4/+5 (frame 1156/1157, i.e. the first IP bytes) are non-zero keeps its address
    type; nopped, the type is always set to 3 before the host answers. Our System Link works without it (two copies on
    one PC; our peers have synthetic 169.254.250.x addresses), so not needed so far; if a two-PC join fails at
    "getinfo" (host never answers), try the same in a hook (force frame+1152 = 3 in that branch).
  - Test server: the user's NAS, TrueNAS 25.10 (Docker apps), Ryzen 5 3500 (Zen 2: AVX, so current mongo images
    run). No prebuilt server image: build it on the PC from Xenia-WebServices (dockerfile), push to a registry, then
    Apps -> Install via YAML with backend (image, API_PORT 36000, MONGO_URI mongodb://database:27017/) + database
    (mongo, /data/db on a dataset). Draft YAML in the chat of 25 Nov 22:15; untested. Docker Desktop on the PC is the
    quick alternative (docker compose up -d in the repo).
  - Server security (checked 25 Nov 22:21): none built in beyond helmet on the web page, a profanity filter and input
    validation: no auth (players identified by IP + XUID, clients trusted), no rate limiting, no HTTPS of its own,
    CORS open. Fine on the home network; before opening it to the internet: reverse proxy or tunnel (HTTPS, rate
    limits), only the game's API paths public (not /api Swagger, not MongoDB), isolated app/dataset, backups,
    updates. A shared key between port and server only if abuse happens (needs a server change).
  - Plan from here: (a) port XLiveAPI with a WinHTTP backend + whoami/players (sign-in, online XUID); (b)
    xlivebase messages incl. 0x00058035; (c) xsession (create/search/join/leave) over the existing netplay socket
    layer; (d) real QoS; (e) UPnP; (f) launcher server address + hosting toggle. Server: set up privately on the NAS
    at the start of (a); opening it to the internet (proxy/tunnel, the list above) after (f), before releasing online
    play.
  Also: player instructions written (docs/multiplayer.md, 25 Nov 21:55, linked from the README; multiplayer to-do
  item 5). 0.2.0 and 0.2.1 released (user publishes).
- **Graphics options asked for (user, 25 Nov 21:43), dvars found in default.xex (strings, not yet traced):**
  shadows: sm_enable, sm_sunShadowEnable, sm_spotShadowEnable, sm_maxLights, sm_sunSampleSizeNear ("Shadow sample
  size": smaller = sharper sun shadows near the camera), sm_fastSunShadow, r_spotLightShadows, r_playerShadowEnable
  ("Hirez Player Shadows"); shadow map sizes are the engine's render targets (sun/spot/player-sun, already scaled by
  render scale in the native renderer). LOD: r_lodScale, r_lodBias, r_forceLod, r_highLodDist / r_mediumLodDist /
  r_lowLodDist / r_lowestLodDist; streaming/sc_offscreenCasterLodScale. DoF: r_dof_enable (+ near/far start/end/blur,
  r_dof_viewModelStart/End, r_dof_bias), dvar weaponDof ("Far Weapon DoF"); scripts set DoF per scene
  (setdepthoffield, customcamera_setdof), so an off switch must hold it every frame (as fov.cpp). Hit markers: the
  "enemy hit indicator" (hud_fade_enemyhit fade time), drawing code to find. Weapon FOV: no gun-FOV dvar (only
  cg_gun_x/y/z offsets, cg_fov); keeping the weapon model's own FOV needs a hook in the view model's projection.
- **Next update, launcher (user, 25 Nov 21:37):** fixed window size with a vertical scroll bar on each tab's page;
  the Display tab now overflows (the Skullburglar credit line is cut off at the bottom in the user's screenshot).
  tkinter: each page a Canvas + inner Frame + ttk.Scrollbar, mouse wheel bound while the pointer is over it.
- **In-game cutscene letterbox off (25 Nov 21:15; works, user 21:13: "looks much better"):** the bars the user meant (Nov 21 recording, start of
  whites_estate) are in-game, not a video. common.ff's letterbox_on(): setsaveddvar letterboxActive 1, r_pipMainMode 1,
  r_pip1Anchor 4, cg_pipmain_border 2 / color "0 0 0.2 1" (the thin dark blue lines), r_pip1Scale "1 1 1 0", then
  level.player animatepip(1000, 1, -1, -1, 1, 0.8, 1, 0): the main view cropped to 80% height. letterbox_off():
  animatepip back to 1, r_pipMainMode 0, letterboxActive 0. src/letterbox.cpp (from the Com_Frame hook): while
  letterboxActive (dvar ptr 0x8264C054, byte +12) keeps r_pipMainMode (ptr 0x84055EFC, int +12) at 0.
  --cutscene_letterbox (default false); launcher Display "Black bars in in-game cutscenes" (default off). The 1440p
  level-1 trace (level1-trace2.txt) showed the bars as four 1x1-texture quads (vs 73FF89529D64FE5C) drawn into the
  1024x608 view before the 608 -> 720 stretch. Videos (Bolivia intro, level intro) keep their own bars; their
  see-through bars at 1440p (plain upscale) were fixed on 28 Nov by native 0119.
5. (last: describes everything above) **User instructions:** how to play multiplayer (launcher gamertag, firewall prompt, System Link on a LAN, two PCs,
   VPN with netplay_peers / port forwarding UDP 11000+ (16 per slot)), installing DLC.
6. (after everything above; adds to it, replaces nothing) **Xbox LIVE via Xenia Web Services:** make the SDK's
   Live calls (XSessionCreate/Search/join/leave, the XLIVEBASE messages such as 0x00058035) talk to Xenia Web
   Services (AdrianCassar's REST API for Xenia Canary Netplay; public server or self-hosted) instead of the LAN
   broadcast: a real server list in the Xbox LIVE menu. Real QoS probes for Live (System Link keeps the 1 ms
   answer); router ports via UPnP / forwarding / VPN; launcher: server address setting. XUID rule (user, 25 Nov
   08:39): one active XUID per player: the launcher's E000 offline one until the player first connects, then the
   server's online (0009...) one for both Live and System Link, kept from then on (never switch per mode: MP
   unlocks may be stored per XUID); the launcher keeps one read-only XUID box, maybe "(online)/(offline)".
- **Dropped (user, 25 Nov 00:26): headless server.** QoS hosts as a listen server: the host's own client 0 takes a
  slot and counts toward the minimum players while not playing, so a headless host costs a player slot. Notes kept
  in git history (commit 7186ac0's HANDOFF.md). Players host from the game as on the console.
- No two-PC test: the user has no second PC. Two instances on one PC (24 Nov 23:39) stand in for it; LAN and VPN
  paths (broadcast to other machines, netplay_peers) stay untested until a user reports.

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
- **FSR 1 (22 Nov, 16:45, launcher only, not tested in game yet):** the runtime's presenter (Vulkan too) has
  `--present_effect` bilinear / cas / fsr / fsr2 / fsr3 (FidelityFX EASU + RCAS, CAS; fsr2/3 are temporal and need
  motion vectors the native renderer doesn't make). Launcher Display tab: "Upscaling" (Bilinear, AMD FSR 1, AMD CAS)
  -> `--present_effect=fsr|cas`. Only upscales when the render resolution is below the window (e.g. render 2x =
  2560x1440 on a 4K screen); at equal sizes FSR may do nothing or sharpen only. Test: qos.exe ... --present_effect=fsr
  with BOTS_GFX_RENDER_SCALE=2 on the 4K screen, compare with bilinear.
- **Title screen prompts (23 Nov, 18:15; works, user 18:09):** user asked for the main menu's two prompts on the Press
  Start screen, B relabelled: "A SELECT" over "B QUIT (HOLD)" (was "B QUIT (HOLD)" alone in A's place). Measured from
  the user's 4K screenshots (main menu): discs 60 px (30 at 1080p), lines 90 px apart (45), A disc at 707-766 x,
  1844-1903 y. `tools/make_quit_hint.py` now draws `hints/start_prompts.png` (A green disc, our own art) and
  `start_prompts_ps.png` (blue cross, circle), 172x77; b_quit*.png removed. menu_quit.cpp: menu_hint_images
  start_prompts,start_prompts_ps, menu_hint_pos 353,921. The image is over 0013's hint limit, so **native patch
  0096-hint-two-lines**: hints up to 256x96 / 13600 texels (present UBO 64,384 bytes; static_assert <= 65536, NVIDIA's
  maxUniformBufferRange). Needs build_native + game build; an old renderer logs "too big" and shows no hint.
- **Red rock face / missing haze (23 Nov, 18:10-18:40):** user saw a red-brown rock face with a hard vertical edge
  (4th level, a desert rock passage, checkpoint). Xenos (build_Screenshot 18:32) shows the same red face: the game's
  own material, not a bug. But Xenos draws a bright dusty haze over the scene there (light through dust, softening the
  far rocks) that the native renderer doesn't: matches 21 Nov's "Xenos darker, hazier (more fog)". Suspects: dust /
  light-shaft particles (soft sprites reading scene depth) dropped or invisible, or fog not applied.
  **Native patch 0097-trace-now (written, not built):** the draw trace on demand: a file `trace-now` in the working
  folder (polled every 30 frames, deleted) traces the next BOTS_DRAW_TRACE_FRAMES (default 2) frames like
  BOTS_DRAW_TRACE=1, the trace lines now carry vs/ps ucode hashes, and each pixel shader met in the window is printed
  once (microcode + non-zero pixel constants). Markers "=== draw trace start/end ===" in stderr. Next: a trace at that
  checkpoint (2> haze-trace.txt), look for additive/alpha-blended sprite draws, depth-reading shaders, skipped draws.
  **Update 18:48:** user played on with native: the dust effects do show. The two screenshots weren't a fair pair
  (native 17:44, ammo 81, later in the fight; Xenos 18:32 just after loading the checkpoint, ammo 76): the dust is
  probably a timed effect that had settled in the native shot. Likely no bug. To be sure: both renderers right after
  loading the same checkpoint. 0097 stays (off unless trace-now is created).
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

## Mouse look (23 Nov): the game's own PC mouse code, fed real mouse counts

`src/mouse_look.cpp`, **built but not yet tested in game**. The 360 executable still has CoD4's PC mouse code,
whole: `CL_MouseMove` = sub_821B8908 (called every frame from CL_CreateCmd sub_821B94F8 unless `cl_ignoreinput`),
reading the dvars `sensitivity` (default 5), `cl_mouseAccel`, `m_yaw`/`m_pitch` (0.022), `m_side`/`m_forward`
(0.25), `m_filter`, `cl_freelook`; `CL_GetMouseMovement` = sub_821B87E0. `cl` is at 0x82809AD8: `mouseDx[2]`
+0x9B40, `mouseDy[2]` +0x9B48, `mouseIndex` +0x9B50 (ints); `viewangles` pitch/yaw at +0x9B98/+0x9B9C; frame_msec
0x827E3310. Dvar pointers: sensitivity 0x82809818, m_yaw 0x82809820, m_pitch 0x82809824, cl_mouseAccel 0x82809A98,
cl_freelook 0x82809ACC, m_filter 0x82809AC4. Nothing on the console fills mouseDx/Dy.
The hook puts the SDK's keyboard/mouse driver in direct mode (SDK 0009 `SetMouseDirect`) and, before CL_MouseMove,
adds `ConsumeMouseDelta` counts to mouseDx/Dy[mouseIndex]. So the game's sensitivity, zoom scaling and pitch limits
apply: 0.11 degrees a count at `--mouse_sensitivity=1`. Movement with a gap over 250 ms between calls (menus,
loading) is dropped. Cvars: `--mouse_look` (on), `--mouse_sensitivity` (0.05-10), `--mouse_invert`. Needs the SDK's
keyboard/mouse input on (`--mnk_mouse=true` and the driver enabled). Log: "mouse look: ..." and counts per second
for the first 3 s of movement. Found with `xexdec.py` (cloud) + a small PPC decoder over the decrypted image.
**Works (user, 23 Nov 21:45): "basic controls work".**
**Default binds + launcher (23 Nov, 22:00), not built/tested:** the game's pad layout is `buttons_default.cfg` in
english/code_post_gfx.ff (RT +attack, LT +speed (ADS), RB +frag, LB weapnext, RS +melee_qk, LS +dash, A +cover2,
B +stance, X +usereload, Y +gostand2, D-pad up nextfiretype / down togglesilencer / left-right grenadenext;
alternatives buttons_alternative*.cfg). `src/kbm_binds.cpp` (called first in OnPreSetup) sets the SDK's keybind_*
cvars still at their compiled-in default: WASD, Shift sprint (LS), Q takedown (RS), LMB fire, RMB aim, G grenade,
F/1/2/MMB swap weapon, E/Return A (cover, menu select), C/Control/Backspace B, R X, Space Y, X/Up fire type, 4/Down
silencer, 3/Left grenade type, Right, Tab back (phone), Escape start; right-stick keys cleared. Bind tokens from
the SDK's src/ui/keybinds.cpp (LMB/RMB/MMB, Shift, Control, Escape, Return...; no mouse wheel). Launcher Controls:
"Keyboard and mouse" (default on: --mnk_mode=true --mnk_mouse=true), "Mouse sensitivity (%)" 10-300 ->
--mouse_sensitivity, "Invert mouse up/down". Check: Q does the takedown (if not, B might: its comment says "Melee /
QK / Exit cover"). Still to do: keyboard prompts.
**WASD takedowns (23 Nov, 22:10, user: "i want the takedowns to also be WASD"), not built/tested:** SDK patch
`0015-mnk-stick-keys-face-buttons` (`SetStickKeysAsFaceButtons(bool)`: the left-stick keys press Y/X/A/B by their
place on the pad, up/left/down/right, and move nothing; keyboard pad only; build_sdk tag own15, launcher SDK bundle
qos4). `src/takedown_keys.cpp` turns it on in the quick-kill begin method sub_823866D0 and off in its end
sub_823877C8 (virtual methods: vtables at 0x82059068/0x82059070...; begin saves and sets kill-camera dvars:
cg_showBarrelModel, r_motionblur_enable, r_lodBias, cg_showHurtBlur, cg_hudDamageIconTime, health_regen_enable,
ik_enable; end restores them), with a 20 s safety off from the Com_Frame hook. `--takedown_wasd` (on). Log
"takedown: begins/ends". The quick-kill dvars (bg_quick_kill_*, qk_*) are registered around 0x8214B000; their
readers 0x82384180, 0x82384580, 0x82385850, 0x82385C28, 0x8238A920. The on-screen diamond still shows pad buttons
(keyboard prompts to do). Also noticed: Skullburglar has SDK patch 0018-netplay-system-link (for multiplayer later).

## Keyboard prompts (23 Nov, 22:40), written, not built or tested

User: "create the keyboard glyphs. the takedowns should keep their colour and shape"; ignore the 3D tilt keys.
- **SDK patch 0016-keyboard-mouse-active** (`rex::input::KeyboardMouseActive()` in pad_style.h): true while the MnK
  device is the input system's active device for user 0 (ActiveDeviceTracker: real input takes over, a neutral pad
  never does; mouse movement alone doesn't count, a key or click does). build_sdk tag own16.
- **Native patch 0098-keyboard-prompts:** a second list `prompts/keyboard.txt` (same syntax as playstation.txt) for
  pad_prompts_active 2. prompt_atlas Find*/Build*/ReplacementPixels take the mode; mode 0 asks every list, so a
  texture any list changes is marked pad_sheet and reloaded when the mode changes. Syntax-checked prompt_atlas.cpp
  only (texture.cpp not compiled here).
- `src/pad_prompts.cpp`: active 2 while KeyboardMouseActive() (--pad_prompts auto; also `keyboard` forces it, xbox /
  playstation force those). Launcher "Button prompts" gains "Keyboard only".
- `prompts/keyboard.txt`: menus btn_accept -> km_enter, btn_back -> km_backspace; tutorial pictures -> kt_r (X disc
  82C5...), kt_f, kt_mouse_right/left, kt_g, kt_tab, kt_shift (LS 748B...), kt_q (RS 9849...); takedown 50 px discs ->
  tk_w (Y), tk_a (X), tk_s (A), tk_d (B); HUD font 28/14 px cells -> k_e, k_c, k_r, k_space_sq, k_f, k_g, k_esc, k_tab,
  k_shift (LS 372-405 x 190-222; 14 px 466-483 x 422-439), k_q (RS 406-439; 14 px 0-17 x 426-443),
  k_mouse_right/left. D-pad glyph left. Image names are relative to prompts/ (kbm/...).
- Art: `tools/make_kbm_prompts.py` (new): tk_w/a/s/d = make_ps_buttons.py's disc with a Montserrat Black letter, in
  the game's colours (decoded from texdump4 this evening: yellow disc (125,87,29) letter (205,143,51); blue
  (57,66,100)/(122,138,180); green (74,97,57)/(133,168,112); red as the HUD glyph); km_backspace (arrow); k_space_sq.
  Title screen: `hints/start_prompts_kbm.png` (ENTER SELECT / Backspace QUIT (HOLD), 176x77 = 13,552 texels, under
  0096's 13,600), menu_hint_images gets it as image 3. A cloud DXT5 decoder for the dumps: rewritten this evening
  (tiled, 8-in-16), not kept in the repo.
- Test: build_native (SDK own16 + native 0098) + game build. Log "pad prompts: prompts/keyboard.txt: 1 textures, N
  images, 16 replaced textures", "pad_prompts: keyboard keys" after a key press, "Xbox buttons" after a pad press.

**Keyboard prompts test 1 (23 Nov, 22:35-22:52, qos_142):** works: keyboard.txt loaded (14 replaced textures), menu,
HUD font, tutorial and takedown swaps seen in the log; takedown begin/end pairs come back to back within one takedown
(no gap where WASD moves). User's fixes (screenshots): the Scaleform A/B pictures also show in tutorial text
("Press A to take cover"), so btn_accept -> E and btn_back -> C (both bound in menus too); the tutorial LS picture is
the stick ("pull away from cover with LS") -> S; and all prompts light grey: tutorial pictures now `km_*` (menu style,
light grey with shadow; make_kbm_prompts.py draws km_e c r f g s q tab mouse_left/right). Prompts-only change: a game
build copies them. "Hold B and pull away from cover with LS to stay low": user wants Shift and S there, pictures
only, binds unchanged (22:54): btn_back -> km_shift_sq; then (23:04, main menu "SHIFT BACK") a BACK key, km_back_sq.

**Open (23 Nov, 23:07, user: "we'll try again later"):** one game picture serves several hints, so one key can't fit
all of them. Tutorial LS picture (748B...): "pull away from cover with LS" (user wanted S) and "Click LS while
pushing forward to sprint" (user wanted Shift; sprint is Ctrl since 24 Nov). Scaleform B (btn_back): main menu BACK (now a BACK key), phone EXIT
(user asked for Tab), "Hold B ... to stay low" (user asked for Shift). Options put to the user: Shift everywhere / S
everywhere / one picture with both keys / find which hint is up (hook the tutorial hint code) and pick per hint.
Undecided; keyboard.txt currently has LS -> km_s, btn_back -> km_back_sq.

**Next tests (23 Nov, 23:11):** (1) busy-spot profile (not the airport: that was another game): `BOTS_DRAW_PROFILE=1 BOTS_GFX_RECORD_THREAD=1`,
`--vsync=false --frame_rate=60` (last split, 22 Nov qos_120, with 0028/0029/0092/0093/0094: textures 3.94, tracker
2.08, descriptors 1.88, setup 1.75, constants 1.63, rest ~4.5 ms). Ideas: tracker memo by page stamps (like 0094),
descriptor reuse / push descriptors, setup from register dirty bits, constants by changed ranges, page-level hash for
the unchanged-texture check, PM4 parse thread (<= 2.4 ms). (2) level-start stall: `BOTS_HITCH_MS=20` from a checkpoint
load: CP idle (game) vs textures (renderer) in the slow frames.

## Keyboard binds as the PC game's manual (24 Nov, 06:50), written, not built or tested

User shared the PC manual's "Movement Controls" page. `src/kbm_binds.cpp` now matches it: WASD, mouse, Mouse 1 fire
(RT), Mouse 2 ADS (LT), G grenade (RB), 3 switch grenade (D-pad left), Q take down (RS), **Ctrl sprint** (LS, was
Shift), **1 & 2 cycle weapon** (LB, was F/1/2/MMB), C stance (B; Ctrl removed; Backspace unbound later the same morning, so B is C only, menus too), **Shift walk** (new
SDK patch `0017-mnk-walk-key`: keybind_walk, mnk_walk_scale 0.4, build_sdk own17), R reload and **F use** (both X,
+usereload), Space jump (Y), E cover (A), 4 silencer (D-pad down), X rate of fire (D-pad up), Tab phone (BACK), Esc
pause (START). The manual's phone submenus on the arrow keys are not mapped (on the pad they are the triggers; the
mouse buttons do it). Prompts: X glyph and tutorial X disc -> F, LB -> 1 (HUD k_1, tutorial km_1), HUD LS -> CTRL
(k_ctrl); the tutorial LS picture stays S (shared with the cover hints; the sprint hint would want Ctrl now). Launcher
note updated. Test: build_native (SDK own17) + game build; log "keyboard binds: 25 of 25".

**Esc goes back in menus (24 Nov, 07:00), written, not built or tested:** SDK patch `0018-mnk-start-keys-back`
(`SetStartKeysAsBack`: keys bound to Start press B while on; a press keeps the meaning it started with, so the Esc that
opens the pause menu doesn't close it; build_sdk own18). `src/menu_keys.cpp` (from the Com_Frame hook) turns it on
while KEYCATCH_UI (0x10) is set in clientUIActive.keyCatchers at 0x82809ADC (CL_SetKeyCatchers = sub_821BBED8,
called with 16 by the pause menu's open sub_822CE670, which also sets cl_paused 1; cl_paused dvar ptr 0x82770100 /
0x8337417C). Logs the first 24 key-catcher values ("menu keys: key catchers 0x..."): check main menu, Press Start,
pause menu and the phone (the phone may not use the UI catcher). `--menu_esc_back` (on). Backspace unbound (B = C).
**Back on Esc (24 Nov, 16:36, user):** the B prompt picture (btn_back: menus' back, phone exit, the "Hold B ... stay
low" tutorial) shows ESC (km_esc_sq); the Press Start screen counts as a menu too (menu_quit.cpp OnTitleScreen), so
Esc is B there and holding it quits; title hint "ENTER SELECT / ESC QUIT (HOLD)". In game B stays on C.

**Tutorial pictures per command (24 Nov, 17:10), written, not built:** the hints are localized strings with command
tokens ([[{move}]], [[{+dash}]], [[{+stance}]], [[{+cover}]], [[{+usereload}]], [[{weapnext}]], [[{+speed}]],
[[{fullmap}]], [[{+melee_qk}]]: whites_estate in common.ff, PLATFORM_TUTORIAL_* in code_post_gfx.ff). sub_823D5480
turns each into `<img src="NAME" width="42" height="42" vspace="-20">`; NAME from sub_823D4580(movie, command): "move"/
"look" -> stick_left/right, else the bound pad button via the table at 0x8261CCF0 (wide names: 1 btn_accept, 2
btn_back, 3 btn_refresh, 4 btn_y, 5 bumper_left, 6 bumper_right, 15 button_back, 16 stick_left, 17 stick_right, 18
trigger_left, 19 trigger_right, 20 btn_nav_up, 21 btn_nav_down). Library: scaleform/CmSharedPlatform.gfx (also
btn_left, btn_right, btn_navigate_h/v/all). Texture hash = FNV-1a 64 of the 16 KiB tiled guest texture (64x64 DXT5,
pitch 4); ui.ff's copy isn't byte-identical (couldn't hash offline). `src/tutorial_keys.cpp` hooks sub_823D4580: in
keyboard mode +dash -> btn_left, +stance -> btn_right, +cover -> btn_navigate_h (menus A then Enter) (wide names in SystemHeapAlloc memory); logs each command once.
Next: a texture dump (BOTS_GFX_DUMP_TEXTURES) of the sprint and stay-low hints to get btn_left/btn_right's hashes,
then `replace` lines (placeholders in keyboard.txt) -> km_ctrl_sq / km_c.
**User's fixes (24 Nov, 16:57, screenshots from the build before the hook):** stay-low hint C + S (hook: C once
btn_right's hash is known), reload hint R (tutorial X picture btn_refresh 82C5 -> km_r; HUD X glyphs stay F), phone
EXIT Tab (not Esc), sprint hint Ctrl (hook). Phone: qos_144 shows the phone sets the UI key catcher (0x10) like menus,
so `menu_keys.cpp` tells it apart by hud_phoneObjectives (ptr 0x827CBC08), hud_phoneMap (0x827CBC10),
r_phoneRenderMode (0x84055D08), logged on change ("menu keys: phone ..."; check the cameras and data screens). Phone up
+ keyboard -> pad_prompts_active 3; native 0098 (revised: build_native) reads `prompts/keyboard_phone.txt` first, then
keyboard.txt: btn_back -> km_tab on the phone.
**Phone detection, second try (18:50):** qos_149 (cameras screen up) had hud_phoneObjectives, hud_phoneMap and
r_phoneRenderMode all 0, so EXIT still showed ESC. togglephone (sub_821AB7B0) refuses while cl_paused is set (the
pause menu sets it; the phone doesn't pause), so `menu_keys.cpp` now says phone = UI catcher and (in a level, i.e.
clientUIActive.cgameInitialized byte 0x82809ADA, and not paused, or any of the old dvars). Log line "menu keys:
phone true|false (... paused N clientUIActive a b c d)": check main menu (should be false), pause menu (false), phone
(true). sub_823011A8 (the client's "phonemode N") is an empty stub.

**Hint rewording + more library pictures (24 Nov, 18:20, user's list 05-10), written, not built:** `tutorial_keys.cpp`
also hooks sub_823D5480 (hint text -> Scaleform HTML) and, only inside it, sub_822C7298 (SEH_LocalizeTextMessage,
returns the localized text): in keyboard mode it rewrites sprint ("Holding [[{+dash}]] while pressing [[{kbm_w}]]
to sprint"), weapons ("Press [[{weapnext}]] / [[{kbm_2}]] ..."), fire mode (plain text "Press up on the D-Pad..." ->
[[{nextfiretype}]]) and silencer (Opera: "Press Down on the D-Pad to attach/remove" -> [[{togglesilencer}]]); copies
in SystemHeapAlloc memory, logged. Pictures: +dash btn_left (Ctrl), +stance btn_right (C), +cover btn_navigate_h (E),
kbm_w btn_navigate_all (W), kbm_2 btn_y (2), nextfiretype btn_nav_up (X), togglesilencer btn_nav_down (4).
btn_navigate_v not used (the phone's cameras screen shows it). In the user's 17:31-18:09 screenshots btn_left and
btn_right both drew the same arrows icon (maybe one texture) and btn_navigate_h a D-pad left/right icon. Next: the
dump run (BOTS_GFX_DUMP_TEXTURES=texdump6), find the seven 64x64 hashes, fill keyboard.txt's placeholder lines.

**18:35 update:** built and seen (user's screenshots): all rewording works. The arrow pictures differ by which arrow
is lit (btn_left: left; btn_navigate_all: all; btn_nav_up: up), so they're separate pictures. btn_y is not spare (the
Y button's own picture, Space on keyboard): kbm_2 (weapons "2") now uses btn_navigate_v, and keyboard_phone.txt gives
btn_navigate_v a W/S picture (km_ws; user: W and S scroll the cameras) on the phone's cameras screen. `--tutorial_keys_test=true` makes
every hint show the seven pictures 1-7 (btn_left, btn_right, btn_navigate_h, btn_navigate_v, btn_navigate_all,
btn_nav_up, btn_nav_down)
for the dump run. **Hashes found (texdump6, 19:05):** btn_left 02836603EC40E4E1 (Ctrl), btn_right 63408C2EDB77A49B (C),
btn_navigate_h C9B83BD6DF60188B (E), btn_navigate_v 23A5DEDB971F4898 (2; phone W/S, user: W and S scroll the
cameras), btn_navigate_all FAEF750EA28E92AC (W), btn_nav_up 210B1F6745121A4F (X), btn_nav_down BA4F5117256B7585 (4).
All in keyboard.txt; not built/tested. 23A5 also showed in texdump4 during tutorials: watch for a hint showing 2.
**Works (user, 19:27; qos_151):** all hints show their keys. Phone detection works: "phone true" only with the
phone up (pause menu: paused 1 -> false; main menu: cgameInitialized 0 -> false), keyboard_phone.txt used there.
**Silencer hint (19:28, user screenshot, Opera):** not reworded: that long box ("An Elite squad will deploy ... Press
Down on the D-Pad to attach/remove your weapon's silencer.") never reaches sub_823D5480. Its text field holds "@KEY"
and the Scaleform translator sub_823D3A08 (r4 result string, r5 wide key; calls sub_822C7298, UTF-8 -> wide
sub_822C7AE8, sets the result with sub_8245AAD0) localizes it. tutorial_keys.cpp now hooks sub_823D3A08 too and, in
keyboard mode, swaps "Press Down/Up on the D-Pad" for "Press <img src=btn_nav_down|up ...>" (tutorial_keys_html,
default on) or "Press 4 / X" (off). Unknown whether that text field takes HTML: if the raw tag shows, default the cvar
off. Log "tutorial keys (box): ...". **Works (qos_152, 19:32):** reworded to the img tag and btn_nav_down drawn as 4.
**Balance beam hint (19:40-19:44):** has no button picture at all (plain text, no log line). Options put to the user:
keyboard-only rewording (pads keep the original text) or pictures for every pad style (stick picture; keyboard A/D
needs a spare picture, all seven used). **User: leave it, keyboard hints done for now; may revisit later.** **Dropped (user, 28 Nov 22:16).**

## To do, 24 Nov (user, 23:20)

Test like the launcher: `--vsync=false` (the 23 Nov dev run had vsync on: 18 ms frames became 33 ms, so the intro
dipped to 50 FPS; the 0.1.1 launcher run, vsync off, had 1 frame > 25 ms at the same level start).
1. **Draw CPU profile — done (24 Nov 20:05, qos_157, 4K render 2x, recorder on, profiler on):** busiest windows
   5,200-6,066 draws, 8.2-9.0 ms in draws, frames held at the 60 cap (16.7 ms wall, CP idle 5.2-6.2 ms), >20 ms 7-31
   of 600. Phases at 6,066 draws: tracker 1.39, textures 1.22, descriptors 1.20, setup 1.19, constants 0.89, record
   0.64, indices 0.51, pipeline 0.47, shaders 0.34, targets 0.34, pass 0.24, ring 0.12 (~1.48 us/draw profiled);
   587 texture reloads per 600 frames; memory sync 50M requests, 2.5M took the lock. Spot was lighter than 22 Nov's
   11k. Biggest: tracker, then textures/descriptors/setup (all close). Was:  `BOTS_DRAW_PROFILE=1 BOTS_GFX_RECORD_THREAD=1 --vsync=false --frame_rate=60` at a busy spot
   (the 22 Nov one, ~11,000 draws a frame). Pick the biggest phase from it. (The "airport level" FPS dip mentioned on
   23 Nov was another game, not 007: user, 23:20.)
2. **Level-start stall:** `BOTS_HITCH_MS=20` from a checkpoint load; CP idle (game) vs textures (renderer). Worst frame
   500 ms in the dev build vs ~130 ms in 0.1.1: A/B the dev build without `--mnk_mode=true --mnk_mouse=true` (0098's
   prompt lookups) to see whether tonight's code adds to it.
3. **Faster loads — written 24 Nov 20:15, not built:** `src/native_inflate.cpp` + `thirdparty/miniz` (miniz 2c50092,
   MIT, inflate only, its .c built as C++ without the PCH). The game's zlib 1.1.4 copy: inflateInit_ sub_8230BA70
   (-> inflateInit2_ sub_8230B930, wbits 15), inflate sub_8230BA80, inflateReset sub_8230B818, inflateEnd sub_8230B898;
   users: fastfile reader sub_821C1E10 (Z_SYNC_FLUSH; init 0x821C217C, end 0x821C1FF8) and Scaleform (0x82470130,
   0x824A99E4 ...). Hooks keep the guest init/end, add a host mz_stream per guest z_stream, and run mz_inflate on guest
   memory, writing next_in/avail_in/total_in/next_out/avail_out/total_out/adler back (BE). Cloud test: ui.ff streamed
   in 256 KB/1 MB chunks through the same wrapper = byte-identical to Python zlib, 71 MB in 321 ms. Log "inflate: ...
   native" per stream >= 1 MB; `--native_inflate=false` logs the recompiled time ("recompiled") for A/B. The
   multiplayer inflate-patch try was removed from no_multiplayer.cpp (same hook).
   **Test (20:12-20:17):** recompiled times (qos_158, native off): 5.0 MB 47 ms, 54.9 MB 656 ms, 68.1 MB 655 ms
   (84-107 MB/s), so ~1.3 s of a level load. That run then crashed in the menus (crash-15880: sub_823DFC58 reading
   0x30, chain 823E4DF8 <- 823E5578 <- 823E55E0 <- 822D8D90 <- 822D90C8 <- 822CF050 <- 821C0060; Scaleform/UI;
   probably unrelated, first seen). Native on: stalls before the first frame, twice: "Win32FileHandle::Read(...,
   0x1282af8c2, 0x20000) returned ERROR_NOACCESS. Read-only memory?" on the first fastfile read, no frames, display
   standby. **native_inflate now defaults off.** To find: what 0x282AF8C2 is (read destination, unaligned) and why
   its page is read-only only in native mode (guest zlib state never advanced? DB reader at 0x828A1870 uses next_in
   == ring end (r31+96) -> ring start (r31+92) wrap; reads issued by sub_821C1C40 / sub_821BF390).
   Was:  hook the game's fastfile inflate (recompiled PPC zlib) and use host zlib (a whites_estate load
   ~5 s, 23:13:19.7-23:13:25.0 in 0.1.1's game.log). Find the inflate routine first (zlib strings / the fastfile
   loader).
4. **Unchanged-texture check by written pages:** hash only the 4 KB pages the tracker saw written (now XXH3 of the
   whole texture, 0095). Helps the level-start burst (~2,100 checks in 10 s) and steady play (~600 reloads a minute).
5. **Memory tracking / uploads (24 Nov 21:30-21:36):** the "tracker" phase (1.4-1.8 ms) is not the lock-free page
   check (that's cheap) but ~235 real re-uploads a frame of vertex data the game rewrites each frame (memory sync:
   ~5% of requests take the SDK lock, nearly all of those upload). Each upload (Renderer::UploadGuestMemory) takes the
   global lock, copies, re-protects the pages and calls EndRendering("upload"): it splits the render pass. A/B with
   BOTS_DRAW_PROFILE=1 + 3 threads: qos_166 copies (passes ~300-340/frame at 5-6k draws, tracker 1.6-1.8 ms) vs
   qos_167 BOTS_GFX_ZERO_COPY=1 ("the GPU reads guest memory in place"): **25 render passes/frame at 8,900 draws**,
   tracker still ~1.3 ms (lock/protect remain), but **the whole screen glitched** (user): the game rewrites dynamic
   vertex memory while the GPU, running further behind than on the 360, still reads it; copies are needed. Next idea
   (not started, 1-2 sessions): hoist each upload to before the current render pass when no earlier draw of that pass
   read those pages (copy the data now, record the copy ahead of the pass), so uploads stop splitting passes.
   Earlier ideas:
   **Draw CPU ideas** (from the profile): tracker memo by page stamps / lock-free "pages unwritten" fast path
   (60M memory sync requests, 3.7M took the lock, 23 Nov), descriptor reuse or push descriptors, setup from register
   dirty bits, constants by changed ranges.
6. **Third render thread — written 24 Nov 20:55, not built:** native patch `0099-texture-worker.patch`
   (`src/gfx/vk/texture_worker.h`): BOTS_GFX_WORKER=1 starts one worker thread; in LoadTexture each level of >= 64 KB
   guest data has its block rows (all layers/slices back to back) split in two: the render thread copies/untiles/
   byte-swaps and BC-decodes/shuffles rows [0, mid), the worker [mid, rows), at once; the render thread waits, then
   records the upload as before. Textures log line adds "N levels copied with the texture worker". Checked: applies
   after 0001-0098 (cloud Skullburglar tree /home/claude/sk/base, commits per patch), texture.cpp/draw.cpp syntax-check
   against the cloud SDK apart from its known ext_EXT_external_memory_host mismatch. Launcher: Graphics "Render
   threads" dropdown 1 (original) / 2 / 3 (user: 2 = 1+2, 3 = all three): BOTS_GFX_RECORD_THREAD 0/1/1,
   BOTS_GFX_WORKER 0/0/1; default 2; old render_thread checkbox migrates. SDK bundle name qos5. Not yet moved: the
   unchanged-texture hash checks (they run at the draw that finds the texture dirty; an early hash could miss later
   writes). Test: build_native + game build; A/B a level start and a streaming walk with BOTS_GFX_WORKER=1 vs 0,
   BOTS_HITCH_MS=20. **First test (21:13, qos_165, worker on; compared with qos_164, 21:03, same settings but the
   renderer from before 0099, 2 threads):** worker log line present; it copied ~600-690 levels per 10 s (mostly the
   one per-frame rewritten texture) and 435-685 in the level-load windows. Texture time per frame 0.46 -> 0.30 ms;
   gameplay windows' frames > 20 ms per 10 s: 7-26 (avg ~16) -> 1-23 (avg ~9) (different walks, so rough). Level-
   start stalls unchanged (game side: CP idle 50-98 ms; a 127-129 ms frame in both). Startup frame 1 has one 75-90 ms
   texture (1x1 format 6 tiled) in both. No 2-thread run on the new DLL yet. User: no visual issues (21:23). Launcher
   default stays 2 for now.
   Earlier plan:  (user, 23:47: worth trying despite the risk). Off by default, one switch per duty, e.g.
   `BOTS_GFX_WORKER=textures,hash` (duties: `hash` unchanged-texture checks, `textures` loads/untiling/decompression
   and per-frame reloads, `uploads` vertex/constant copies, `parse` PM4 + register writes, maybe its own thread, i.e.
   a 4th). **Agreed start (user, 23:48): `hash` + `textures`** (the dips are texture-write/streaming bursts; the worker
   runs beside the render thread, which waits for a texture that isn't ready, so the worst case is no gain, not wrong
   pictures; also takes the menu video's per-frame upload). `parse` later (more risk, mainly for slower CPUs). Measure a
   level start and a streaming walk with the worker on/off, vsync off.
   Duties, chosen by the profile: (a) texture loading/untiling/hashing on a worker that stays ahead
   of the draws (most help in bursts); (b) PM4 parsing/register writes on their own thread (<= 2.4 ms a frame, a clean
   split like 0093's recorder). Draw setup itself doesn't split well (each draw depends on the last).
7. **Menu video frames:** 0.1.1 menu had 109 frames > 20 ms in 10 s at 19 draws a frame: the game's Bink decode
   (game side) plus a 900 KB texture upload each frame; the upload side could be cheaper. Low priority.
9. **Disable MULTIPLAYER on the main menu** (user, 23:23): choosing it closes the game (multiplayer is parked).
   Grey it out or hide it, or make selecting it do nothing. Ways in: the main menu is Scaleform (ui.ff .gfx files,
   zlib SWF "CFX"), so either hook the menu action that starts multiplayer (find what selecting it calls: likely a
   command string such as the MP fastfile / "xstartprivateparty"-style entry; search the decrypted XEX and ui.ff
   for "multiplayer") and swallow it, or patch the menu data. Check what the crash/close is first (log, crash-*.txt).
   **Found (24 Nov):** multiplayer is a separate executable, `game/default_mp.xex` (6.3 MB; mp_*.ff and level_mp/ beside
   it). sub_821BCD08 shuts systems down (sub_8229BE60, sub_8256F7B8, sub_825CE62C, sub_822A1290) and then calls
   sub_82423D78("default_mp.xex", string at 0x8201D510): the title relaunch (XamLoaderLaunchTitle path). ReXGlue runs
   only the recompiled default.xex, so the relaunch ends the process: that's the "close". Fix: hook sub_821BCD08 and
   return before it shuts anything down (the menu should stay as it is; check it doesn't sit in a loading state), or
   hide the menu entry. Playing MP would need default_mp.xex as its own recompilation project (parked).
   **Done (24 Nov 19:50), not built/tested:** sub_821BCD08 is the console command "startMultiplayer" (Cmd_AddCommand
   sub_82299178 at 0x821BE8D4, name 0x8201D714). `src/no_multiplayer.cpp` hooks it and returns before the shutdown;
   log "multiplayer: startMultiplayer ignored"; `--allow_multiplayer=true` calls the original. Check: MULTIPLAYER on
   the main menu does nothing and the menu still works (no stuck fade/loading).
   **Test 1 (19:47):** stuck on LOADING...: the MULTIPLAYER item (CmMainMenu.gfx, game/scaleform) opens screen
   "splaunchmp", a loading screen that runs exec "startMultiplayer". Now the hook queues `--multiplayer_fallback`
   (default "disconnect", the pause menu's quit command) with Cbuf_AddText sub_82298750(0, text) to leave it. Not
   built. If disconnect does nothing on the front end, other ideas: hide the item (its visibleWhen dvar test) or patch
   the SWF's "splaunchmp" target when the movie loads.
   **Test 2 (19:51, qos_155):** hook ran, "disconnect" did nothing (still LOADING...). **Now (not built):** the item is
   greyed out instead: the menu movies (game/scaleform/*.gfx, CFX = zlib SWF; CmMainMenu, CmFrontEnd, SPMainMenu,
   Sp_ig_PmMain share the menu definitions) build it as `menus.splaunchmp = new frontend.datatypes.LoadingScreen(
   "@UI_MULTIPLAYER", r1.NORMAL, 'exec "startMultiplayer";')`; no_multiplayer.cpp hooks the two zlib inflate copies
   (sub_8230BA80 Scaleform's, sub_82408B78) and in their output changes that push's constant 20 "NORMAL" to 23
   "FLAG_INACTIVE" (pattern 96 0a 00 09 n 01 09 n+1 01 04 01 08 14 4e 96 0a 00 09 n+2 01 07 03 00 00 00, n 0x35/0x36/
   0x39). Log "multiplayer: menu item greyed out ...". `--multiplayer_menu_inactive=false` off; multiplayer_fallback
   now empty by default. Tools: cloud /home/claude/tools2/swfas.py (AS1 action/constant-pool reader).
   **Test 3 (19:55, qos_156):** patch applied (5 movies) but FLAG_INACTIVE is a *screen* flag (credits, popups use
   it), not "item disabled": MULTIPLAYER still opened splaunchmp, now black. multiplayer_menu_inactive now defaults
   off. **User wants MULTIPLAYER visible but Accept doing nothing (19:57).** Findings so far: LoadingScreen
   (DoInitAction tag 0xf828 in CmMainMenu) = MenuItem subclass, ctor (title, flags, command) adds LOADING_SCREEN to
   _screenFlags, movie "cmloading"; its onEnterScreen calls as.frontend.<root>.shutdown() and then
   getURL("FSCommand:run_game_script", command), so the whole front end is gone before the game sees the command:
   the "does nothing" has to happen in the menu's ActionScript (stop the click on that item / never enter the screen).
   Main menu array: menus.cmmainmenu built with [menus.spmainmenu, menus.spmainmenu_savenotreadable,
   menus.splaunchmp] (bytes at CmMainMenu 0xd996). Game side: sub_823D2098 picks the start screen (cmmainmenu /
   cmpressstart ...) when the front end starts (called from sub_823D2740); "as.frontend.GlobalFunctions.JumpToScreen"
   is invoked by the game at 0x823D2490. Key input reaches AS as Key codes (KeyHandler class, tag 0x11f07; list
   controllers handle 38/40/37/39/8; Enter 13 in other classes). Disassembly dumps: cloud /tmp/all.txt (not kept).
   Next: find which controller handles Enter on the main menu list and how it opens the item's screen; then patch that
   item (e.g. make its click a no-op).
   **Parked (user, 20:01: "move on").** **Reverted (user, 20:22):** src/no_multiplayer.cpp removed, so MULTIPLAYER
   closes the game again, as before (code in git history, last at commit c0b0a76).

**Launcher: input device (24 Nov 20:25, not tested):** Controls tab "Input device" dropdown: Auto, Keyboard and mouse,
Xbox controller, PlayStation controller (user). Auto = controllers only (prompts follow the connected pad; keyboard
and mouse off, user 20:26); Keyboard and mouse = --pad_prompts=keyboard, mnk on;
Xbox / PlayStation = --pad_prompts=xbox|playstation and --mnk_mode=false --mnk_mouse=false. Controller settings (dead
zone, vibration) shown for Auto/Xbox/PlayStation, keyboard settings (binds note, sensitivity, invert) only for Keyboard
and mouse (two frames, grid_remove). Replaces "Button prompts" and the "Keyboard and mouse" checkbox; old settings'
pad_prompts migrate. New strings translated in launcher_text.py.
**Works (user, 20:42)** after a reinstall: the first launcher test ran the old 0.1.1 install (built 23 Nov 23:12,
no keyboard binds yet), and the reinstall first failed because build_installer.bat didn't copy thirdparty\ (miniz):
fixed (d12282d; miniz licence in licenses\). The launcher only offers "Reinstall now" when GAME_VERSION changes
(still 0.1.1): use Game files -> Reinstall to test new builds, or raise the version for a release.
**Control maps (21:20, user):** Controls shows a "Controller map" (PAD_MAP: buttons_default.cfg layout, Xbox names,
PlayStation names when PlayStation controller is picked) under the controller settings and a "Keyboard and mouse map"
(KBM_MAP: kbm_binds.cpp) under the keyboard settings, each two columns of control / action; the mouse sensitivity
slider (10-300 %) is in the keyboard section. The keyboard map reads "W A S D: Move / Takedowns / Lockpicking" (user,
21:24) and "A / D: Balancing" (21:17). Display only, no remapping. Strings translated in launcher_text.py.

**Parked: multiplayer (user, 24 Nov 06:57), after single player is done.**
1. Read Skullburglar's SDK patch `0018-netplay-system-link` (patches/rexglue-sdk in the Skullburglar repo, base
   67aacb28): what it implements (XNet addressing, System Link broadcast between PCs, XSession) and how complete it is.
2. Check 007's System Link needs against it (QoS 360 MP: up to 12 players, online or System Link; no split-screen or
   bots as far as known). Reference: Xenia Canary Netplay (AdrianCassar), Xenia-WebServices.
3. If it fits: port `game/default_mp.xex` as a second recompilation project (same engine: reuse the codegen setup,
   jump-table/setjmp fixes, renderer, input, frame-rate hooks), make the two executables switch (SP's
   sub_821BCD08 -> launch MP; MP's way back), then test LAN / VPN (e.g. Tailscale) play between two PCs.
- **What default_mp.xex holds (24 Nov, decrypted in the cloud with xexdec.py, strings only):** CoD4's MP stack.
  Listen server (one player hosts; "Listen server cannot sound restart"), plus a `dedicated` dvar ("True if this is a
  dedicated server", probably unused on 360). sv_* (maxclients, fps, maxRate, maxPing, mapRotation, hostname,
  privateClients), ~40 party_* lobby dvars (host, min/max players, gametype, map, timers, team balance, map veto), host
  migration (PartyMigrate_*, XSessionMigrateHost). Modes: systemlink, onlinegame, xblive_privatematch,
  xblive_rankedmatch. Uses XNetStartup, XNetXnAddrToInAddr, XNetServerToInAddr, XSession*, XOnlineGetServiceInfo.
  Test bots: `addtestclient`, sv_botsPressAttackBtn (a way to test a match on one PC). **No split-screen**: the MP
  menus (scaleform MpMainMenu: Xbox Live and System Link; MpSys* System Link create/join/map/mode/options; MpXb*
  Live lobbies, private match, playlists) have none, and menus run `nosplitscreen`; the split-screen code is CoD4
  leftover. **Target: System Link** (no servers; LAN or a VPN); Live would need a matchmaking stand-in.
10. **Gameplay dips in easy areas** (user, 23:45): in qos_142 the renderer was mostly idle during them (22:38:08-18:
    5 ms in draws, CP idle 11.5 ms, 79 textures streamed in, a takedown), so the game thread was late; vsync on turned
    each ~18 ms frame into 33 ms. Capture a dip with Tracy (built in: TRACY_ON_DEMAND, connect the Tracy app) to see
    the game thread's work (streaming, sound, AI, scripts); if it's decompression it shares item 3's native zlib.
    Launcher V-sync default now off (user, 24 Nov 21:27).
8. **Shared prompt pictures** (keyboard prompts, open question above): LS picture (S vs Shift), B picture (BACK / Tab
   / Shift); options: one key, a two-key picture, or pick per hint by hooking the tutorial hint code.

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
