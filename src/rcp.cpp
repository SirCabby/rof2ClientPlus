// rof2ClientPlus - central service singleton implementation.
#include "rcp.h"

#include <windows.h>

#include "binds.h"
#include "callbacks.h"
#include "camera_mods.h"
#include "chase_cam.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "mouse_mods.h"
#include "rcp_options_ui.h"
#include "ui_manager.h"
#include "ui_skin.h"

RcpService *RcpService::ptr_service = nullptr;

RcpService::RcpService() {
  // Populate the singleton pointer immediately: submodule constructors reach
  // back through RcpService::get_instance() while they install their hooks.
  ptr_service = this;
  logger::log("RcpService: constructing modules");

  // FOUNDATION SUBSET (stock RoF2 port, milestone 1): only construct the modules
  // whose addresses have been repointed to the stock RoF2 client. The remaining
  // modules (callbacks, entity_manager, binds, camera_mods, ui) still carry TAKP
  // addresses, so they are compiled but NOT constructed - their wrong-address
  // hooks never install and cannot crash. They come back online as they are
  // ported. The per-module logging flushes each line, so the last log line
  // identifies exactly which constructor faulted if one does.
  logger::log("  -> IO_ini");
  ini = std::make_unique<IO_ini>(IO_ini::kRcpIniFilename);
  logger::log("  -> HookWrapper");
  hooks = std::make_unique<HookWrapper>();
  logger::log("  -> ChatCommands");
  commands_hook = std::make_unique<ChatCommands>(this);  // Installs the InterpretCmd hook.
  logger::log("  -> MouseMods");
  mouse_mods = std::make_unique<MouseMods>(this);  // Mouse hook installs in-game (deferred).
  logger::log("  -> ChaseCam");
  chase_cam_mod = std::make_unique<ChaseCam>(this);  // Cam6 positioner detour (installs now; acts only in chase view).
  logger::log("  -> RcpOptionsUI");
  options_ui = std::make_unique<RcpOptionsUI>(this);  // LoadSidl hook also installs in-game (deferred).
  logger::log("  -> modules done (foundation subset)");

  logger::log("RcpService: modules constructed");
}

RcpService::~RcpService() { ptr_service = nullptr; }
