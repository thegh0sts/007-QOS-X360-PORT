// qos - damage blur and bloom on or off (--damage_blur, --bloom; both default true). Shared by both programs.
//
// Damage blur: the blur over the screen when Bond is hurt, drawn by the HUD code (sub_8217E110) while the dvars
// cg_showHurtBlur and cg_showVeryHurtBlur are set. Both are bools registered at 1, not saved; their pointers, from the
// registration in the decrypted single-player executable: 0x8266F534 and 0x827700EC (value byte at dvar + 12). A
// takedown's start saves cg_showHurtBlur and sets its own value, its end puts it back (sub_823866D0 / sub_823877C8).
// Off: both bytes kept at 0 at the start of every frame. Multiplayer has neither dvar (no damage blur there).
//
// Bloom: each frame the renderer fills the view's glow settings (sub_82561DC0, multiplayer sub_824C6D98: 56 bytes at
// r3 + 22916, then a copy of them at 0x841EF1B4, multiplayer 0x8518FE64) from the level's vision set, or from the
// r_glowTweak* dvars while r_glowUseTweaks is set. Layout, from the tweak path: +0 enabled (byte), +4 bloom cutoff,
// +8 bloom desaturation, +12 bloom intensity, +16 radius, +20 ray expansion, +24 ray intensity, then more from the
// vision set. (The dvar r_glow is registered but never read.) Off: the bloom intensity is set to 0 in both copies
// after the function runs (src/glow_hook.cpp, mp/src/glow_hook.cpp), so nothing is added to the picture; the glow
// pass and the light rays are left as they are.

#if __has_include("generated/default/qos_pch.h")
#include "generated/default/qos_pch.h"
#endif

#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(damage_blur, true, "Game", "Blur over the screen when hurt (off: none)");
REXCVAR_DEFINE_BOOL(bloom, true, "Game", "Bloom: the glow around bright light (off: none; light rays stay)");

namespace qos::effects {

namespace {
uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

void HoldOff(uint8_t* base, uint32_t dvar_pointer, const char* name) {
  if (!dvar_pointer) return;
  const uint32_t dvar = LoadBE32(base + dvar_pointer);
  if (!dvar) return;
  uint8_t* value = base + dvar + 12;
  if (*value) {
    static int logged = 0;
    if (logged < 4) {
      ++logged;
      REXLOG_INFO("damage blur: {} kept at 0", name);
    }
    *value = 0;
  }
}
}  // namespace

// From Com_Frame. hurt / very_hurt: cg_showHurtBlur / cg_showVeryHurtBlur's pointers, 0 if the program has none.
void OnFrame(uint8_t* base, uint32_t hurt, uint32_t very_hurt) {
  if (REXCVAR_GET(damage_blur)) return;
  HoldOff(base, hurt, "cg_showHurtBlur");
  HoldOff(base, very_hurt, "cg_showVeryHurtBlur");
}

// After the renderer fills the glow settings: glow = r3 + 22916, copy = the global copy's address.
void AfterGlow(uint8_t* base, uint32_t glow, uint32_t copy) {
  if (REXCVAR_GET(bloom)) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    const uint8_t* p = base + glow + 12;
    const uint32_t bits = LoadBE32(p);
    REXLOG_INFO("bloom: off (glow enabled {}, bloom intensity was 0x{:08X})", base[glow], bits);
  }
  for (uint32_t a : {glow, copy}) {
    uint8_t* p = base + a + 12;
    p[0] = p[1] = p[2] = p[3] = 0;  // 0.0f
  }
}

}  // namespace qos::effects
