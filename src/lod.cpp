// qos - detail distance (--lod_scale, 0-1, default 1 = the game's): how far away models keep their detailed versions.
//
// Each frame the renderer (single player sub_82563E30, multiplayer sub_824C8E08) works out the distance it picks a
// model's level of detail by: distance * r_lodScale + r_lodBias, both scaled by the field of view. A model switches
// to a simpler version, and past its last one isn't drawn, as that distance passes the model's own thresholds. So a
// smaller r_lodScale keeps the detailed versions further out: 0.5 twice as far, 0 always (the developers' minimap
// capture script sets r_lodscale 0 to "turn off lods"). r_lodScale is a saved float registered at 1 with a range of
// 1-4 (the console only allows less detail), so it's written directly: the current value, a float at dvar + 12.
// Dvar pointers from the registration in the decrypted executables: single player 0x84055C78, multiplayer 0x8518BF18.
// r_lodBias (the takedown camera changes it) is left alone.
//
// Written at the start of every frame (Com_Frame, hooked in src/fps_unlock.cpp and mp/src/frame_rate_fov.cpp), also
// at 1, so a value saved by an earlier run with another setting doesn't stay. Shared by both programs.

#if __has_include("generated/default/qos_pch.h")
#include "generated/default/qos_pch.h"
#endif

#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_DOUBLE(lod_scale, 1.0, "Game",
                      "Detail distance: 1 the game's, 0.5 detailed models twice as far, 0 always detailed");

namespace qos::lod {

namespace {
uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void StoreBE32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v);
}
}  // namespace

void OnFrame(uint8_t* base, uint32_t dvar_pointer) {
  const uint32_t dvar = LoadBE32(base + dvar_pointer);
  if (!dvar) return;
  double setting = REXCVAR_GET(lod_scale);
  const float scale = float(setting < 0.0 ? 0.0 : setting > 1.0 ? 1.0 : setting);
  uint32_t bits;
  std::memcpy(&bits, &scale, 4);
  uint8_t* value = base + dvar + 12;
  const uint32_t was = LoadBE32(value);
  if (was == bits) return;
  static int logged = 0;
  if (logged < 2) {
    ++logged;
    float old;
    std::memcpy(&old, &was, 4);
    REXLOG_INFO("detail distance: r_lodScale was {}, set to {}", old, scale);
  }
  StoreBE32(value, bits);
}

}  // namespace qos::lod
