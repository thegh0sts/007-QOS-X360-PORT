// qos - real mouse look (--mouse_look, on by default; needs the SDK's keyboard/mouse input with --mnk_mouse).
//
// The Xbox 360 build kept Call of Duty 4's PC mouse code whole (found in the decrypted executable, 2026-11-23):
//   sub_821B8908  CL_MouseMove(usercmd* r3), called every frame from CL_CreateCmd (sub_821B94F8) unless the dvar
//                 cl_ignoreinput is set. It reads the dvars sensitivity (default 5), cl_mouseAccel, m_yaw and m_pitch
//                 (0.022), m_side / m_forward (0.25), m_filter and cl_freelook, and turns cl.viewangles by
//                 counts * sensitivity * m_yaw / m_pitch (with the zoom's sensitivity scale while aiming), exactly
//                 as on PC.
//   sub_821B87E0  CL_GetMouseMovement: takes cl.mouseDx[cl.mouseIndex] / cl.mouseDy[...] (ints), flips
//                 mouseIndex and clears the new slot.
//   cl (clientActive) is at 0x82809AD8: mouseDx[2] at +0x9B40, mouseDy[2] at +0x9B48, mouseIndex at +0x9B50.
// Nothing on the console ever fills mouseDx/mouseDy (CL_MouseEvent's input never comes), so the code runs on zeros.
// Here the SDK's keyboard/mouse driver is put in direct mode (SDK patch 0009: mouse movement no longer becomes
// right-stick deflection) and, just before CL_MouseMove runs, the mouse counts since the last frame are added to
// cl.mouseDx/mouseDy[mouseIndex]. The game's own sensitivity, zoom scaling and pitch limits then apply: one count
// turns the view 5 * 0.022 = 0.11 degrees at --mouse_sensitivity=1 (CoD4 PC's default feel).
// Movement made while the game isn't asking (menus, loading, pause) is dropped, so the view doesn't jump.

#include "generated/default/qos_pch.h"

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

REX_EXTERN(__imp__sub_821B8908);  // CL_MouseMove

namespace {

constexpr uint32_t kCl = 0x82809AD8;
constexpr uint32_t kMouseDx = kCl + 0x9B40;
constexpr uint32_t kMouseDy = kCl + 0x9B48;
constexpr uint32_t kMouseIndex = kCl + 0x9B50;

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

extern "C" REX_FUNC(sub_821B8908) {
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
      const uint32_t index = Load(base, kMouseIndex) & 1;
      Add(base, kMouseDx + index * 4, int32_t(ix));
      Add(base, kMouseDy + index * 4, int32_t(iy));
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
  __imp__sub_821B8908(ctx, base);
}
