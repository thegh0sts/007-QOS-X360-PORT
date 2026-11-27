// qos - balance beams at any frame rate (--balance_fix, on).
//
// A balance traversal is updated once per player move (sub_8215F070, called with the pmove r3 and its locals pml r4,
// whose msec at +40 is the frame's length). The player's balance is a float in the traversal state (playerState
// +2260, +60). Each update:
//   1. the drift from the stick and the scripted forces is added, scaled by the frame's seconds (msec * 0.001) and
//      bal_wobble_sensitivity: frame-rate independent;
//   2. the balance is multiplied by bal_wobble_decel (0.93) when it is in the middle zones, or by bal_wobble_accel
//      (1.02) when it is in the outer ones (the zones come from bal_wobble_levels 0.06 / 0.27 / 0.8): once per update,
//      whatever its length.
// Step 2 is the frame-rate bug: the designers tuned it at the game's 30 FPS, where one second applies 1.02 30 times
// (x1.8 a second off balance) and 0.93 30 times (x0.11). At 60 that is x3.3 and x0.013, at 120 x10.8 and x0.0002:
// off balance runs away, and near the middle the pull back gets far stronger, so a controller's small stick
// movements swing it from one side to the other.
//
// Here the two multipliers are raised to the power msec / 33.3 for the length of each update (their dvars' values
// at +12, restored after), so every frame rate behaves as the original 30 FPS. The dvars (registered in
// sub_82158AE0) are held in a table at 0x826596A0: decel at +24, accel at +72.

#include "generated/default/qos_pch.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(balance_fix, true, "Game",
                    "Balance beams behave as at the original 30 FPS at any frame rate (off: the game's own, which "
                    "gets harder above 30)");

REX_EXTERN(__imp__sub_8215F070);  // balance traversal update (r3 pmove, r4 pml)

namespace {

constexpr uint32_t kDecelPointer = 0x826596B8;  // bal_wobble_decel
constexpr uint32_t kAccelPointer = 0x826596E8;  // bal_wobble_accel
constexpr uint32_t kPmlMsec = 40;
constexpr float kOriginalFrameMs = 1000.0f / 30.0f;

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void StoreBE32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v); }
float LoadFloat(const uint8_t* p) {
  const uint32_t v = LoadBE32(p);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}
void StoreFloat(uint8_t* p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  StoreBE32(p, v);
}

}  // namespace

extern "C" REX_FUNC(sub_8215F070) {
  const uint32_t decel = LoadBE32(base + kDecelPointer), accel = LoadBE32(base + kAccelPointer);
  const int32_t msec = int32_t(LoadBE32(base + ctx.r4.u32 + kPmlMsec));
  if (!REXCVAR_GET(balance_fix) || !decel || !accel || msec <= 0) {
    __imp__sub_8215F070(ctx, base);
    return;
  }
  uint8_t* decel_value = base + decel + 12;
  uint8_t* accel_value = base + accel + 12;
  const float d = LoadFloat(decel_value), a = LoadFloat(accel_value);
  const float steps = float(msec) / kOriginalFrameMs;
  const float d_scaled = std::pow(d, steps), a_scaled = std::pow(a, steps);
  static int logged = 0;
  if (logged < 3) {
    ++logged;
    REXLOG_INFO("balance: {} ms update, bal_wobble_decel {} -> {}, bal_wobble_accel {} -> {} (as at 30 FPS)", msec,
                d, d_scaled, a, a_scaled);
  }
  StoreFloat(decel_value, d_scaled);
  StoreFloat(accel_value, a_scaled);
  __imp__sub_8215F070(ctx, base);
  StoreFloat(decel_value, d);
  StoreFloat(accel_value, a);
}
