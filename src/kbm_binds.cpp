// qos - default keyboard and mouse binds for the SDK's keyboard/mouse pad (--mnk_mode, --mnk_mouse).
//
// The game's default pad layout (buttons_default.cfg in english/code_post_gfx.ff, read 2026-11-23):
//   RT +attack (fire)          LT +speed (aim down sights)   RB +frag (grenade)     LB weapnext (swap weapon)
//   RS +melee_qk (takedown)    LS +dash (sprint)
//   A +cover2 (cover)          B +stance (crouch / exit cover)  X +usereload (use, reload)  Y +gostand2 (jump)
//   D-pad up nextfiretype      down togglesilencer      left/right grenadenext
// The keys follow the PC version's layout (user, 21 Nov): WASD move, Space jump, E cover, mouse 1 fire,
// mouse 2 aim, R reload/use, G grenade, 3 grenade type, 4 silencer, X fire mode, Q takedown, Tab phone; and
// Shift sprint, C crouch, F / 1 / 2 / middle button swap weapon, Escape pause, Return and Backspace for the menus'
// A and B, arrows for the D-pad (menus). The right stick has no keys: the mouse turns the view (mouse_look.cpp).
// Only binds still at the SDK's compiled-in default are set, so the command line, the config file and the
// Rebind screen win.

#include "generated/default/qos_pch.h"

#include <string_view>

#include <rex/cvar.h>
#include <rex/logging.h>

namespace qos::kbm_binds {

void SetDefaults() {
  static constexpr std::pair<std::string_view, std::string_view> kBinds[] = {
      {"keybind_lstick_up", "W"},           {"keybind_lstick_down", "S"},
      {"keybind_lstick_left", "A"},         {"keybind_lstick_right", "D"},
      {"keybind_lstick_press", "Shift"},    {"keybind_rstick_press", "Q"},
      {"keybind_rstick_up", ""},            {"keybind_rstick_down", ""},
      {"keybind_rstick_left", ""},          {"keybind_rstick_right", ""},
      {"keybind_right_trigger", "LMB"},     {"keybind_left_trigger", "RMB"},
      {"keybind_right_shoulder", "G"},      {"keybind_left_shoulder", "F,1,2,MMB"},
      {"keybind_a", "E,Return"},            {"keybind_b", "C,Control,Backspace"},
      {"keybind_x", "R"},                   {"keybind_y", "Space"},
      {"keybind_dpad_up", "X,Up"},          {"keybind_dpad_down", "4,Down"},
      {"keybind_dpad_left", "3,Left"},      {"keybind_dpad_right", "Right"},
      {"keybind_back", "Tab"},              {"keybind_start", "Escape"},
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
