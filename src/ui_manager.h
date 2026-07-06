#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "game_ui.h"
#include "ui_options.h"
#include "rcp_settings.h"

// Trimmed from Zeal's UIManager (MIT): the client SIDL/uifile plumbing plus the
// button/slider/checkbox callback registry, keeping only the options window.
class UIManager {
 public:
  Rcp::GameUI::SliderWnd *GetSlider(std::string name);
  Rcp::GameUI::BasicWnd *GetCheckbox(std::string name);
  Rcp::GameUI::BasicWnd *GetButton(std::string name);
  Rcp::GameUI::BasicWnd *GetCombo(std::string name);
  std::function<void(Rcp::GameUI::SliderWnd *, int)> GetSliderCallback(Rcp::GameUI::SliderWnd *wnd);
  std::function<void(Rcp::GameUI::BasicWnd *, int)> GetComboCallback(Rcp::GameUI::BasicWnd *wnd);
  std::function<void(Rcp::GameUI::BasicWnd *)> GetButtonCallback(Rcp::GameUI::BasicWnd *wnd);
  std::function<void(Rcp::GameUI::BasicWnd *)> GetCheckboxCallback(Rcp::GameUI::BasicWnd *wnd);

  Rcp::GameUI::BasicWnd *clicked_button = nullptr;
  std::unordered_map<Rcp::GameUI::BasicWnd *, std::unordered_map<std::string, Rcp::GameUI::BasicWnd *>>
      WindowChildren;
  void SetLabelValue(std::string name, const char *format, ...);
  void SetSliderValue(std::string name, int value);
  void SetSliderValue(std::string name, float value);
  void SetComboValue(std::string name, int value);
  void SetChecked(std::string name, bool checked);
  Rcp::GameUI::BasicWnd *AddButtonCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                            std::function<void(Rcp::GameUI::BasicWnd *)> callback,
                                            bool log_errors = true);
  Rcp::GameUI::BasicWnd *AddCheckboxCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                              std::function<void(Rcp::GameUI::BasicWnd *)> callback,
                                              bool log_errors = true);
  Rcp::GameUI::BasicWnd *AddSliderCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                            std::function<void(Rcp::GameUI::SliderWnd *, int)> callback,
                                            int max_val = 100, bool log_errors = true);
  Rcp::GameUI::BasicWnd *AddComboCallback(Rcp::GameUI::BasicWnd *wnd, std::string name,
                                           std::function<void(Rcp::GameUI::BasicWnd *, int)> callback,
                                           bool log_errors = true);
  void AddLabel(Rcp::GameUI::BasicWnd *wnd, std::string name, bool log_errors = true);
  void AddListItems(Rcp::GameUI::ComboWnd *wnd, const std::vector<std::string> data);
  void AddListItems(Rcp::GameUI::ListWnd *wnd, const std::vector<std::vector<std::string>> data);
  void AddListItems(Rcp::GameUI::ListWnd *wnd, const std::vector<std::string> data);
  Rcp::GameUI::SidlWnd *CreateSidlScreenWnd(const std::string &name);
  void DestroySidlScreenWnd(Rcp::GameUI::SidlWnd *sidl_wnd);
  bool WriteTemporaryUI(const std::filesystem::path &orig_xml_file, const std::filesystem::path &merged_xml_file);
  void RemoveTemporaryUI(const std::filesystem::path &file_path);
  UIManager(class RcpService *rcp);
  std::shared_ptr<ui_options> options = nullptr;

  RcpSetting<bool> setting_show_ui_errors = {true, "Rcp", "ShowUIErrors", false};

 private:
  bool handle_uierrors(const std::vector<std::string> &args);

  std::unordered_map<std::string, Rcp::GameUI::BasicWnd *> checkbox_names;
  std::unordered_map<std::string, Rcp::GameUI::BasicWnd *> button_names;
  std::unordered_map<Rcp::GameUI::BasicWnd *, std::function<void(Rcp::GameUI::BasicWnd *)>> checkbox_callbacks;
  std::unordered_map<Rcp::GameUI::BasicWnd *, std::function<void(Rcp::GameUI::BasicWnd *)>> button_callbacks;
  std::unordered_map<Rcp::GameUI::SliderWnd *, std::function<void(Rcp::GameUI::SliderWnd *, int)>> slider_callbacks;
  std::unordered_map<Rcp::GameUI::BasicWnd *, std::function<void(Rcp::GameUI::BasicWnd *, int)>> combo_callbacks;
  std::unordered_map<std::string, Rcp::GameUI::SliderWnd *> slider_names;

  std::unordered_map<std::string, Rcp::GameUI::BasicWnd *> combo_names;
  std::unordered_map<std::string, Rcp::GameUI::BasicWnd *> label_names;

  void CleanUI();
};
