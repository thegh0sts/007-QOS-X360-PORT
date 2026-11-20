// qos - frame rate (--frame_rate).
//
// Quantum of Solace runs on Treyarch's Call of Duty 4 engine, which caps its frame rate with the
// com_maxfps setting (a "dvar"). The game registers it with a default of 30 (the 360 version ran
// at 30). Dvars are registered through Dvar_RegisterInt (sub_822E4188: r3 name, r4 default,
// r5 min, r6 max, r7 flags, r8 description; returns the dvar). This hook changes com_maxfps's
// default when it is registered, so the engine's own limiter targets --frame_rate instead.
// 0 is unlimited (the engine treats com_maxfps 0 as no cap). Run with --vsync=false so the
// graphics plugin's imitation of the 360's vblank wait doesn't hold the game at 30 anyway.
//
// The engine is built for a variable frame rate (Call of Duty 4 ran at 60 on the 360), but
// QoS's own gameplay was only tuned at 30: animations, cutscenes and physics need testing.

#include "generated/default/qos_pch.h"

#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(frame_rate, 30, "Game",
                     "Frame rate cap (com_maxfps): 30 is the original, 60 or more is "
                     "experimental, 0 is unlimited")
    .range(0, 1000);

REX_EXTERN(__imp__sub_822E4188);  // Dvar_RegisterInt

extern "C" REX_FUNC(sub_822E4188) {
  const char* name = reinterpret_cast<const char*>(base + ctx.r3.u32);
  if (std::strcmp(name, "com_maxfps") == 0) {
    const int32_t fps = REXCVAR_GET(frame_rate);
    if (fps != 30) {
      ctx.r4.u64 = uint32_t(fps);
      REXLOG_INFO("frame rate: com_maxfps default {} (was 30)", fps);
    }
  }
  __imp__sub_822E4188(ctx, base);
}
