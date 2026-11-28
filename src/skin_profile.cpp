// qos - character skinning timer (--skin_profile, off): how long the game's CPU vertex skinning takes, and on which
// threads.
//
// A Tracy capture of mission 2's rooftop chase (28 Nov) put the game's four hottest functions in one family:
// sub_825A68F8 calls sub_825A65A0, which calls one of four vector loops (sub_825A59A0 / 5B98 / 5DF8 / 60B0) that
// skin a character's vertices: blend bone matrices and unpack / repack the 2-10-10-10 normals, one vertex per pass.
// Before speeding those loops up, this measures them: each call of sub_825A68F8 is timed and, every 2 seconds, the
// log says how many milliseconds a second (and so a frame) each thread spent in it, and how many calls.

#include "generated/default/qos_pch.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <rex/cvar.h>
#include <fmt/format.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(skin_profile, false, "Debug", "Log the time spent in the game's character skinning (diagnostic)");

REX_EXTERN(__imp__sub_825A68F8);  // skinning entry (r3 the skinning job)

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kSlots = 16;
struct Slot {
  std::atomic<size_t> thread{0};  // std::hash of the thread id, 0 = free
  std::atomic<int64_t> ns{0};
  std::atomic<uint32_t> calls{0};
};
Slot g_slots[kSlots];
std::atomic<int64_t> g_window_start{0};
std::mutex g_log_mutex;

Slot* SlotForThisThread() {
  thread_local Slot* slot = nullptr;
  if (slot) return slot;
  const size_t id = std::hash<std::thread::id>{}(std::this_thread::get_id()) | 1;
  for (Slot& s : g_slots) {
    size_t expected = 0;
    if (s.thread.load() == id || s.thread.compare_exchange_strong(expected, id)) return slot = &s;
  }
  return slot = &g_slots[kSlots - 1];  // more threads than slots: share the last
}

void MaybeLog(int64_t now) {
  int64_t start = g_window_start.load(std::memory_order_relaxed);
  if (start == 0) {
    g_window_start.compare_exchange_strong(start, now);
    return;
  }
  const int64_t window = now - start;
  if (window < 2'000'000'000) return;
  std::lock_guard lock(g_log_mutex);
  if (g_window_start.load() != start) return;  // another thread logged this window
  g_window_start.store(now);
  const double seconds = double(window) * 1e-9;
  std::string per_thread;
  double total_ms = 0;
  uint32_t total_calls = 0;
  for (int i = 0; i < kSlots; ++i) {
    const int64_t ns = g_slots[i].ns.exchange(0);
    const uint32_t calls = g_slots[i].calls.exchange(0);
    if (!calls) continue;
    const double ms_per_s = double(ns) * 1e-6 / seconds;
    total_ms += ms_per_s;
    total_calls += calls;
    per_thread += fmt::format(" [thread {} {:.1f} ms/s, {} calls]", i, ms_per_s, uint32_t(calls / seconds));
  }
  REXLOG_INFO("skin profile: {:.1f} ms of skinning a second ({:.2f} ms a frame at 60 FPS), {} calls a second;{}",
              total_ms, total_ms / 60.0, uint32_t(total_calls / seconds), per_thread);
}

}  // namespace

extern "C" REX_FUNC(sub_825A68F8) {
  if (!REXCVAR_GET(skin_profile)) {
    __imp__sub_825A68F8(ctx, base);
    return;
  }
  Slot* slot = SlotForThisThread();
  const auto start = Clock::now();
  __imp__sub_825A68F8(ctx, base);
  const auto end = Clock::now();
  slot->ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(),
                     std::memory_order_relaxed);
  slot->calls.fetch_add(1, std::memory_order_relaxed);
  MaybeLog(std::chrono::duration_cast<std::chrono::nanoseconds>(end.time_since_epoch()).count());
}
