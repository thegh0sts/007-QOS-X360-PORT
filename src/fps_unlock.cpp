// qos - frame rate (--frame_rate, 20 to 60; 30 is the original).
//
// Quantum of Solace runs on Treyarch's Call of Duty 4 engine. Its frame loop (Com_Frame, sub_8229B7C0) waits until
// 1000 / com_maxfps milliseconds have passed since the last frame (sleeping 1 ms at a time), so com_maxfps is the
// frame cap; the game registers it at 30 (Dvar_RegisterInt, sub_822E4188: r3 name, r4 default, r5 min, r6 max, r7
// flags, r8 description; returns the dvar; its current value is the big-endian int at dvar + 12). With r_vsync on (the
// default) and timescale 1, the same loop also waits at least one display refresh (1000 / refresh rate ms, the refresh
// at 0x841ED138), which allows 60. D3D's own wait is one vblank (sub_820F2188 maps the device's presentation interval
// at device + 13596 to the swap's: 0 or 1 -> 1); a device set to D3DPRESENT_INTERVAL_TWO (2) would hold 30, so above
// 30 an interval of 2 is passed as 1 (logged once either way).
//
// Here: com_maxfps's default is --frame_rate when it is registered, and its current value is set to --frame_rate at
// the start of every frame (the game's config files run "exec default_720p.cfg" and could set it back).
//
// The engine times everything in real milliseconds per frame, so the game speed stays right at any rate. Xenia
// Canary's "Unlock FPS" patch for this game (Sowa_95) does the same through byte patches and warns that above 30 some
// missions can softlock: 60 needs a play-through.

#include "generated/default/qos_pch.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(frame_rate, 30, "Game",
                     "Frame rate cap (com_maxfps), 20 to 60: 30 is the original, 60 is smoother (some missions may "
                     "softlock above 30)")
    .range(20, 60);

REX_EXTERN(__imp__sub_822E4188);  // Dvar_RegisterInt
REX_EXTERN(__imp__sub_8229B7C0);  // Com_Frame
REX_EXTERN(__imp__sub_820F2188);  // D3D: writes the swap packet (presentation interval at device + 13596)

namespace qos::menu_quit {
void OnDvarRegistered(const char* name, uint32_t dvar);  // menu_quit.cpp: finds ui_reachedIIS
}  // namespace qos::menu_quit
namespace qos::fov {
void OnFrame(uint8_t* base);  // fov.cpp: keeps cg_fov at --fov
}  // namespace qos::fov

namespace {

std::atomic<uint32_t> g_maxfps{0};  // guest address of the com_maxfps dvar, 0 until registered

int32_t FrameRate() {
  const int32_t fps = REXCVAR_GET(frame_rate);
  return fps < 20 ? 20 : fps > 60 ? 60 : fps;
}

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void StoreBE32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v); }

}  // namespace

extern "C" REX_FUNC(sub_822E4188) {
  const char* name = reinterpret_cast<const char*>(base + ctx.r3.u32);
  const bool maxfps = std::strcmp(name, "com_maxfps") == 0;
  if (maxfps) {
    const int32_t fps = FrameRate();
    ctx.r4.u64 = uint32_t(fps);
    REXLOG_INFO("frame rate: com_maxfps default {} (the game's is 30)", fps);
  }
  __imp__sub_822E4188(ctx, base);
  if (maxfps) g_maxfps.store(ctx.r3.u32);
  qos::menu_quit::OnDvarRegistered(name, ctx.r3.u32);  // r3: the dvar
}

extern "C" REX_FUNC(sub_8229B7C0) {
  if (const uint32_t dvar = g_maxfps.load()) {
    const int32_t fps = FrameRate();
    uint8_t* value = base + dvar + 12;
    if (int32_t(LoadBE32(value)) != fps) {
      static int logged = 0;
      if (logged < 4) {
        ++logged;
        REXLOG_INFO("frame rate: com_maxfps was {}, set to {}", int32_t(LoadBE32(value)), fps);
      }
      StoreBE32(value, uint32_t(fps));
    }
  }
  qos::fov::OnFrame(base);
  __imp__sub_8229B7C0(ctx, base);
}

extern "C" REX_FUNC(sub_820F2188) {
  const uint32_t field = ctx.r3.u32 + 13596;
  const uint32_t interval = LoadBE32(base + field);
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("frame rate: D3D presentation interval {} (1 = every vblank, 2 = every second one)", interval);
  }
  if (interval == 2 && FrameRate() > 30) {
    StoreBE32(base + field, 1);
    __imp__sub_820F2188(ctx, base);
    StoreBE32(base + field, 2);
    return;
  }
  __imp__sub_820F2188(ctx, base);
}
