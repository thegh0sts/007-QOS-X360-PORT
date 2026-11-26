// qos - sun shadow range (--sun_shadow_range, 1-4, default 1 = the game's) and the fast sun shadow mode
// (--sun_shadow_fast, default true = the game's; a test switch for now).
//
// The sun's shadow maps cover the area around the camera in texels of sm_sunSampleSizeNear world units (a float,
// registered at 0.381875, range 0.0625-32): bigger texels cover more ground, so sun shadows reach further, but each
// texel is coarser. The levels set it themselves as the player moves (whites_estate 0.25 / 0.569 / 1.208, eco_hotel
// 0.25 / 0.7, shantytown 0.38, by setsaveddvar in their scripts), and sub_8223F750 resets it at a level's start. So
// the range is a multiplier on whatever the game last set: each frame (Com_Frame, hooked in src/fps_unlock.cpp and
// mp/src/frame_rate_fov.cpp) a value that isn't the one written here is taken as the game's, and the dvar is set to
// it times --sun_shadow_range (capped at 32). The native renderer draws shadow maps at the render scale too, so at
// render scale 2 a range of 2 reaches twice as far with the original's sharpness.
//
// sm_fastSunShadow (bool, registered 1, not saved): CoD4's faster sun shadow path; --sun_shadow_fast=false holds it at
// 0 to compare the look and cost.
//
// Dvar pointers (current value at dvar + 12) from the registration in the decrypted executables:
// sm_sunSampleSizeNear single player 0x84055E5C, multiplayer 0x8518C0FC; sm_fastSunShadow 0x84055D98 / 0x8518C038.
// Shared by both programs.

#if __has_include("generated/default/qos_pch.h")
#include "generated/default/qos_pch.h"
#endif

#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_DOUBLE(sun_shadow_range, 1.0, "Game",
                      "Sun shadow range: 1 the game's, 2 twice as far (coarser unless the render scale is raised)");
REXCVAR_DEFINE_BOOL(sun_shadow_fast, true, "Game", "The game's fast sun shadow mode (sm_fastSunShadow)");

namespace qos::shadows {

namespace {
uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void StoreBE32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v);
}
float AsFloat(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
uint32_t AsBits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

bool g_written_valid = false;
uint32_t g_written = 0;  // the value last written here
}  // namespace

void OnFrame(uint8_t* base, uint32_t sample_size_pointer, uint32_t fast_pointer) {
  if (const uint32_t dvar = LoadBE32(base + sample_size_pointer)) {
    uint8_t* value = base + dvar + 12;
    const uint32_t now = LoadBE32(value);
    if (!g_written_valid || now != g_written) {
      // The game's own value (its default, or a level's).
      const double setting = REXCVAR_GET(sun_shadow_range);
      const double range = setting < 1.0 ? 1.0 : setting > 4.0 ? 4.0 : setting;
      const float game = AsFloat(now);
      float want = float(game * range);
      if (want > 32.0f) want = 32.0f;
      static int logged = 0;
      if (range != 1.0 && logged < 12) {
        ++logged;
        REXLOG_INFO("sun shadows: the game set sm_sunSampleSizeNear {}, made {} (range x{})", game, want, range);
      }
      g_written = AsBits(want);
      g_written_valid = true;
      if (g_written != now) StoreBE32(value, g_written);
    }
  }
  if (!REXCVAR_GET(sun_shadow_fast)) {
    if (const uint32_t dvar = LoadBE32(base + fast_pointer)) {
      uint8_t* enabled = base + dvar + 12;
      if (*enabled) {
        static bool logged = false;
        if (!logged) {
          logged = true;
          REXLOG_INFO("sun shadows: sm_fastSunShadow kept at 0");
        }
        *enabled = 0;
      }
    }
  }
}

}  // namespace qos::shadows
