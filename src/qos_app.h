// qos - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

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

class QosApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<QosApp>(new QosApp(ctx, "qos",
        PPCImageConfig));
  }

  void OnPostInitLogging() override { qos::crash::Install(); }

  std::unique_ptr<rex::ui::AchievementNotificationDialog> CreateAchievementNotificationDialog()
      override {
    if (!REXCVAR_GET(achievement_popups)) {
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
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    if (config.gpu_plugin == "xenos-vulkan") {
      config.graphics = rex::system::LoadGpuPlugin("xenos", "vulkan");
    }
  }

  // Other hooks available for customization:
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
