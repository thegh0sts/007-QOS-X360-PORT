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
//   +cover  -> btn_navigate_h (E)
// Some hints are reworded for keyboard too (user, 24 Nov), in sub_823D5480's call of sub_822C7298
// (SEH_LocalizeTextMessage: localized text for the string id; hooked only while sub_823D5480 runs):
//   "Click [[{+dash}]] while pushing forward to sprint"  -> "Holding [[{+dash}]] while pressing [[{kbm_w}]] to sprint"
//   "Press [[{weapnext}]] to switch weapons"              -> "Press [[{weapnext}]] / [[{kbm_2}]] to switch weapons"
//   "Press up on the D-Pad to change the fire mode"       -> "Press [[{nextfiretype}]] to change the fire mode"
//   "Press Down on the D-Pad to attach/remove ..."        -> "Press [[{togglesilencer}]] to attach/remove ..."
// with more pictures from the library: kbm_w -> btn_navigate_all (W), kbm_2 -> btn_y (2), nextfiretype ->
// btn_nav_up (X), togglesilencer -> btn_nav_down (4). btn_navigate_v is left alone: the phone's cameras screen
// shows it (scroll cameras). English text only.
// Every command it sees is logged once ("tutorial keys: ..."), to find more.

#include "generated/default/qos_pch.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(tutorial_keys, true, "Input",
                    "Keyboard prompts: give sprint, stance and cover their own pictures in tutorial text");
REXCVAR_DEFINE_BOOL(tutorial_keys_test, false, "Input",
                    "Keyboard prompts test: every hint shows the library's spare pictures, numbered 1-6, "
                    "to find which look alike and to dump their textures");

REX_EXTERN(__imp__sub_823D4580);  // command -> button image name (wide string)
REX_EXTERN(__imp__sub_823D5480);  // hint text -> Scaleform HTML with button images
REX_EXTERN(__imp__sub_822C7298);  // SEH_LocalizeTextMessage

namespace qos::pad_prompts {
bool Keyboard();
}  // namespace qos::pad_prompts

namespace {

struct Swap {
  const char* command;
  const char* image;
  uint32_t guest = 0;  // the image name as a wide (UTF-16BE) string in guest memory
};
Swap g_swaps[] = {{"+dash", "btn_left"},          {"+stance", "btn_right"},        {"+cover", "btn_navigate_h"},
                  {"kbm_w", "btn_navigate_all"},   {"kbm_2", "btn_y"},              {"nextfiretype", "btn_nav_up"},
                  {"togglesilencer", "btn_nav_down"},
                  // tutorial_keys_test: the spare pictures, one each
                  {"kbm_t1", "btn_left"},          {"kbm_t2", "btn_right"},         {"kbm_t3", "btn_navigate_h"},
                  {"kbm_t4", "btn_navigate_v"},    {"kbm_t5", "btn_navigate_all"},  {"kbm_t6", "btn_y"}};
constexpr char kTestText[] =
    "1 [[{kbm_t1}]] 2 [[{kbm_t2}]] 3 [[{kbm_t3}]] 4 [[{kbm_t4}]] 5 [[{kbm_t5}]] 6 [[{kbm_t6}]]";
uint32_t g_test_text = 0;

struct Reword {
  const char* find;
  const char* replace;
};
constexpr Reword kRewords[] = {
    {"Click [[{+dash}]] while pushing forward to sprint", "Holding [[{+dash}]] while pressing [[{kbm_w}]] to sprint"},
    {"Press [[{weapnext}]] to switch weapons", "Press [[{weapnext}]] / [[{kbm_2}]] to switch weapons"},
    {"Press up on the D-Pad to change the fire mode", "Press [[{nextfiretype}]] to change the fire mode"},
    {"Press Down on the D-Pad to attach/remove", "Press [[{togglesilencer}]] to attach/remove"},
};
thread_local bool t_in_hint = false;  // inside sub_823D5480 (hint text with [[{cmd}]] tokens)
std::unordered_map<std::string, uint32_t> g_reworded;  // original text -> reworded copy in guest memory
int g_reworded_logged = 0;

uint32_t GuestText(uint8_t* base, const std::string& text) {
  const uint32_t a = rex::system::kernel_state()->memory()->SystemHeapAlloc(uint32_t(text.size() + 1));
  if (a) std::memcpy(base + a, text.c_str(), text.size() + 1);
  return a;
}
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

extern "C" REX_FUNC(sub_823D5480) {
  t_in_hint = true;
  __imp__sub_823D5480(ctx, base);
  t_in_hint = false;
}

extern "C" REX_FUNC(sub_822C7298) {
  __imp__sub_822C7298(ctx, base);
  if (!t_in_hint || !ctx.r3.u32 || !REXCVAR_GET(tutorial_keys) || !qos::pad_prompts::Keyboard()) return;
  const std::string text(reinterpret_cast<const char*>(base + ctx.r3.u32));
  std::lock_guard<std::mutex> lock(g_mutex);
  if (REXCVAR_GET(tutorial_keys_test)) {
    if (!g_test_text) g_test_text = GuestText(base, kTestText);
    if (g_test_text) ctx.r3.u64 = g_test_text;
    return;
  }
  auto it = g_reworded.find(text);
  if (it == g_reworded.end()) {
    std::string out = text;
    for (const Reword& r : kRewords) {
      const size_t at = out.find(r.find);
      if (at != std::string::npos) out.replace(at, std::strlen(r.find), r.replace);
    }
    const uint32_t guest = out == text ? 0 : GuestText(base, out);
    it = g_reworded.emplace(text, guest).first;
    if (guest && g_reworded_logged < 16) {
      ++g_reworded_logged;
      REXLOG_INFO("tutorial keys: \"{}\" -> \"{}\"", text, out);
    }
  }
  if (it->second) ctx.r3.u64 = it->second;
}
