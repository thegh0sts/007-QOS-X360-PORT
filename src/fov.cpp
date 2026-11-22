// qos - field of view (--fov, 65 to 110 degrees; 65 is the original).
//
// The first-person view's angle is the dvar cg_fov ("The field of view angle in degrees"), registered by
// Dvar_RegisterFloat (sub_822E41D0: r3 name, f1 default, f2 min, f3 max, r7 flags, r8 description; returns the dvar)
// with default 65, range 1-160 (from the decrypted executable, 2026-11-21). Its current value is the big-endian
// float at dvar + 12. Here its default becomes --fov, and at the start of every frame (Com_Frame, hooked in
// fps_unlock.cpp) a current value of 65 (the game's own, e.g. after its config files run) is set to --fov again;
// any other value the game sets is left alone. Aiming down sights scales the view from cg_fov, so it widens too.

#include "generated/default/qos_pch.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_DOUBLE(fov, 65.0, "Game", "Field of view in degrees, 65 to 110: 65 is the original")
    .range(65.0, 110.0);

REX_EXTERN(__imp__sub_822E41D0);  // Dvar_RegisterFloat

namespace {

constexpr float kGameFov = 65.0f;
std::atomic<uint32_t> g_cg_fov{0};  // guest address of the cg_fov dvar, 0 until registered

float Fov() {
  const double fov = REXCVAR_GET(fov);
  return float(fov < 65.0 ? 65.0 : fov > 110.0 ? 110.0 : fov);
}

uint32_t Bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

}  // namespace

namespace qos::fov {

// Called at the start of every frame (fps_unlock.cpp's Com_Frame hook).
void OnFrame(uint8_t* base) {
  const uint32_t dvar = g_cg_fov.load();
  if (!dvar) return;
  const float fov = Fov();
  if (fov == kGameFov) return;
  uint8_t* p = base + dvar + 12;
  const uint32_t current = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
  if (current != Bits(kGameFov)) return;
  const uint32_t v = Bits(fov);
  p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v);
  static int logged = 0;
  if (logged < 4) {
    ++logged;
    REXLOG_INFO("fov: cg_fov back at 65, set to {}", fov);
  }
}

}  // namespace qos::fov

extern "C" REX_FUNC(sub_822E41D0) {
  const char* name = reinterpret_cast<const char*>(base + ctx.r3.u32);
  const bool cg_fov = std::strcmp(name, "cg_fov") == 0;
  if (cg_fov) {
    ctx.f1.f64 = double(Fov());
    REXLOG_INFO("fov: cg_fov default {} (the game's is 65)", Fov());
  }
  __imp__sub_822E41D0(ctx, base);
  if (cg_fov) g_cg_fov.store(ctx.r3.u32);
}
