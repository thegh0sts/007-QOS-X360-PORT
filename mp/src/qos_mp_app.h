// qos_mp - the multiplayer executable's app: for now only what single player's app does without game hooks (crash
// reports, the default GPU plugin, the native renderer's nested draws). Single player's hooks (src/) use
// default.xex's function addresses and don't apply here; each gets its own multiplayer version when needed.

#pragma once

#include <cstdlib>
#include <memory>

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

class QosMpApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<QosMpApp>(new QosMpApp(ctx, "qos_mp", PPCImageConfig));
  }

  void OnPostInitLogging() override { qos::crash::Install(); }

  void OnPreSetup(rex::RuntimeConfig& config) override {
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
