// qos - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>
#include <memory>

#include <rex/rex_app.h>

#include <rex/cvar.h>
#include <rex/system/gpu_plugin.h>
#include <rex/ui/overlay/achievement_notification.h>

#include "crash_win.h"

// The achievement popup was off by default while runs crashed with heap corruption (0xC0000374)
// around achievements. The cause was the SDK's input system rebuilding its device list from two
// threads at once (fixed by tools\sdk-patches\0005 in the Bourne project's SDK build); the
// popup polls the controller from its own thread, so it made the crash more likely. On again.
// --achievement_popups=false hides it.
REXCVAR_DEFINE_BOOL(achievement_popups, true, "UI", "Show the achievement unlocked popup");

// Read by the native renderer (native-plugin/patches/0006-fps-counter.patch), which draws the counter into the
// picture itself, so it shows in fullscreen; set by the launcher (installer/qos_launcher.py). As in the Bourne and
// Eat Lead ports.
REXCVAR_DEFINE_BOOL(show_fps, false, "UI", "Show a small frame rate counter in the corner (native renderer)");
REXCVAR_DEFINE_BOOL(show_fps_detail, false, "UI",
                    "With show_fps: also frame time, GPU, CPU and memory (native renderer)");
REXCVAR_DEFINE_DOUBLE(brightness, 1.0, "GPU", "Picture brightness, native renderer (1 = the game's)")
    .range(0.5, 2.0);

namespace qos {
// The log's first lines: the release, the build time and the launch options (version_log.cpp).
void LogVersion();
}  // namespace qos

namespace qos::achievements {
// <user data>/achievements/<title id>-list.json and the icons, for the launcher's Achievements tab
// (achievement_list.cpp, from the Eat Lead port).
void ExportList(rex::Runtime* runtime);
}  // namespace qos::achievements

namespace qos::kbm_binds {
void SetDefaults();
}  // namespace qos::kbm_binds

class QosApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<QosApp>(new QosApp(ctx, "qos",
        PPCImageConfig));
  }

  void OnPostInitLogging() override {
    qos::crash::Install();
    qos::LogVersion();
  }

  void OnPostSetup() override { qos::achievements::ExportList(runtime()); }

  std::unique_ptr<rex::ui::AchievementNotificationDialog> CreateAchievementNotificationDialog()
      override {
    // None with the native renderer either: it draws the popup itself, in the same style
    // (native-plugin patches), so with the runtime's own as well two showed, one over the other
    // (21 Nov).
    if (!REXCVAR_GET(achievement_popups) || native_renderer_) {
      return nullptr;  // the runtime skips the popup when there's none
    }
    return rex::ReXApp::CreateAchievementNotificationDialog();
  }

  // The SDK leaves --gpu_plugin empty, and with no GPU plugin the game runs with a black window
  // (the log says "no GPU emulation loaded"). Default to the one this project builds.
  //
  // --gpu_plugin=xenos-vulkan runs ReXGlue's Xenos renderer on Vulkan instead of Direct3D 12 (it
  // has both when the SDK was built with Vulkan, as tools\build_native.bat in the Bourne project
  // does). For comparing against the native Vulkan renderer: both translate the game's shaders
  // with the same SPIR-V translator, so a picture that's right here and wrong there points at
  // the native renderer itself.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    qos::kbm_binds::SetDefaults();  // keyboard/mouse binds for 007 (src/kbm_binds.cpp)
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    if (config.gpu_plugin == "xenos-vulkan") {
      config.graphics = rex::system::LoadGpuPlugin("xenos", "vulkan");
    }
    native_renderer_ = config.gpu_plugin == "native";
    // The native renderer needs nested draws for this game's menus and text (native patch 0001); set here, so a
    // start without the launcher (Play.bat, a build started by hand) has it too. A value already set is kept.
    if (native_renderer_ && !std::getenv("BOTS_GFX_NESTED_DRAWS")) {
#ifdef _WIN32
      _putenv_s("BOTS_GFX_NESTED_DRAWS", "1");
#else
      setenv("BOTS_GFX_NESTED_DRAWS", "1", 0);
#endif
    }
  }

 private:
  bool native_renderer_ = false;

 public:

  // Other hooks available for customization:
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
