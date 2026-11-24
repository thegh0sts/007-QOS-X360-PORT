// qos - the game's zlib inflate run as native code (miniz) instead of the recompiled PPC copy.
//
// Fastfiles (a level's .ff: one zlib stream of ~100-250 MB of zone data) and Scaleform's .gfx movies are inflated by
// the game's zlib 1.1.4 copy: inflateInit_ sub_8230BA70 (-> inflateInit2_ sub_8230B930, windowBits 15), inflate
// sub_8230BA80, inflateReset sub_8230B818, inflateEnd sub_8230B898. Callers: the fastfile reader (DB, sub_821C1E10
// inflates with Z_SYNC_FLUSH into the zone buffer; init at 0x821C217C, end 0x821C1FF8) and Scaleform's ZLib file
// (0x82470130 / 0x824A99E4 init, 0x82470198 / 0x824A9B0C inflate, 0x824A9A3C reset). Recompiled, that inflate is a
// big part of a level load (to do item 3, 24 Nov).
//
// Each stream the game initializes gets a host miniz stream, keyed by the guest z_stream's address. inflate() then
// runs miniz directly on guest memory (base + address: guest memory is one linear block) and writes next_in,
// avail_in, total_in, next_out, avail_out, total_out and adler back to the guest z_stream (zlib 1.1.4 layout,
// big-endian: next_in 0, avail_in 4, total_in 8, next_out 12, avail_out 16, total_out 20, msg 24, state 28, ...,
// adler 48). The guest's own init still runs (its state stays allocated and is freed by its own inflateEnd), so the
// game sees a normal stream. Return codes and flush values are the same in zlib 1.1 and miniz.
// --native_inflate=false keeps the recompiled inflate. Each finished stream of 1 MB or more is logged with its size
// and time ("inflate: ..."), to compare the two.

#include "generated/default/qos_pch.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#include "miniz.h"

REXCVAR_DEFINE_BOOL(native_inflate, true, "Game",
                    "Decompress fastfiles and menu movies with native zlib (miniz) instead of the recompiled one");

REX_EXTERN(__imp__sub_8230BA70);  // inflateInit_(strm, version, stream_size)
REX_EXTERN(__imp__sub_8230BA80);  // inflate(strm, flush)
REX_EXTERN(__imp__sub_8230B818);  // inflateReset(strm)
REX_EXTERN(__imp__sub_8230B898);  // inflateEnd(strm)

namespace {

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void SetBe32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

struct HostStream {
  mz_stream z{};
  double ms = 0;  // time spent in mz_inflate
  bool reported = false;
  HostStream() { mz_inflateInit2(&z, MZ_DEFAULT_WINDOW_BITS); }
  ~HostStream() { mz_inflateEnd(&z); }
};

std::mutex g_mutex;
std::unordered_map<uint32_t, std::unique_ptr<HostStream>> g_streams;  // guest z_stream address -> host stream

HostStream* Find(uint32_t strm) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_streams.find(strm);
  return it == g_streams.end() ? nullptr : it->second.get();
}

void Report(uint32_t strm, HostStream& h, const char* how) {
  if (h.reported || h.z.total_out < (1u << 20)) return;
  h.reported = true;
  REXLOG_INFO("inflate: {} MB -> {:.1f} MB in {:.0f} ms native ({:.0f} MB/s), stream {:08X} ({})",
              h.z.total_in >> 20, h.z.total_out / 1048576.0, h.ms, h.ms > 0 ? h.z.total_out / 1048576.0 / (h.ms / 1000) : 0,
              strm, how);
}

// Recompiled-inflate timing, for --native_inflate=false runs.
struct GuestTiming {
  double ms = 0;
  uint32_t out = 0;
};
std::unordered_map<uint32_t, GuestTiming> g_guest;

}  // namespace

extern "C" REX_FUNC(sub_8230BA70) {
  const uint32_t strm = ctx.r3.u32;
  __imp__sub_8230BA70(ctx, base);
  if (!strm || ctx.r3.s32 != 0) return;  // Z_OK only
  std::lock_guard<std::mutex> lock(g_mutex);
  if (REXCVAR_GET(native_inflate)) {
    auto h = std::make_unique<HostStream>();
    g_streams[strm] = std::move(h);
  } else {
    g_guest[strm] = {};
  }
}

extern "C" REX_FUNC(sub_8230B818) {
  const uint32_t strm = ctx.r3.u32;
  if (HostStream* h = strm ? Find(strm) : nullptr) {
    Report(strm, *h, "reset");
    mz_inflateReset(&h->z);
    h->ms = 0;
    h->reported = false;
  }
  __imp__sub_8230B818(ctx, base);
}

extern "C" REX_FUNC(sub_8230B898) {
  const uint32_t strm = ctx.r3.u32;
  if (strm) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_streams.find(strm);
    if (it != g_streams.end()) {
      Report(strm, *it->second, "end");
      g_streams.erase(it);
    }
    auto gt = g_guest.find(strm);
    if (gt != g_guest.end()) {
      if (gt->second.out >= (1u << 20)) {
        REXLOG_INFO("inflate: {:.1f} MB out in {:.0f} ms recompiled ({:.0f} MB/s), stream {:08X}",
                    gt->second.out / 1048576.0, gt->second.ms,
                    gt->second.ms > 0 ? gt->second.out / 1048576.0 / (gt->second.ms / 1000) : 0, strm);
      }
      g_guest.erase(gt);
    }
  }
  __imp__sub_8230B898(ctx, base);
}

extern "C" REX_FUNC(sub_8230BA80) {
  const uint32_t strm = ctx.r3.u32;
  HostStream* h = strm ? Find(strm) : nullptr;
  if (!h) {
    // Recompiled path (stream from before the switch, or --native_inflate=false): time it.
    const auto t0 = std::chrono::steady_clock::now();
    const uint32_t out0 = strm ? Be32(base + strm + 20) : 0;
    __imp__sub_8230BA80(ctx, base);
    if (strm) {
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      std::lock_guard<std::mutex> lock(g_mutex);
      auto it = g_guest.find(strm);
      if (it != g_guest.end()) {
        it->second.ms += ms;
        it->second.out += Be32(base + strm + 20) - out0;
      }
    }
    return;
  }
  uint8_t* g = base + strm;
  const uint32_t next_in = Be32(g + 0), avail_in = Be32(g + 4);
  const uint32_t next_out = Be32(g + 12), avail_out = Be32(g + 16);
  const int flush = ctx.r4.s32;
  mz_stream& z = h->z;
  z.next_in = base + next_in;
  z.avail_in = avail_in;
  z.next_out = base + next_out;
  z.avail_out = avail_out;
  const auto t0 = std::chrono::steady_clock::now();
  // Z_FINISH from zlib 1.1 doesn't demand that all output fit; miniz's does, so it runs as a sync flush.
  const int rc = mz_inflate(&z, flush == MZ_FINISH ? MZ_SYNC_FLUSH : flush);
  h->ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  const uint32_t used_in = avail_in - uint32_t(z.avail_in);
  const uint32_t made_out = avail_out - uint32_t(z.avail_out);
  SetBe32(g + 0, next_in + used_in);
  SetBe32(g + 4, uint32_t(z.avail_in));
  SetBe32(g + 8, Be32(g + 8) + used_in);
  SetBe32(g + 12, next_out + made_out);
  SetBe32(g + 16, uint32_t(z.avail_out));
  SetBe32(g + 20, Be32(g + 20) + made_out);
  SetBe32(g + 48, uint32_t(z.adler));
  if (rc != MZ_OK && rc != MZ_STREAM_END && rc != MZ_BUF_ERROR) {
    REXLOG_WARN("inflate: native inflate of stream {:08X} returned {} ({})", strm, rc, mz_error(rc));
  }
  if (rc == MZ_STREAM_END) Report(strm, *h, "stream end");
  ctx.r3.s64 = rc;
}
