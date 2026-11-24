// qos - the main menu's MULTIPLAYER does nothing.
//
// Multiplayer is a separate executable on the disc (game/default_mp.xex). Choosing MULTIPLAYER runs the console
// command "startMultiplayer" (registered with Cmd_AddCommand sub_82299178 at 0x821BE8D4; name at 0x8201D714), whose
// handler sub_821BCD08 shuts the game's systems down (sub_8229BE60, sub_8256F7B8, sub_825CE62C, sub_822A1290) and then
// relaunches the title as default_mp.xex (sub_82423D78, "default_mp.xex" at 0x8201D510). Only default.xex is
// recompiled, so the relaunch ended the process: the game closed. The handler now returns before shutting anything
// down, so the menu stays where it is. --allow_multiplayer=true calls the original (it still closes the game).

#include "generated/default/qos_pch.h"

#include <atomic>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(allow_multiplayer, false, "Game",
                    "Let the main menu's MULTIPLAYER relaunch the title as default_mp.xex (not recompiled: the game "
                    "closes)");

REX_EXTERN(__imp__sub_821BCD08);  // "startMultiplayer" command handler

namespace {
std::atomic<int> g_logged{0};
}  // namespace

extern "C" REX_FUNC(sub_821BCD08) {
  if (REXCVAR_GET(allow_multiplayer)) {
    REXLOG_INFO("multiplayer: starting default_mp.xex (allow_multiplayer)");
    __imp__sub_821BCD08(ctx, base);
    return;
  }
  if (g_logged.fetch_add(1) < 4) REXLOG_INFO("multiplayer: startMultiplayer ignored (not part of this port)");
}
