// rof2ClientPlus - remove the client's distance fog. See no_fog.h.
#include "no_fog.h"

#include <windows.h>
#include <d3d9.h>

#include <string>
#include <vector>

#include "commands.h"
#include "directx.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

// Standard IDirect3DDevice9 vtable layout: EndScene is slot 42 (see directx.cpp,
// where that index is verified live), which places SetRenderState at slot 57.
static constexpr int kSetRenderStateVtIndex = 57;

typedef HRESULT(WINAPI *SetRenderState_t)(IDirect3DDevice9 *, D3DRENDERSTATETYPE, DWORD);

static SetRenderState_t g_orig_srs = nullptr;  // Captured from the vtable before we patch it.
static bool g_enabled = true;                  // On by default: removing fog is the whole point.
static bool g_hooked = false;                  // Guards the one-time vtable install.

static constexpr char kIniSection[] = "Fog";
static constexpr char kIniKey[] = "RemoveDistanceFog";

static void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKey)) g_enabled = ini.getValue<bool>(kIniSection, kIniKey);
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, kIniKey, g_enabled);
}

// Accessors for the options-window UI. The filter reads g_enabled every frame,
// so a set takes effect on the next SetRenderState call - no re-hook needed.
namespace no_fog_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  save_settings();
}
}  // namespace no_fog_settings

// Hot path: the client calls this thousands of times per frame. Keep it a bare
// branch plus a tail call to the captured original - no hook-map lookup, no
// guard. When the feature is on we clamp the fog master switch off so the world
// pass never applies its depth haze; every other render state passes straight
// through. Toggling the feature just flips g_enabled - no re-hooking needed,
// because the client re-issues D3DRS_FOGENABLE every frame.
static HRESULT WINAPI hkSetRenderState(IDirect3DDevice9 *dev, D3DRENDERSTATETYPE state, DWORD value) {
  if (g_enabled && state == D3DRS_FOGENABLE) value = FALSE;
  return g_orig_srs(dev, state, value);
}

// Install the SetRenderState filter the first time EndScene hands us a live
// device. It shares one static vtable with the throwaway device directx.cpp
// patched for EndScene, so this one swap routes the game's device through us.
// Runs on the render thread between frames, so no SetRenderState call races the
// swap. We capture the original pointer BEFORE Add() patches the slot.
static void ensure_hooked(IDirect3DDevice9 *dev) {
  if (g_hooked || !dev) return;
  g_hooked = true;  // Set first so a transient failure doesn't retry every frame.

  void **vtable = *reinterpret_cast<void ***>(dev);
  g_orig_srs = reinterpret_cast<SetRenderState_t>(vtable[kSetRenderStateVtIndex]);
  int slot_addr = reinterpret_cast<int>(&vtable[kSetRenderStateVtIndex]);
  RcpService::get_instance()->hooks->Add("rcp_nofog_srs", slot_addr, hkSetRenderState, hook_type_vtable);
  logger::logf("[fog] SetRenderState filter installed (device=%p slot=0x%x) remove_fog=%d", (void *)dev, slot_addr,
               (int)g_enabled);
}

static void print_status() {
  Rcp::Game::print_chat("rof2ClientPlus distance fog: %s",
                        g_enabled ? "REMOVED (/rcpfog off restores it)" : "ON - client default (/rcpfog on removes it)");
}

NoFog::NoFog(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  logger::logf("[fog] settings loaded: remove_distance_fog=%d (filter installs on first frame)", (int)g_enabled);

  // Watch each EndScene for the live device; ensure_hooked installs once then no-ops.
  directx::add_render_callback([](IDirect3DDevice9 *dev) { ensure_hooked(dev); });

  rcp->commands_hook->Add(
      "/rcpfog", {"/nofog"},
      "Distance fog: '/rcpfog on' removes it, '/rcpfog off' restores the client default, '/rcpfog' toggles.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1" || a == "remove")
            g_enabled = true;
          else if (a == "off" || a == "0" || a == "restore")
            g_enabled = false;
          else
            g_enabled = !g_enabled;
        } else {
          g_enabled = !g_enabled;
        }
        save_settings();
        print_status();
        return true;
      });
  logger::log("[fog] /rcpfog registered");
}
