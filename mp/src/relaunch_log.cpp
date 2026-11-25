// qosmp - log why the game relaunches itself.
//
// Sys_Error (sub_822E14E0, MP's; "===Restarting for Sys_Error===") formats the error into a 984-byte block at
// 0x84E8D7D0 (+4: the message, up to 900 bytes), hands it on as launch data (XamLoaderSetLaunchData, import stub
// 0x8252516C) and relaunches default_mp.xex (sub_82372B60(name, flags), the title launch; also used with
// "default.xex" by sub_821C5598, multiplayer's way back to single player). ReXGlue doesn't relaunch: the process just
// ends ("Execution complete"), so the error message was lost (the first System Link match ended that way after its
// score screen, 24 Nov). This logs the launch and the message.
//
// A restart passes the launch data on (25 Nov): the block the game handed to XamLoaderSetLaunchData is written to a
// file in %TEMP% and its path put in QOS_LAUNCH_DATA, inherited by the new qosmp.exe, which loads it back as its own
// launch data before the game starts (RestoreLaunchData, from OnPostSetup) and deletes the file. At start-up the game
// reads it (sub_822E1738: XamLoaderGetLaunchData into 0x84E8D7D0, version 5 at +0) and, with a message at +4, shows
// the error as on the console. --test_sys_error=TEXT raises a Sys_Error with that text ~10 s after start (not in a
// restarted copy), to test this.

#include "generated/default/qosmp_pch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/kernel/xam/module.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

#include "title_switch.h"

REXCVAR_DEFINE_STRING(test_sys_error, "", "Game",
                      "Test: raise a Sys_Error with this text about 10 s after start (not in the restarted copy)");

REX_EXTERN(__imp__sub_82372B60);  // launch title (name, flags)
REX_EXTERN(sub_822E14E0);         // Sys_Error(format, ...)

namespace {
constexpr char kLaunchDataEnv[] = "QOS_LAUNCH_DATA";

rex::kernel::xam::XamModule* Xam() {
  auto* kernel = rex::system::kernel_state();
  return kernel ? kernel->GetKernelModule<rex::kernel::xam::XamModule>("xam.xex").get() : nullptr;
}

// The launch data the game set, to a file for the restarted copy (path in QOS_LAUNCH_DATA, inherited).
void PassLaunchData() {
  auto* xam = Xam();
  if (!xam || !xam->loader_data().launch_data_present) return;
  const auto& data = xam->loader_data().launch_data;
  const char* tmp = std::getenv("TEMP");
  const std::string path = std::string(tmp ? tmp : ".") + "\\qosmp_launch_data.bin";
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()))) {
    REXLOG_ERROR("game relaunch: couldn't write the launch data to {}", path);
    return;
  }
  f.close();
#ifdef _WIN32
  _putenv_s(kLaunchDataEnv, path.c_str());
#else
  setenv(kLaunchDataEnv, path.c_str(), 1);
#endif
  REXLOG_INFO("game relaunch: launch data ({} bytes) passed on in {}", data.size(), path);
}
constexpr uint32_t kErrorBlock = 0x84E8D7D0;  // +4: Sys_Error message

std::string GuestString(const uint8_t* base, uint32_t addr, size_t max) {
  if (!addr) return {};
  const char* s = reinterpret_cast<const char*>(base + addr);
  return std::string(s, strnlen(s, max));
}
}  // namespace

extern "C" REX_FUNC(sub_82372B60) {
  const std::string name = GuestString(base, ctx.r3.u32, 260);
  const std::string error = GuestString(base, kErrorBlock + 4, 900);
  REXLOG_ERROR("game relaunch: \"{}\" (flags {:X}); Sys_Error message: \"{}\"", name, ctx.r4.u32, error);
  // "default.xex": multiplayer's way back to single player; "default_mp.xex": a restart (Sys_Error). Start that
  // program first (title_switch.h); the launch then ends this process as before. (The launch data, the error
  // message for the restarted game to show, goes with a restart: PassLaunchData.)
  if (name.find("default_mp.xex") != std::string::npos) {
    PassLaunchData();
    qos::title_switch::Start(true, true, "");
  } else if (name.find("default.xex") != std::string::npos) {
    qos::title_switch::Start(false, false, "");
  }
  __imp__sub_82372B60(ctx, base);
}

namespace qosmp::relaunch {

// In the restarted copy: the launch data the last one passed on becomes this one's (before the game reads it).
void RestoreLaunchData() {
  const char* path = std::getenv(kLaunchDataEnv);
  if (!path || !*path) return;
  const std::string file = path;
#ifdef _WIN32
  _putenv_s(kLaunchDataEnv, "");
#else
  unsetenv(kLaunchDataEnv);
#endif
  std::ifstream f(file, std::ios::binary);
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  f.close();
  std::remove(file.c_str());
  auto* xam = Xam();
  if (!xam || data.empty()) return;
  auto& loader = xam->loader_data();
  loader.launch_data = std::move(data);
  loader.launch_data_present = true;
  const std::string message = loader.launch_data.size() > 4
                                  ? std::string(reinterpret_cast<const char*>(loader.launch_data.data() + 4),
                                                strnlen(reinterpret_cast<const char*>(loader.launch_data.data() + 4),
                                                        loader.launch_data.size() - 4))
                                  : std::string();
  REXLOG_INFO("game relaunch: launch data from the last run restored ({} bytes; message \"{}\")",
              loader.launch_data.size(), message);
}

// --test_sys_error: from Com_Frame (frame_rate_fov.cpp).
void OnFrame(PPCContext& ctx, uint8_t* base) {
  static int frames = 0;
  static bool done = false;
  if (done) return;
  const std::string text = REXCVAR_GET(test_sys_error);
  if (text.empty() || std::getenv("QOS_RELAUNCHED")) {
    done = true;
    return;
  }
  if (++frames < 600) return;
  done = true;
  auto* memory = rex::system::kernel_state()->memory();
  const uint32_t a = memory->SystemHeapAlloc(uint32_t(text.size() + 1));
  if (!a) return;
  // A format string: '%' would be read as a conversion.
  std::string safe = text;
  for (char& c : safe) {
    if (c == '%') c = ' ';
  }
  std::memcpy(base + a, safe.c_str(), safe.size() + 1);
  REXLOG_INFO("test: Sys_Error(\"{}\")", safe);
  ctx.r3.u64 = a;
  sub_822E14E0(ctx, base);
}

}  // namespace qosmp::relaunch
