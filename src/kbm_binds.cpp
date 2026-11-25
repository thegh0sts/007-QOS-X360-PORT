// qos - default keyboard and mouse binds for the SDK's keyboard/mouse pad (--mnk_mode, --mnk_mouse).
//
// The game's default pad layout (buttons_default.cfg in english/code_post_gfx.ff, read 2026-11-23):
//   RT +attack (fire)          LT +speed (aim down sights)   RB +frag (grenade)     LB weapnext (swap weapon)
//   RS +melee_qk (takedown)    LS +dash (sprint)
//   A +cover2 (cover)          B +stance (crouch / exit cover)  X +usereload (use, reload)  Y +gostand2 (jump)
//   D-pad up nextfiretype      down togglesilencer      left/right grenadenext
// The keys follow the PC version's manual (Quantum of Solace PC, "Movement Controls", user 24 Nov): WASD move,
// mouse look, mouse 1 fire, mouse 2 aim down sight, G grenade, 3 switch grenade, Q take down, Ctrl sprint, 1 & 2
// cycle weapon, C stance, Shift walk, R reload, Space jump/mantle, E cover/dash to cover, 4 silencer, X rate of fire,
// Tab phone, Esc pause, F use. The pad's X is both use and reload (+usereload), so R and F both press it. Shift walks
// through SDK patch 0017 (keybind_walk: the stick keys push the stick part way, mnk_walk_scale). Menus: Return as A
// too (B is C only: Backspace unbound, user 24 Nov), arrows as the D-pad. The right stick has no keys: the mouse turns the view
// (mouse_look.cpp). Only binds still at the SDK's compiled-in default are set, so the command line, the config file
// and the Rebind screen win.

// Also built into multiplayer (mp/), whose pad layout puts the same actions on the same buttons (default_mp.cfg execs
// buttons_default_mp.cfg, then BACK -> scoreboard, START -> menu): the keys follow the pad positions.
#if __has_include("generated/default/qos_pch.h")
#include "generated/default/qos_pch.h"
#endif

#include <string_view>

#include <rex/cvar.h>
#include <rex/logging.h>

namespace qos::kbm_binds {

void SetDefaults() {
  static constexpr std::pair<std::string_view, std::string_view> kBinds[] = {
      {"keybind_lstick_up", "W"},           {"keybind_lstick_down", "S"},
      {"keybind_lstick_left", "A"},         {"keybind_lstick_right", "D"},
      {"keybind_lstick_press", "Control"},  {"keybind_rstick_press", "Q"},
      {"keybind_rstick_up", ""},            {"keybind_rstick_down", ""},
      {"keybind_rstick_left", ""},          {"keybind_rstick_right", ""},
      {"keybind_right_trigger", "LMB"},     {"keybind_left_trigger", "RMB"},
      {"keybind_right_shoulder", "G"},      {"keybind_left_shoulder", "1,2"},
      {"keybind_a", "E,Return"},            {"keybind_b", "C"},
      {"keybind_x", "R,F"},                {"keybind_y", "Space"},
      {"keybind_dpad_up", "X,Up"},          {"keybind_dpad_down", "4,Down"},
      {"keybind_dpad_left", "3,Left"},      {"keybind_dpad_right", "Right"},
      {"keybind_back", "Tab"},              {"keybind_start", "Escape"},
      {"keybind_walk", "Shift"},
  };
  int set = 0;
  for (const auto& [name, keys] : kBinds) {
    if (rex::cvar::GetFlagSource(name) != rex::cvar::Source::kDefault) continue;
    if (rex::cvar::SetFlagByName(name, keys)) ++set;
  }
  REXLOG_INFO("keyboard binds: {} of {} set to 007's defaults (the rest came from the config or command line)", set,
              std::size(kBinds));
}

}  // namespace qos::kbm_binds
