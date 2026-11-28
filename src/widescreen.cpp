// qos - ultrawide (--screen_ratio, native renderer only; first test: 2560x1080).
//
// The game always uses a 1280x720 back buffer. Its renderer asks for the display setup in sub_8256F478 (fills a
// struct: +0 widescreen byte, +8/+12 the 3D scene's size 1024x608 (rendered smaller and stretched, 2x MSAA), +16/+20
// the back buffer 1280x720), and sub_8256F110 copies that into the video config at 0x841ED128 (CoD4's vidConfig:
// +0 scene width, +4 scene height, +8 display width, +12 display height, +32 aspectRatioWindow (16/9 from a constant
// when widescreen, else 4/3), +36 aspectRatioScenePixel = sceneHeight * aspect / sceneWidth, +40
// aspectRatioDisplayPixel = displayHeight * aspect / displayWidth).
//
// With a wider ratio the back buffer keeps its width and gets shorter (2560x1080: 2.37:1 -> 1280x540), the scene
// height shrinks with it (608 -> 456), and aspectRatioWindow becomes width / height, so the camera sees more at the
// sides (Hor+), as Skullburglar's widescreen.cpp does for Bourne. The native renderer presents a frame wider than the
// display's shape at its own shape (native.cpp). --screen_ratio_height overrides the back buffer height (tests: the
// GPU's resolves align to 8 rows, so 544 may be needed).
//
// Full-screen videos (the splash logos, pre-rendered cutscenes) are drawn by sub_82566A48: black bars above and below
// when the window is narrower than 16:9, then the movie as one stretch pic (sub_82570C08: f1-f8 x, y, w, h, s0, t0,
// s1, t1; color pointer at the caller's r1+84, material at r1+92) over the whole display. When wider, the movie is
// drawn 16:9 in the middle and the sides are filled black (the white material with the black color the 4:3 bars use).
//
// The HUD (CoD4's ScreenPlacement): sub_821BCB18 sets up three placements (0x828A1708, 0x828A1758, 0x828A17A8)
// with sub_821C1318 (r3 placement, f1-f4 x, y, w, h = 0, 0, display width, display height). Elements are placed
// from a 640x480 virtual screen by sub_821C1710 (rect: r4-r7 pointers to x, y, w, h; r8 horizontal alignment, r9
// vertical) and sub_821C1570 (x: f1, r5 alignment): 0 the 4:3 area in the middle (+68 its left edge), 1 left of the
// safe area (+52), 2 the middle (+44 the width, halved), 3 right of the safe area (+60), 4 the full width (+8 =
// width / 640, no offset), 5 unscaled, 6 to 640, 7 the middle of the safe area. When wider than 16:9 the placements
// are set up for the 16:9 area in the middle (x = (width - 16:9 width) / 2), then +68 gets that x added and +44 the
// whole width (the middle stays the middle); full-width elements are moved into the 16:9 area too, except the ones
// that cover the whole virtual width (fades, damage, scope masks), which keep stretching to the edges.
//
// Most of the HUD, the hints, menus, the phone and the loading screen are Scaleform movies. Each is drawn by
// sub_82589B08 (r3 the movie's entry, its short name at +0 and the movie at +552; r4-r7 left, top, width, height):
// it sets the movie's viewport (the buffer's size from 0x841ED170 +4/+8) and displays it. The movies are 16:9 and
// shown whole in the middle of a wider viewport, but they place things from the edges of the viewport (the HUD at
// the far left, the loading bar), so a full-width viewport is narrowed to the 16:9 area in the middle.

#include "generated/default/qos_pch.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_STRING(screen_ratio, "16:9", "Display",
                      "Shape of the picture: 16:9 (the original) or an ultrawide output size such as 2560x1080 "
                      "(the back buffer keeps 1280 columns and gets shorter; more is seen at the sides). Native "
                      "renderer only; applies when the game starts");
REXCVAR_DEFINE_INT32(screen_ratio_height, 0, "Display",
                     "Test: the back buffer's height for an ultrawide --screen_ratio (0: worked out from the ratio)");

REX_EXTERN(__imp__sub_8256F478);  // display setup query (r3 = struct to fill)
REX_EXTERN(__imp__sub_8256F110);  // video config from that struct
REX_EXTERN(__imp__sub_82566A48);  // draws the full-screen video
REX_EXTERN(__imp__sub_82570C08);  // R_AddCmdDrawStretchPic
REX_EXTERN(__imp__sub_821C1318);  // ScrPlace setup
REX_EXTERN(__imp__sub_821C1710);  // ScrPlace_ApplyRect
REX_EXTERN(__imp__sub_821C1570);  // ScrPlace_ApplyX
REX_EXTERN(__imp__sub_82589B08);  // displays a Scaleform movie (r3 entry, r4-r7 viewport)

namespace qos::widescreen {

namespace {
constexpr uint32_t kWidth = 1280, kHeight = 720, kSceneHeight = 608;
constexpr uint32_t kVidConfig = 0x841ED128;
constexpr uint32_t kWhiteMaterial = 0x841EB5E8 + 6544;  // the material sub_82566A48 draws its bars with
constexpr uint32_t kBlack = 0x82049A90;                 // {0, 0, 0, 1}

std::once_flag g_once;
uint32_t g_height = kHeight;

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void StoreBE32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v); }
float LoadBEFloat(const uint8_t* p) {
  const uint32_t u = LoadBE32(p);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
void StoreBEFloat(uint8_t* p, float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  StoreBE32(p, u);
}

// "2560x1080", "21:9", "2.37"; 0 when it can't be read.
float Requested(const std::string& s) {
  const size_t sep = s.find_first_of("x:");
  if (sep == std::string::npos) return std::strtof(s.c_str(), nullptr);
  const float a = std::strtof(s.c_str(), nullptr), b = std::strtof(s.c_str() + sep + 1, nullptr);
  return b > 0 ? a / b : 0.0f;
}

void Decide() {
  const std::string want = REXCVAR_GET(screen_ratio);
  if (rex::cvar::GetFlagByName("gpu_plugin") != "native") {
    if (want != "16:9") REXLOG_INFO("widescreen: 16:9 (--screen_ratio needs the native renderer)");
    return;
  }
  const float aspect = Requested(want);
  if (!(aspect > 16.0f / 9.0f * 1.02f)) {
    if (want != "16:9") REXLOG_INFO("widescreen: 16:9 (--screen_ratio {} is not wider)", want);
    return;
  }
  uint32_t h = uint32_t(std::lround(float(kWidth) / std::min(aspect, 32.0f / 9.0f) / 2.0f)) * 2;  // even
  if (const int32_t forced = REXCVAR_GET(screen_ratio_height); forced >= 360 && forced < int32_t(kHeight)) h = forced;
  g_height = h;
  REXLOG_INFO("widescreen: {:.3f}:1 (--screen_ratio {}), back buffer {}x{}, scene height {}", float(kWidth) / float(h),
              want, kWidth, h, kSceneHeight * h / kHeight);
}
}  // namespace

uint32_t Height() {
  std::call_once(g_once, Decide);
  return g_height;
}
bool Wide() { return Height() != kHeight; }

void FixDisplaySetup(uint8_t* s) {
  if (!Wide() || LoadBE32(s + 20) != kHeight || LoadBE32(s + 16) != kWidth) return;
  const uint32_t h = Height();
  const uint32_t scene_h = LoadBE32(s + 12) * h / kHeight & ~1u;
  StoreBE32(s + 20, h);
  StoreBE32(s + 12, scene_h);
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("widescreen: display setup {}x{}, scene {}x{}", LoadBE32(s + 16), h, LoadBE32(s + 8), scene_h);
  }
}

void FixVidConfig(uint8_t* base) {
  if (!Wide()) return;
  uint8_t* v = base + kVidConfig;
  const float scene_w = float(LoadBE32(v + 0)), scene_h = float(LoadBE32(v + 4));
  const float disp_w = float(LoadBE32(v + 8)), disp_h = float(LoadBE32(v + 12));
  if (!(scene_w > 0 && disp_w > 0 && disp_h > 0)) return;
  const float aspect = disp_w / disp_h;
  const float old = LoadBEFloat(v + 32);
  StoreBEFloat(v + 32, aspect);
  StoreBEFloat(v + 36, scene_h * aspect / scene_w);
  StoreBEFloat(v + 40, disp_h * aspect / disp_w);
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("widescreen: video config aspect {:.4f} (was {:.4f}), scene pixel {:.4f}", aspect, old,
                scene_h * aspect / scene_w);
  }
}

bool g_in_video = false;

constexpr uint32_t kPlacements[] = {0x828A1708, 0x828A1758, 0x828A17A8};
float g_box_x = 0.0f, g_full_w = 0.0f;  // the 16:9 area's left edge and the display width, once boxed

bool Boxed(uint32_t placement) {
  if (g_box_x <= 0.0f) return false;
  for (uint32_t p : kPlacements)
    if (p == placement) return true;
  return false;
}

void SetUpPlacement(PPCContext& ctx, uint8_t* base) {
  const uint32_t p = ctx.r3.u32;
  const double w = ctx.f3.f64, h = ctx.f4.f64;
  const double box_w = std::floor(h * 16.0 / 9.0 / 2.0) * 2.0;
  bool known = false;
  for (uint32_t k : kPlacements) known |= k == p;
  if (!Wide() || !known || ctx.f1.f64 != 0.0 || !(w > box_w + 2.0)) {
    __imp__sub_821C1318(ctx, base);
    return;
  }
  const double x = (w - box_w) / 2.0;
  ctx.f1.f64 = x, ctx.f3.f64 = box_w;
  __imp__sub_821C1318(ctx, base);
  uint8_t* s = base + p;
  StoreBEFloat(s + 68, LoadBEFloat(s + 68) + float(x));
  StoreBEFloat(s + 44, float(w));
  g_box_x = float(x), g_full_w = float(w);
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("widescreen: HUD in the middle {}x{} of {} (from x {})", box_w, h, w, x);
  }
}

// After the original: full-width (alignment 4) rects into the 16:9 area unless they span the whole virtual width.
void ApplyRect(PPCContext& ctx, uint8_t* base) {
  const uint32_t p = ctx.r3.u32, px = ctx.r4.u32, pw = ctx.r6.u32, horz = ctx.r8.u32;
  const bool fix = horz == 4 && Boxed(p) && px && pw;
  const float vx = fix ? LoadBEFloat(base + px) : 0.0f, vw = fix ? LoadBEFloat(base + pw) : 0.0f;
  __imp__sub_821C1710(ctx, base);
  if (!fix) return;
  const bool whole = vx <= 0.5f && vx + vw >= 639.5f;
  const float scale = (whole ? g_full_w : g_full_w - 2.0f * g_box_x) / 640.0f;
  StoreBEFloat(base + px, vx * scale + (whole ? 0.0f : g_box_x));
  StoreBEFloat(base + pw, vw * scale);
}

void ApplyX(PPCContext& ctx, uint8_t* base) {
  const bool fix = ctx.r5.u32 == 4 && Boxed(ctx.r3.u32);
  __imp__sub_821C1570(ctx, base);
  if (fix) ctx.f1.f64 = double(float(ctx.f1.f64 + g_box_x));
}

void DisplayMovie(PPCContext& ctx, uint8_t* base) {
  char name[32] = {};
  std::memcpy(name, base + ctx.r3.u32, sizeof(name) - 1);
  const uint32_t w = ctx.r6.u32, h = ctx.r7.u32;
  const uint32_t box_w = h * 16 / 9 & ~1u;
  if (Wide() && ctx.r4.u32 == 0 && w > box_w + 2) {
    ctx.r4.u64 = (w - box_w) / 2;
    ctx.r6.u64 = box_w;
  }
  static std::mutex m;
  static std::string seen;
  {
    const std::string key = std::string("|") + name + "|";
    std::lock_guard<std::mutex> lock(m);
    if (seen.find(key) == std::string::npos) {
      seen += key;
      REXLOG_INFO("widescreen: movie '{}' at {},{} {}x{}", name, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
    }
  }
  __imp__sub_82589B08(ctx, base);
}

// The movie's stretch pic, drawn 16:9 in the middle of a wider display with black at the sides.
void DrawVideoPillarboxed(PPCContext& ctx, uint8_t* base) {
  const double x = ctx.f1.f64, w = ctx.f3.f64, h = ctx.f4.f64;
  const double inner = h * 16.0 / 9.0;
  if (!(w > inner + 1.0) || x != 0.0) {
    __imp__sub_82570C08(ctx, base);
    return;
  }
  const double side = std::floor((w - inner) / 2.0);
  uint8_t* args = base + ctx.r1.u32;
  const uint32_t color = LoadBE32(args + 84), material = LoadBE32(args + 92);
  const PPCContext saved = ctx;
  auto bar = [&](double bx, double bw) {
    ctx = saved;
    ctx.f1.f64 = bx, ctx.f3.f64 = bw;
    ctx.f5.f64 = 0.0, ctx.f6.f64 = 0.0, ctx.f7.f64 = 1.0, ctx.f8.f64 = 1.0;
    StoreBE32(args + 84, kBlack);
    StoreBE32(args + 92, LoadBE32(base + kWhiteMaterial));
    __imp__sub_82570C08(ctx, base);
  };
  bar(0.0, side);
  bar(side + inner, w - side - inner);
  StoreBE32(args + 84, color);
  StoreBE32(args + 92, material);
  ctx = saved;
  ctx.f1.f64 = side, ctx.f3.f64 = inner;
  __imp__sub_82570C08(ctx, base);
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("widescreen: video pillarboxed, {}x{} in the middle of {}", inner, h, w);
  }
}

}  // namespace qos::widescreen

extern "C" REX_FUNC(sub_8256F478) {
  const uint32_t out = ctx.r3.u32;
  __imp__sub_8256F478(ctx, base);
  if (out) qos::widescreen::FixDisplaySetup(base + out);
}

extern "C" REX_FUNC(sub_8256F110) {
  __imp__sub_8256F110(ctx, base);
  qos::widescreen::FixVidConfig(base);
}

extern "C" REX_FUNC(sub_82566A48) {
  qos::widescreen::g_in_video = qos::widescreen::Wide();
  __imp__sub_82566A48(ctx, base);
  qos::widescreen::g_in_video = false;
}

extern "C" REX_FUNC(sub_82570C08) {
  if (qos::widescreen::g_in_video) {
    qos::widescreen::DrawVideoPillarboxed(ctx, base);
    return;
  }
  __imp__sub_82570C08(ctx, base);
}

extern "C" REX_FUNC(sub_821C1318) { qos::widescreen::SetUpPlacement(ctx, base); }
extern "C" REX_FUNC(sub_821C1710) { qos::widescreen::ApplyRect(ctx, base); }
extern "C" REX_FUNC(sub_821C1570) { qos::widescreen::ApplyX(ctx, base); }
extern "C" REX_FUNC(sub_82589B08) { qos::widescreen::DisplayMovie(ctx, base); }
