// qosmp - the renderer's glow settings for the frame (sub_824C6D98, single player's sub_82561DC0: the same code),
// followed by ../src/effects.cpp's bloom switch (--bloom). See effects.cpp for the layout.

#include "generated/default/qosmp_pch.h"

#include <cstdint>

namespace qos::effects {
void AfterGlow(uint8_t* base, uint32_t glow, uint32_t copy);  // ../src/effects.cpp
}  // namespace qos::effects

REX_EXTERN(__imp__sub_824C6D98);  // fills the glow settings at r3 + 22916, copied to 0x8518FE64

extern "C" REX_FUNC(sub_824C6D98) {
  const uint32_t glow = ctx.r3.u32 + 22916;
  __imp__sub_824C6D98(ctx, base);
  qos::effects::AfterGlow(base, glow, 0x8518FE64);
}
