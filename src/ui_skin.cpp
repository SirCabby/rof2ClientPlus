#include "ui_skin.h"

#include <windows.h>

#include <array>
#include <filesystem>
#include <fstream>

#include "game_functions.h"
#include "io_ini.h"
#include "rcp.h"

// Required rof2ClientPlus XML files. EQUI_RcpOptions.xml is merged into the
// client UI.xml by UIManager; it defines our standalone options window whose
// visibility is synced to the stock Options window (we deliberately do not
// override the client's own EQUI_OptionsWindow.xml).
static constexpr std::array<const char *, 1> kRcpXmlUiFiles = {"EQUI_RcpOptions.xml"};
// Tab files are referenced from EQUI_RcpOptions.xml (not the client UI.xml).
// XMLRead redirects their load to the uifiles/rcp folder.
static constexpr std::array<const char *, 1> kRcpXmlTabFiles = {"EQUI_Tab_Cam.xml"};

std::string UISkin::get_global_default_ui_skin_name() {
  IO_ini ini(IO_ini::kClientFilename);
  return ini.getValue<std::string>("Defaults", "UISkin");
}

bool UISkin::is_ui_skin_big_fonts_mode(const char *ui_skin) {
  if (!ui_skin || !ui_skin[0]) return false;
  std::filesystem::path ui_skin_path = std::filesystem::path(ui_skin);  // Drop uifiles if present.
  std::filesystem::path ui_skin_name = ui_skin_path.filename();
  if (ui_skin_name.empty()) ui_skin_name = ui_skin_path.parent_path().filename();
  std::filesystem::path file_path = Rcp::Game::get_game_path() / std::filesystem::path("uifiles") / ui_skin_name /
                                    std::filesystem::path(kBigFontsTriggerFilename);
  return std::filesystem::exists(file_path);
}

void UISkin::initialize_mode(RcpService *rcp) {
  (void)rcp;
  is_big_fonts = false;  // Big-fonts UI scaling is not ported.
  rcp_resources_path =
      Rcp::Game::get_game_path() / std::filesystem::path("uifiles") / std::filesystem::path(kDefaultRcpFileSubfolder);
  rcp_xml_path = rcp_resources_path;
}

bool UISkin::is_rcp_xml_file(const std::string &xml_file) {
  for (auto file : kRcpXmlUiFiles) {
    if (xml_file == std::string(file)) return true;
  }
  for (auto file : kRcpXmlTabFiles) {
    if (xml_file == std::string(file)) return true;
  }
  return false;
}

std::vector<const char *> UISkin::get_rcp_ui_xml_files() {
  std::vector<const char *> xml_files(kRcpXmlUiFiles.begin(), kRcpXmlUiFiles.end());
  return xml_files;
}

bool UISkin::configuration_check() {
  std::filesystem::path rcp_ui_path = get_rcp_xml_path();

  bool filepathExists = std::filesystem::is_directory(rcp_ui_path);
  if (not filepathExists) {
    std::wstring missing =
        L"A required uifiles folder that contains the rof2ClientPlus xml files is missing from:\n" +
        rcp_ui_path.wstring() + L"\n" + L"rof2ClientPlus UI will not function properly!";
    MessageBoxW(NULL, missing.c_str(), L"rof2ClientPlus installation error", MB_OK | MB_ICONEXCLAMATION);
    return false;
  }

  std::wstring missing_files = L"";
  for (auto file : kRcpXmlUiFiles) {
    std::filesystem::path this_file = rcp_ui_path / std::filesystem::path(file);
    if (not std::filesystem::exists(this_file)) missing_files += this_file.wstring() + L"\n";
  }
  for (auto file : kRcpXmlTabFiles) {
    std::filesystem::path this_file = rcp_ui_path / std::filesystem::path(file);
    if (not std::filesystem::exists(this_file)) missing_files += this_file.wstring() + L"\n";
  }
  if (missing_files.length() > 0) {
    missing_files = L"The following files are missing from your 'uifiles\\rcp' directory:\n" + missing_files +
                    L"\nrof2ClientPlus UI will not function properly!";
    MessageBoxW(NULL, missing_files.c_str(), L"rof2ClientPlus installation error", MB_OK | MB_ICONEXCLAMATION);
    return false;
  }

  return true;
}
