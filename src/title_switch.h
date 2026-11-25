// qos / qosmp - start the other half of the game when it asks the console to launch a title.
//
// On the 360, single player (default.xex) and multiplayer (default_mp.xex) are two titles: choosing MULTIPLAYER
// launches default_mp.xex, multiplayer's way back launches default.xex, and multiplayer's Sys_Error relaunches
// itself. ReXGlue runs one recompiled title per process and ends the process on a title launch, so each program
// starts the other one (qos.exe / qosmp.exe) first, with the same command line, and then lets the launch end itself.
//
// Where the other exe is: beside this one, else at its build folder (project\out\build\<preset>\qos.exe and
// project\mp\out\build\<preset>\qosmp.exe, as the installer and a developer build lay them out).
// Command line: this process's own (the launcher gives both the same settings; each ignores the other's: SDK cvar
// parsing keeps unknown --name=value), with these changes:
//  - to multiplayer: --mp_extra_args's text added (the launcher puts the gamertag and XUID there: single player
//    doesn't take them, the SDK's profile folder is named after the gamertag), log game.log -> game_mp.log;
//  - to single player: --user_gamertag / --user_xuid dropped, game_mp.log -> game.log.
// Header-only, included by one .cpp in each program.

#pragma once

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <rex/logging.h>

namespace qos::title_switch {

#ifdef _WIN32
inline std::wstring Widen(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(size_t(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
  return w;
}

inline std::string Narrow(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(size_t(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

// This process's command line without argv[0].
inline std::wstring OwnArgs() {
  std::wstring cl = GetCommandLineW();
  size_t i = 0;
  if (!cl.empty() && cl[0] == L'"') {
    i = cl.find(L'"', 1);
    i = i == std::wstring::npos ? cl.size() : i + 1;
  } else {
    while (i < cl.size() && cl[i] != L' ' && cl[i] != L'\t') ++i;
  }
  while (i < cl.size() && (cl[i] == L' ' || cl[i] == L'\t')) ++i;
  return cl.substr(i);
}

// Splits a Windows command line into arguments, each kept with its own quoting.
inline std::vector<std::wstring> RawArgs(const std::wstring& s) {
  std::vector<std::wstring> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
    if (i >= s.size()) break;
    size_t start = i;
    bool quoted = false;
    for (; i < s.size(); ++i) {
      if (s[i] == L'\\' && i + 1 < s.size() && s[i + 1] == L'"') {
        ++i;  // \" is a quote inside the argument
        continue;
      }
      if (s[i] == L'"') quoted = !quoted;
      else if (!quoted && (s[i] == L' ' || s[i] == L'\t')) break;
    }
    out.push_back(s.substr(start, i - start));
  }
  return out;
}

inline void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to) {
  for (size_t p = s.find(from); p != std::wstring::npos; p = s.find(from, p + to.size())) s.replace(p, from.size(), to);
}

inline std::wstring ExeDir() {
  wchar_t path[MAX_PATH * 4];
  DWORD n = GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
  std::wstring p(path, n);
  return p.substr(0, p.find_last_of(L"\\/"));
}

inline bool IsFile(const std::wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// to_multiplayer: start qosmp.exe from qos.exe; else qos.exe from qosmp.exe. relaunch: start this same exe.
inline bool Start(bool to_multiplayer, bool relaunch, const std::string& mp_extra_args) {
  const std::wstring dir = ExeDir();
  const std::wstring leaf = dir.substr(dir.find_last_of(L"\\/") + 1);  // the preset folder
  std::wstring exe;
  if (relaunch) {
    // One restart only: a game that fails again at once would otherwise restart forever.
    wchar_t flag[8];
    if (GetEnvironmentVariableW(L"QOS_RELAUNCHED", flag, 8)) {
      REXLOG_ERROR("title switch: already restarted once, not again");
      return false;
    }
    SetEnvironmentVariableW(L"QOS_RELAUNCHED", L"1");  // inherited by the new process
    wchar_t path[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
    exe.assign(path, n);
  } else {
    const std::wstring name = to_multiplayer ? L"qosmp.exe" : L"qos.exe";
    std::vector<std::wstring> candidates = {dir + L"\\" + name};
    if (to_multiplayer) candidates.push_back(dir + L"\\..\\..\\..\\mp\\out\\build\\" + leaf + L"\\" + name);
    else candidates.push_back(dir + L"\\..\\..\\..\\..\\out\\build\\" + leaf + L"\\" + name);
    for (const auto& c : candidates) {
      if (IsFile(c)) {
        exe = c;
        break;
      }
    }
    if (exe.empty()) {
      REXLOG_ERROR("title switch: {} not found (looked beside this exe and in its build folder)", Narrow(name));
      return false;
    }
  }

  std::wstring args;
  for (auto& a : RawArgs(OwnArgs())) {
    const std::wstring_view bare = std::wstring_view(a).substr(a.starts_with(L'"') ? 1 : 0);
    if (!relaunch && !to_multiplayer && (bare.starts_with(L"--user_gamertag") || bare.starts_with(L"--user_xuid"))) {
      continue;
    }
    if (!args.empty()) args += L' ';
    args += a;
  }
  if (!relaunch) {
    if (to_multiplayer) {
      ReplaceAll(args, L"\\game.log", L"\\game_mp.log");
      if (!mp_extra_args.empty()) args += L' ' + Widen(mp_extra_args);
    } else {
      ReplaceAll(args, L"\\game_mp.log", L"\\game.log");
    }
  }

  std::wstring cmd = L"\"" + exe + L"\" " + args;
  const std::wstring cwd = exe.substr(0, exe.find_last_of(L"\\/"));
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si, &pi)) {
    REXLOG_ERROR("title switch: couldn't start {} (error {})", Narrow(exe), GetLastError());
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  REXLOG_INFO("title switch: started {}", Narrow(exe));
  return true;
}
#else
inline bool Start(bool, bool, const std::string&) {
  REXLOG_ERROR("title switch: only on Windows so far");
  return false;
}
#endif

}  // namespace qos::title_switch
