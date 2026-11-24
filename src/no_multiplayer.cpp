// qos - the main menu's MULTIPLAYER does nothing.
//
// Multiplayer is a separate executable on the disc (game/default_mp.xex). Choosing MULTIPLAYER runs the console
// command "startMultiplayer" (registered with Cmd_AddCommand sub_82299178 at 0x821BE8D4; name at 0x8201D714), whose
// handler sub_821BCD08 shuts the game's systems down (sub_8229BE60, sub_8256F7B8, sub_825CE62C, sub_822A1290) and then
// relaunches the title as default_mp.xex (sub_82423D78, "default_mp.xex" at 0x8201D510). Only default.xex is
// recompiled, so the relaunch ended the process: the game closed. The handler now returns before shutting anything
// down. --allow_multiplayer=true calls the original (it still closes the game).
// The MULTIPLAYER item first opens the menu's "splaunchmp" screen (a LOADING... screen whose job is
// exec "startMultiplayer"), so doing nothing left that screen up for good (user, 19:47; "disconnect" from there did
// nothing either). So the item itself is made inactive (greyed out, can't be chosen): the menus' ActionScript
// (scaleform/*.gfx: CmMainMenu, CmFrontEnd, SPMainMenu, Sp_ig_PmMain all carry the same menu definitions) builds it as
//   menus.splaunchmp = new frontend.datatypes.LoadingScreen("@UI_MULTIPLAYER", <class>.NORMAL, 'exec "startMultiplayer";')
// i.e. the action bytes
//   96 0a 00 | 09 n 01 | 09 n+1 01 | 04 01 | 08 14 | 4e    push C[n]="splaunchmp", C[n+1]=exec, r1, C[20]="NORMAL"; GetMember
//   96 0a 00 | 09 n+2 01 | 07 03 00 00 00 ...              push C[n+2]="@UI_MULTIPLAYER", 3 (args)
// (n = 0x135, 0x136 or 0x139 depending on the movie; constant 23 is "FLAG_INACTIVE" in all four). The .gfx files are
// zlib-compressed ("CFX"), so the bytes are patched as they come out of the game's inflate (sub_8230BA80, the zlib
// copy Scaleform uses; sub_82408B78 the other copy): constant 20 -> 23, NORMAL -> FLAG_INACTIVE.
// --multiplayer_menu_inactive=false leaves the movies alone.

#include "generated/default/qos_pch.h"

#include <atomic>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

#include <cstring>
#include <string>

REXCVAR_DEFINE_BOOL(allow_multiplayer, false, "Game",
                    "Let the main menu's MULTIPLAYER relaunch the title as default_mp.xex (not recompiled: the game "
                    "closes)");

REXCVAR_DEFINE_BOOL(multiplayer_menu_inactive, true, "Game",
                    "Grey out the main menu's MULTIPLAYER (patches the menu movies as they're decompressed)");
REXCVAR_DEFINE_STRING(multiplayer_fallback, "", "Game",
                      "Console command run in place of startMultiplayer (none by default)");

REX_EXTERN(__imp__sub_821BCD08);  // "startMultiplayer" command handler
REX_EXTERN(sub_82298750);         // Cbuf_AddText(localClientNum, text)
REX_EXTERN(__imp__sub_8230BA80);  // zlib inflate(z_stream*, flush) (Scaleform's copy)
REX_EXTERN(__imp__sub_82408B78);  // zlib inflate(z_stream*, flush) (another copy)

namespace {
std::atomic<int> g_logged{0};
std::atomic<int> g_patched{0};

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

// Looks for the MULTIPLAYER item's flags push in [from, to) of guest memory and makes it FLAG_INACTIVE.
void PatchMultiplayerItem(uint8_t* base, uint32_t from, uint32_t to) {
  constexpr uint32_t kLen = 25;
  if (to < from + kLen) return;
  uint8_t* p = base + from;
  uint8_t* const end = base + to - kLen + 1;
  while (p < end) {
    p = static_cast<uint8_t*>(std::memchr(p, 0x96, size_t(end - p)));
    if (!p) return;
    const uint8_t n = p[4];
    if (p[1] == 0x0a && p[2] == 0 && p[3] == 0x09 && p[5] == 0x01 && p[6] == 0x09 && p[7] == uint8_t(n + 1) &&
        p[8] == 0x01 && p[9] == 0x04 && p[10] == 0x01 && p[11] == 0x08 && p[12] == 0x14 && p[13] == 0x4e &&
        p[14] == 0x96 && p[15] == 0x0a && p[16] == 0 && p[17] == 0x09 && p[18] == uint8_t(n + 2) && p[19] == 0x01 &&
        p[20] == 0x07 && p[21] == 0x03 && p[22] == 0 && p[23] == 0 && p[24] == 0 &&
        (n == 0x35 || n == 0x36 || n == 0x39)) {
      p[12] = 0x17;  // "NORMAL" -> "FLAG_INACTIVE"
      if (g_patched.fetch_add(1) < 8) {
        REXLOG_INFO("multiplayer: menu item greyed out in a menu movie (guest {:08X})", uint32_t(p - base));
      }
    }
    ++p;
  }
}

template <void (*Original)(PPCContext&, uint8_t*)>
void Inflate(PPCContext& ctx, uint8_t* base) {
  const uint32_t strm = ctx.r3.u32;
  const bool on = strm && REXCVAR_GET(multiplayer_menu_inactive);
  const uint32_t out_before = on ? Be32(base + strm + 12) : 0;  // next_out
  Original(ctx, base);
  if (!on) return;
  const uint32_t out_after = Be32(base + strm + 12);
  if (out_after <= out_before || out_after - out_before > 0x10000000) return;
  // 24 bytes back too, for a pattern split across two calls (same output buffer), if on the same page.
  PatchMultiplayerItem(base, (out_before & 0xFFF) >= 24 ? out_before - 24 : out_before, out_after);
}
}  // namespace

extern "C" REX_FUNC(sub_8230BA80) { Inflate<__imp__sub_8230BA80>(ctx, base); }
extern "C" REX_FUNC(sub_82408B78) { Inflate<__imp__sub_82408B78>(ctx, base); }

extern "C" REX_FUNC(sub_821BCD08) {
  if (REXCVAR_GET(allow_multiplayer)) {
    REXLOG_INFO("multiplayer: starting default_mp.xex (allow_multiplayer)");
    __imp__sub_821BCD08(ctx, base);
    return;
  }
  const std::string cmd = REXCVAR_GET(multiplayer_fallback);
  if (g_logged.fetch_add(1) < 4) {
    REXLOG_INFO("multiplayer: startMultiplayer ignored (not part of this port); running \"{}\"", cmd);
  }
  if (cmd.empty()) return;
  const std::string text = cmd + "\n";
  const uint32_t guest = rex::system::kernel_state()->memory()->SystemHeapAlloc(uint32_t(text.size() + 1));
  if (!guest) return;
  std::memcpy(base + guest, text.c_str(), text.size() + 1);  // Cbuf copies it; one small block per press
  ctx.r3.u64 = 0;
  ctx.r4.u64 = guest;
  sub_82298750(ctx, base);
}
