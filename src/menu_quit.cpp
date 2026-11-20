// qos - holding B on the "Press Start" title screen closes the game, with a "B QUIT (HOLD)" hint.
//
// On the console, B does nothing on the title screen ("PRESS START"): there's no way back from it. On PC that leaves
// no way out but the window's close button, so holding B there for a second quits (held, so a stray press doesn't),
// and a "B QUIT (HOLD)" prompt in the game's own prompt style shows where the main menu has its "A SELECT".
//
// How the title screen is told apart (from the decrypted executable, 2026-11-20):
// - The dvar ui_reachedIIS: "Has the player reached the PRESS START IIS? 0 not yet, 1-2 there now (normal=1/
//   brightness=2), 3 past it". Registered through Dvar_RegisterInt (sub_822E4188, hooked in fps_unlock.cpp, which
//   passes every registration to OnDvarRegistered here); its current value is the big-endian int at dvar + 12.
//   1 and 2 are both the title screen (2 when the brightness setup follows it, as at a first start: seen on the PC
//   2026-11-20, the Press Start screen showed with 2); 3 is past it.
// - Attract mode: after com_attractmodeduration (90 s) without input, the title screen plays a demo. sub_821AF8C8
//   starts it ("InAttract") and sets the byte at 0x827CDD8C to 1; sub_821AF7B8 clears it when the demo ends. While it
//   is set, B is left to the game (it ends the demo) and no hint shows (the demo is a video too: see below).
// - Videos: ui_reachedIIS is already 2 while the intro logos play (Treyarch, Activision, MGM, the 007 logo), so a
//   playing video is ruled out too. The engine's cinematic state (CoD4's cinematicGlob) is at 0x841E56A0: +256 the
//   current video's name (cleared by R_Cinematic_StopPlayback, sub_825667C8), +777 "finished" (returned by
//   sub_82566BA8; the attract code stops the video once it's set). A name and not finished = a video is playing,
//   except the menus' looping backgrounds (bink_bkg_loop_0/1, bink_ig_bkg_loop_1: the title screen itself plays
//   bink_bkg_loop_1 behind it, seen 2026-11-20), which don't count.
// - The title screen counts only after all of these have held for 0.4 s, so the press that ends a demo or opens the screen
//   doesn't also quit, and the hint doesn't flash during transitions.
//
// The controller: the engine reads all four pads every frame through XInputGetState (sub_82425198: r3 user, r4
// XINPUT_STATE*, big-endian, buttons at +4), hooked here. B pressed on the title screen (after it has settled) and
// held for kHold on that pad -> quit; letting go, or the screen changing, cancels it.
//
// The hint is drawn by the native renderer (native-plugin patch 0013, from the Eat Lead port): --menu_hint picks image
// N of --menu_hint_images (hints/<name>.png beside the exe), drawn at --menu_hint_pos (top-left, 1080p pixels).
// b_quit.png / b_quit_ps.png are our own art (tools/make_quit_hint.py); the PlayStation one shows with PlayStation
// prompts (pad_prompts.cpp). The Xenos renderer doesn't draw hints; B still quits there.
// --menu_b_quit=false turns all of it off.

#include "generated/default/qos_pch.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>

#include <rex/cvar.h>
#include <rex/input/pad_style.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

REXCVAR_DEFINE_BOOL(menu_b_quit, true, "UI", "Holding B on the Press Start screen closes the game");
// Read by the native renderer each frame (native-plugin/patches/0013-menu-hint.patch); set here, not by hand.
REXCVAR_DEFINE_INT32(menu_hint, 0, "UI", "Menu hint the renderer draws: 0 none, N the Nth of menu_hint_images; set by the game");
REXCVAR_DEFINE_STRING(menu_hint_images, "b_quit,b_quit_ps", "UI", "The menu hints' images, hints/<name>.png, comma-separated");
REXCVAR_DEFINE_STRING(menu_hint_pos, "354,922", "UI", "Where the menu hint goes: top-left x,y in 1080p pixels of the picture");

namespace qos::pad_prompts {
void Update();       // pad_prompts.cpp
bool PlayStation();
}  // namespace qos::pad_prompts

namespace {

constexpr uint32_t kAttractFlag = 0x827CDD8C;  // byte: 1 while the attract-mode demo plays
constexpr uint32_t kCinematic = 0x841E56A0;    // cinematicGlob
constexpr uint32_t kCinematicName = kCinematic + 256;     // char[]: the current video, "" when none
constexpr uint32_t kCinematicFinished = kCinematic + 777;  // byte: the video has reached its end
constexpr uint16_t kButtonB = 0x2000;
constexpr auto kSettle = std::chrono::milliseconds(400);
constexpr auto kHold = std::chrono::milliseconds(1000);

using Clock = std::chrono::steady_clock;

std::atomic<uint32_t> g_reached_iis{0};  // guest address of the ui_reachedIIS dvar, 0 until registered

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

struct State {
  std::mutex mutex;
  uint16_t last[4] = {};
  bool title = false;           // title screen, no demo, as of the last poll
  Clock::time_point since;      // when `title` last changed
  bool quitting = false;
  int holding = -1;              // the pad holding B to quit, -1 none
  Clock::time_point hold_since;
  int32_t reached_logged = -1;  // last ui_reachedIIS value logged
  char video_name_logged[64] = {};  // last video name logged
};

State& S() {
  static State s;
  return s;
}

void RequestQuit() {
  REXLOG_INFO("menu_b_quit: B held on the Press Start screen, closing the game");
  auto* runtime = rex::Runtime::instance();
  if (!runtime) return;
  auto* window = runtime->display_window();
  auto* app = runtime->app_context();
  if (window && app) app->CallInUIThread([window] { window->RequestClose(); });
}

}  // namespace

namespace qos::menu_quit {

// Every Dvar_RegisterInt: r3 was the name, `dvar` the returned dvar (guest address).
void OnDvarRegistered(const char* name, uint32_t dvar) {
  if (dvar && std::strcmp(name, "ui_reachedIIS") == 0) {
    g_reached_iis.store(dvar, std::memory_order_relaxed);
    REXLOG_INFO("menu_b_quit: ui_reachedIIS at {:08X}", dvar);
  }
}

// Each XInputGetState: user 0-3, `buttons` that pad's buttons now.
void OnPad(uint8_t* base, uint32_t user, uint16_t buttons) {
  if (!REXCVAR_GET(menu_b_quit)) {
    if (REXCVAR_GET(menu_hint) != 0) REXCVAR_SET(menu_hint, 0);
    return;
  }
  const uint32_t dvar = g_reached_iis.load(std::memory_order_relaxed);
  const int32_t reached = dvar ? int32_t(LoadBE32(base + dvar + 12)) : -1;
  const bool attract = base[kAttractFlag] != 0;
  const char* video_name = reinterpret_cast<const char*>(base + kCinematicName);
  const bool background = std::strncmp(video_name, "bink_bkg_loop", 13) == 0 ||
                          std::strncmp(video_name, "bink_ig_bkg_loop", 16) == 0;
  const bool video = video_name[0] != 0 && base[kCinematicFinished] == 0 && !background;
  bool quit = false;
  {
    State& s = S();
    std::lock_guard<std::mutex> lock(s.mutex);
    const auto now = Clock::now();
    if (reached != s.reached_logged) {
      REXLOG_INFO("menu_b_quit: ui_reachedIIS {}", reached);
      s.reached_logged = reached;
    }
    char name[64] = {};
    for (int i = 0; i < 63 && video_name[i]; ++i) name[i] = video_name[i];
    if (std::strcmp(name, s.video_name_logged) != 0) {
      REXLOG_INFO("menu_b_quit: video {}{}", name[0] ? name : "(none)", background ? " (menu background)" : "");
      std::memcpy(s.video_name_logged, name, sizeof(name));
    }
    const bool title = (reached == 1 || reached == 2) && !attract && !video;
    if (title != s.title) {
      s.title = title;
      s.since = now;
    }
    const bool settled = title && now - s.since >= kSettle && !s.quitting;
    const bool ps = qos::pad_prompts::PlayStation();
    const int32_t hint = !settled ? 0 : ps ? 2 : 1;  // b_quit / b_quit_ps
    if (REXCVAR_GET(menu_hint) != hint) REXCVAR_SET(menu_hint, hint);
    const uint16_t pressed = buttons & ~s.last[user];
    s.last[user] = buttons;
    if (s.holding == int(user) && (!settled || !(buttons & kButtonB))) {
      s.holding = -1;  // let go, or the screen changed
    } else if (s.holding < 0 && settled && (pressed & kButtonB)) {
      s.holding = int(user);
      s.hold_since = now;
    }
    if (s.holding == int(user) && now - s.hold_since >= kHold) s.quitting = quit = true;
  }
  if (quit) RequestQuit();
}

}  // namespace qos::menu_quit

REX_EXTERN(__imp__sub_82425198);  // XInputGetState(user, XINPUT_STATE*)

extern "C" REX_FUNC(sub_82425198) {
  const uint32_t user = ctx.r3.u32;
  const uint32_t state = ctx.r4.u32;
  __imp__sub_82425198(ctx, base);
  if (user == 0) qos::pad_prompts::Update();
  if (user > 3 || !state) return;
  // r3 is the result: 0 (ERROR_SUCCESS) when a pad is connected there; treat others as no buttons held.
  const uint16_t buttons = ctx.r3.u32 == 0 ? uint16_t(base[state + 4] << 8 | base[state + 5]) : 0;
  qos::menu_quit::OnPad(base, user, buttons);
}
