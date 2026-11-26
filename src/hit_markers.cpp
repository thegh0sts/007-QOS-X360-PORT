// qos - the enemy hit indicator on or off (--hit_markers, default true; false: nothing shows when a shot lands).
//
// Single player's hit indicator is code, not script (its _damagefeedback::init() is commented out in common.ff):
// one of the HUD's owner-draws, sub_82183188, called from the owner-draw switch sub_821887F8 at 0x82188F98. It fades
// the indicator over hud_fade_enemyhit seconds (dvar pointer 0x827C3664) from the time of the last hit and draws it
// (sub_822C7D60). Off: the hook returns without drawing. Multiplayer's marker is a script's hud element, turned off
// in mp/src/hit_markers.cpp.

#include "generated/default/qos_pch.h"

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(hit_markers, true, "Game", "Show the hit marker when a shot hits an enemy");

REX_EXTERN(__imp__sub_82183188);  // draws the enemy hit indicator (a HUD owner-draw)

extern "C" REX_FUNC(sub_82183188) {
  if (!REXCVAR_GET(hit_markers)) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      REXLOG_INFO("hit markers: the enemy hit indicator is off");
    }
    return;
  }
  __imp__sub_82183188(ctx, base);
}
