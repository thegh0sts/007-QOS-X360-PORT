// qosmp - the game's own console output in the log (--game_console_log, on by default).
//
// Com_Printf (sub_8225D048: channel, format, ...) formats into a 4 KB buffer and passes it to Com_PrintMessage
// (sub_8225CE50: r3 channel, r4 text, r5 3), as Sys_Error does (relaunch_log.cpp). On the console that text goes to
// the debug output only. Here each line is logged as "game: ...": the engine explains itself there (e.g. why a System
// Link server was dropped from the join list: "Throwing out server at ... because the ping was ...").

#include "generated/default/qosmp_pch.h"

#include <cstring>
#include <string>
#include <string_view>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(game_console_log, true, "Game", "Copy the game's console output (Com_Printf) into the log");

REX_EXTERN(__imp__sub_8225CE50);  // Com_PrintMessage(channel, text, flags)

extern "C" REX_FUNC(sub_8225CE50) {
  if (REXCVAR_GET(game_console_log) && ctx.r4.u32) {
    const char* text = reinterpret_cast<const char*>(base + ctx.r4.u32);
    std::string_view all(text, strnlen(text, 4096));
    const uint32_t channel = ctx.r3.u32;
    while (!all.empty()) {
      const size_t end = all.find('\n');
      std::string_view line = all.substr(0, end);
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
      if (!line.empty()) REXLOG_INFO("game[{}]: {}", channel, line);
      if (end == std::string_view::npos) break;
      all.remove_prefix(end + 1);
    }
  }
  __imp__sub_8225CE50(ctx, base);
}
