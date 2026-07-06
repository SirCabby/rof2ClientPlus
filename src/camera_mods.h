#pragma once
#include <windows.h>

#include <chrono>
#include <functional>

#include "game_functions.h"
#include "memory.h"
#include "vectors.h"
#include "rcp_settings.h"

class CameraMods {
 public:
  RcpSetting<bool> enabled = {true, "Rcp", "MouseSmoothing", false, [this](bool val) { synchronize_set_enable(); }};
  RcpSetting<bool> cam_lock = {true, "Rcp", "CamLock", false};
  RcpSetting<bool> use_old_sens = {false, "Rcp", "OldSens", false};
  RcpSetting<float> user_sensitivity_x = {0.1f, "Rcp", "MouseSensitivityX", false};
  RcpSetting<float> user_sensitivity_y = {0.1f, "Rcp", "MouseSensitivityY", false};
  RcpSetting<float> user_sensitivity_x_3rd = {0.1f, "Rcp", "MouseSensitivityX3rd", false};
  RcpSetting<float> user_sensitivity_y_3rd = {0.1f, "Rcp", "MouseSensitivityY3rd", false};
  RcpSetting<float> fov = {45.f, "Rcp", "Fov", false, [this](float val) { synchronize_fov(); }};
  RcpSetting<int> pan_delay = {0, "Rcp", "PanDelay", false};
  RcpSetting<bool> setting_selfclickthru = {false, "Rcp", "SelfClickThru", false};
  RcpSetting<bool> setting_playerclickthru = {false, "Rcp", "PlayerClickThru", false};
  RcpSetting<bool> setting_leftclickcon = {false, "Rcp", "LeftClickCon", false};
  RcpSetting<bool> setting_toggle_overhead_view = {true, "Camera", "ToggleOverheadView", false};
  RcpSetting<bool> setting_toggle_rcp_view = {true, "Camera", "ToggleRcpView", false};
  RcpSetting<bool> setting_toggle_free1_view = {true, "Camera", "ToggleFree1View", false};
  RcpSetting<bool> setting_toggle_free2_view = {true, "Camera", "ToggleFree2View", false};
  RcpSetting<bool> setting_dampen_levitation = {false, "Rcp", "DampenLevitation", false,
                                                 [this](float val) { synchronize_lev(); }};
  const float max_zoom_out = 100;
  const float min_zoom_in = 5.f;  // Transitions to first person below this.
  const float zoom_speed = 5.f;

  CameraMods(class RcpService *pHookWrapper);
  ~CameraMods();
  void handle_process_mouse_and_get_key();
  bool handle_mouse_wheel(int delta);
  bool handle_proc_mouse();
  void handle_proc_rmousedown(int x, int y);
  void handle_do_cam_ai();

  void add_options_callback(std::function<void()> callback) { update_options_ui_callback = callback; };

 private:
  float fps = 60.f;
  float rcp_cam_pitch = 0.f;
  float rcp_cam_yaw = 0.f;
  float rcp_cam_zoom = 0.f;
  float desired_zoom = 0.f;
  ULONGLONG lmouse_time = 0;
  POINT lmouse_cursor_pos;
  bool hide_cursor = false;
  float sensitivity_x = 0.7f;
  float sensitivity_y = 0.4f;
  std::chrono::steady_clock::time_point lastTime;
  bool ui_active = false;
  bool reset_camera = false;       // Allows for a deferred reset of rcp cam.
  bool chase_mode_active = false;  // Defers camera yaw sync until after process_physics.
  std::function<void()> update_options_ui_callback;

  void synchronize_set_enable();
  void synchronize_old_ui();
  void handle_toggle_cam();
  void callback_zone();
  bool callback_packet(UINT opcode, char *buffer, UINT len);
  void update_desired_zoom(float zoom);
  void synchronize_fov();
  void synchronize_lev();
  void set_rcp_cam_active(bool activate);
  bool is_rcp_cam_active() const;
  bool calc_camera_positions(Vec3 &head_pos, Vec3 &wanted_pos) const;
  void update_left_pan(DWORD camera_view);
  void update_autofollow();
  void interpolate_zoom();
  void process_time_tick();
  void update_fps_sensitivity();
};
