// rof2ClientPlus - central service singleton.
//
// The singleton that owns every feature module (modeled on Zeal's service
// class; see NOTICE), trimmed to the modules this build needs. The DllMain worker
// creates a single RcpService, whose constructor installs all hooks, commands,
// callbacks, and UI wiring. Modules are ordered by dependency: the core
// framework (ini, hooks, callbacks, commands, entities, binds) first, then the
// feature modules that build on them.
#pragma once

#include <memory>
#include <string>
#include <vector>

#define RCP_VERSION "0.1.0"

class RcpService {
 private:
  RcpService();  // Use create(); this is a singleton.
  RcpService(const RcpService &) = delete;
  RcpService &operator=(const RcpService &) = delete;

 public:
  ~RcpService();

  // Creates the singleton (idempotent). Called once from the DllMain worker.
  static void create() {
    if (!ptr_service) new RcpService();
  }

  // Returns the singleton, or nullptr before create().
  static RcpService *get_instance() { return ptr_service; }

  // Defers a chat message until the game UI is ready to print it.
  void queue_chat_message(const std::string &message) { print_buffer.push_back(message); }

  // Core framework (minimal internal dependencies), in construction order.
  std::unique_ptr<class IO_ini> ini;
  std::unique_ptr<class HookWrapper> hooks;             // Inline detour engine.
  std::unique_ptr<class CallbackManager> callbacks;    // Uses hooks.
  std::unique_ptr<class ChatCommands> commands_hook;   // Uses hooks.
  std::unique_ptr<class EntityManager> entity_manager; // Uses hooks.
  std::unique_ptr<class Binds> binds_hook;             // Uses hooks + callbacks.

  // Feature modules.
  std::unique_ptr<class MouseMods> mouse_mods;        // The /rcpcam mouse handling (stock RoF2 port).
  std::unique_ptr<class ChaseCam> chase_cam_mod;      // The /rcpchase third-person chase camera (Phase 2).
  std::unique_ptr<class NamePlate> nameplate_mod;     // The /rcpnameplate nameplate tinting (Phase N1).
  std::unique_ptr<class WindowWatch> window_watch_mod; // The /rcpwindow diagnostics + windowed-start self-heal.
  std::unique_ptr<class KeyBinds> keybinds_mod;       // The /rcpbinds extra key binds (Zeal binds port).
  std::unique_ptr<class RcpOptionsUI> options_ui;     // Options window (SIDL/EQUI port, in progress).
  std::unique_ptr<class CameraMods> camera_mods;  // Zeal chase camera (TAKP addresses; not yet ported).
  std::unique_ptr<class UIManager> ui;            // Options window + uifiles loading.

 private:
  static RcpService *ptr_service;
  std::vector<std::string> print_buffer;  // Queues prints until the UI is ready.
};
