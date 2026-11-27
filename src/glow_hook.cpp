// qos - the renderer's glow settings for the frame (sub_82561DC0, r3 the view's frame data, r4 the vision set),
// followed by src/effects.cpp's bloom switch (--bloom). See effects.cpp for the layout.

#include "generated/default/qos_pch.h"

#include <cstdint>

namespace qos::effects {
void AfterGlow(uint8_t* base, uint32_t glow, uint32_t copy);  // effects.cpp
}  // namespace qos::effects

REX_EXTERN(__imp__sub_82561DC0);  // fills the glow settings at r3 + 22916, copied to 0x841EF1B4

extern "C" REX_FUNC(sub_82561DC0) {
  const uint32_t glow = ctx.r3.u32 + 22916;
  __imp__sub_82561DC0(ctx, base);
  qos::effects::AfterGlow(base, glow, 0x841EF1B4);
}
