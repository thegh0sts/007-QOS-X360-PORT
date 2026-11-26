// qos - depth of field on or off (--depth_of_field, default true; false turns the blur off everywhere).
//
// The renderer's depth of field is CoD4's: each frame the view's DoF values (near/far start, end and blur, set by
// the scripts' setdepthoffield for cutscenes and by aiming down the sights) are copied into the frame's settings only
// while the dvar r_dof_enable is set; when it's 0 they're zeroed and nothing is blurred (single player sub_825616C8
// at 0x82561A58, multiplayer the same code at 0x824C6A30). r_dof_enable is a saved bool registered at 1; its pointer,
// from the registration in the decrypted executables: single player 0x84055E40, multiplayer 0x8518C0E0. The current
// value is the byte at dvar + 12.
//
// Off: the byte is kept at 0 at the start of every frame (Com_Frame, hooked in src/fps_unlock.cpp and
// mp/src/frame_rate_fov.cpp). On: it's set back to 1 once, as the dvar is saved and a run with the option off may
// have left it at 0. Single player and multiplayer share this file, each with its own pointer.

#if __has_include("generated/default/qos_pch.h")
#include "generated/default/qos_pch.h"
#endif

#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(depth_of_field, true, "Game", "Depth of field blur (off: nothing is blurred)");

namespace qos::dof {

namespace {
uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
}  // namespace

void OnFrame(uint8_t* base, uint32_t dvar_pointer) {
  const uint32_t dvar = LoadBE32(base + dvar_pointer);
  if (!dvar) return;
  uint8_t* enabled = base + dvar + 12;
  static bool restored = false;
  if (REXCVAR_GET(depth_of_field)) {
    if (!restored) {
      restored = true;
      if (!*enabled) {
        REXLOG_INFO("depth of field: r_dof_enable was 0, set back to 1");
        *enabled = 1;
      }
    }
    return;
  }
  if (*enabled) {
    static int logged = 0;
    if (logged < 2) {
      ++logged;
      REXLOG_INFO("depth of field: r_dof_enable kept at 0");
    }
    *enabled = 0;
  }
}

}  // namespace qos::dof
