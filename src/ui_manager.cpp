#include <format>
#include "ui_manager.h"

#include <algorithm>
#include <array>
#include <fstream>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "logger.h"
#include "rcp.h"
#include "string_util.h"
#include "ui_skin.h"

Rcp::GameUI::SidlWnd *UIManager::CreateSidlScreenWnd(const std::string &name) {
  Rcp::GameUI::SidlWnd *wnd = (Rcp::GameUI::SidlWnd *)HeapAlloc(*Rcp::Game::Heap, 0, sizeof(Rcp::GameUI::SidlWnd));
  mem::set((int)wnd, 0, sizeof(Rcp::GameUI::SidlWnd));
  Rcp::GameUI::CXSTR name_cxstr(name);  // Constructor calls FreeRep() internally.
  Rcp::Game::GameInternal::CSidlScreenWndConstructor(wnd, 0, nullptr, name_cxstr);
  wnd->SetupCustomVTable();
  wnd->CreateChildren();
  return wnd;
}

// The caller should nullptr the wnd after calling.
void UIManager::DestroySidlScreenWnd(Rcp::GameUI::SidlWnd *wnd) {
  if (!wnd) return;

  // The SidlScreenWndDestructor call below also appears to release all children resources (at 0x005711e0),
  // but probably doesn't handle any resources directly allocated in the custom SidlWnd class.
  wnd->DeleteCustomVTable();
  Rcp::Game::GameInternal::CSidlScreenWndDestructor(wnd, 0, true);  // Set true to dealloc memory.
}

static int __fastcall ButtonClick_hook(Rcp::GameUI::BasicWnd *pWnd, int unused, Rcp::GameUI::CXPoint pt,
                                       unsigned int flag) {
  UIManager *ui = RcpService::get_instance()->ui.get();
  int rval =
      RcpService::get_instance()->hooks->hook_map["ButtonClick"]->original(ButtonClick_hook)(pWnd, unused, pt, flag);
  auto cb = ui->GetButtonCallback(pWnd);
  if (cb) {
    ui->clicked_button = pWnd;
    cb(pWnd);
  }
  return rval;
}

static int __fastcall CheckboxClick_hook(Rcp::GameUI::BasicWnd *pWnd, int unused, Rcp::GameUI::CXPoint pt,
                                         unsigned int flag) {
  UIManager *ui = RcpService::get_instance()->ui.get();
  int rval = RcpService::get_instance()->hooks->hook_map["CheckboxClick"]->original(CheckboxClick_hook)(pWnd, unused,
                                                                                                         pt, flag);

  auto cb = ui->GetCheckboxCallback(pWnd);
  if (cb) cb(pWnd);

  return rval;
}

static void __fastcall SetSliderValue_hook(Rcp::GameUI::SliderWnd *pWnd, int unused, int value) {
  UIManager *ui = RcpService::get_instance()->ui.get();
  RcpService::get_instance()->hooks->hook_map["SetSliderValue"]->original(SetSliderValue_hook)(pWnd, unused, value);

  if (value < 0) value = 0;
  if (value > pWnd->max_val) value = pWnd->max_val;

  auto cb = ui->GetSliderCallback(pWnd);
  if (cb) cb(pWnd, value);
}

static void __fastcall SetComboValue_hook(Rcp::GameUI::BasicWnd *pWnd, int unused, int value) {
  UIManager *ui = RcpService::get_instance()->ui.get();
  RcpService::get_instance()->hooks->hook_map["SetComboValue"]->original(SetComboValue_hook)(pWnd, unused, value);

  auto cb = ui->GetComboCallback(pWnd);
  auto cb_parent = ui->GetComboCallback(pWnd->ParentWnd);
  if (cb)
    cb(pWnd, value);
  else if (cb_parent)
    cb_parent(pWnd->ParentWnd, value);
}

Rcp::GameUI::BasicWnd *UIManager::AddButtonCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                                     std::function<void(Rcp::GameUI::BasicWnd *)> callback,
                                                     bool log_errors) {
  if (wnd) {
    Rcp::GameUI::BasicWnd *btn = wnd->GetChildItem(name, log_errors);
    if (btn) {
      button_callbacks[btn] = callback;
      button_names[name] = btn;
      return btn;
    }
  }
  return nullptr;
}

Rcp::GameUI::BasicWnd *UIManager::AddCheckboxCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                                       std::function<void(Rcp::GameUI::BasicWnd *)> callback,
                                                       bool log_errors) {
  if (wnd) {
    Rcp::GameUI::BasicWnd *btn = wnd->GetChildItem(name, log_errors);
    if (btn) {
      checkbox_callbacks[btn] = callback;
      checkbox_names[name] = btn;
      return btn;
    }
  }
  return nullptr;
}

Rcp::GameUI::BasicWnd *UIManager::AddSliderCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                                     std::function<void(Rcp::GameUI::SliderWnd *, int)> callback,
                                                     int max_val, bool log_errors) {
  if (wnd) {
    Rcp::GameUI::SliderWnd *btn = (Rcp::GameUI::SliderWnd *)wnd->GetChildItem(name, log_errors);
    if (btn) {
      slider_callbacks[btn] = callback;
      slider_names[name] = btn;
      btn->max_val = max_val;
      return btn;
    }
  }
  return nullptr;
}

Rcp::GameUI::BasicWnd *UIManager::AddComboCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                                    std::function<void(Rcp::GameUI::BasicWnd *, int)> callback,
                                                    bool log_errors) {
  if (wnd) {
    Rcp::GameUI::BasicWnd *btn = (Rcp::GameUI::BasicWnd *)wnd->GetChildItem(name, log_errors);
    if (btn) {
      combo_callbacks[btn] = callback;
      combo_names[name] = btn;
      return btn;
    }
  }
  return nullptr;
}

void UIManager::AddLabel(Rcp::GameUI::BasicWnd *wnd, std::string name, bool log_errors) {
  if (wnd) {
    Rcp::GameUI::BasicWnd *btn = wnd->GetChildItem(name, log_errors);
    if (btn) {
      label_names[name] = btn;
    }
  }
}

void UIManager::SetSliderValue(std::string name, int value) {
  if (slider_names.count(name) > 0) {
    RcpService::get_instance()->hooks->hook_map["SetSliderValue"]->original(SetSliderValue_hook)(slider_names[name], 0,
                                                                                                  value);
  }
}

void UIManager::SetSliderValue(std::string name, float value) {
  if (slider_names.count(name) > 0) {
    RcpService::get_instance()->hooks->hook_map["SetSliderValue"]->original(SetSliderValue_hook)(
        slider_names[name], 0, static_cast<int>(value));
  }
}

void UIManager::AddListItems(Rcp::GameUI::ListWnd *wnd, const std::vector<std::string> data) {
  for (int row = 0; auto &current_row : data) {
    int x = wnd->AddString("");
    wnd->SetItemText(current_row, row, 0);
    wnd->SetItemData(row);
    row++;
  }
}

void UIManager::AddListItems(Rcp::GameUI::ComboWnd *wnd, const std::vector<std::string> data) {
  for (auto &current_row : data) wnd->InsertChoice(current_row);
}

void UIManager::AddListItems(Rcp::GameUI::ListWnd *wnd, const std::vector<std::vector<std::string>> data) {
  for (int row = 0; auto &current_row : data) {
    int x = wnd->AddString("");
    for (int col = 0; auto &current_col : current_row) {
      wnd->SetItemText(current_col, row, col);
      col++;
    }
    wnd->SetItemData(row);
    row++;
  }
}

void UIManager::SetChecked(std::string name, bool checked) {
  if (checkbox_names.count(name) > 0) checkbox_names[name]->Checked = checked;
}

void UIManager::SetLabelValue(std::string name, const char *format, ...) {
  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);
  if (label_names.count(name) > 0) {
    Rcp::Game::GameInternal::CXStr_PrintString(&label_names[name]->Text, buffer);
  }
}

void UIManager::SetComboValue(std::string name, int value) {
  if (combo_names.count(name) > 0) {
    RcpService::get_instance()->hooks->hook_map["SetComboValue"]->original(SetComboValue_hook)(
        combo_names[name]->CmbListWnd, 0, value);
  }
}

Rcp::GameUI::SliderWnd *UIManager::GetSlider(std::string name) {
  if (slider_names.count(name)) return slider_names[name];
  return nullptr;
}

Rcp::GameUI::BasicWnd *UIManager::GetCheckbox(std::string name) {
  if (checkbox_names.count(name)) return checkbox_names[name];
  return nullptr;
}

Rcp::GameUI::BasicWnd *UIManager::GetButton(std::string name) {
  if (button_names.count(name)) return button_names[name];
  return nullptr;
}

Rcp::GameUI::BasicWnd *UIManager::GetCombo(std::string name) {
  if (combo_names.count(name)) return combo_names[name];
  return nullptr;
}

std::function<void(Rcp::GameUI::SliderWnd *, int)> UIManager::GetSliderCallback(Rcp::GameUI::SliderWnd *wnd) {
  if (slider_callbacks.count(wnd)) return slider_callbacks[wnd];
  return nullptr;
}

std::function<void(Rcp::GameUI::BasicWnd *, int)> UIManager::GetComboCallback(Rcp::GameUI::BasicWnd *wnd) {
  if (combo_callbacks.count(wnd)) return combo_callbacks[wnd];
  return nullptr;
}

std::function<void(Rcp::GameUI::BasicWnd *)> UIManager::GetButtonCallback(Rcp::GameUI::BasicWnd *wnd) {
  if (button_callbacks.count(wnd)) return button_callbacks[wnd];
  return nullptr;
}

std::function<void(Rcp::GameUI::BasicWnd *)> UIManager::GetCheckboxCallback(Rcp::GameUI::BasicWnd *wnd) {
  if (checkbox_callbacks.count(wnd)) return checkbox_callbacks[wnd];
  return nullptr;
}

void UIManager::CleanUI() {
  Rcp::Game::print_debug("Clean UI UIMANAGER");
  combo_names.clear();
  combo_callbacks.clear();
  checkbox_names.clear();
  checkbox_callbacks.clear();
  slider_names.clear();
  slider_callbacks.clear();
  label_names.clear();
  button_names.clear();
  button_callbacks.clear();
}

// Creates a temporary ui.xml by merging the extra required rof2ClientPlus xml files into the active ui.xml.
bool UIManager::WriteTemporaryUI(const std::filesystem::path &orig_file, const std::filesystem::path &merged_file) {
  if (orig_file.empty()) return false;
  std::ifstream infile(orig_file);
  if (!infile) return false;

  std::stringstream buffer;
  std::string line;
  bool compositeFound = false;
  std::string modifiedContent;

  const auto rcpXmlFiles = UISkin::get_rcp_ui_xml_files();

  // Read file line by line
  while (std::getline(infile, line)) {
    // Make comparisons case insensitive.
    std::string loweredLine = line;
    std::transform(loweredLine.begin(), loweredLine.end(), loweredLine.begin(), ::tolower);

    // Exclude xml files that we provide ourselves (specifically OptionsWindow.xml).
    bool duplicate = false;
    const std::string startTag = "<include>";
    size_t startPos = loweredLine.find(startTag);
    size_t endPos = loweredLine.find("</include>");
    if (startPos != std::string::npos && endPos != std::string::npos) {
      startPos += startTag.length();
      std::string xml_file_name = loweredLine.substr(startPos, endPos - startPos);
      for (const auto &file : rcpXmlFiles) {
        std::string rcp_file = file;
        std::transform(rcp_file.begin(), rcp_file.end(), rcp_file.begin(), ::tolower);
        if (rcp_file == xml_file_name) duplicate = true;
      }
    }
    if (duplicate) continue;  // Skip adding file.

    // Search for the closing </composite> tag (case insensitive)
    if (!compositeFound && loweredLine.find("</composite>") != std::string::npos) {
      compositeFound = true;

      for (const auto &file : UISkin::get_rcp_ui_xml_files())
        // Add the new lines before the closing tag
        modifiedContent += "        <Include>" + std::string(file) + "</Include>\n";
    }

    // Add the current line to the buffer
    modifiedContent += line + "\n";
  }
  infile.close();

  std::filesystem::create_directories(merged_file.parent_path());
  std::ofstream outfile(merged_file);
  if (!outfile) return false;

  outfile << modifiedContent;
  outfile.close();
  return true;
}

void UIManager::RemoveTemporaryUI(const std::filesystem::path &rcp_equi_file) {
  if (std::filesystem::exists(rcp_equi_file)) {
    std::error_code ec;
    if (!std::filesystem::remove(rcp_equi_file, ec))  // No exceptions
      Rcp::Game::print_chat("Error removing %s: %s", rcp_equi_file.string().c_str(), ec.message().c_str());
  }
}

static void show_big_fonts_error_text(bool is_current_ui_big_fonts_mode) {
  std::string global_skin = UISkin::get_global_default_ui_skin_name();
  bool is_global_big_fonts_mode = UISkin::is_ui_skin_big_fonts_mode(global_skin.c_str());

  std::string message;
  if (is_current_ui_big_fonts_mode == is_global_big_fonts_mode)
    message = std::format(
        "Your new skin '{0}' does not match the active big fonts mode. You must restart the client"
        " and then run '/load <ui_skin> 0' again to reset to the proper appearance.",
        Rcp::Game::get_ui_skin());
  else
    message = std::format(
        "Your character skin '{0}' does not match the big fonts mode of your global default skin '{1}'. To fix, "
        "use options UI Loadskin or the /load <ui_skin> command with your desired ui, which will update both your "
        "global default and character setting, and then restart the client and run '/load <ui_skin> 0' again.",
        Rcp::Game::get_ui_skin(), UISkin::get_global_default_ui_skin_name());
  RcpService::get_instance()->queue_chat_message(message);  // Queued in order to defer print to after UI loaded.
}

static bool is_message_an_error(const std::string &message) {
  // Ignore all except errors (like Warnings).
  if (!message.starts_with("Error:")) return false;

  // Ignore some optional missing buttons in the XML that don't cause crashes.
  static constexpr std::array<const char *, 11> optionals = {
      // UI skins with simplified bag windows.
      "Error: Could not find child Container_Icon in window ContainerWindow",
      "Error: Could not find child DoneButton in window ContainerWindow",
      "Error: Could not find child Container_Label in window ContainerWindow",

      // UI skins with simplified druid / bard tracking window.
      "Error: Could not find child TRW_TrackSortCombobox in window TrackingWnd",
      "Error: Could not find child TRW_FilterRedButton in window TrackingWnd",
      "Error: Could not find child TRW_FilterYellowButton in window TrackingWnd",
      "Error: Could not find child TRW_FilterWhiteButton in window TrackingWnd",
      "Error: Could not find child TRW_FilterBlueButton in window TrackingWnd",
      "Error: Could not find child TRW_FilterLightBlueButton in window TrackingWnd",
      "Error: Could not find child TRW_FilterGreenButton in window TrackingWnd",
      "Error: Could not find child Buff0 in window ShortDurationBuffWindow"};

  for (const auto &optional : optionals)
    if (message == optional) return false;

  return true;
}

// Intercept error messages going to the uierrors.txt file to clarify the messages and optionally show a dialog.
static void LogUIError(const char *error_message) {
  if (!error_message) return;

  std::string message = error_message;
  static constexpr std::string warning_str = "Warning: ";
  static constexpr std::string error_str = "Error: ";

  if (message.back() == '\n') message.pop_back();  // Strip trailing \n which is already added by the client call.

  // Change the typical "fallback to default files" from a scary warning to an info message. Also modify
  // the expected errors from the short duration song buff window to be less concerning/confusing.
  if (message.starts_with(warning_str) && message.ends_with("Attempting to use file from Default skin."))
    message = "Info: " + message.substr(warning_str.length());
  else if (message.starts_with("Error: Could not find child Buff") && message.ends_with("ShortDurationBuffWindow"))
    message = "Info: " + message.substr(error_str.length());

  bool is_error = is_message_an_error(message);

  auto rcp = RcpService::get_instance();
  if (is_error && rcp->ui->setting_show_ui_errors.get()) {
    std::string dialog_msg = message;
    dialog_msg += "\n\nTo prevent showing future UI error messages, select Cancel which executes '/uierrors off'.";
    int result_id = MessageBoxA(GetForegroundWindow(), dialog_msg.c_str(), "Info: Unknown UI XML skin error",
                                MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST);
    if (result_id == IDCANCEL) rcp->ui->setting_show_ui_errors.set(false);
  }

  error_message = message.c_str();
  rcp->hooks->hook_map["LogUIError"]->original(LogUIError)(error_message);
}

// This handles severe parsing errors that are likely to cause an abort. Show in dialog vs hunting for uierrors.txt.
static Rcp::GameUI::CXSTR *__fastcall SidlManager__GetParsingErrorMsg(Rcp::GameUI::SidlManager *sidl_manager,
                                                                       int unused_edx,
                                                                       Rcp::GameUI::CXSTR *msg_result) {
  if (sidl_manager->ErrorMsg.Data) {
    std::string error = std::string(sidl_manager->ErrorMsg);
    if (!error.empty())
      MessageBoxA(GetForegroundWindow(), error.c_str(), "Severe UI XML parsing error", MB_ICONWARNING);
  }
  return RcpService::get_instance()->hooks->hook_map["SidlManager__GetParsingErrorMsg"]->original(
      SidlManager__GetParsingErrorMsg)(sidl_manager, unused_edx, msg_result);
}

void __fastcall LoadSidlHk(void *t, int unused, Rcp::GameUI::CXSTR path1, Rcp::GameUI::CXSTR path2,
                           Rcp::GameUI::CXSTR filename) {
  std::string str_filename = filename;
  if (str_filename != "EQUI.xml") {
    RcpService::get_instance()->hooks->hook_map["LoadSidl"]->original(LoadSidlHk)(t, unused, path1, path2, filename);
    return;
  }

  // Check that the current ui skin big font mode matches the currently active mode.
  bool is_current_ui_big_fonts_mode = UISkin::is_ui_skin_big_fonts_mode(Rcp::Game::get_ui_skin());
  if (is_current_ui_big_fonts_mode != UISkin::is_big_fonts_mode())
    show_big_fonts_error_text(is_current_ui_big_fonts_mode);

  UIManager *ui = RcpService::get_instance()->ui.get();
  std::filesystem::path active_ui_path = std::string(path1);
  std::filesystem::path active_equi_file = active_ui_path / std::filesystem::path(str_filename);
  std::filesystem::path default_equi_file =
      std::filesystem::path(std::string(path2)) / std::filesystem::path(str_filename);
  std::filesystem::path equi_file = std::filesystem::exists(active_equi_file) ? active_equi_file : default_equi_file;

  const char *rcp_equi_filename = "EQUI_Rcp.xml";
  std::filesystem::path rcp_equi_file = active_ui_path / std::filesystem::path(rcp_equi_filename);
  if (ui->WriteTemporaryUI(equi_file, rcp_equi_file))
    filename.Set(rcp_equi_filename);
  else {
    std::string message =
        std::format("rof2ClientPlus failed to generate {0} from {1}", rcp_equi_file.string(), equi_file.string());
    MessageBoxA(NULL, message.c_str(), "rof2ClientPlus EQUI.xml failure", MB_OK | MB_ICONERROR | MB_TOPMOST);
  }

  RcpService::get_instance()->hooks->hook_map["LoadSidl"]->original(LoadSidlHk)(t, unused, path1, path2, filename);
  ui->RemoveTemporaryUI(rcp_equi_file);
}

int __fastcall XMLRead(void *t, int unused, Rcp::GameUI::CXSTR path1, Rcp::GameUI::CXSTR path2,
                       Rcp::GameUI::CXSTR filename) {
  if (UISkin::is_rcp_xml_file(std::string(filename))) path1.Set(UISkin::get_rcp_xml_path().append("").string());
  return RcpService::get_instance()->hooks->hook_map["XMLRead"]->original(XMLRead)(t, unused, path1, path2, filename);
}

int __fastcall XMLReadNoValidate(void *t, int unused, Rcp::GameUI::CXSTR path1, Rcp::GameUI::CXSTR path2,
                                 Rcp::GameUI::CXSTR filename) {
  if (UISkin::is_rcp_xml_file(std::string(filename))) path1.Set(UISkin::get_rcp_xml_path().append("").string());
  return RcpService::get_instance()->hooks->hook_map["XMLReadNoValidate"]->original(XMLReadNoValidate)(
      t, unused, path1, path2, filename);
}

bool UIManager::handle_uierrors(const std::vector<std::string> &args) {
  if (args.size() != 2 || (args[1] != "on" && args[1] != "off")) {
    Rcp::Game::print_chat("Usage: /uierrors <on | off>");
  } else {
    bool enable = (args[1] == "on");
    RcpService::get_instance()->ui->setting_show_ui_errors.set(enable);
    Rcp::Game::print_chat("Showing UI errors is %s.", enable ? "enabled" : "disabled");
  }
  return true;
}

UIManager::UIManager(RcpService *rcp) {
  bool new_ui = Rcp::Game::is_new_ui();
  logger::logf("[ui] UIManager ctor: is_new_ui=%d (registers /rcpoptions only if 1)", (int)new_ui);
  if (!new_ui) return;  // Old UI not supported.

  rcp->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);

  rcp->commands_hook->Add("/uierrors", {}, "Sets (on) or clears (off) the reporting of xml ui errors.",
                           [this](const std::vector<std::string> &args) { return handle_uierrors(args); });

  options = std::make_shared<ui_options>(rcp, this);

  rcp->hooks->Add("ButtonClick", 0x5951E0, ButtonClick_hook, hook_type_detour);
  rcp->hooks->Add("CheckboxClick", 0x5c3480, CheckboxClick_hook, hook_type_detour);
  rcp->hooks->Add("SetSliderValue", 0x5a6c70, SetSliderValue_hook, hook_type_detour);
  rcp->hooks->Add("SetComboValue", 0x579af0, SetComboValue_hook, hook_type_detour);
  rcp->hooks->Add("LoadSidl", 0x5992c0, LoadSidlHk, hook_type_detour);
  rcp->hooks->Add("SidlManager__GetParsingErrorMsg", 0x0058e300, SidlManager__GetParsingErrorMsg, hook_type_detour);
  rcp->hooks->Add("LogUIError", 0x00435eae, LogUIError, hook_type_detour);
  rcp->hooks->Add("XMLRead", 0x58D640, XMLRead, hook_type_detour);
  rcp->hooks->Add("XMLReadNoValidate", 0x58DA10, XMLReadNoValidate, hook_type_detour);
}
