// qos - in-game cutscene letterbox off (--cutscene_letterbox=false, the default; true keeps the game's bars).
//
// The scripts' letterbox_on() (english/common.ff, maps/_utility-style) no longer draws black hud bars: it sets the
// saved dvar letterboxActive 1, puts the main view into a picture-in-picture window (r_pipMainMode 1, anchored top
// middle, a 2-pixel dark blue border cg_pipmain_border_color "0 0 0.2 1") and animates that window's height to 0.8
// with animatepip; letterbox_off() animates it back to 1 and sets r_pipMainMode 0. So the bars are the view cropped
// to 80% of the screen's height, not something drawn over it.
//
// Here, while letterboxActive is set, r_pipMainMode is kept at 0 at the start of every frame (Com_Frame, hooked in
// fps_unlock.cpp): the view stays full screen and the window's animation has nothing to show. Pre-rendered
// cutscene videos keep their own bars (they're in the video). Dvar pointers from their registration in the
// decrypted executable: r_pipMainMode (Dvar_RegisterInt, default 0, 0-4) at 0x84055EFC, letterboxActive
// (Dvar_RegisterBool) at 0x8264C054; current values at dvar + 12 (int, big-endian; bool, one byte).

#include "generated/default/qos_pch.h"

#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(cutscene_letterbox, false, "Game",
                    "Show the black bars of in-game cutscenes (off: the view stays full screen)");

namespace qos::letterbox {

namespace {
constexpr uint32_t kPipMainModePtr = 0x84055EFC;
constexpr uint32_t kLetterboxActivePtr = 0x8264C054;

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
}  // namespace

void OnFrame(uint8_t* base) {
  if (REXCVAR_GET(cutscene_letterbox)) return;
  const uint32_t active_dvar = LoadBE32(base + kLetterboxActivePtr);
  const uint32_t mode_dvar = LoadBE32(base + kPipMainModePtr);
  if (!active_dvar || !mode_dvar) return;
  if (!base[active_dvar + 12]) return;
  uint8_t* mode = base + mode_dvar + 12;
  if (LoadBE32(mode) == 0) return;
  static int logged = 0;
  if (logged < 4) {
    ++logged;
    REXLOG_INFO("letterbox: cutscene letterbox (r_pipMainMode {}) kept off", LoadBE32(mode));
  }
  mode[0] = mode[1] = mode[2] = mode[3] = 0;
}

}  // namespace qos::letterbox
