# Launcher and installer

`QuantumOfSolace.exe` is a single program for players:

- **Not installed yet:** shows the installer. The player picks their own Xbox 360 disc image and it builds the game
  on their PC: reads the ISO, checks it's 007: Quantum of Solace (title ID 415607FF), finds or installs the C++ build
  tools, unpacks the ReXGlue SDK packed into it (with the native renderer), recompiles and builds `qos.exe`, and
  creates a desktop shortcut to the launcher.
- **Installed:** the settings screen, **Save settings** and **Play**.

Nothing from the game ships with it. Running the installer again resumes where it stopped.

## Settings

| Tab | Setting | How it's applied |
|---|---|---|
| Display | Fullscreen, output resolution | `--fullscreen`, `--resolution` (or `--video_mode_*` and `--window_*` windowed) |
| Display | Render resolution | `BOTS_GFX_RENDER_SCALE` 1 / 2 / 3 (the game draws 1280×720) |
| Display | Anti-aliasing | `BOTS_GFX_FXAA` 1 / 2 (native renderer patch 0010) |
| Display | Upscaling: bilinear, AMD FSR 1, AMD CAS | `--present_effect=fsr` / `cas` (the runtime presenter's FidelityFX EASU + RCAS, or CAS) |
| Display | Frame rate 30 / 40 / 50 / 60 | `--frame_rate` (`src/fps_unlock.cpp`) |
| Display | V-sync | `--vsync` |
| Display | Field of view 65-110 | `--fov` (`src/fov.cpp`) |
| Display | FPS counter | `--show_fps`, `--show_fps_detail` (drawn by the native renderer) |
| Display | Hide achievement pop-ups | `--achievement_popups=false` |
| Graphics | Brightness | `--brightness` (native renderer's last pass) |
| Graphics | Texture filtering | `BOTS_GFX_FORCE_ANISO` |
| Controls | Input device (Auto, Keyboard and mouse, Xbox controller, PlayStation controller) | `--pad_prompts=auto|keyboard|xbox|playstation` (`src/pad_prompts.cpp`, native patches 0014, 0021, 0022, 0098); keyboard and mouse (`--mnk_mode`, `--mnk_mouse`) on only for Keyboard and mouse. Only the chosen device's settings are shown |
| Controls | Mouse sensitivity, Invert mouse (keyboard and mouse only) | `--mouse_sensitivity`, `--mouse_invert` (`src/mouse_look.cpp`) |
| Controls | Extra stick dead zone | `--left_stick_deadzone_percentage`, `--right_stick_deadzone_percentage` (the SDK's) |
| Controls | Vibration | `--vibration=false` when off |
| Multiplayer | Gamertag; XUID (shown, read-only); Play multiplayer | starts `qosmp.exe` (beside qos.exe; not in the installer yet) with `--user_gamertag` (letters, digits, spaces, 15 at most) and `--user_xuid` (an offline XUID `E000...` made once and kept in settings.json, so two PCs differ: SDK patch 0020), plus game_args without the settings qosmp.exe doesn't have yet (button prompts, mouse look, achievement pop-ups): display, graphics, frame rate and FOV (`mp/src/frame_rate_fov.cpp`), render threads and controller settings are the same as single player's. Clear and rebuild clears both shader caches (`Documents\qos`, `Documents\qosmp`); log `game_mp.log` |
| Game files | Game language | `--user_language` (English, French: the disc's two) |
| Game files | Shader cache, Clear and rebuild | deletes `Documents\qos\cache\native` (native patch 0027 builds it again at the next start) |

The native renderer is the only one offered; `BOTS_GFX_NESTED_DRAWS=1` is always set (qos.exe sets it too). Settings
live in `%LOCALAPPDATA%\QuantumOfSolacePC\settings.json`, the install location in `launcher.json` beside it.

## Building

Needs Python 3 on PATH, and the SDK built with `..\tools\build_native.bat` (into 007's own `C:\rexglue-src-007`,
so the Bourne and Eat Lead builds in `C:\rexglue-src` don't change it). From this folder:

```
build_installer.bat
```

It gathers the project files into `payload\` (with `hints\`, `prompts\` and `pipelines\`), zips the SDK into it,
installs PyInstaller and writes `dist\QuantumOfSolace.exe`. Change `SDK_BUNDLE` in the script when the SDK or the
renderer patches change, and `VERSION` / `GAME_VERSION` in `qos_launcher.py` (with `QOS_RELEASE` in
`CMakeLists.txt`) for a release.

To run without packaging: `python qos_launcher.py` (after `build_installer.bat` once, so `payload\` exists).

## Files

- `qos_launcher.py`: entry point, settings screen, starts the game.
- `qos_installer.py`: install logic and install screen.
- `launcher_text.py`: the launcher's text in German, French, Spanish and Italian (from the Bourne port; strings new
  here show in English until translated).
- `xex_achievements.py`: reads the achievement list from default.xex for the Achievements tab before the first start.
- `icon.ico`: our own icon (no game art).

Adapted from the Eat Lead port's launcher (thegh0sts/EAT-LEAD-X360-PORT), itself from the Bourne Conspiracy port's.
