// qos - MULTIPLAYER on the main menu starts qosmp.exe (title_switch.h).
//
// The menu's startMultiplayer command (sub_821BCD08) shuts the game down and calls sub_82423D78("default_mp.xex", ...),
// the title launch (XamLoaderLaunchTitle), which ends this process under ReXGlue. The hook starts qosmp.exe first
// with this process's settings plus --mp_extra_args (the launcher's gamertag and XUID), then lets the launch end
// single player as before.

#include "generated/default/qos_pch.h"

#include <cstring>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "title_switch.h"

REXCVAR_DEFINE_STRING(mp_extra_args, "", "Game",
                      "Extra command-line settings for multiplayer when MULTIPLAYER starts it (the launcher's gamertag "
                      "and XUID)");

REX_EXTERN(__imp__sub_82423D78);  // title launch (name, ...)

extern "C" REX_FUNC(sub_82423D78) {
  const char* name = ctx.r3.u32 ? reinterpret_cast<const char*>(base + ctx.r3.u32) : "";
  REXLOG_INFO("title launch: \"{}\"", std::string(name, strnlen(name, 260)));
  if (strstr(name, "default_mp.xex")) qos::title_switch::Start(true, false, REXCVAR_GET(mp_extra_args));
  __imp__sub_82423D78(ctx, base);
}
