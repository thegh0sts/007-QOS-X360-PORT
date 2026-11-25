// qosmp - the multiplayer executable's app: for now only what single player's app does without game hooks (crash
// reports, the default GPU plugin, the native renderer's nested draws). Single player's hooks (src/) use
// default.xex's function addresses and don't apply here; each gets its own multiplayer version when needed.

#pragma once

#include <cstdlib>
#include <memory>
#include <string>

#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/system/gpu_plugin.h>

#include "crash_win.h"

// Read by the native renderer by name (as in single player's qos_app.h), and passed by the launcher.
REXCVAR_DEFINE_BOOL(show_fps, false, "UI", "Show a small frame rate counter in the corner (native renderer)");
REXCVAR_DEFINE_BOOL(show_fps_detail, false, "UI",
                    "With show_fps: also frame time, GPU, CPU and memory (native renderer)");
REXCVAR_DEFINE_DOUBLE(brightness, 1.0, "GPU", "Picture brightness, native renderer (1 = the game's)")
    .range(0.5, 2.0);

namespace qos::kbm_binds {
void SetDefaults();  // ../src/kbm_binds.cpp: 007's keyboard layout for the SDK's keyboard/mouse pad
}  // namespace qos::kbm_binds

namespace qosmp::relaunch {
void RestoreLaunchData();  // relaunch_log.cpp: a restart's launch data (the Sys_Error message) from the last run
}  // namespace qosmp::relaunch

namespace qosmp::dlc {
void InstallFromFolder();  // dlc_folder.cpp: installs the map packs from --dlc_folder
}  // namespace qosmp::dlc

class QosMpApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<QosMpApp>(new QosMpApp(ctx, "qosmp", PPCImageConfig));
  }

  void OnPostInitLogging() override { qos::crash::Install(); }

  // The runtime loads game:\default.xex (single player) unless told otherwise: the first run started single
  // player's entry point (0x82425F18), which isn't in this recompilation ("No function registered at 82425F18").
  void OnLoadXexImage(std::string& xex_image) override { xex_image = "game:\\default_mp.xex"; }

  // DLC: after the XEX is loaded (the title ID is known) and before the game starts enumerating content.
  void OnPostSetup() override {
    qosmp::relaunch::RestoreLaunchData();
    qosmp::dlc::InstallFromFolder();
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    qos::kbm_binds::SetDefaults();
    if (config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
    if (config.gpu_plugin == "xenos-vulkan") config.graphics = rex::system::LoadGpuPlugin("xenos", "vulkan");
    if (config.gpu_plugin == "native" && !std::getenv("BOTS_GFX_NESTED_DRAWS")) {
#ifdef _WIN32
      _putenv_s("BOTS_GFX_NESTED_DRAWS", "1");
#else
      setenv("BOTS_GFX_NESTED_DRAWS", "1", 0);
#endif
    }
  }
};
