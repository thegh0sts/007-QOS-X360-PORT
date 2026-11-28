# 007: Quantum of Solace — Xbox 360 recompilation

A native Windows port of *007: Quantum of Solace* (Treyarch, 2008; Xbox 360 version, title ID `415607FF`), made by
statically recompiling the game's PowerPC code to C++ with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)
and drawing it with [Skullburglar](https://github.com/nlwstein/skullburglar)'s native Vulkan renderer.

**No game files are included.** You need your own copy of the Xbox 360 disc. Everything here is our own code, patches,
tools and art.

**Status: in progress.** The game runs from the title screen through the levels tested so far, at up to 60 FPS.
Expect bugs; see [HANDOFF.md](HANDOFF.md) for the detailed state of the work.

## Features

- **Native renderer** with render scales up to 3× (2560×1440 or 3840×2160 from the game's 1280×720), FXAA,
  forced anisotropic filtering, and AMD FidelityFX **FSR 1** or **CAS** for the final scale to the screen.
- **Made for 60 FPS** (the game is 30 FPS): see [Playing at 60 FPS](#playing-at-60-fps) below.
- **Field of view** from 65 (the original) to 110.
- **PlayStation button prompts** when a DualShock or DualSense is connected: menus, HUD, tutorials and takedowns.
- **Shaders built at first start** from the game's own files (a few seconds), so levels don't pop in on their first
  visit.
- **"B QUIT (HOLD)"** on the title screen, an FPS counter, brightness, extra stick dead zone, vibration on/off,
  achievements with pop-ups, save backups.
- **Launcher and installer** (`installer/`): pick your disc image and it builds the game on your PC; then settings and
  Play.

Not done yet: keyboard and mouse (the key pictures are made, the input isn't wired up), FSR 2/3.

## For players

Build the launcher (see [installer/README.md](installer/README.md)), or use a release when there is one. Run
`QuantumOfSolace.exe`, choose your Xbox 360 ISO of the game, and it does the rest: reads the disc, installs the C++
build tools if needed, recompiles and builds the game, and makes a desktop shortcut. After that it opens the settings
screen with **Play**.

Requirements: 64-bit Windows, a GPU with up-to-date Vulkan drivers, and a few GB of disk for the build tools and the
game.

**Multiplayer (System Link) and the map pack:** see [docs/multiplayer.md](docs/multiplayer.md).

### Playing at 60 FPS

The aim of this port is a 60 FPS game, not just the 30 FPS original running on a PC. The game was built and tuned for
30 FPS, and some of its code assumes it: running it faster can break things that have nothing to do with graphics.
We fix those as they're found, so they play at 60 the way they played at 30. Examples so far:

- **Balance beams:** the wobble grew far faster at 60 FPS (uncontrollable with a controller); it now builds up as at 30.
- **Train car jumps (mission 10):** at 60 FPS the mantle onto the next car never triggered, so Bond fell every time
  and the mission couldn't be finished (a softlock); wall runs and mantles now move as at 30.
- **Weapon detail at wide fields of view** and cutscene camera fixes found on the way.

The whole single-player campaign has been played through at 60 FPS. 90 and 120 FPS are in the launcher too, but
they're experimental: less tested, and busy scenes can drop below them. If something behaves differently from the
original at 60 FPS (a jump you can't make, a timing that feels off, a stuck script), please report it with where it
happened; meanwhile, setting the frame rate back to 30 in the launcher plays it as the original did.

## For developers

### What you need

- Windows with a recent Visual Studio (C++ desktop workload with clang, CMake and Ninja), git and Python 3.
- The game's disc contents extracted into `game/` (with `default.xex` and the `.ff` fastfiles). `game/` is ignored by
  git.

### Building

From an *x64 Native Tools Command Prompt for VS*:

```bat
rem 1. The ReXGlue SDK with our patches and the native renderer (once, and when a patch changes).
rem    Built in 007's own folder, C:\rexglue-src-007 (takes a while the first time).
tools\build_native.bat

rem 2. The game. Codegen runs as part of the build (generated\default\ from the XEX).
cmake --preset win-amd64-relwithdebinfo -DCMAKE_PREFIX_PATH=C:/rexglue-src-007/rexglue-sdk/out/install/win-amd64
cmake --build out\build\win-amd64-relwithdebinfo

rem 3. Run.
cd out\build\win-amd64-relwithdebinfo
qos.exe --game_data_root=<repo>\game --gpu_plugin native
```

The build copies the renderer (`rexgpu-nativerd.dll`) and the assets (`hints/`, `prompts/`, `pipelines/`) next to
`qos.exe`. Logs go to `logs\qos_NNN.log` in the build folder, crashes to `logs\crash-*.txt/.dmp`.

Useful options: `--frame_rate=60`, `--fov=90`, `--vsync=false`, `--present_effect=fsr`, `--pad_prompts=playstation`,
`--show_fps=true`. Renderer switches are environment variables, e.g. `BOTS_GFX_RENDER_SCALE=2`, `BOTS_GFX_FXAA=1`,
`BOTS_GFX_FORCE_ANISO=16`, `BOTS_GFX_RECORD_THREAD=1`, `BOTS_DRAW_PROFILE=1` (per-frame timings),
`BOTS_HITCH_MS=20` (slow-frame log). The launcher sets these from its settings (`installer/qos_launcher.py`).

### Layout

| Path | What it is |
|---|---|
| `src/` | The game app on the SDK: frame rate and FOV hooks, title-screen quit, PlayStation prompts, crash handler |
| `tools/sdk-patches/` | Our patches to the ReXGlue SDK (codegen fixes, XMA audio, input, shader translator) |
| `native-plugin/patches/` | Our patches to Skullburglar's renderer: 007's draws, shadows, resolves, render scale, prompt textures, shader refs, and the per-draw CPU work |
| `native-patches/` | Debugging patches from the early menu investigation (not applied) |
| `tools/` | SDK build scripts, `ff_shader_refs.py` (shader list from the fastfiles), the art generators |
| `pipelines/` | The shader list built at first start (names where each shader is in the game files; no game data) |
| `prompts/`, `hints/` | Our own button and hint pictures, and the list of game textures they replace |
| `installer/` | The launcher and installer (Python, packed with PyInstaller) |
| `default_functions*.toml`, `qos_manifest.toml` | Codegen configuration |
| `HANDOFF.md` | Working notes: findings, decisions, test results, what's next |

## Credits and licences

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) — the recompiler and runtime (from the Xenia emulator).
- [Skullburglar](https://github.com/nlwstein/skullburglar) — the native Vulkan renderer and SDK performance patches
  (BSD 3-Clause, [licenses/skullburglar-LICENSE.txt](licenses/skullburglar-LICENSE.txt)).
- AMD FidelityFX — FSR 1 and CAS in the presenter.
- The launcher is adapted from this author's *Eat Lead* and *The Bourne Conspiracy* ports.
- Xenia Canary's game patch notes (by Sowa_95) pointed to the frame limiter.

*007: Quantum of Solace* is a trademark of its owners. This project is not affiliated with or endorsed by
Activision, Treyarch, MGM, Danjaq or Microsoft.
