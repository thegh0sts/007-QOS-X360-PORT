// qosmp - Esc goes back in menus (--menu_esc_back, on by default; keyboard only), as single player's
// src/menu_keys.cpp: while the UI key catcher is set, the SDK's keyboard pad presses B for the Start keys (SDK patch
// 0018 SetStartKeysAsBack; a press keeps the meaning it started with, so the Esc that opens the menu doesn't also
// close it). Without it Esc is START, bound to togglemenu in multiplayer (default_mp.cfg), so in a settings screen
// it closed the menu instead of going back (user, 25 Nov).
//
// Multiplayer keeps clientUIActive inside each local client's cl block (0x82C3A1D8 + n * 0x1FAE8C): keyCatchers is
// its int at +4. Found from Key_IsCatcherActive (sub_821C2FC8: (cl[n].keyCatchers & mask) != 0). KEYCATCH_UI is
// 0x10 as in single player. Called every frame from Com_Frame (frame_rate_fov.cpp).

#include "generated/default/qosmp_pch.h"

#include <cstdint>

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(menu_esc_back, true, "Input", "Keyboard: Esc (the Start keys) goes back while a menu has focus");

namespace qosmp::menu_keys {

namespace {
constexpr uint32_t kKeyCatchers = 0x82C3A1D8 + 4;  // local client 0
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

}  // namespace qosmp::menu_keys
