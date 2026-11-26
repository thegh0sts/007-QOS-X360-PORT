// qosmp - frame rate (--frame_rate, 20-60) and field of view (--fov, 65-110), as single player's src/fps_unlock.cpp
// and src/fov.cpp (see there), at default_mp.xex's addresses (24 Nov, from the decrypted image): Com_Frame
// sub_8225E8B8 (single player sub_8229B7C0; same frame wait on com_maxfps, registered at 30, the dvar pointer at
// 0x83A678D0), Dvar_RegisterInt sub_822BBEB8 (sp sub_822E4188), Dvar_RegisterFloat sub_822BBF00 (sp sub_822E41D0;
// cg_fov registered at 0x82184F20), D3D swap packet sub_820F2188 (the same address and code as single player's,
// presentation interval at device + 13596). The same --frame_rate / --fov settings as single player, so the launcher
// gives both the same.

#include "generated/default/qosmp_pch.h"

namespace qosmp::menu_keys {
void OnFrame(uint8_t* base);  // menu_keys.cpp: Esc goes back in menus
}  // namespace qosmp::menu_keys
namespace qosmp::relaunch {
void OnFrame(PPCContext& ctx, uint8_t* base);  // relaunch_log.cpp: --test_sys_error
}  // namespace qosmp::relaunch
namespace qosmp::dlc {
void OnFrame(PPCContext& ctx, uint8_t* base);  // dlc_content.cpp: loads the map pack's names and pictures
}  // namespace qosmp::dlc

namespace qos::dof {
void OnFrame(uint8_t* base, uint32_t dvar_pointer);  // ../src/dof.cpp: depth of field on or off (--depth_of_field)
}  // namespace qos::dof
namespace qos::lod {
void OnFrame(uint8_t* base, uint32_t dvar_pointer);  // ../src/lod.cpp: detail distance (--lod_scale)
}  // namespace qos::lod
namespace qos::shadows {
void OnFrame(uint8_t* base, uint32_t sample_size_pointer, uint32_t fast_pointer);  // ../src/shadows.cpp
}  // namespace qos::shadows

namespace qos::pad_prompts {
void Update();  // ../src/pad_prompts.cpp: which button pictures the renderer shows
}  // namespace qos::pad_prompts

#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_DOUBLE(fov, 65.0, "Game", "Field of view in degrees, 65 to 110: 65 is the original")
    .range(65.0, 110.0);

REX_EXTERN(__imp__sub_822BBF00);  // Dvar_RegisterFloat

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

namespace qosmp::fov {

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

}  // namespace qosmp::fov

extern "C" REX_FUNC(sub_822BBF00) {
  const char* name = reinterpret_cast<const char*>(base + ctx.r3.u32);
  const bool cg_fov = std::strcmp(name, "cg_fov") == 0;
  if (cg_fov) {
    ctx.f1.f64 = double(Fov());
    REXLOG_INFO("fov: cg_fov default {} (the game's is 65)", Fov());
  }
  __imp__sub_822BBF00(ctx, base);
  if (cg_fov) g_cg_fov.store(ctx.r3.u32);
}


REXCVAR_DEFINE_INT32(frame_rate, 30, "Game",
                     "Frame rate cap (com_maxfps), 20 to 60: 30 is the original, 60 is smoother (some missions may "
                     "softlock above 30)")
    .range(20, 60);

REX_EXTERN(__imp__sub_822BBEB8);  // Dvar_RegisterInt
REX_EXTERN(__imp__sub_8225E8B8);  // Com_Frame
REX_EXTERN(__imp__sub_820F2188);  // D3D: writes the swap packet (presentation interval at device + 13596)


namespace {

std::atomic<uint32_t> g_maxfps{0};  // guest address of the com_maxfps dvar, 0 until registered

int32_t FrameRate() {
  const int32_t fps = REXCVAR_GET(frame_rate);
  return fps < 20 ? 20 : fps > 60 ? 60 : fps;
}

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void StoreBE32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v); }

}  // namespace

extern "C" REX_FUNC(sub_822BBEB8) {
  const char* name = reinterpret_cast<const char*>(base + ctx.r3.u32);
  const bool maxfps = std::strcmp(name, "com_maxfps") == 0;
  if (maxfps) {
    const int32_t fps = FrameRate();
    ctx.r4.u64 = uint32_t(fps);
    REXLOG_INFO("frame rate: com_maxfps default {} (the game's is 30)", fps);
  }
  __imp__sub_822BBEB8(ctx, base);
  if (maxfps) g_maxfps.store(ctx.r3.u32);
}

extern "C" REX_FUNC(sub_8225E8B8) {
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
  qosmp::fov::OnFrame(base);
  qosmp::menu_keys::OnFrame(base);
  qos::pad_prompts::Update();
  qos::dof::OnFrame(base, 0x8518C0E0);  // r_dof_enable
  qos::lod::OnFrame(base, 0x8518BF18);  // r_lodScale
  qos::shadows::OnFrame(base, 0x8518C0FC, 0x8518C038);  // sm_sunSampleSizeNear, sm_fastSunShadow
  qosmp::dlc::OnFrame(ctx, base);
  qosmp::relaunch::OnFrame(ctx, base);
  __imp__sub_8225E8B8(ctx, base);
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
