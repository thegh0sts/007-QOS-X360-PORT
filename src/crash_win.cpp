// qos - crash reports on Windows (from the Bourne Conspiracy port, tbc).
//
// Based on Skullburglar's src/crash_win.cpp (https://github.com/nlwstein/skullburglar,
// BSD 3-Clause licence, Copyright (c) 2026, the Skullburglar contributors): unwind the
// host stack into module+offset lines and write a minidump with dbghelp.dll, which is
// loaded at crash time so qos.exe doesn't link it. Frames are named from qos.pdb when it's
// next to the exe, so a crash in recompiled code reads as sub_82XXXXXX+offset.

#include "crash_win.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>

#include <csignal>
#include <cstdio>
#include <filesystem>

namespace qos::crash {
namespace {

std::filesystem::path g_logs;  // <exe folder>\logs, worked out at startup

std::filesystem::path LogPath(const wchar_t* ext) {
  wchar_t name[64];
  swprintf(name, 64, L"crash-%lu.%ls", GetCurrentProcessId(), ext);
  return g_logs / name;
}

void WriteMinidump(EXCEPTION_POINTERS* ep, FILE* report) {
  using Fn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                           PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
  HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
  auto write = dbghelp ? reinterpret_cast<Fn>(GetProcAddress(dbghelp, "MiniDumpWriteDump")) : nullptr;
  if (!write) {
    if (report) std::fprintf(report, "minidump: dbghelp.dll unavailable\n");
    return;
  }
  const std::filesystem::path path = LogPath(L"dmp");
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), ep, FALSE};
  // Not full memory: the guest address space alone is several GB of reservations.
  auto type = MINIDUMP_TYPE(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory |
                            MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData);
  const BOOL ok = write(GetCurrentProcess(), GetCurrentProcessId(), file, type, ep ? &info : nullptr,
                        nullptr, nullptr);
  CloseHandle(file);
  if (report) std::fprintf(report, "minidump: %s\n", ok ? "written" : "FAILED");
}

// Symbol lookup through dbghelp, loaded at crash time (nullptr when unavailable).
using SymFromAddrFn = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
SymFromAddrFn g_sym_from_addr = nullptr;

void InitSymbols() {
  HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
  if (!dbghelp) return;
  using InitFn = BOOL(WINAPI*)(HANDLE, PCSTR, BOOL);
  using OptFn = DWORD(WINAPI*)(DWORD);
  auto init = reinterpret_cast<InitFn>(GetProcAddress(dbghelp, "SymInitialize"));
  auto opts = reinterpret_cast<OptFn>(GetProcAddress(dbghelp, "SymSetOptions"));
  if (!init) return;
  if (opts) opts(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);
  if (init(GetCurrentProcess(), nullptr, TRUE)) {
    g_sym_from_addr = reinterpret_cast<SymFromAddrFn>(GetProcAddress(dbghelp, "SymFromAddr"));
  }
}

void PrintFrame(FILE* out, int i, DWORD64 pc) {
  HMODULE module = nullptr;
  char name[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(pc), &module)) {
    GetModuleFileNameA(module, name, sizeof(name));
  }
  const char* base = name;
  for (const char* p = name; *p; ++p) {
    if (*p == '\\' || *p == '/') base = p + 1;
  }
  std::fprintf(out, "  #%02d %s+0x%llx", i, base,
               static_cast<unsigned long long>(pc - reinterpret_cast<DWORD64>(module)));
  if (g_sym_from_addr) {
    alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    DWORD64 disp = 0;
    if (g_sym_from_addr(GetCurrentProcess(), pc, &disp, sym)) {
      std::fprintf(out, "  %s+0x%llx", sym->Name, static_cast<unsigned long long>(disp));
    }
  }
  std::fprintf(out, "\n");
}

void WriteReport(EXCEPTION_POINTERS* ep, const char* what) {
  std::error_code ec;
  std::filesystem::create_directories(g_logs, ec);
  FILE* out = nullptr;
  if (_wfopen_s(&out, LogPath(L"txt").c_str(), L"w") != 0) out = nullptr;
  if (out) {
    std::fprintf(out, "qos crash: %s\n", what);
    if (ep) {
      const EXCEPTION_RECORD* er = ep->ExceptionRecord;
      if (er->ExceptionCode != 0) {
        std::fprintf(out, "exception 0x%08lx at %p", er->ExceptionCode, er->ExceptionAddress);
      } else {
        std::fprintf(out, "stack at abort");
      }
      if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
        std::fprintf(out, " (%s 0x%llx)", er->ExceptionInformation[0] ? "write" : "read",
                     static_cast<unsigned long long>(er->ExceptionInformation[1]));
      }
      std::fprintf(out, "\n");
      InitSymbols();
      CONTEXT ctx = *ep->ContextRecord;
      for (int i = 0; i < 64 && ctx.Rip; ++i) {
        PrintFrame(out, i, ctx.Rip);
        DWORD64 image_base = 0;
        if (PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &image_base, nullptr)) {
          void* handler_data = nullptr;
          DWORD64 frame = 0;
          RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, ctx.Rip, fn, &ctx, &handler_data, &frame,
                           nullptr);
        } else {  // leaf function: the return address is on top of the stack
          ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
          ctx.Rsp += 8;
        }
      }
    }
    std::fflush(out);
  }
  WriteMinidump(ep, out);
  if (out) std::fclose(out);
}

LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* ep) {
  static LONG once = 0;
  if (InterlockedExchange(&once, 1) == 0) WriteReport(ep, "unhandled exception");
  return EXCEPTION_CONTINUE_SEARCH;
}

void OnAbort(int) {
  static LONG once = 0;
  if (InterlockedExchange(&once, 1) != 0) return;
  // No exception here (a FATAL log, assert or std::terminate): capture this thread's stack, so
  // the report still shows which guest function got there.
  CONTEXT ctx{};
  RtlCaptureContext(&ctx);
  EXCEPTION_RECORD record{};
  record.ExceptionAddress = reinterpret_cast<void*>(ctx.Rip);
  EXCEPTION_POINTERS ep{&record, &ctx};
  WriteReport(&ep, "abort");
}

}  // namespace

void Install() {
  wchar_t exe[MAX_PATH * 4];
  const DWORD n = GetModuleFileNameW(nullptr, exe, DWORD(sizeof(exe) / sizeof(exe[0])));
  if (n == 0 || n >= sizeof(exe) / sizeof(exe[0])) return;
  g_logs = std::filesystem::path(exe).parent_path() / L"logs";
  SetUnhandledExceptionFilter(UnhandledFilter);
  std::signal(SIGABRT, OnAbort);
}

}  // namespace qos::crash

#else
namespace qos::crash {
void Install() {}
}  // namespace qos::crash
#endif
