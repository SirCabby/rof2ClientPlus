#include <algorithm>
#include "rebase.h"
#include <format>
#include "callbacks.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "rcp.h"

namespace {

// Simple tracing class to log the last major callback activity.
class CallbackTrace {
 public:
  CallbackTrace(const char *_trace) {
    trace = _trace;
    status = "Enter";
    addr = 0;
  }

  static void SetAddress(int address) { addr = address; }

  ~CallbackTrace() {
    status = "Exit";
    addr = 0;
  }

  static std::string get_trace() { return std::format("{} : {} (0x{:x})", trace, status, addr); }

 private:
  static const char *trace;
  static const char *status;
  static int addr;
};

const char *CallbackTrace::trace = "Startup";
const char *CallbackTrace::status = "Unknown";
int CallbackTrace::addr = 0;
}  // namespace

std::string CallbackManager::get_trace() const { return CallbackTrace::get_trace(); }

CallbackManager::~CallbackManager() {}

static void __fastcall CDisplayRender_MinWorld_hk(int t, int unused) {
  // The CDisplay::Render_MinWorld() method is called within the character select processing loop.
  CallbackTrace trace("CharSelectLoop");
  RcpService *rcp = RcpService::get_instance();
  rcp->callbacks->invoke_generic(callback_type::CharacterSelectLoop);
  rcp->callbacks->invoke_delayed();
  rcp->hooks->hook_map["CDisplayRender_MinWorld"]->original(CDisplayRender_MinWorld_hk)(t, unused);
}

void __fastcall main_loop_hk(int t, int unused) {
  CallbackTrace trace("MainLoopHook");
  RcpService *rcp = RcpService::get_instance();
  rcp->callbacks->invoke_generic(callback_type::MainLoop);
  rcp->callbacks->invoke_delayed();
  rcp->hooks->hook_map["MainLoop"]->original(main_loop_hk)(t, unused);
}

void __fastcall render_hk(int t, int unused) {
  CallbackTrace trace("Render");
  RcpService *rcp = RcpService::get_instance();
  rcp->hooks->hook_map["Render"]->original(render_hk)(t, unused);
  rcp->callbacks->invoke_generic(callback_type::Render);
}

void render_ui(int x) {
  CallbackTrace trace("RenderUI");
  RcpService *rcp = RcpService::get_instance();
  rcp->callbacks->invoke_generic(callback_type::RenderUI);
  rcp->hooks->hook_map["RenderUI"]->original(render_ui)(x);
}

void __fastcall charselect_hk(int t, int u) {
  CallbackTrace trace("DoCharacterSelection");
  RcpService *rcp = RcpService::get_instance();
  rcp->callbacks->invoke_generic(callback_type::CharacterSelect);
  rcp->hooks->hook_map["DoCharacterSelection"]->original(charselect_hk)(t, u);
  rcp->callbacks->invoke_generic(callback_type::CleanCharSelectUI);
}

void CallbackManager::invoke_generic(callback_type fn) {
  for (auto &f : generic_functions[fn]) {
    CallbackTrace::SetAddress(reinterpret_cast<int>(&f));  // Pointer to std::function<> state, not the function.
    f();
  }
}

void CallbackManager::AddDelayed(std::function<void()> callback_function, int ms) {
  delayed_functions.push_back({GetTickCount64() + ms, callback_function});
}

void CallbackManager::AddGeneric(std::function<void()> callback_function, callback_type fn) {
  generic_functions[fn].push_back(callback_function);
}

void CallbackManager::AddPacket(std::function<bool(UINT, char *, UINT)> callback_function, callback_type type) {
  packet_functions[type].push_back(callback_function);
}

void CallbackManager::AddCommand(std::function<bool(UINT, int)> callback_function, callback_type type) {
  cmd_functions[type].push_back(callback_function);
}

void CallbackManager::AddOutputText(
    std::function<void(Rcp::GameUI::ChatWnd *&wnd, std::string &msg, short &channel)> callback_function) {
  output_text_functions.push_back(callback_function);
}

void __fastcall enterzone_hk(int t, int unused, int hwnd) {
  CallbackTrace trace("EnterZone");
  RcpService *rcp = RcpService::get_instance();
  rcp->hooks->hook_map["EnterZone"]->original(enterzone_hk)(t, unused, hwnd);
  rcp->callbacks->invoke_generic(callback_type::EnterZone);
}

void __fastcall initgameui_hk(int t, int u) {
  CallbackTrace trace("InitGameUI");
  RcpService *rcp = RcpService::get_instance();
  rcp->hooks->hook_map["InitGameUI"]->original(initgameui_hk)(t, u);
  rcp->callbacks->invoke_generic(callback_type::InitUI);
}

void __fastcall initcharselectui_hk(int t, int u) {
  CallbackTrace trace("InitCharSelectUI");
  RcpService *rcp = RcpService::get_instance();
  rcp->hooks->hook_map["InitCharSelectUI"]->original(initcharselectui_hk)(t, u);
  rcp->callbacks->invoke_generic(callback_type::InitCharSelectUI);
}

static void __fastcall CDisplayCleanGameUI(void *cdisplay_this, int unused_edx) {
  CallbackTrace trace("CleanUpUI");
  RcpService *rcp = RcpService::get_instance();
  rcp->callbacks->invoke_generic(callback_type::CleanUI);
  rcp->hooks->hook_map["CDisplayCleanGameUI"]->original(CDisplayCleanGameUI)(cdisplay_this, unused_edx);
}

void CallbackManager::invoke_delayed() {
  ULONGLONG current_time = GetTickCount64();
  for (auto &[end_time, fn] : delayed_functions) {
    if (current_time >= end_time) fn();
  }
  delayed_functions.erase(std::remove_if(delayed_functions.begin(), delayed_functions.end(),
                                         [current_time](const std::pair<ULONGLONG, std::function<void()>> &item) {
                                           return current_time > item.first;
                                         }),
                          delayed_functions.end());
}

bool CallbackManager::invoke_packet(callback_type cb_type, UINT opcode, char *buffer, UINT len) {
  for (auto &fn : packet_functions[cb_type]) {
    if (fn(opcode, buffer, len)) return true;
  }
  return false;
}

bool CallbackManager::invoke_command(callback_type cb_type, UINT opcode, bool state) {
  for (auto &fn : cmd_functions[cb_type]) {
    if (fn(opcode, state)) return true;
  }
  return false;
}

void CallbackManager::AddEntity(std::function<void(Rcp::GameStructures::Entity *)> callback_function,
                                callback_type type) {
  player_spawn_functions[type].push_back(callback_function);
}

void CallbackManager::invoke_player(Rcp::GameStructures::Entity *ent, callback_type cb) {
  for (auto &fn : player_spawn_functions[cb]) fn(ent);
}

void CallbackManager::invoke_outputtext(Rcp::GameUI::ChatWnd *&wnd, std::string &msg, short &channel) {
  for (auto &fn : output_text_functions) fn(wnd, msg, channel);
}

char __fastcall handleworldmessage_hk(int *connection, int unused, UINT unk, UINT opcode, char *buffer, UINT len) {
  RcpService *rcp = RcpService::get_instance();

  // static std::vector<UINT> ignored_opcodes = { 16543, 16629, 16530, 16425, 16562, 16526, 16694, 8253, 16735, 16727,
  // 16735, 16458 }; if (!std::count(ignored_opcodes.begin(), ignored_opcodes.end(), opcode))
  //	Rcp::Game::print_chat("opcode: 0x%x len: %i", opcode, len);

  // if (!Rcp::Game::get_self() && opcode == 0x4107) //a fix for a crash reported by Ecliptor at 0x004E2803
  //	return 1;

  if (rcp->callbacks->invoke_packet(callback_type::WorldMessage, opcode, buffer, len)) return 1;

  char result = rcp->hooks->hook_map["HandleWorldMessage"]->original(handleworldmessage_hk)(connection, unused, unk,
                                                                                             opcode, buffer, len);

  rcp->callbacks->invoke_packet(callback_type::WorldMessagePost, opcode, buffer, len);
  return result;
}

void send_message_hk(int *connection, UINT opcode, char *buffer, UINT len, int unknown) {
  RcpService *rcp = RcpService::get_instance();
  // Rcp::Game::print_chat("Opcode %i   len: %i", opcode, len);
  if (rcp->callbacks->invoke_packet(callback_type::SendMessage_, opcode, buffer, len)) return;

  rcp->hooks->hook_map["SendMessage"]->original(send_message_hk)(connection, opcode, buffer, len, unknown);
}

void executecmd_hk(UINT cmd, int isdown, int unk2) {
  CallbackTrace trace("ExecuteCmd");
  RcpService *rcp = RcpService::get_instance();
  // Rcp::Game::print_chat(USERCOLOR_SHOUT, "Cmd: %i", cmd);

  if (cmd == 0x8 && Rcp::Game::is_new_ui() && Rcp::Game::Windows && !Rcp::Game::Windows->SpellBook)
    return;  // don't allow auto attack to happen when spellbook is nullptr (happens in explore mode).. will cause a
             // crash
  if (cmd == 0xd2 && isdown == 1 && unk2 == 1) rcp->callbacks->invoke_generic(callback_type::EndMainLoop);
  if (rcp->callbacks->invoke_command(callback_type::ExecuteCmd, cmd, isdown)) return;

  rcp->hooks->hook_map["ExecuteCmd"]->original(executecmd_hk)(cmd, isdown, unk2);
}

int __fastcall DrawWindows(int t, int u) {
  CallbackTrace trace("DrawWindows");
  RcpService *rcp = RcpService::get_instance();
  rcp->callbacks->invoke_generic(callback_type::DrawWindows);
  return RcpService::get_instance()->hooks->hook_map["DrawWindows"]->original(DrawWindows)(t, u);
}

Rcp::GameStructures::Entity *__fastcall GamePlayer(Rcp::GameStructures::Entity *ent_buffer, int unused,
                                                    Rcp::GameStructures::Entity *ent_cpy, BYTE Gender, WORD Race,
                                                    BYTE Class, const char *Name) {
  RcpService *rcp = RcpService::get_instance();
  if (ent_cpy) rcp->callbacks->invoke_player(ent_buffer, callback_type::EntityDespawn);
  Rcp::GameStructures::Entity *ret_ent =
      rcp->hooks->hook_map["GamePlayer"]->original(GamePlayer)(ent_buffer, unused, ent_cpy, Gender, Race, Class, Name);
  if (ret_ent) rcp->callbacks->invoke_player(ret_ent, callback_type::EntitySpawn);
  return ret_ent;
}

void __fastcall GamePlayerDeconstruct(Rcp::GameStructures::Entity *ent, int unused) {
  RcpService *rcp = RcpService::get_instance();
  if (ent) rcp->callbacks->invoke_player(ent, callback_type::EntityDespawn);
  rcp->hooks->hook_map["GamePlayerDeconstruct"]->original(GamePlayerDeconstruct)(ent, unused);
}

void __fastcall OutputText(Rcp::GameUI::ChatWnd *wnd, int u, Rcp::GameUI::CXSTR msg, short channel) {
  RcpService *rcp = RcpService::get_instance();
  short new_channel = channel;
  if (msg.Data) {
    // msg_data is passed by reference so that it can be modified in the callbacks.
    std::string msg_data = msg.CastToCharPtr();
    rcp->callbacks->invoke_outputtext(wnd, msg_data, new_channel);
    if (msg_data.empty()) return;  // Bail out if the msg was suppressed in a callback.

    if (!wnd) wnd = Rcp::Game::Windows->ChatManager->ChatWindows[0];

    msg.Set(msg_data);
  }
  // Note: The top-level caller of OutputText will handle FreeRep() of msg (unlike in print_chat_wnd).
  rcp->hooks->hook_map["AddOutputText"]->original(OutputText)(wnd, u, msg, new_channel);
}

///*000*/	UINT16	target;
///*002*/	UINT16	source;
///*004*/	UINT16	type;
///*006*/	INT16	spellid;
///*008*/	INT16	damage;
///*00A*/	float	force;
///*00E*/	float	sequence; // see above notes in Action_Struct
///*012*/	float	pushup_angle; // associated with force.  Sine of this angle, multiplied by force, will be z
/// push.
///
void CallbackManager::AddReportSuccessfulHit(
    std::function<void(struct Rcp::GameStructures::Entity *source, struct Rcp::GameStructures::Entity *target,
                       WORD type, short spell_id, short damage, char output_text)>
        callback_function) {
  ReportSuccessfulHit_functions.push_back(callback_function);
}

void CallbackManager::invoke_ReportSuccessfulHit(Rcp::Packets::Damage_Struct *dmg, char output_text) {
  auto em = RcpService::get_instance()->entity_manager.get();
  Rcp::GameStructures::Entity *target = Rcp::Game::get_entity_by_id(dmg->target);
  Rcp::GameStructures::Entity *source = Rcp::Game::get_entity_by_id(dmg->source);
  if (target && source) {
    for (const auto &fn : ReportSuccessfulHit_functions)
      fn(source, target, dmg->type, dmg->spellid, dmg->damage, output_text);
  }
}

static void __fastcall ReportSuccessfulHit(int t, int u, Rcp::Packets::Damage_Struct *dmg, char output_text,
                                           int always_zero) {
  RcpService::get_instance()->callbacks->invoke_ReportSuccessfulHit(dmg, output_text);
  RcpService::get_instance()->hooks->hook_map["ReportSuccessfulHit"]->original(ReportSuccessfulHit)(
      t, u, dmg, output_text, always_zero);
  RcpService::get_instance()->callbacks->invoke_generic(callback_type::ReportSuccessfulHitPost);
}

void DeactivateMainUI() {
  CallbackTrace trace("DeactivateMainUI");
  RcpService::get_instance()->callbacks->invoke_generic(callback_type::DeactivateUI);
  RcpService::get_instance()->hooks->hook_map["DeactivateMainUI"]->original(DeactivateMainUI)();
}

CallbackManager::CallbackManager(RcpService *rcp) {
  rcp->hooks->Add("DrawWindows", ::Rcp::eqva(0x59E000), DrawWindows,
                   hook_type_detour);  // render in this hook so damage is displayed behind ui
  rcp->hooks->Add("ExecuteCmd", ::Rcp::eqva(0x54050c), executecmd_hk, hook_type_detour);
  rcp->hooks->Add("MainLoop", ::Rcp::eqva(0x5473c3), main_loop_hk, hook_type_detour);
  rcp->hooks->Add("CDisplayRender_MinWorld", ::Rcp::eqva(0x004abe54), CDisplayRender_MinWorld_hk, hook_type_detour);
  rcp->hooks->Add("Render", ::Rcp::eqva(0x4AA8BC), render_hk, hook_type_detour);
  HMODULE gfx_dx8 = GetModuleHandleA("eqgfx_dx8.dll");
  if (gfx_dx8) rcp->hooks->Add("RenderUI", (DWORD)gfx_dx8 + 0x6b7f0, render_ui, hook_type_detour);

  rcp->hooks->Add("EnterZone", ::Rcp::eqva(0x53D2C4), enterzone_hk, hook_type_detour);
  rcp->hooks->Add("CDisplayCleanGameUI", ::Rcp::eqva(0x4A6EBC), CDisplayCleanGameUI, hook_type_detour);  // Also char select.
  rcp->hooks->Add("DoCharacterSelection", ::Rcp::eqva(0x53b9cf), charselect_hk, hook_type_detour);
  rcp->hooks->Add("InitGameUI", ::Rcp::eqva(0x4a60b5), initgameui_hk, hook_type_detour);
  rcp->hooks->Add("InitCharSelectUI", ::Rcp::eqva(0x4a5f85), initcharselectui_hk, hook_type_detour);
  rcp->hooks->Add("HandleWorldMessage", ::Rcp::eqva(0x4e829f), handleworldmessage_hk, hook_type_detour);
  rcp->hooks->Add("SendMessage", ::Rcp::eqva(0x54e51a), send_message_hk, hook_type_detour);
  rcp->hooks->Add("GamePlayer", ::Rcp::eqva(0x506802), GamePlayer, hook_type_detour);
  rcp->hooks->Add("GamePlayerDeconstruct", ::Rcp::eqva(0x50723D), GamePlayerDeconstruct, hook_type_detour);
  rcp->hooks->Add("AddOutputText", ::Rcp::eqva(0x4139A2), OutputText, hook_type_detour);
  rcp->hooks->Add("ReportSuccessfulHit", ::Rcp::eqva(0x5297D2), ReportSuccessfulHit, hook_type_detour);
  rcp->hooks->Add("DeactivateMainUI", ::Rcp::eqva(0x4A7705), DeactivateMainUI, hook_type_detour);
}
