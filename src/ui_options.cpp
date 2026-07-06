#include "ui_options.h"

#include <filesystem>

#include "callbacks.h"
#include "camera_mods.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "rcp.h"
#include "ui_manager.h"
#include "ui_skin.h"

// Slider values are integers 0..100; sensitivities are stored as 0.0..1.0.
static float GetSensitivityFromSlider(int value) { return value / 100.f; }
static int GetSensitivityForSlider(RcpSetting<float> &value) {
  return value.get() > 0 ? static_cast<int>(value.get() * 100.f) : 0;
}

void ui_options::InitUI() {
  if (wnd) Rcp::Game::print_chat("Warning: Init out of sync for ui_options");

  std::filesystem::path xml_file = UISkin::get_rcp_xml_path() / std::filesystem::path("EQUI_RcpOptions.xml");
  if (!wnd && ui && std::filesystem::exists(xml_file)) wnd = ui->CreateSidlScreenWnd("RcpOptions");

  if (!wnd) {
    Rcp::Game::print_chat("Error: Failed to load %s", xml_file.string().c_str());
    return;
  }

  wnd->IsVisible = false;  // Created hidden; opened later via /rcpoptions.
  InitCamera();

  // Control states are synchronized to the current settings in toggle_window()
  // each time the window is opened.
}

void ui_options::InitCamera() {
  if (!wnd) return;

  ui->AddCheckboxCallback(wnd, "Rcp_Cam",
                          [](Rcp::GameUI::BasicWnd *w) { RcpService::get_instance()->camera_mods->enabled.set(w->Checked); });
  ui->AddCheckboxCallback(wnd, "Rcp_UseOldSens", [](Rcp::GameUI::BasicWnd *w) {
    RcpService::get_instance()->camera_mods->use_old_sens.set(w->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Rcp_Cam_TurnLocked", [](Rcp::GameUI::BasicWnd *w) {
    RcpService::get_instance()->camera_mods->cam_lock.set(w->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Rcp_Cam_ToggleOverheadView", [](Rcp::GameUI::BasicWnd *w) {
    RcpService::get_instance()->camera_mods->setting_toggle_overhead_view.set(w->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Rcp_Cam_ToggleRcpView", [](Rcp::GameUI::BasicWnd *w) {
    RcpService::get_instance()->camera_mods->setting_toggle_rcp_view.set(w->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Rcp_Cam_ToggleFree1View", [](Rcp::GameUI::BasicWnd *w) {
    RcpService::get_instance()->camera_mods->setting_toggle_free1_view.set(w->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Rcp_Cam_ToggleFree2View", [](Rcp::GameUI::BasicWnd *w) {
    RcpService::get_instance()->camera_mods->setting_toggle_free2_view.set(w->Checked);
  });

  ui->AddSliderCallback(wnd, "Rcp_PanDelaySlider", [this](Rcp::GameUI::SliderWnd *w, int value) {
    RcpService::get_instance()->camera_mods->pan_delay.set(value * 4);
    ui->SetLabelValue("Rcp_PanDelayValueLabel", "%d ms", RcpService::get_instance()->camera_mods->pan_delay.get());
  });
  ui->AddSliderCallback(wnd, "Rcp_FirstPersonSlider_X", [this](Rcp::GameUI::SliderWnd *w, int value) {
    RcpService::get_instance()->camera_mods->user_sensitivity_x.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Rcp_FirstPersonLabel_X", "%.2f",
                      RcpService::get_instance()->camera_mods->user_sensitivity_x.get());
  });
  ui->AddSliderCallback(wnd, "Rcp_FirstPersonSlider_Y", [this](Rcp::GameUI::SliderWnd *w, int value) {
    RcpService::get_instance()->camera_mods->user_sensitivity_y.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Rcp_FirstPersonLabel_Y", "%.2f",
                      RcpService::get_instance()->camera_mods->user_sensitivity_y.get());
  });
  ui->AddSliderCallback(wnd, "Rcp_ThirdPersonSlider_X", [this](Rcp::GameUI::SliderWnd *w, int value) {
    RcpService::get_instance()->camera_mods->user_sensitivity_x_3rd.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Rcp_ThirdPersonLabel_X", "%.2f",
                      RcpService::get_instance()->camera_mods->user_sensitivity_x_3rd.get());
  });
  ui->AddSliderCallback(wnd, "Rcp_ThirdPersonSlider_Y", [this](Rcp::GameUI::SliderWnd *w, int value) {
    RcpService::get_instance()->camera_mods->user_sensitivity_y_3rd.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Rcp_ThirdPersonLabel_Y", "%.2f",
                      RcpService::get_instance()->camera_mods->user_sensitivity_y_3rd.get());
  });
  ui->AddSliderCallback(wnd, "Rcp_FoVSlider", [this](Rcp::GameUI::SliderWnd *w, int value) {
    float val = 45.0f + (static_cast<float>(value) / 100.0f) * 45.0f;
    RcpService::get_instance()->camera_mods->fov.set(val);
    ui->SetLabelValue("Rcp_FoVValueLabel", "%.0f", val);
  });

  ui->AddLabel(wnd, "Rcp_PanDelayValueLabel");
  ui->AddLabel(wnd, "Rcp_FirstPersonLabel_X");
  ui->AddLabel(wnd, "Rcp_FirstPersonLabel_Y");
  ui->AddLabel(wnd, "Rcp_ThirdPersonLabel_X");
  ui->AddLabel(wnd, "Rcp_ThirdPersonLabel_Y");
  ui->AddLabel(wnd, "Rcp_FoVValueLabel");
}

void ui_options::UpdateOptions() {
  if (!wnd) return;
  UpdateOptionsCamera();
}

void ui_options::UpdateOptionsCamera() {
  if (!wnd) return;
  auto cam = RcpService::get_instance()->camera_mods.get();

  ui->SetChecked("Rcp_Cam", cam->enabled.get());
  ui->SetChecked("Rcp_UseOldSens", cam->use_old_sens.get());
  ui->SetChecked("Rcp_Cam_TurnLocked", cam->cam_lock.get());
  ui->SetChecked("Rcp_Cam_ToggleOverheadView", cam->setting_toggle_overhead_view.get());
  ui->SetChecked("Rcp_Cam_ToggleRcpView", cam->setting_toggle_rcp_view.get());
  ui->SetChecked("Rcp_Cam_ToggleFree1View", cam->setting_toggle_free1_view.get());
  ui->SetChecked("Rcp_Cam_ToggleFree2View", cam->setting_toggle_free2_view.get());

  ui->SetSliderValue("Rcp_PanDelaySlider", cam->pan_delay.get() > 0 ? cam->pan_delay.get() / 4 : 0);
  ui->SetSliderValue("Rcp_FirstPersonSlider_X", GetSensitivityForSlider(cam->user_sensitivity_x));
  ui->SetSliderValue("Rcp_FirstPersonSlider_Y", GetSensitivityForSlider(cam->user_sensitivity_y));
  ui->SetSliderValue("Rcp_ThirdPersonSlider_X", GetSensitivityForSlider(cam->user_sensitivity_x_3rd));
  ui->SetSliderValue("Rcp_ThirdPersonSlider_Y", GetSensitivityForSlider(cam->user_sensitivity_y_3rd));
  ui->SetSliderValue("Rcp_FoVSlider", static_cast<int>((cam->fov.get() - 45.0f) / 45.0f * 100.0f));

  ui->SetLabelValue("Rcp_PanDelayValueLabel", "%d ms", cam->pan_delay.get());
  ui->SetLabelValue("Rcp_FoVValueLabel", "%.0f", cam->fov.get());
  ui->SetLabelValue("Rcp_FirstPersonLabel_X", "%.2f", cam->user_sensitivity_x.get());
  ui->SetLabelValue("Rcp_FirstPersonLabel_Y", "%.2f", cam->user_sensitivity_y.get());
  ui->SetLabelValue("Rcp_ThirdPersonLabel_X", "%.2f", cam->user_sensitivity_x_3rd.get());
  ui->SetLabelValue("Rcp_ThirdPersonLabel_Y", "%.2f", cam->user_sensitivity_y_3rd.get());
}

// Toggles the options window (the /rcpoptions command). Refreshes the controls
// to the current settings whenever it is opened.
void ui_options::toggle_window() {
  if (!wnd) {
    Rcp::Game::print_chat("rof2ClientPlus options window is unavailable (is uifiles/rcp installed?).");
    return;
  }
  bool make_visible = !wnd->IsVisible;
  if (make_visible) UpdateOptions();
  wnd->show(make_visible, true);
}

void ui_options::CleanUI() {
  if (!wnd) return;
  RcpService::get_instance()->ui->DestroySidlScreenWnd(wnd);
  wnd = nullptr;
}

void ui_options::Deactivate() {
  if (!wnd) return;
  wnd->show(0, false);
}

DWORD ui_options::GetColor(int index) const { return Rcp::Game::get_user_color(index); }

ui_options::ui_options(RcpService *rcp, UIManager *mgr) : ui(mgr) {
  wnd = nullptr;
  rcp->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  rcp->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  rcp->callbacks->AddGeneric([this]() { Deactivate(); }, callback_type::DeactivateUI);

  // Explicit trigger to open/close the options window.
  rcp->commands_hook->Add("/rcpoptions", {"/rcpopts"},
                          "Opens or closes the rof2ClientPlus options window (Cam tab).",
                          [this](std::vector<std::string> &args) {
                            toggle_window();
                            return true;
                          });

  // Keep the UI in sync when settings change via /rcpcam and friends.
  rcp->camera_mods->add_options_callback([this]() { UpdateOptionsCamera(); });
}

ui_options::~ui_options() {}
