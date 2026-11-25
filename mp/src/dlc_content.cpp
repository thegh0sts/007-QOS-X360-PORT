// qosmp - the map pack's text and map pictures (its <language>\content.ff).
//
// The Camille Map Pack holds the four maps (mp_*.ff, mp_*_load.ff), their list (jbmaps.arena: longname
// "@CONTENT_MP_BAGS" ...), mappack1.id, and per language a zone content.ff with the CONTENT_MP_* strings and the
// mapimg_mp_* preview materials. This executable mounts the pack (sub_822D7D58: "Mounted drive mappack1 as
// mappack 1"), reads its arena and loads its maps from the drive, but nothing in it ever asks for a zone "content"
// (no such name in the XEX; probably the title update that came with the pack did), so the menus showed
// "CONTENT_MP_BAGS" and no picture ("Could not load material mapimg_mp_bags", user 25 Nov).
//
// So once a pack is mounted, the next Com_Frame loads zone "content" with the game's own zone loader
// (DB_LoadXAssets sub_821F2A98, one XZoneInfo {name, allocFlags 1, freeFlags 0}: flag 1 as code_post_gfx_mp's
// common zones, never unloaded by the menu or map zones), and the zone path builder (sub_821F0EA8: name, ext, size,
// buffer, language) puts it at "mappack1:\<language>\content.ff". --dlc_content=false turns it off.

#include "generated/default/qosmp_pch.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(dlc_content, true, "Game", "Load the map pack's names and pictures (its content.ff)");

REX_EXTERN(__imp__sub_822D7D58);  // mounts content pack n (returns 1 when mounted)
REX_EXTERN(__imp__sub_821F0EA8);  // zone path: (name, ext, size, buffer, language or 0)
REX_EXTERN(sub_821F2A98);         // DB_LoadXAssets(zoneInfo, count, r5, r6)
REX_EXTERN(sub_82296340);         // the language folder's name

namespace qosmp::dlc {

namespace {
constexpr char kZone[] = "content";
bool g_pending = false;
bool g_loaded = false;

void StoreBE32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v);
}

// The game's language folder ("english", ...); the pack has english, french, german, italian, japanese, korean,
// russian and spanish: anything else reads the English one.
std::string Language(PPCContext& ctx, uint8_t* base) {
  PPCContext saved = ctx;
  sub_82296340(ctx, base);
  const uint32_t p = ctx.r3.u32;
  ctx = saved;
  std::string lang = p ? std::string(reinterpret_cast<const char*>(base + p)) : std::string();
  while (!lang.empty() && (lang.back() == '\\' || lang.back() == '/')) lang.pop_back();
  static const char* kPack[] = {"english", "french", "german", "italian", "japanese", "korean", "russian", "spanish"};
  for (const char* k : kPack) {
    if (lang == k) return lang;
  }
  return "english";
}
}  // namespace

void OnFrame(PPCContext& ctx, uint8_t* base) {
  if (!g_pending || g_loaded) return;
  g_pending = false;
  g_loaded = true;
  auto* memory = rex::system::kernel_state()->memory();
  const uint32_t block = memory->SystemHeapAlloc(32);
  if (!block) return;
  uint8_t* p = base + block;
  std::memset(p, 0, 32);
  std::memcpy(p + 16, kZone, sizeof(kZone));
  StoreBE32(p + 0, block + 16);  // name
  StoreBE32(p + 4, 1);           // allocFlags
  StoreBE32(p + 8, 0);           // freeFlags
  REXLOG_INFO("dlc: loading the map pack's names and pictures (zone {})", kZone);
  PPCContext saved = ctx;
  ctx.r3.u64 = block;
  ctx.r4.u64 = 1;
  ctx.r5.u64 = 0;
  ctx.r6.u64 = 1;
  sub_821F2A98(ctx, base);
  ctx = saved;
}

}  // namespace qosmp::dlc

extern "C" REX_FUNC(sub_822D7D58) {
  __imp__sub_822D7D58(ctx, base);
  if (ctx.r3.u32 == 1 && REXCVAR_GET(dlc_content) && !qosmp::dlc::g_loaded) qosmp::dlc::g_pending = true;
}

extern "C" REX_FUNC(sub_821F0EA8) {
  const char* name = ctx.r3.u32 ? reinterpret_cast<const char*>(base + ctx.r3.u32) : "";
  if (std::strcmp(name, qosmp::dlc::kZone) != 0) {
    __imp__sub_821F0EA8(ctx, base);
    return;
  }
  const char* ext = ctx.r4.u32 ? reinterpret_cast<const char*>(base + ctx.r4.u32) : "";
  const uint32_t size = ctx.r5.u32;
  char* out = reinterpret_cast<char*>(base + ctx.r6.u32);
  const std::string lang = qosmp::dlc::Language(ctx, base);
  std::snprintf(out, size, "mappack1:\\%s\\%s%s", lang.c_str(), name, ext);
  REXLOG_INFO("dlc: zone {} read from {}", name, out);
}
