// qos - motion blur override (--motion_blur: original, off or on).
//
// The renderer's motion blur runs while the dvar r_motionblur_enable is set. It's a bool registered at 0 with flags
// 0x4000 (not saved), so it's off unless something turns it on: a takedown's start saves the value and sets it to 1
// (Dvar_SetBoolByName in sub_823866D0, 0x82386764) and its end puts the saved value back (0x82387838). Level scripts
// can set it too. Its pointer, from the registration in the decrypted executables: single player 0x84055FC0 (stored
// at 0x8255EFD4), multiplayer 0x8518C260. The current value is the byte at dvar + 12.
//
// original: left alone. off: the byte is kept at 0 at the start of every frame, so a takedown's blur is cleared the
// frame it's turned on. on: the byte is kept at 1 (a takedown's end then puts back 1). Called from Com_Frame
// (src/fps_unlock.cpp and mp/src/frame_rate_fov.cpp); single player and multiplayer share this file, each with its own
// pointer. Every change the game makes is logged (the first few), so a run shows when the game uses motion blur.

#if __has_include("generated/default/qos_pch.h")
#include "generated/default/qos_pch.h"
#endif

#include <cstdint>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_STRING(motion_blur, "original", "Game",
                      "Motion blur: original (the game's: takedowns), off (never) or on (always)");

namespace qos::motion_blur {

namespace {
uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
}  // namespace

void OnFrame(uint8_t* base, uint32_t dvar_pointer) {
  const uint32_t dvar = LoadBE32(base + dvar_pointer);
  if (!dvar) return;
  uint8_t* enabled = base + dvar + 12;

  // What the game set since the last frame (after our own write, so only the game's changes show).
  static int last = -1;
  static int logged = 0;
  const int now = *enabled ? 1 : 0;
  if (last >= 0 && now != last && logged < 20) {
    ++logged;
    REXLOG_INFO("motion blur: the game turned r_motionblur_enable {}", now ? "on" : "off");
  }

  const std::string mode = REXCVAR_GET(motion_blur);
  int want = now;
  if (mode == "off") {
    want = 0;
  } else if (mode == "on") {
    want = 1;
  }
  if (want != now) *enabled = uint8_t(want);
  last = want;
}

}  // namespace qos::motion_blur
