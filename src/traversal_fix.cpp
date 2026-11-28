// qos - wall and mantle moves at any frame rate (--traversal_fix, on), and a movement diagnostic
// (--traversal_log, off).
//
// sub_82157A28 is Pmove (r3 pmove: +0 playerState, +4 the command's serverTime): it runs the player's movement
// (PmoveSingle, sub_82156E88) in steps of up to 66 ms from playerState's commandTime (+0) to serverTime. The same
// function runs for the client's prediction, which replays commands the server already ran.
//
// Mission 10's gap jumps between train cars (wall move along the car's side, type 5, then a mantle, type 3; a
// traversal is active while pm_flags (+12) has 0x8 and its type is at playerState +2260, names in the table at
// 0x825E65D0) work at the game's 30 FPS (33 ms steps) but not at 60 (16-17 ms steps): there the mantle ends at the
// same point every time, just before the gap, and the player falls to the tracks (qos_237 vs qos_238, 28 Nov).
// --traversal_fix runs wall and mantle moves in steps of at least 33 ms, as the original: while one is active, a
// call with less than 33 ms to run moves nothing (serverTime set to commandTime for the call, the path the game
// takes when no time has passed), and the next call runs the time saved up. Other movement is unchanged.
//
// --traversal_log logs every change of pm_flags, the traversal type, and whether the player is falling (vertical
// speed below -50), with the time step, origin (+32) and velocity (+44), up to --traversal_log_lines lines; only
// moves past the newest command time seen (not prediction replays).

#include "generated/default/qos_pch.h"

#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(traversal_fix, true, "Game",
                    "Wall and mantle moves (mission 10's train car gaps) run in 33 ms steps as at the original 30 FPS "
                    "(off: the frame's own steps, which drop the player at the gaps above 30 FPS)");
REXCVAR_DEFINE_BOOL(traversal_log, false, "Game", "Log the player's traversals, movement flags and falls (diagnostic)");
REXCVAR_DEFINE_INT32(traversal_log_lines, 3000, "Game", "Most lines --traversal_log writes");

REX_EXTERN(__imp__sub_82157A28);  // Pmove (r3 pmove)

namespace {

uint32_t LoadBE32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
float LoadFloat(const uint8_t* p) {
  const uint32_t v = LoadBE32(p);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

struct State {
  uint32_t time, flags, type;
  bool falling;
  float origin[3], velocity[3];
};

State Read(const uint8_t* base, uint32_t ps) {
  const uint8_t* p = base + ps;
  State s{};
  s.time = LoadBE32(p);
  s.flags = LoadBE32(p + 12);
  s.type = LoadBE32(p + 2260);
  for (int i = 0; i < 3; ++i) s.origin[i] = LoadFloat(p + 32 + 4 * i), s.velocity[i] = LoadFloat(p + 44 + 4 * i);
  s.falling = s.velocity[2] < -50.0f;
  return s;
}

const char* TypeName(const uint8_t* base, uint32_t type) {
  // "none", "balance", "climb", "mantle", "transition", "wall" at 0x825E65D0 (the order the game names them in).
  if (type > 5) return "?";
  const uint32_t name = LoadBE32(base + 0x825E65D0 + 4 * type);
  return name ? reinterpret_cast<const char*>(base + name) : "?";
}

State last{};
bool have_last = false;
uint32_t newest = 0;  // newest command time logged (prediction replays older ones)
constexpr int32_t kOriginalStepMs = 33;
int lines = 0;

void Log(const uint8_t* base, const char* what, const State& before, const State& s) {
  if (lines >= REXCVAR_GET(traversal_log_lines)) return;
  if (++lines == REXCVAR_GET(traversal_log_lines)) REXLOG_INFO("traversal log: line limit reached");
  REXLOG_INFO(
      "traversal log: {} at {} ms (step {} ms): pm_flags {:X} -> {:X}, traversal {} ({}) -> {} ({}), origin {:.1f} "
      "{:.1f} {:.1f}, velocity {:.1f} {:.1f} {:.1f} (was {:.1f} {:.1f} {:.1f})",
      what, s.time, int32_t(s.time - before.time), before.flags, s.flags, before.type, TypeName(base, before.type),
      s.type, TypeName(base, s.type), s.origin[0], s.origin[1], s.origin[2], s.velocity[0], s.velocity[1],
      s.velocity[2], before.velocity[0], before.velocity[1], before.velocity[2]);
}

void Compare(const uint8_t* base, const State& before, const State& s) {
  if (s.flags != before.flags || s.type != before.type) {
    Log(base, (s.flags & 8) && !(before.flags & 8) ? "traversal begins" : (before.flags & 8) && !(s.flags & 8)
                                                                               ? "traversal ends"
                                                                               : "flags",
        before, s);
  } else if (s.falling != before.falling) {
    Log(base, s.falling ? "falling" : "landed", before, s);
  }
}

bool HoldBack(uint8_t* base, uint32_t pm, uint32_t ps) {
  if (!REXCVAR_GET(traversal_fix) || !ps) return false;
  const uint8_t* p = base + ps;
  if (!(LoadBE32(p + 12) & 8)) return false;
  const uint32_t type = LoadBE32(p + 2260);
  if (type != 3 && type != 5) return false;  // mantle, wall
  const int32_t pending = int32_t(LoadBE32(base + pm + 4) - LoadBE32(p));
  return pending > 0 && pending < kOriginalStepMs;
}

void StoreBE32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v); }

}  // namespace

extern "C" REX_FUNC(sub_82157A28) {
  const uint32_t pm = ctx.r3.u32;
  const uint32_t ps = LoadBE32(base + pm);
  if (HoldBack(base, pm, ps)) {
    static int logged = 0;
    if (logged < 3) {
      ++logged;
      REXLOG_INFO("traversal fix: wall/mantle move held for a {} ms step (as at 30 FPS)", kOriginalStepMs);
    }
    uint8_t* server_time = base + pm + 4;
    const uint32_t saved = LoadBE32(server_time);
    StoreBE32(server_time, LoadBE32(base + ps));
    __imp__sub_82157A28(ctx, base);
    StoreBE32(base + pm + 4, saved);
    return;
  }
  if (!REXCVAR_GET(traversal_log) || !ps) {
    __imp__sub_82157A28(ctx, base);
    return;
  }
  const State before = Read(base, ps);
  __imp__sub_82157A28(ctx, base);
  const State after = Read(base, ps);
  // A level restart or load starts command times again from near 0.
  if (newest > after.time + 10000) newest = 0, have_last = false;
  if (after.time <= newest) return;  // a replay (prediction) of a move already logged
  newest = after.time;
  // The normal move between two of these calls (PmoveSingle after it) changes the state too: compare with the end
  // of the last logged call first, then this call's own changes.
  if (have_last) Compare(base, last, before);
  Compare(base, before, after);
  last = after, have_last = true;
}
