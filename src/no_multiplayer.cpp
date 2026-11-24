// qos - the main menu's MULTIPLAYER does nothing.
//
// Multiplayer is a separate executable on the disc (game/default_mp.xex). Choosing MULTIPLAYER runs the console
// command "startMultiplayer" (registered with Cmd_AddCommand sub_82299178 at 0x821BE8D4; name at 0x8201D714), whose
// handler sub_821BCD08 shuts the game's systems down (sub_8229BE60, sub_8256F7B8, sub_825CE62C, sub_822A1290) and then
// relaunches the title as default_mp.xex (sub_82423D78, "default_mp.xex" at 0x8201D510). Only default.xex is
// recompiled, so the relaunch ended the process: the game closed. The handler now returns before shutting anything
// down. --allow_multiplayer=true calls the original (it still closes the game).
// The MULTIPLAYER item first opens the menu's "splaunchmp" screen (CmMainMenu.gfx: a LOADING... screen whose job is
// exec "startMultiplayer"), so doing nothing left that screen up for good (user, 19:47). So the handler queues
// --multiplayer_fallback (default "disconnect", what the pause menu's quit runs: back to the main menu) instead.

#include "generated/default/qos_pch.h"

#include <atomic>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

#include <cstring>
#include <string>

REXCVAR_DEFINE_BOOL(allow_multiplayer, false, "Game",
                    "Let the main menu's MULTIPLAYER relaunch the title as default_mp.xex (not recompiled: the game "
                    "closes)");

REXCVAR_DEFINE_STRING(multiplayer_fallback, "disconnect", "Game",
                      "Console command run in place of startMultiplayer (leaves the menu's loading screen)");

REX_EXTERN(__imp__sub_821BCD08);  // "startMultiplayer" command handler
REX_EXTERN(sub_82298750);         // Cbuf_AddText(localClientNum, text)

namespace {
std::atomic<int> g_logged{0};
}  // namespace

extern "C" REX_FUNC(sub_821BCD08) {
  if (REXCVAR_GET(allow_multiplayer)) {
    REXLOG_INFO("multiplayer: starting default_mp.xex (allow_multiplayer)");
    __imp__sub_821BCD08(ctx, base);
    return;
  }
  const std::string cmd = REXCVAR_GET(multiplayer_fallback);
  if (g_logged.fetch_add(1) < 4) {
    REXLOG_INFO("multiplayer: startMultiplayer ignored (not part of this port); running \"{}\"", cmd);
  }
  if (cmd.empty()) return;
  const std::string text = cmd + "\n";
  const uint32_t guest = rex::system::kernel_state()->memory()->SystemHeapAlloc(uint32_t(text.size() + 1));
  if (!guest) return;
  std::memcpy(base + guest, text.c_str(), text.size() + 1);  // Cbuf copies it; one small block per press
  ctx.r3.u64 = 0;
  ctx.r4.u64 = guest;
  sub_82298750(ctx, base);
}
