// qos - PlayStation button pictures with a PlayStation pad.
//
// Every button picture in 007's menus is a texture: Scaleform's platform library (scaleform/CmSharedPlatform.gfx)
// draws btn_accept (A), btn_back (B), btn_y, bumpers, triggers, sticks and so on from images in ui.ff, 64x64 DXT5
// discs. With a DualShock or DualSense connected, the native renderer shows our own PlayStation pictures in their place
// as it loads them (native-plugin/patches/0021-replace-textures.patch), listed in prompts/playstation.txt by the
// hash of the game's texture (found with a BOTS_GFX_DUMP_TEXTURES run, patch 0020); the pictures are drawn by
// tools/make_ps_buttons.py. Which pad is connected comes from the SDK (rex::input::ConnectedPadStyle); this passes
// the choice on through --pad_prompts_active, read by the renderer each frame. Same scheme as the Eat Lead port.
//
// --pad_prompts=auto         PlayStation pictures while a PlayStation pad is the one connected (default)
// --pad_prompts=xbox         always the game's own
// --pad_prompts=playstation  always PlayStation's
// With no pad connected (one unplugged before the next is plugged in) the last pad's pictures stay.
//
// Keyboard and mouse (23 Nov): while keyboard or mouse is what player 1 used last (the SDK's active-device rule:
// SDK patch 0016 KeyboardMouseActive), pad_prompts_active is 2 and the renderer shows the key pictures listed in
// prompts/keyboard.txt (native patch 0098), whatever --pad_prompts says; --pad_prompts=keyboard always shows them.
//
// Not done yet: text that names buttons (the Press Start screen's text, the controls screen's labels).

// Also built into multiplayer (mp/): there Update() runs from the Com_Frame hook and PhoneUp() is always false.
#if __has_include("generated/default/qos_pch.h")
#include "generated/default/qos_pch.h"
#endif

#include <cstdint>
#include <string>

#include <rex/cvar.h>
#include <rex/input/pad_style.h>
#include <rex/logging.h>

REXCVAR_DEFINE_STRING(pad_prompts, "auto", "Input",
                      "Button prompts: auto (PlayStation buttons with a PlayStation pad, keys while keyboard/mouse is in "
                      "use), xbox, playstation, keyboard");
REXCVAR_DEFINE_INT32(pad_prompts_active, 0, "Input",
                     "The buttons the prompts show now, 0 Xbox, 1 PlayStation, 2 keyboard, 3 keyboard with the phone up; set by the game for the renderer");

namespace qos::menu_keys {
bool PhoneUp();
}  // namespace qos::menu_keys

namespace qos::pad_prompts {

// Called on every controller poll (menu_quit.cpp's XInputGetState hook).
void Update() {
  static rex::input::PadStyle last_pad = rex::input::PadStyle::kXbox;
  if (const auto style = rex::input::ConnectedPadStyle(); style != rex::input::PadStyle::kNone) last_pad = style;
  const std::string mode = REXCVAR_GET(pad_prompts);
  const bool keys = mode == "keyboard" || (mode != "xbox" && mode != "playstation" && rex::input::KeyboardMouseActive());
  const bool ps = mode == "playstation" || (mode == "auto" && last_pad == rex::input::PadStyle::kPlayStation);
  const int32_t active = keys ? (qos::menu_keys::PhoneUp() ? 3 : 2) : ps ? 1 : 0;
  if (REXCVAR_GET(pad_prompts_active) != active) {
    REXCVAR_SET(pad_prompts_active, active);
    REXLOG_INFO("pad_prompts: {}", active == 3 ? "keyboard keys (phone)"
                                    : keys      ? "keyboard keys"
                                    : ps        ? "PlayStation buttons"
                                                : "Xbox buttons");
  }
}

bool PlayStation() { return REXCVAR_GET(pad_prompts_active) == 1; }
bool Keyboard() { return REXCVAR_GET(pad_prompts_active) >= 2; }

}  // namespace qos::pad_prompts
