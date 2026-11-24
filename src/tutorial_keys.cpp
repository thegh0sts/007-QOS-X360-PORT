// qos - a button picture per command in tutorial text, for keyboard prompts.
//
// The tutorial and hint texts are localized strings with command tokens, e.g.
//   "Leave cover by pulling away from it with [[{move}]]"            (WHITES_ESTATE_TUTORIAL_LEAVE)
//   "Hold [[{+stance}]] and pull away from cover with [[{move}]] to stay low"
//   "Click [[{+dash}]] while pushing forward to sprint"               (PLATFORM_TUTORIAL_SPRINT)
//   "Press [[{+cover}]] to take cover", "Click [[{+melee_qk}]] to take down an enemy" ...
// sub_823D5480 turns each [[{cmd}]] into <img src="NAME" width="42" height="42" vspace="-20"> for Scaleform; NAME
// comes from sub_823D4580(r3 = movie, r4 = command): "move" / "look" give stick_left / stick_right, any other command
// the pad button it's bound to, through a table at 0x8261CCF0 (button -> wide image name: 1 btn_accept, 2 btn_back,
// 3 btn_refresh, 4 btn_y, 5 bumper_left, 6 bumper_right, 15 button_back, 16 stick_left, 17 stick_right,
// 18 trigger_left, 19 trigger_right, 20 btn_nav_up, 21 btn_nav_down). The images are the shared platform library's
// (scaleform/CmSharedPlatform.gfx), which also holds btn_left, btn_right, btn_navigate_h/v/all.
//
// So two commands on one pad button get one picture: "move" and "+dash" both the left stick, "+stance" the menus'
// B. The keyboard prompts (prompts/keyboard.txt) swap pictures by texture, so they can't tell them apart. While
// keyboard prompts are on, this hook gives such commands a picture of their own from the library's unused ones,
// which keyboard.txt then shows as the right key:
//   +dash   -> btn_left   (Ctrl: sprint)
//   +stance -> btn_right  (C: stance)
//   +cover  -> btn_navigate_h (E: cover; the menus' A picture, btn_accept, then shows Enter like the title screen)
// Every command it sees is logged once ("tutorial keys: ..."), to find more.

#include "generated/default/qos_pch.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(tutorial_keys, true, "Input",
                    "Keyboard prompts: give sprint, stance and cover their own pictures in tutorial text");

REX_EXTERN(__imp__sub_823D4580);  // command -> button image name (wide string)

namespace qos::pad_prompts {
bool Keyboard();
}  // namespace qos::pad_prompts

namespace {

struct Swap {
  const char* command;
  const char* image;
  uint32_t guest = 0;  // the image name as a wide (UTF-16BE) string in guest memory
};
Swap g_swaps[] = {{"+dash", "btn_left"}, {"+stance", "btn_right"}, {"+cover", "btn_navigate_h"}};
std::mutex g_mutex;
std::set<std::string> g_logged;

uint32_t GuestWide(uint8_t* base, const char* text) {
  const size_t n = std::strlen(text);
  const uint32_t a = rex::system::kernel_state()->memory()->SystemHeapAlloc(uint32_t((n + 1) * 2));
  if (!a) return 0;
  for (size_t i = 0; i <= n; ++i) {
    base[a + 2 * i] = 0;
    base[a + 2 * i + 1] = uint8_t(text[i]);  // ASCII names
  }
  return a;
}

}  // namespace

extern "C" REX_FUNC(sub_823D4580) {
  char command[64] = {};
  const char* src = reinterpret_cast<const char*>(base + ctx.r4.u32);
  for (int i = 0; i < 63 && src[i]; ++i) command[i] = src[i];
  const bool keys = REXCVAR_GET(tutorial_keys) && qos::pad_prompts::Keyboard();
  uint32_t swap = 0;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (keys) {
      for (Swap& s : g_swaps) {
        if (std::strcmp(command, s.command) != 0) continue;
        if (!s.guest) s.guest = GuestWide(base, s.image);
        swap = s.guest;
        break;
      }
    }
    if (g_logged.size() < 64 && g_logged.insert(command).second) {
      REXLOG_INFO("tutorial keys: [[{{{}}}]]{}", command, swap ? " -> its own picture" : "");
    }
  }
  if (swap) {
    ctx.r3.u64 = swap;
    return;
  }
  __imp__sub_823D4580(ctx, base);
}
