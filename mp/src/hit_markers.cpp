// qosmp - the hit marker on or off (--hit_markers, default true; false: no marker when a shot lands).
//
// Multiplayer's hit marker is CoD4's script one: maps/mp/gametypes/_damagefeedback.gsc gives each player a hud
// element with the damage_feedback shader, and updateDamageFeedback() sets its alpha to 1 and fades it out over a
// second, with the MP_hit_alert sound. The scripts are source text (rawfiles in common_mp.ff), compiled as a map
// loads: the script loader sub_822735E8 names the file "<script>.gsc" and gets its text from sub_82274EE0
// (r4 the file name, the text returned in r3), then compiles it. Off: in that text "self.hud_damagefeedback.alpha = 1;"
// becomes "... = 0;" (same length), so the marker stays invisible. The sound still plays.

#include "generated/default/qosmp_pch.h"

#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(hit_markers, true, "Game", "Show the hit marker when a shot hits an enemy");

REX_EXTERN(__imp__sub_82274EE0);  // the script loader's source text (r4 the file name; returns the text)

extern "C" REX_FUNC(sub_82274EE0) {
  const uint32_t name_address = ctx.r4.u32;
  __imp__sub_82274EE0(ctx, base);
  if (REXCVAR_GET(hit_markers) || !name_address || !ctx.r3.u32) return;
  const char* name = reinterpret_cast<const char*>(base + name_address);
  if (std::strcmp(name, "maps/mp/gametypes/_damagefeedback.gsc") != 0) return;
  char* text = reinterpret_cast<char*>(base + ctx.r3.u32);
  static constexpr char kShow[] = "self.hud_damagefeedback.alpha = 1;";
  char* at = std::strstr(text, kShow);
  if (!at) {
    REXLOG_WARN("hit markers: _damagefeedback.gsc has no \"{}\"; the marker stays on", kShow);
    return;
  }
  at[sizeof(kShow) - 3] = '0';  // the 1 before the ;
  REXLOG_INFO("hit markers: off (_damagefeedback.gsc patched)");
}
