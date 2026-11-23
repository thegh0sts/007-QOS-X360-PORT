// qos - WASD for takedowns (--takedown_wasd, on by default; keyboard only).
//
// A quick kill (takedown) asks for button presses shown as the pad's diamond: Y top, X left, A bottom, B right.
// While one runs, the SDK's keyboard pad presses those buttons from the left-stick keys instead of moving
// (SDK patch 0015: W Y, A X, S A, D B); the player can't walk during a takedown anyway. A real pad is untouched.
//
// Found in the decrypted executable (2026-11-23): the quick-kill objects' virtual methods
//   sub_823866D0  begin: saves and sets dvars for the kill camera (cg_showBarrelModel off, r_motionblur_enable on,
//                 r_lodBias, cg_showHurtBlur, cg_hudDamageIconTime, health_regen_enable, ik_enable ...)
//   sub_823877C8  end: puts them back
// (in the vtables at 0x82059068 / 0x82059070 and more; the begin is also called directly from 0x823884D0,
// 0x82388528, 0x82388598, 0x82389D58). The keys switch on at begin and off at end, and off anyway after 20 s in
// case an end is missed (a level change in the middle of one).

#include "generated/default/qos_pch.h"

#include <atomic>
#include <chrono>

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(takedown_wasd, true, "Input",
                    "Keyboard: during a takedown, W A S D press the diamond's buttons (Y X A B) instead of moving");

REX_EXTERN(__imp__sub_823866D0);  // quick kill: begin
REX_EXTERN(__imp__sub_823877C8);  // quick kill: end

namespace {

std::atomic<int64_t> g_since{0};  // steady_clock ms when the current takedown began, 0 when none
int g_logged = 0;

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void Set(bool on) {
  rex::input::mnk::SetStickKeysAsFaceButtons(on);
  g_since.store(on ? NowMs() : 0);
  if (g_logged < 8) {
    ++g_logged;
    REXLOG_INFO("takedown: {}", on ? "begins, W A S D press Y X A B" : "ends, W A S D move again");
  }
}

}  // namespace

namespace qos::takedown_keys {

// Called every frame (fps_unlock.cpp's Com_Frame hook): the safety switch-off.
void OnFrame() {
  const int64_t since = g_since.load();
  if (since && NowMs() - since > 20000) {
    REXLOG_WARN("takedown: no end seen after 20 s, W A S D move again");
    Set(false);
  }
}

}  // namespace qos::takedown_keys

extern "C" REX_FUNC(sub_823866D0) {
  __imp__sub_823866D0(ctx, base);
  if (REXCVAR_GET(takedown_wasd)) Set(true);
}

extern "C" REX_FUNC(sub_823877C8) {
  __imp__sub_823877C8(ctx, base);
  if (g_since.load()) Set(false);
}
