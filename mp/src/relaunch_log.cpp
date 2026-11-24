// qosmp - log why the game relaunches itself.
//
// Sys_Error (sub_822E14E0, MP's; "===Restarting for Sys_Error===") formats the error into a 984-byte block at
// 0x84E8D7D0 (+4: the message, up to 900 bytes), hands it on as launch data (XamLoaderSetLaunchData, import stub
// 0x8252516C) and relaunches default_mp.xex (sub_82372B60(name, flags), the title launch; also used with
// "default.xex" by sub_821C5598, multiplayer's way back to single player). ReXGlue doesn't relaunch: the process just
// ends ("Execution complete"), so the error message was lost (the first System Link match ended that way after its
// score screen, 24 Nov). This logs the launch and the message.

#include "generated/default/qosmp_pch.h"

#include <cstring>
#include <string>

#include <rex/logging.h>

REX_EXTERN(__imp__sub_82372B60);  // launch title (name, flags)

namespace {
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
  __imp__sub_82372B60(ctx, base);
}
