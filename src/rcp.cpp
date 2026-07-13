// rof2ClientPlus - central service singleton implementation.
#include "rcp.h"

#include <windows.h>

#include "aa_exp.h"
#include "binds.h"
#include "callbacks.h"
#include "camera_mods.h"
#include "chase_cam.h"
#include "chat_shortcuts.h"
#include "chat_timestamp.h"
#include "commands.h"
#include "entity_manager.h"
#include "equip_item.h"
#include "floating_damage.h"
#include "font_overlay.h"
#include "game_functions.h"
#include "hide_corpse.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "keybinds.h"
#include "logger.h"
#include "mouse_mods.h"
#include "nameplate.h"
#include "no_fog.h"
#include "rcp_options_ui.h"
#include "sound_mods.h"
#include "target_ring.h"
#include "view_distance.h"
#include "ui_manager.h"
#include "ui_skin.h"
#include "window_watch.h"

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
  logger::log("  -> NamePlate");
  nameplate_mod = std::make_unique<NamePlate>(this);  // SetNameSpriteTint detour (installs now; acts only when enabled).
  logger::log("  -> WindowWatch");
  window_watch_mod = std::make_unique<WindowWatch>(this);  // Registers /rcpwindow; diagnostics already live from on_attach.
  logger::log("  -> KeyBinds");
  keybinds_mod = std::make_unique<KeyBinds>(this);  // Extra key binds; detours install now, act only on hijacked cmds.
  logger::log("  -> RcpOptionsUI");
  options_ui = std::make_unique<RcpOptionsUI>(this);  // LoadSidl hook also installs in-game (deferred).
  logger::log("  -> FontOverlay");
  font_overlay = std::make_unique<FontOverlay>(this);  // Registers a directx render callback + /rcpfont.
  logger::log("  -> NoFog");
  no_fog = std::make_unique<NoFog>(this);  // /rcpfog: filters D3DRS_FOGENABLE on the shared d3d9 device.
  logger::log("  -> TargetRing");
  target_ring = std::make_unique<TargetRing>(this);  // /rcpring: draws the solid ring at font_overlay's in-scene seam.
  logger::log("  -> ViewDistance");
  view_distance = std::make_unique<ViewDistance>(this);  // /rcpviewdist: forces the terrain clip-plane int past the slider cap.
  logger::log("  -> EquipItem");
  equip_item = std::make_unique<EquipItem>(this);  // /rcpequip: right-click a bag item to auto-equip (RoF2-native CInvSlot RButton detour).
  logger::log("  -> ChatShortcuts");
  chat_shortcuts = std::make_unique<ChatShortcuts>(this);  // /rcpchat: Zeal-style chat tokens (%n/%h/%loc/%thp) via DoPercentConvert detour.
  logger::log("  -> ChatTimestamp");
  chat_timestamp = std::make_unique<ChatTimestamp>(this);  // /timestamp: Zeal-style chat timestamps via dsp_chat detour.
  logger::log("  -> SoundMods");
  sound_mods = std::make_unique<SoundMods>(this);  // /rcpsound: mute sounds by name (Asset::Play detour; thunder to start).
  logger::log("  -> FloatingDamage");
  floating_damage = std::make_unique<FloatingDamage>(this);  // /rcpfcd: floating combat damage numbers (ReportSuccessfulHit detour + billboard).
  logger::log("  -> HideCorpse");
  hide_corpse = std::make_unique<HideCorpse>(this);  // /hidecorpses always + showlast: continuous NPC-corpse hiding (render-callback re-assert).
  logger::log("  -> AaExp");
  aa_exp = std::make_unique<AaExp>(this);  // /rcpaaexp: auto-gate AA experience % by current-level XP (writes PercentEXPtoAA + server sync).
  logger::log("  -> modules done (foundation subset)");

  logger::log("RcpService: modules constructed");
}

RcpService::~RcpService() { ptr_service = nullptr; }
