#pragma once
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Declare a single function from game_functions.h to avoid pulling in too many headers.
namespace Rcp::Game {
void print_chat(const char *format, ...);
}

// Declared here (defined in rcp_profiles.cpp) for the same reason: every settings
// translation unit includes this header, and none of them should have to know about
// the profile module. Maps a mod-ini section into the ACTIVE profile's namespace
// (identity for the default profile and for any section profiles do not cover).
namespace rcp_profiles {
std::string map_ini_section(const std::string &section);
std::string map_ini_section(const std::string &section, const std::string &key);
}

class IO_ini {
 private:
  std::string filename;
  // True for the mod's own ini: its feature sections follow the active settings
  // profile (see rcp_profiles.h). eqclient.ini and raw profile bookkeeping opt out.
  bool profiled;

  // The physical section to read/write for the logical section a caller asked for.
  // Whole-section form (bulk read / delete): only fully-profiled sections are redirected.
  std::string sec(const std::string &section) const {
    return profiled ? rcp_profiles::map_ini_section(section) : section;
  }
  // Single-value form: also redirects the individually profiled keys.
  std::string sec(const std::string &section, const std::string &key) const {
    return profiled ? rcp_profiles::map_ini_section(section, key) : section;
  }

 public:
  static constexpr char kClientFilename[] = ".\\eqclient.ini";
  // rof2ClientPlus settings live next to eqgame.exe.
  static constexpr char kRcpIniFilename[] = ".\\rof2ClientPlus.ini";

  // profile_aware=false gives raw, unmapped section access to the mod ini (used by
  // rcp_profiles itself to copy/erase whole profiles and to keep [Profiles] global).
  IO_ini(const std::string &filename, bool profile_aware = true)
      : filename(filename), profiled(profile_aware && filename == kRcpIniFilename){};

  void set(std::string path) {
    filename = path;
    profiled = profiled && filename == kRcpIniFilename;
  }

  bool exists(const std::string &section_in, const std::string &key) const {
    const std::string section = sec(section_in, key);
    char buffer[256];
    DWORD bytesRead =
        GetPrivateProfileStringA(section.c_str(), key.c_str(), "", buffer, sizeof(buffer), filename.c_str());

    if (bytesRead == 0) {
      return false;
    }
    return true;
  }

  // Raw physical section names (profile-prefixed ones included, unmapped).
  std::vector<std::string> getSectionNames() {
    std::vector<std::string> sectionNames;
    const DWORD bufferSize = 4096;  // Adjust buffer size as needed
    char buffer[bufferSize];

    DWORD result = GetPrivateProfileSectionNamesA(buffer, bufferSize, filename.c_str());
    if (result == 0) {
      std::cerr << "Failed to read INI file: " << filename << std::endl;
      return sectionNames;
    }

    for (char *p = buffer; *p != '\0'; p += strlen(p) + 1) {
      sectionNames.push_back(p);
    }

    return sectionNames;
  }

  // Returns every key=value pair in a section (for sections whose keys are not known ahead of time,
  // e.g. an open-ended set of per-sound overrides). Empty if the section is absent.
  std::vector<std::pair<std::string, std::string>> getSection(const std::string &section_in) const {
    const std::string section = sec(section_in);
    std::vector<std::pair<std::string, std::string>> out;
    std::vector<char> buf(8192);
    DWORD n = GetPrivateProfileSectionA(section.c_str(), buf.data(), static_cast<DWORD>(buf.size()), filename.c_str());
    if (n == 0) return out;
    for (char *p = buf.data(); *p != '\0'; p += strlen(p) + 1) {
      std::string line(p);
      size_t eq = line.find('=');
      if (eq == std::string::npos) continue;
      out.emplace_back(line.substr(0, eq), line.substr(eq + 1));
    }
    return out;
  }

  // Deletes a single key from a section (WritePrivateProfileString with a null value).
  bool deleteKey(const std::string &section_in, const std::string &key) {
    const std::string section = sec(section_in, key);
    return WritePrivateProfileStringA(section.c_str(), key.c_str(), nullptr, filename.c_str()) != 0;
  }

  bool deleteSection(const std::string &section_in) {
    const std::string sectionName = sec(section_in);
    // Delete the section and its contents by writing an empty string to it
    if (!WritePrivateProfileSectionA(sectionName.c_str(), nullptr, filename.c_str())) {
      return false;
    } else {
      return true;
    }
  }

  template <typename T>
  T getValue(std::string section_in, std::string key) const {
    const std::string section = sec(section_in, key);
    char buffer[256];
    DWORD bytesRead =
        GetPrivateProfileStringA(section.c_str(), key.c_str(), "", buffer, sizeof(buffer), filename.c_str());

    if (bytesRead == 0) {
      return T{};
    }
    if constexpr (std::is_same_v<T, std::string>) return buffer;
    return convertFromString<T>(std::string(buffer));
  }

  template <typename T>
  void setValue(const std::string &section_in, const std::string &key, const T &value) {
    const std::string section = sec(section_in, key);
    std::string valueStr;
    if constexpr (std::is_same_v<T, bool>) {
      valueStr = value ? "TRUE" : "FALSE";
    } else if constexpr (!std::is_same_v<T, std::string>) {
      valueStr = std::to_string(value);
    } else {
      valueStr = value;
    }
    BOOL result = WritePrivateProfileStringA(section.c_str(), key.c_str(), valueStr.c_str(), filename.c_str());
    if (!result) {
      Rcp::Game::print_chat("Error writing value to INI file.");
    }
  }

 private:
  template <typename T>
  T convertFromString(const std::string &str) const {
    if constexpr (std::is_same_v<T, bool>) {
      if (str == "TRUE")
        return true;
      else
        return false;
    }
    std::istringstream iss(str);
    T value;
    iss >> std::boolalpha >> value;
    return value;
  }
};
