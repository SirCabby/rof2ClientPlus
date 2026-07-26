// rof2ClientPlus - respawn-window auto-close (see respawn_close.h).
#include <windows.h>

#include "respawn_close.h"

#include <cstdint>
#include <cstring>

#include "crash_handler.h"   // rcp_guard::run - detour/POD-body fault net
#include "game_functions.h"  // Rcp::Game::is_in_game()
#include "logger.h"          // logger::log/logf (temporary diagnostics)
#include "rcp.h"
#include "rebase.h"          // ::Rcp::eqva

namespace {

// ---- stock RoF2 addresses / offsets (mirrored from spellbook_ui.cpp find_game_screen
// / cxstr_eq, rcp_options_ui.cpp is_visible / show_window, chat_shortcuts.cpp self
// pointer; RespawnTimer offset from eqlib PlayerClient.h, whose neighbouring offsets
// match the disasm-confirmed ones chat_shortcuts already uses). ----
char **const kDisplay = reinterpret_cast<char **>(::Rcp::eqva(0xDD2660));
constexpr int kGameScreensOffset = 0x2d84;
constexpr int kScreenRecSize = 0x10;
constexpr int kSidlTextOffset = 0x1dc;
constexpr int kRepLength = 0x08, kRepEncoding = 0x0c, kRepData = 0x14;
constexpr int kDShowOffset = 0x196;  // CXWnd::dShow
constexpr int kShowVtOffset = 0xD8;  // CXWnd vtable Show()

// Local player (pinstLocalPlayer) + entity field offsets (position from chat_shortcuts.cpp;
// PlayerState/StandState from eqlib PlayerClient.h - logged for diagnostics/fallback).
void **const kSelfPtr = reinterpret_cast<void **>(::Rcp::eqva(0xDD2630));
constexpr int kEntY = 0x64, kEntX = 0x68, kEntZ = 0x6c;
constexpr int kPlayerState = 0x14c;  // uint32
constexpr int kStandState = 0x35c;   // uint8

bool cxstr_eq(const void *cxstr_field, const char *want) {
  char *rep = *reinterpret_cast<char *const *>(cxstr_field);
  if (!rep || reinterpret_cast<uintptr_t>(rep) < 0x10000) return false;
  const uint32_t len = *reinterpret_cast<uint32_t *>(rep + kRepLength);
  const uint32_t enc = *reinterpret_cast<uint32_t *>(rep + kRepEncoding);
  if (enc != 0 || len == 0 || len > 256) return false;
  const size_t want_len = std::strlen(want);
  if (len != want_len) return false;
  return _strnicmp(rep + kRepData, want, want_len) == 0;
}

void *find_game_screen(const char *name) {
  char *disp = *kDisplay;
  if (!disp) return nullptr;
  char *mgr = disp + kGameScreensOffset;
  const int len = *reinterpret_cast<int *>(mgr);
  char *arr = *reinterpret_cast<char **>(mgr + 4);
  if (!arr || len <= 0 || len > 4096) return nullptr;
  for (int i = 0; i < len; ++i) {
    char **ppwnd = *reinterpret_cast<char ***>(arr + i * kScreenRecSize);
    if (!ppwnd || !*ppwnd) continue;
    if (cxstr_eq(*ppwnd + kSidlTextOffset, name)) return *ppwnd;
  }
  return nullptr;
}

bool is_visible(void *wnd) {
  return wnd ? *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(wnd) + kDShowOffset) != 0 : false;
}

void hide_window(void *wnd) {
  if (!wnd) return;
  void *vtable = *reinterpret_cast<void **>(wnd);
  void *show_fn = *reinterpret_cast<void **>(reinterpret_cast<char *>(vtable) + kShowVtOffset);
  reinterpret_cast<int(__thiscall *)(void *, int, int, int)>(show_fn)(wnd, 0, 1, 1);
}

}  // namespace

RespawnClose::RespawnClose(RcpService * /*rcp*/) {}

void RespawnClose::on_frame() {
  rcp_guard::run("respawnclose.frame", [this]() {
    // NB: do NOT reset the anchor here. A cross-zone respawn drops is_in_game()
    // to false during the zone load; we must keep the death-spot anchor so the
    // big position delta is still detected once we're back in-game at bind.
    if (!Rcp::Game::is_in_game()) return;

    void *wnd = find_game_screen("RespawnWnd");

    // Actively dismissing after a detected respawn: keep hiding for a short burst
    // so a re-show can't survive it.
    if (closing_frames_ > 0) {
      --closing_frames_;
      if (wnd && is_visible(wnd)) hide_window(wnd);
      return;
    }

    if (!wnd || !is_visible(wnd)) {  // chooser not up -> reset, nothing to do
      have_anchor_ = false;
      return;
    }

    char *self = *reinterpret_cast<char **>(kSelfPtr);
    if (!self) return;
    const float y = *reinterpret_cast<float *>(self + kEntY);
    const float x = *reinterpret_cast<float *>(self + kEntX);
    const float z = *reinterpret_cast<float *>(self + kEntZ);
    const unsigned int pstate = *reinterpret_cast<unsigned int *>(self + kPlayerState);
    const unsigned int stand = *reinterpret_cast<unsigned char *>(self + kStandState);

    // First frame the chooser is up: anchor the death-spot position (and log the
    // hovering-dead StandState/PlayerState for reference).
    if (!have_anchor_) {
      ax_ = x; ay_ = y; az_ = z; have_anchor_ = true;
      logger::logf("[respawn] chooser up: pos=(%d,%d,%d) StandState=%u PlayerState=%u",
                   (int)x, (int)y, (int)z, stand, pstate);
      return;
    }

    // The chooser is up and we've moved far from the death spot -> we picked an
    // option and respawned/teleported (this cannot fire while standing still and
    // deciding). Dismiss the lingering chooser.
    const float dx = x - ax_, dy = y - ay_, dz = z - az_;
    const float dist2 = dx * dx + dy * dy + dz * dz;
    if (dist2 > 100.0f) {  // > ~10 units
      logger::logf("[respawn] respawn move dist2=%d StandState=%u PlayerState=%u -> hiding chooser",
                   (int)dist2, stand, pstate);
      if (wnd && is_visible(wnd)) hide_window(wnd);
      closing_frames_ = 30;  // ~0.5s of follow-up hides in case it re-shows
      have_anchor_ = false;
    }
  });
}
