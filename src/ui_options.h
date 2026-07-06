#pragma once

#include <windows.h>

#include "game_ui.h"

// Trimmed from Zeal's ui_options (MIT): a single custom SIDL options window
// (EQUI_RcpOptions.xml) with just the Cam tab. Unlike Zeal (which slaves the
// window's visibility to the stock Options window), this window is opened and
// closed explicitly with the /rcpoptions command (and its own close box). The
// checkbox/slider callbacks are delivered through UIManager's global
// button/slider hooks, so no custom WndNotification override is required.
class ui_options {
 public:
  ui_options(class RcpService *rcp, class UIManager *mgr);
  ~ui_options();

  // Returns a color for a client color index. This trimmed build has no custom
  // color table, so it defers to the client's own user colors (used by con
  // color helpers in game_functions.cpp).
  DWORD GetColor(int index) const;

  Rcp::GameUI::SidlWnd *GetOptionsWindow() { return wnd; }  // Short-term access only.

 private:
  void InitUI();
  void InitCamera();
  void UpdateOptions();
  void UpdateOptionsCamera();
  void toggle_window();  // /rcpoptions: show/hide the window.
  void CleanUI();
  void Deactivate();

  Rcp::GameUI::SidlWnd *wnd = nullptr;
  UIManager *const ui;
};
