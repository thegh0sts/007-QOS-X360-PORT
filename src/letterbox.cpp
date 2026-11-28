// qos - in-game cutscene letterbox off (--cutscene_letterbox=false, the default; true keeps the game's bars).
//
// The scripts' letterbox_on() (english/common.ff, maps/_utility-style) no longer draws black hud bars: it sets the
// saved dvar letterboxActive 1, puts the main view into a picture-in-picture window (r_pipMainMode 1, anchored top
// middle, a 2-pixel dark blue border: cg_pipmain_border 2, cg_pipmain_border_color "0 0 0.2 1") and animates that
// window's height to 0.8 with animatepip(1000, 1, -1, -1, 1, 0.8, 1, 0); letterbox_off() animates it back to 1 and
// sets r_pipMainMode 0. So the bars are the view cropped to 80% of the screen's height, not something drawn over it.
//
// The window has to stay: some cutscenes frame their shot through it (mission 11, the reception desk after the
// keypad, 28 Nov: with r_pipMainMode held at 0, as this file first did, the camera and Bond were out of place).
// So only the crop goes. The script function animatePip (sub_82230440) sends the client the command
// "animatePip <time> <screen> <x> <y> <scalex> <scaley> <cropx> <cropy>" through SV_GameSendServerCommand
// (sub_822C1F50, r3 client, r4 text): while letterboxActive is set, a main-screen (1) command's scaley is rewritten
// to 1.000000 in place (same length), so the window stays full screen, and cg_pipmain_border (float, dvar pointer
// 0x8276A078) is held at 0 each frame (Com_Frame, hooked in fps_unlock.cpp), so there's no border. Pre-rendered
// cutscene videos keep their own bars (they're in the video). letterboxActive (Dvar_RegisterBool) at 0x8264C054;
// current values at dvar + 12.

#include "generated/default/qos_pch.h"

#include <cstdint>
#include <cstring>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(cutscene_letterbox, false, "Game",
                    "Show the black bars of in-game cutscenes (off: the view stays full screen)");

REX_EXTERN(__imp__sub_822C1F50);  // SV_GameSendServerCommand (r3 client, r4 text)

namespace qos::widescreen {
bool Wide();  // widescreen.cpp: an ultrawide back buffer
}

namespace qos::letterbox {

// The bars as asked for; never with an ultrawide picture (it is already wider than the bars' 80% crop of 16:9, and
// the crop would leave a ~3:1 strip).
bool BarsOn() { return REXCVAR_GET(cutscene_letterbox) && !qos::widescreen::Wide(); }

namespace {
constexpr uint32_t kLetterboxActivePtr = 0x8264C054;
constexpr uint32_t kPipMainBorderPtr = 0x8276A078;

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

bool LetterboxActive(const uint8_t* base) {
  const uint32_t dvar = LoadBE32(base + kLetterboxActivePtr);
  return dvar && base[dvar + 12];
}
}  // namespace

void OnFrame(uint8_t* base) {
  if (BarsOn() || !LetterboxActive(base)) return;
  const uint32_t border = LoadBE32(base + kPipMainBorderPtr);
  if (!border) return;
  uint8_t* value = base + border + 12;
  if (LoadBE32(value) != 0) std::memset(value, 0, 4);  // 0.0f
}

// The command's scaley: rewritten to 1.000000 when it's the main screen's and less than full height.
void FullHeight(uint8_t* base, uint32_t text) {
  char* s = reinterpret_cast<char*>(base + text);
  constexpr char kName[] = "animatePip ";
  if (std::strncmp(s, kName, sizeof(kName) - 1) != 0) return;
  // Fields after the name: time, screen, x, y, scalex, scaley, ...
  char* field[8] = {};
  int n = 0;
  for (char* p = s + sizeof(kName) - 1; *p && n < 8;) {
    while (*p == ' ') ++p;
    if (!*p) break;
    field[n++] = p;
    while (*p && *p != ' ') ++p;
  }
  if (n < 6 || std::strncmp(field[1], "1 ", 2) != 0) return;
  char* scaley = field[5];
  size_t length = 0;
  while (scaley[length] && scaley[length] != ' ') ++length;
  constexpr char kFull[] = "1.000000";
  if (length != sizeof(kFull) - 1 || std::strncmp(scaley, kFull, length) == 0) return;
  static int logged = 0;
  if (logged < 4) {
    ++logged;
    REXLOG_INFO("letterbox: cutscene view kept full height (animatePip scaley {} -> 1)", std::string(scaley, length));
  }
  std::memcpy(scaley, kFull, length);
}

}  // namespace qos::letterbox

extern "C" REX_FUNC(sub_822C1F50) {
  if (!qos::letterbox::BarsOn() && ctx.r4.u32 && qos::letterbox::LetterboxActive(base)) {
    qos::letterbox::FullHeight(base, ctx.r4.u32);
  }
  __imp__sub_822C1F50(ctx, base);
}
