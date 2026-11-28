// qos - frame rate (--frame_rate, 20 to 1000, 0 = no cap; 30 is the original).
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
//
// Above 60 (27 Nov, experimental): the two waits for a display refresh go too. The game's own r_vsync wait in
// Com_Frame (bool dvar, pointer at 0x84055C2C, value byte at dvar + 12) is held at 0, and the swap is passed
// D3DPRESENT_INTERVAL_IMMEDIATE (0x80000000, which sub_820F2188 maps to the swap's 0: no vblank wait) instead of 1.
// --frame_rate=0 sets com_maxfps to 1000, its registered maximum (1 ms frames at the least).
//
// Balance beams got harder with the frame rate (their wobble multiplies once per update): balance_fix.cpp.

#include "generated/default/qos_pch.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(frame_rate, 30, "Game",
                     "Frame rate cap (com_maxfps), 20 to 1000, 0 = no cap: 30 is the original, 60 is smoother (some "
                     "missions may softlock above 30; above 60 is experimental)")
    .range(0, 1000);

REX_EXTERN(__imp__sub_822E4188);  // Dvar_RegisterInt
REX_EXTERN(__imp__sub_8229B7C0);  // Com_Frame
REX_EXTERN(__imp__sub_820F2188);  // D3D: writes the swap packet (presentation interval at device + 13596)

namespace qos::menu_quit {
void OnDvarRegistered(const char* name, uint32_t dvar);  // menu_quit.cpp: finds ui_reachedIIS
}  // namespace qos::menu_quit
namespace qos::fov {
void OnFrame(uint8_t* base);  // fov.cpp: keeps cg_fov at --fov
}  // namespace qos::fov
namespace qos::letterbox {
void OnFrame(uint8_t* base);  // letterbox.cpp: in-game cutscenes full screen (--cutscene_letterbox)
}  // namespace qos::letterbox
namespace qos::dof {
void OnFrame(uint8_t* base, uint32_t dvar_pointer);  // dof.cpp: depth of field on or off (--depth_of_field)
}  // namespace qos::dof
namespace qos::motion_blur {
void OnFrame(uint8_t* base, uint32_t dvar_pointer);  // motion_blur.cpp: motion blur override (--motion_blur)
}  // namespace qos::motion_blur
namespace qos::effects {
void OnFrame(uint8_t* base, uint32_t hurt, uint32_t very_hurt);  // effects.cpp: damage blur on or off (--damage_blur)
}  // namespace qos::effects
namespace qos::lod {
void OnFrame(uint8_t* base, uint32_t dvar_pointer);  // lod.cpp: detail distance (--lod_scale)
}  // namespace qos::lod
namespace qos::shadows {
void OnFrame(uint8_t* base, uint32_t sample_size_pointer, uint32_t fast_pointer);  // shadows.cpp: sun shadow range
}  // namespace qos::shadows
namespace qos::takedown_keys {
void OnFrame();
}  // namespace qos::takedown_keys
namespace qos::aim_assist {
void OnFrame(uint8_t* base);  // aim_assist.cpp: keyboard and mouse keep only Target Snap (--kbm_aim_assist_snap_only)
}  // namespace qos::aim_assist
namespace qos::menu_keys {
void OnFrame(uint8_t* base);
}  // namespace qos::menu_keys

namespace {

std::atomic<uint32_t> g_maxfps{0};  // guest address of the com_maxfps dvar, 0 until registered

int32_t FrameRate() {
  const int32_t fps = REXCVAR_GET(frame_rate);
  if (fps == 0) return 1000;  // no cap: com_maxfps's registered maximum
  return fps < 20 ? 20 : fps > 1000 ? 1000 : fps;
}

constexpr uint32_t kRVsyncPointer = 0x84055C2C;     // r_vsync (bool)
constexpr uint32_t kPresentImmediate = 0x80000000;  // D3DPRESENT_INTERVAL_IMMEDIATE

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
  if (FrameRate() > 60) {  // no refresh wait in the frame loop
    if (const uint32_t vsync = LoadBE32(base + kRVsyncPointer)) {
      uint8_t* on = base + vsync + 12;
      if (*on) {
        static int logged = 0;
        if (logged < 2) {
          ++logged;
          REXLOG_INFO("frame rate: r_vsync held at 0 (above 60)");
        }
        *on = 0;
      }
    }
  }
  qos::fov::OnFrame(base);
  qos::letterbox::OnFrame(base);
  qos::dof::OnFrame(base, 0x84055E40);  // r_dof_enable
  qos::motion_blur::OnFrame(base, 0x84055FC0);  // r_motionblur_enable
  qos::effects::OnFrame(base, 0x8266F534, 0x827700EC);  // cg_showHurtBlur, cg_showVeryHurtBlur
  qos::lod::OnFrame(base, 0x84055C78);  // r_lodScale
  qos::shadows::OnFrame(base, 0x84055E5C, 0x84055D98);  // sm_sunSampleSizeNear, sm_fastSunShadow
  qos::takedown_keys::OnFrame();
  qos::menu_keys::OnFrame(base);
  qos::aim_assist::OnFrame(base);
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
  if (FrameRate() > 60 && interval != kPresentImmediate) {  // no vblank wait at the swap
    static bool logged_immediate = false;
    if (!logged_immediate) {
      logged_immediate = true;
      REXLOG_INFO("frame rate: above 60, swaps passed as immediate (interval {} -> 0x80000000)", interval);
    }
    StoreBE32(base + field, kPresentImmediate);
    __imp__sub_820F2188(ctx, base);
    StoreBE32(base + field, interval);
    return;
  }
  if (interval == 2 && FrameRate() > 30) {
    StoreBE32(base + field, 1);
    __imp__sub_820F2188(ctx, base);
    StoreBE32(base + field, 2);
    return;
  }
  __imp__sub_820F2188(ctx, base);
}
