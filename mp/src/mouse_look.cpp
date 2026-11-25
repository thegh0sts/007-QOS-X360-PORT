// qosmp - real mouse look in multiplayer, as single player's src/mouse_look.cpp (see there for how it works):
// --mouse_look, --mouse_sensitivity, --mouse_invert; needs the SDK's keyboard/mouse input (--mnk_mouse).
//
// default_mp.xex (found 25 Nov from the dvar registrations: sensitivity, m_yaw, m_pitch, cl_mouseAccel and
// cl_freelook are all read by one function):
//   sub_821BF478  CL_MouseMove(localClientNum r3, usercmd* r4): multiplayer keeps one clientActive per local client.
//   sub_821BF340  CL_GetMouseMovement(cl, &dx, &dy).
//   cl[n] = 0x82C3A1D8 + n * 0x1FAE8C: mouseDx[2] at +0x252EC, mouseDy[2] at +0x252F4, mouseIndex at +0x252FC.
// Dvar pointers: sensitivity 0x82C34C90, m_yaw 0x82C34C9C, m_pitch 0x82C34CB4, cl_mouseAccel 0x82C3A164,
// cl_freelook 0x82C3A1C8, m_filter 0x82C3A1B4.

#include "generated/default/qosmp_pch.h"

#include <chrono>
#include <cmath>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(mouse_look, true, "Input",
                    "Turn the camera with the mouse itself (the game's own PC mouse code) instead of as a right stick");
REXCVAR_DEFINE_DOUBLE(mouse_sensitivity, 1.0, "Input", "Mouse look speed: 1 is CoD4 PC's default (0.11 degrees a count)")
    .range(0.05, 10.0);
REXCVAR_DEFINE_BOOL(mouse_invert, false, "Input", "Mouse look: moving the mouse up looks down");

REX_EXTERN(__imp__sub_821BF478);  // CL_MouseMove(localClientNum, cmd)

namespace {

constexpr uint32_t kCl = 0x82C3A1D8;
constexpr uint32_t kClStride = 0x1FAE8C;
constexpr uint32_t kMouseDx = 0x252EC;
constexpr uint32_t kMouseDy = 0x252F4;
constexpr uint32_t kMouseIndex = 0x252FC;

// The game asks every frame while playing; a longer gap means menus, loading or pause.
constexpr auto kGap = std::chrono::milliseconds(250);

uint32_t Load(const uint8_t* base, uint32_t a) {
  const uint8_t* p = base + a;
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

void Store(uint8_t* base, uint32_t a, uint32_t v) {
  uint8_t* p = base + a;
  p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v);
}

void Add(uint8_t* base, uint32_t a, int32_t n) {
  if (n) Store(base, a, uint32_t(int32_t(Load(base, a)) + n));
}

struct State {
  bool direct = false;
  std::chrono::steady_clock::time_point last{};
  double rest_x = 0.0, rest_y = 0.0;  // fractions of a count left over by the sensitivity scale
  // Log: counts per second for the first few seconds of mouse movement, to check the scale.
  int logged = 0;
  double sum_x = 0.0, sum_y = 0.0;
  std::chrono::steady_clock::time_point log_since{};
} g;

}  // namespace

extern "C" REX_FUNC(sub_821BF478) {
  const bool on = REXCVAR_GET(mouse_look);
  if (on != g.direct) {
    rex::input::mnk::SetMouseDirect(on);
    g.direct = on;
    REXLOG_INFO("mouse look: {}", on ? "the game's own mouse code (direct mouse)" : "off (mouse as right stick)");
  }
  if (on) {
    const auto now = std::chrono::steady_clock::now();
    float dx = 0.0f, dy = 0.0f;
    const bool moved = rex::input::mnk::ConsumeMouseDelta(&dx, &dy);
    const bool fresh = g.last.time_since_epoch().count() != 0 && now - g.last < kGap;
    g.last = now;
    if (moved && fresh) {
      const double k = REXCVAR_GET(mouse_sensitivity);
      const double x = dx * k + g.rest_x;
      const double y = (REXCVAR_GET(mouse_invert) ? -dy : dy) * k + g.rest_y;
      const double ix = std::trunc(x), iy = std::trunc(y);
      g.rest_x = x - ix, g.rest_y = y - iy;
      const uint32_t cl = kCl + (ctx.r3.u32 & 3) * kClStride;  // the local client this command is for
      const uint32_t index = Load(base, cl + kMouseIndex) & 1;
      Add(base, cl + kMouseDx + index * 4, int32_t(ix));
      Add(base, cl + kMouseDy + index * 4, int32_t(iy));
      if (g.logged < 3) {
        if (g.log_since.time_since_epoch().count() == 0) g.log_since = now;
        g.sum_x += dx, g.sum_y += dy;
        if (now - g.log_since >= std::chrono::seconds(1)) {
          ++g.logged;
          REXLOG_INFO("mouse look: {:.0f}, {:.0f} counts in the last second", g.sum_x, g.sum_y);
          g.sum_x = g.sum_y = 0.0;
          g.log_since = now;
        }
      }
    } else if (!fresh) {
      g.rest_x = g.rest_y = 0.0;
    }
  }
  __imp__sub_821BF478(ctx, base);
}
