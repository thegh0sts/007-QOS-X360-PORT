// qosmp - downloadable content from a folder of the player's choice (--dlc_folder, the launcher's Multiplayer tab).
//
// Xbox 360 DLC (the map packs) comes as STFS packages: one file each, starting with "LIVE", "PIRS" or "CON ", named
// with a long hex name and usually kept in Content\0000000000000000\<title id>\00000002\ on the console's drive. The
// game finds DLC through XAM's content enumeration, which the SDK serves from its own content folder (the user data
// root's content\0000000000000000\<title id>\00000002\, one extracted folder and one .header file per package).
//
// At start-up every package of this title in the chosen folder (searched a few folders deep, so the folder can be a
// whole copied Content tree) that isn't installed yet is installed with the SDK's ContentManager::InstallContent:
// extracted into the content folder, with its header. That's done once per package; later starts only check.
// Packages for other titles and other content types (saves, profiles, title updates) are skipped.
//
// --dlc_removed (the launcher's Remove button): package file names, separated by commas. Those are never installed,
// and an installed copy is deleted (its extracted folder and header, the SDK's DeleteContent) at start-up, before the
// game mounts any content. Add back takes the name off the list; the next start installs it again.

#include "generated/default/qosmp_pch.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>

REXCVAR_DEFINE_STRING(dlc_folder, "", "Game",
                      "A folder with the game's downloadable content (STFS packages); installed at start-up");

REXCVAR_DEFINE_STRING(dlc_removed, "", "Game",
                      "DLC package file names (comma-separated) the player removed: not installed, and deleted if "
                      "they were");

namespace qosmp::dlc {

using rex::X_RESULT;  // X_ERROR_SUCCESS expands to (X_RESULT)(0)
namespace {

namespace fs = std::filesystem;

constexpr uint32_t kMarketplaceContent = 0x00000002;  // XContentType::kMarketplaceContent
constexpr int kMaxDepth = 5;

bool IsStfsPackage(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::array<char, 4> magic{};
  if (!f.read(magic.data(), magic.size())) return false;
  const std::string m(magic.data(), magic.size());
  return m == "LIVE" || m == "PIRS" || m == "CON ";
}

void Collect(const fs::path& dir, int depth, std::vector<fs::path>& out) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
       it.increment(ec)) {
    const auto& e = *it;
    std::error_code ec2;
    if (e.is_directory(ec2)) {
      if (depth < kMaxDepth) Collect(e.path(), depth + 1, out);
    } else if (e.is_regular_file(ec2) && e.file_size(ec2) > 0xA000 && IsStfsPackage(e.path())) {
      out.push_back(e.path());
    }
  }
}

std::set<std::string> RemovedNames() {
  std::set<std::string> out;
  const std::string list = REXCVAR_GET(dlc_removed);
  size_t start = 0;
  while (start <= list.size()) {
    size_t end = list.find(',', start);
    if (end == std::string::npos) end = list.size();
    std::string name = list.substr(start, end - start);
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (!name.empty()) out.insert(name);
    start = end + 1;
  }
  return out;
}

}  // namespace

void InstallFromFolder() {
  const std::set<std::string> removed = RemovedNames();
  auto* kernel = rex::system::kernel_state();
  auto* content = kernel ? kernel->content_manager() : nullptr;
  if (content && !removed.empty()) {
    for (const auto& name : removed) {
      rex::system::xam::XCONTENT_AGGREGATE_DATA data{};
      if (content->ReadContentHeaderFile(name, 0, kernel->title_id(), rex::system::XContentType::kMarketplaceContent,
                                         data) != X_ERROR_SUCCESS)
        continue;  // not installed
      data.title_id = kernel->title_id();  // as InstallContent wrote it; never "the running title"
      const X_RESULT r = content->DeleteContent(0, data);
      if (r == X_ERROR_SUCCESS)
        REXLOG_INFO("dlc: {} removed (the launcher's Remove)", name);
      else
        REXLOG_ERROR("dlc: removing {} failed ({:08X})", name, r);
    }
  }
  const std::string folder = REXCVAR_GET(dlc_folder);
  if (folder.empty()) return;
  const fs::path root = rex::to_path(folder);
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    REXLOG_WARN("dlc: folder {} not found", folder);
    return;
  }
  if (!content) return;
  const uint32_t title_id = kernel->title_id();

  // Installed: its header is there (InstallContent writes it last, so a package cut off mid-extraction isn't).
  std::set<std::string> installed;
  auto has_header = [&](const std::string& name) {
    rex::system::xam::XCONTENT_AGGREGATE_DATA data{};
    return installed.count(name) ||
           content->ReadContentHeaderFile(name, 0, title_id, rex::system::XContentType::kMarketplaceContent, data) ==
               X_ERROR_SUCCESS;
  };

  std::vector<fs::path> packages;
  Collect(root, 0, packages);
  int added = 0, present = 0;
  for (const auto& p : packages) {
    const std::string name = rex::path_to_utf8(p.filename());
    {
      rex::filesystem::StfsContainerDevice device("", p);
      if (!device.Initialize()) {
        REXLOG_WARN("dlc: {} isn't a package the SDK can read, skipped", rex::path_to_utf8(p));
        continue;
      }
      const auto& meta = device.header().metadata;
      const uint32_t type = static_cast<uint32_t>(rex::system::XContentType(meta.content_type));
      const uint32_t title = meta.execution_info.title_id;
      if (type != kMarketplaceContent || title != title_id) {
        REXLOG_INFO("dlc: {} is for title {:08X}, type {:08X}: not this game's DLC, skipped", name, title, type);
        continue;
      }
    }
    if (removed.count(name)) {
      REXLOG_INFO("dlc: {} removed by the player, not installed", name);
      continue;
    }
    if (has_header(name)) {
      ++present;
      continue;
    }
    REXLOG_INFO("dlc: installing {}", rex::path_to_utf8(p));
    const X_RESULT r = content->InstallContent(p);
    if (r == X_ERROR_SUCCESS) {
      ++added;
      installed.insert(name);
    } else {
      REXLOG_ERROR("dlc: installing {} failed ({:08X})", name, r);
    }
  }
  REXLOG_INFO("dlc: {} package(s) in {}: {} installed now, {} already installed", packages.size(), folder, added,
              present);
}

}  // namespace qosmp::dlc
