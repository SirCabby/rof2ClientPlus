#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Trimmed from Zeal's UISkin (MIT). Provides the rof2ClientPlus uifiles path
// and the list of custom EQUI xml files that UIManager merges into the client's
// UI on load. The big-fonts UI scaling machinery is not ported.
class UISkin {
 public:
  UISkin() = delete;  // Static-only; call initialize_mode() then the accessors.

  static void initialize_mode(class RcpService* rcp);  // Resolves the uifiles/rcp path.

  static bool configuration_check();  // Returns false (and warns) if the uifiles are missing.

  static bool is_rcp_xml_file(const std::string& xml_file);  // True if one of our xml files.

  static std::vector<const char*> get_rcp_ui_xml_files();  // Files to inject into the client UI xml.

  static std::filesystem::path get_rcp_xml_path() { return rcp_xml_path; }

  static std::filesystem::path get_rcp_resources_path() { return rcp_resources_path; }

  static std::string get_global_default_ui_skin_name();  // Returns the default UISkin in eqclient.ini.

  static bool is_big_fonts_mode() { return is_big_fonts; }  // Always false (not ported).

  static bool is_ui_skin_big_fonts_mode(const char* ui_skin);  // Checks a skin folder for the trigger file.

 private:
  static constexpr char kDefaultRcpFileSubfolder[] = "rcp";              // In uifiles/rcp.
  static constexpr char kBigFontsTriggerFilename[] = "rcp_ui_skin.ini";  // In a ui skin folder.

  inline static std::filesystem::path rcp_xml_path;        // Path to uifiles/rcp.
  inline static std::filesystem::path rcp_resources_path;  // Same as xml path.
  inline static bool is_big_fonts = false;                  // Big-fonts scaling is unsupported here.
};
