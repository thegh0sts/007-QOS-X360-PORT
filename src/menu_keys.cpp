// qos - Esc goes back in menus (--menu_esc_back, on by default; keyboard only).
//
// Esc is the pad's Start (pause). While a menu has focus, the SDK's keyboard pad presses B (back) for the Start
// keys instead (SDK patch 0018 SetStartKeysAsBack; a press keeps the meaning it started with, so the Esc that opens
// the pause menu doesn't also close it).
//
// "A menu has focus" is CoD4's key catcher for the UI: found 2026-11-24 in the decrypted executable. The pause
// menu's open (sub_822CE670) sets cl_paused 1 and calls sub_821BBED8(0, 16), the engine's CL_SetKeyCatchers, which
// keeps the console bit (1) and stores the int at 0x82809ADC (clientUIActive.keyCatchers); 16 is KEYCATCH_UI.
// The Press Start screen counts too (menu_quit.cpp's title detection), so holding Esc there quits like holding B,
// as its hint says (start_prompts_kbm: ENTER SELECT / ESC QUIT (HOLD)). Each frame (Com_Frame hook) it's passed on. Logs its first changes, with the whole value, so
// screens that use something else (the phone?) show up.

#include "generated/default/qos_pch.h"

#include <atomic>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(menu_esc_back, true, "Input", "Keyboard: Esc (the Start keys) goes back while a menu has focus");

namespace qos::menu_quit {
bool OnTitleScreen();
}  // namespace qos::menu_quit

// The phone (24 Nov): it sets the UI key catcher like the menus, so it's told apart by the dvars its screens set
// (dvar pointers from their registrations): hud_phoneObjectives (bool, "Signals the phone objectives list to draw",
// ptr 0x827CBC08), hud_phoneMap (bool, ptr 0x827CBC10) and r_phoneRenderMode (int, "Sets the phone render mode",
// ptr 0x84055D08; set to 0 by sub_82262158). Phone up = the UI catcher and any of them. Their changes are logged
// (first 48) to check the cameras and data screens. With keyboard prompts, pad_prompts.cpp then uses mode 3
// (prompts/keyboard_phone.txt over keyboard.txt): the phone's EXIT (the B picture) shows Tab.

namespace qos::menu_keys {

namespace {
constexpr uint32_t kKeyCatchers = 0x82809ADC;
constexpr uint32_t kKeyCatchUi = 0x10;
constexpr uint32_t kHudPhoneObjectives = 0x827CBC08, kHudPhoneMap = 0x827CBC10, kPhoneRenderMode = 0x84055D08;
bool g_menu = false;
std::atomic<bool> g_phone{false};
int g_phone_logged = 0;
uint32_t g_phone_last = 0xFFFFFFFF;

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
int g_logged = 0;
uint32_t g_last = 0xFFFFFFFF;
}  // namespace

bool PhoneUp() { return g_phone.load(std::memory_order_relaxed); }

void OnFrame(uint8_t* base) {
  const uint8_t* p = base + kKeyCatchers;
  const uint32_t catchers = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
  if (catchers != g_last && g_logged < 24) {
    ++g_logged;
    REXLOG_INFO("menu keys: key catchers {:#x}", catchers);
  }
  g_last = catchers;
  const bool menu =
      REXCVAR_GET(menu_esc_back) && ((catchers & kKeyCatchUi) != 0 || qos::menu_quit::OnTitleScreen());
  {
    const uint32_t objectives = Be32(base + kHudPhoneObjectives), map = Be32(base + kHudPhoneMap);
    const uint32_t render = Be32(base + kPhoneRenderMode);
    const uint32_t o = objectives ? base[objectives + 12] : 0, m = map ? base[map + 12] : 0;
    const uint32_t r = render ? Be32(base + render + 12) : 0;
    const uint32_t key = o | m << 1 | (r & 0xFF) << 2 | ((catchers & kKeyCatchUi) ? 1u << 10 : 0);
    if (key != g_phone_last && g_phone_logged < 48) {
      ++g_phone_logged;
      REXLOG_INFO("menu keys: phone objectives {} map {} render mode {} ui {}", o, m, r, (catchers & kKeyCatchUi) != 0);
    }
    g_phone_last = key;
    g_phone.store((catchers & kKeyCatchUi) != 0 && (o || m || r), std::memory_order_relaxed);
  }
  if (menu != g_menu) {
    g_menu = menu;
    rex::input::mnk::SetStartKeysAsBack(menu);
  }
}

}  // namespace qos::menu_keys
