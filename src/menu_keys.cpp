// qos - Esc goes back in menus (--menu_esc_back, on by default; keyboard only).
//
// Esc is the pad's Start (pause). While a menu has focus, the SDK's keyboard pad presses B (back) for the Start
// keys instead (SDK patch 0018 SetStartKeysAsBack; a press keeps the meaning it started with, so the Esc that opens
// the pause menu doesn't also close it).
//
// "A menu has focus" is CoD4's key catcher for the UI: found 2026-11-24 in the decrypted executable. The pause
// menu's open (sub_822CE670) sets cl_paused 1 and calls sub_821BBED8(0, 16), the engine's CL_SetKeyCatchers, which
// keeps the console bit (1) and stores the int at 0x82809ADC (clientUIActive.keyCatchers); 16 is KEYCATCH_UI.
// Each frame (Com_Frame hook) the bit is read and passed on. Logs its first changes, with the whole value, so
// screens that use something else (the phone?) show up.

#include "generated/default/qos_pch.h"

#include <cstdint>

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(menu_esc_back, true, "Input", "Keyboard: Esc (the Start keys) goes back while a menu has focus");

namespace qos::menu_keys {

namespace {
constexpr uint32_t kKeyCatchers = 0x82809ADC;
constexpr uint32_t kKeyCatchUi = 0x10;
bool g_menu = false;
int g_logged = 0;
uint32_t g_last = 0xFFFFFFFF;
}  // namespace

void OnFrame(uint8_t* base) {
  const uint8_t* p = base + kKeyCatchers;
  const uint32_t catchers = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
  if (catchers != g_last && g_logged < 24) {
    ++g_logged;
    REXLOG_INFO("menu keys: key catchers {:#x}", catchers);
  }
  g_last = catchers;
  const bool menu = REXCVAR_GET(menu_esc_back) && (catchers & kKeyCatchUi) != 0;
  if (menu != g_menu) {
    g_menu = menu;
    rex::input::mnk::SetStartKeysAsBack(menu);
  }
}

}  // namespace qos::menu_keys
