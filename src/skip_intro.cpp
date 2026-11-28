// qos - skip the intro's logo videos (--skip_intro, the launcher's "Skip intro"; default off). The legal screen stays.
//
// How the intro runs (front end movies' ActionScript and the executable):
// - The front end starts on the legal screen "splegal" while the bool dvar com_introPlayed is 0 (sub_823D2098 picks
//   the first screen), else on Press Start ("cmpressstart").
// - Leaving the legal screen for Press Start (menus.legalScreenNextScreenOverride, also steered by the dvar
//   ui_screenAfterLegalOverride) runs `cinematic atvi_treyarch; setdvar nextmap "cinematic mgm"`.
// - When a video ends, sub_821AF7B8 stops it; if it was atvi_treyarch and played past frame 249 (sub_82566710) it
//   queues `cinematic treyarch_logo; set nextmap cinematic mgm`, otherwise it runs the nextmap dvar (0x83374184)
//   and clears it. So: Activision, Treyarch, MGM, then Press Start.
// - The "cinematic" command starts a video with sub_82566580 (r3 name, r4 flags, f1, r6): the name goes to the
//   cinematic state at 0x841E56A0 + 256, +776 = start requested; the player thread sets +777 when the video has
//   ended (sub_82566BA8 reads it), and the frame loop then calls sub_821AF7B8.
// With --skip_intro those three logo videos aren't started: the hook writes the name and marks the video ended at
// once (+772 flags 0, +776 0, +777 1), so the next frame stops it as if it had played and the chain moves on (mgm after
// atvi_treyarch, then Press Start) without a frame of video. Other videos (level intros, cutscenes, the menus'
// background loops) play as normal.

#include "generated/default/qos_pch.h"

#include <cstdint>
#include <cstring>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(skip_intro, false, "Game",
                    "Skip the intro's logo videos (Activision, Treyarch, MGM); the legal screen stays");

REX_EXTERN(__imp__sub_82566580);  // start a video (r3 name)

namespace {
constexpr uint32_t kCinematic = 0x841E56A0;  // +256 name (256 bytes), +776 start requested, +777 ended

bool IsIntroLogo(const char* name) {
  std::string n(name);
  for (auto& c : n) c = char(c >= 'A' && c <= 'Z' ? c + 32 : c);
  return n == "atvi_treyarch" || n == "treyarch_logo" || n == "mgm";
}
}  // namespace

extern "C" REX_FUNC(sub_82566580) {
  const uint32_t name_address = ctx.r3.u32;
  if (!REXCVAR_GET(skip_intro) || !name_address) {
    __imp__sub_82566580(ctx, base);
    return;
  }
  char name[64] = {};
  std::strncpy(name, reinterpret_cast<const char*>(base + name_address), sizeof(name) - 1);
  if (!IsIntroLogo(name)) {
    __imp__sub_82566580(ctx, base);
    return;
  }
  uint8_t* state = base + kCinematic;
  std::memset(state + 256, 0, 256);
  std::memcpy(state + 256, name, std::strlen(name));
  std::memset(state + 772, 0, 4);  // start flags: none, so the stop (sub_825667C8) has no sound to end
  state[776] = 0;
  state[777] = 1;
  REXLOG_INFO("skip intro: {} skipped", name);
}
