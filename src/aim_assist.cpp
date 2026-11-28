// qos - aim assist from the launcher (--aim_target_follow, --aim_target_stick, --aim_target_snap: 1 on, 0 off, -1
// the game's own; keyboard and mouse: only Target Snap, --kbm_aim_assist_snap_only).
//
// The game's Options menu has three aim assist switches, saved with the profile (front end ActionScript:
// onCloseSetDvar): Target Follow = aim_lockon_enabled (lock-on past a stick deflection, default off), Target Stick =
// aim_slowdown_enabled (stick turn rate lowered over an enemy, default on), Target Snap = aim_autoaim_enabled (the view
// turns to a nearby enemy when aiming down the sights, default on). Bool dvars, value byte at dvar + 12; pointers from
// their registration in the decrypted executable: lock-on 0x8263FB74, slowdown 0x8263FB38, auto-aim 0x8263FB6C.
//
// The launcher's Controls tab sets them (controller: all three; keyboard and mouse: Target Snap), and each is held at
// its value every frame, so the in-game switches follow the launcher. Follow and Stick work on the stick's input,
// which a mouse doesn't use: in keyboard-and-mouse mode (the SDK's --mnk_mode) both are held at 0.

#include "generated/default/qos_pch.h"

#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(aim_target_follow, -1, "Game", "Aim assist Target Follow (lock-on): 1 on, 0 off, -1 the game's");
REXCVAR_DEFINE_INT32(aim_target_stick, -1, "Game", "Aim assist Target Stick (slowdown): 1 on, 0 off, -1 the game's");
REXCVAR_DEFINE_INT32(aim_target_snap, -1, "Game", "Aim assist Target Snap (auto-aim): 1 on, 0 off, -1 the game's");
REXCVAR_DEFINE_BOOL(kbm_aim_assist_snap_only, true, "Game",
                    "With keyboard and mouse, Target Follow and Target Stick off (only Target Snap)");

namespace qos::aim_assist {

namespace {
constexpr uint32_t kLockonPointer = 0x8263FB74;    // aim_lockon_enabled (Target Follow)
constexpr uint32_t kSlowdownPointer = 0x8263FB38;  // aim_slowdown_enabled (Target Stick)
constexpr uint32_t kAutoaimPointer = 0x8263FB6C;   // aim_autoaim_enabled (Target Snap)

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

void Hold(uint8_t* base, uint32_t pointer, int32_t want, const char* name) {
  if (want < 0) return;
  const uint32_t dvar = LoadBE32(base + pointer);
  if (!dvar) return;
  uint8_t* value = base + dvar + 12;
  const uint8_t v = want ? 1 : 0;
  if (*value != v) {
    static int logged = 0;
    if (logged < 8) {
      ++logged;
      REXLOG_INFO("aim assist: {} set to {}", name, v);
    }
    *value = v;
  }
}
}  // namespace

// From Com_Frame.
void OnFrame(uint8_t* base) {
  static const bool kbm = REXCVAR_QUERY(bool, mnk_mode);
  const bool snap_only = kbm && REXCVAR_GET(kbm_aim_assist_snap_only);
  Hold(base, kLockonPointer, snap_only ? 0 : REXCVAR_GET(aim_target_follow), "Target Follow (aim_lockon_enabled)");
  Hold(base, kSlowdownPointer, snap_only ? 0 : REXCVAR_GET(aim_target_stick), "Target Stick (aim_slowdown_enabled)");
  Hold(base, kAutoaimPointer, REXCVAR_GET(aim_target_snap), "Target Snap (aim_autoaim_enabled)");
}

}  // namespace qos::aim_assist
