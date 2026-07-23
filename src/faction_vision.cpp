// rof2ClientPlus - faction vision. See faction_vision.h.
#include "faction_vision.h"
#include "rebase.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"  // Rcp::Game::print_chat
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// CEverQuest::dsp_chat(const char* text, int color, bool bLogOk, bool bConvertPercent) - __thiscall.
// Same central chat-display choke ChatTimestamp detours; we install AFTER it so we run outermost and
// see the raw server text. The 4-stack-arg __fastcall(this, edx, ...) shape matches the rest of the
// codebase (chat_timestamp.cpp / chat_shortcuts.cpp).
const uintptr_t kDspChat = ::Rcp::eqva(0x51F1A0);
typedef void(__fastcall *DspChatFn)(void *game, int edx, char *text, int color, bool log_ok, bool convert_pct);
DspChatFn g_orig = nullptr;

bool g_enabled = true;  // Rewrite [RcpFac] lines into readable text (on by default).

constexpr char kIniSection[] = "Faction";
constexpr char kIniKeyEnabled[] = "Vision";
constexpr char kTag[] = "[RcpFac]";
constexpr size_t kTagLen = sizeof(kTag) - 1;  // 8

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKeyEnabled)) g_enabled = ini.getValue<bool>(kIniSection, kIniKeyEnabled);
}

// Parse "[RcpFac]t=kill|id=42|d=-15|cur=1234|min=-2000|max=2000|nm=Name" (or t=con|id|cur|eff|nm) into a
// readable line. Faction names never contain '|' or '=', so plain key=value splitting on '|' is safe;
// nm is taken verbatim. Returns false (line left untouched) if the tag is absent or malformed.
bool rewrite_faction_line(const char *text, std::string *out) {
  if (std::strncmp(text, kTag, kTagLen) != 0) return false;

  std::string t, nm = "Unknown";
  long id = 0, d = 0, cur = 0, mn = 0, mx = 0, eff = 0;
  bool have_d = false, have_cur = false, have_min = false, have_max = false, have_eff = false;

  const std::string body(text + kTagLen);
  size_t pos = 0;
  while (pos <= body.size()) {
    const size_t bar = body.find('|', pos);
    const std::string tok = body.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
    const size_t eq = tok.find('=');
    if (eq != std::string::npos) {
      const std::string k = tok.substr(0, eq);
      const std::string v = tok.substr(eq + 1);
      if (k == "t") t = v;
      else if (k == "nm") nm = v;
      else if (k == "id") id = std::strtol(v.c_str(), nullptr, 10);
      else if (k == "d") { d = std::strtol(v.c_str(), nullptr, 10); have_d = true; }
      else if (k == "cur") { cur = std::strtol(v.c_str(), nullptr, 10); have_cur = true; }
      else if (k == "min") { mn = std::strtol(v.c_str(), nullptr, 10); have_min = true; }
      else if (k == "max") { mx = std::strtol(v.c_str(), nullptr, 10); have_max = true; }
      else if (k == "eff") { eff = std::strtol(v.c_str(), nullptr, 10); have_eff = true; }
    }
    if (bar == std::string::npos) break;
    pos = bar + 1;
  }

  char buf[256];
  if (t == "con") {
    if (have_eff && eff != cur) {
      std::snprintf(buf, sizeof(buf), "Faction | %s (#%ld): standing %ld (effective %ld)", nm.c_str(), id, cur, eff);
    } else {
      std::snprintf(buf, sizeof(buf), "Faction | %s (#%ld): standing %ld", nm.c_str(), id, cur);
    }
  } else {  // kill / quest faction hit -> "Faction | <Name> (#id): [current] +/-delta"
    char delta[24];
    std::snprintf(delta, sizeof(delta), "%+ld", d);
    // "current" = the effective standing (base + personal + race/class/deity/item mods) that drives the
    // con word - NOT the raw personal points (which on an early hit ~= the delta). Fall back to cur/none.
    if (have_eff) {
      std::snprintf(buf, sizeof(buf), "Faction | %s (#%ld): [%ld] %s", nm.c_str(), id, eff, delta);
    } else if (have_cur) {
      std::snprintf(buf, sizeof(buf), "Faction | %s (#%ld): [%ld] %s", nm.c_str(), id, cur, delta);
    } else {
      std::snprintf(buf, sizeof(buf), "Faction | %s (#%ld): %s", nm.c_str(), id, delta);
    }
    // min/max are still parsed (the server sends them) but no longer shown in this format.
    (void)have_d;
    (void)have_min;
    (void)have_max;
    (void)mn;
    (void)mx;
  }
  *out = buf;
  return true;
}

// The detour. When on, rewrite a leading [RcpFac] tag into readable text and forward that; otherwise
// tail-call the original unchanged. g_orig is the trampoline (chains to ChatTimestamp, then real
// dsp_chat), so this never re-enters itself. The `text[0] == '['` pre-check keeps the common path cheap.
void __fastcall dsp_chat_hk(void *game, int edx, char *text, int color, bool log_ok, bool convert_pct) {
  if (g_enabled && text && text[0] == '[' && !crash_handler::shutting_down()) {
    std::string out;
    if (rewrite_faction_line(text, &out)) {
      g_orig(game, edx, out.data(), color, log_ok, convert_pct);
      return;
    }
  }
  g_orig(game, edx, text, color, log_ok, convert_pct);
}

void print_status() {
  Rcp::Game::print_chat("rof2ClientPlus faction vision: %s. '/rcpfaction on|off'.", g_enabled ? "ON" : "off");
}

}  // namespace

namespace faction_vision_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  IO_ini(IO_ini::kRcpIniFilename).setValue<bool>(kIniSection, kIniKeyEnabled, g_enabled);
}
}  // namespace faction_vision_settings

FactionVision::FactionVision(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  // Install AFTER ChatTimestamp (constructed earlier in RcpService) so we are the outermost hook on
  // dsp_chat and see the raw server text; our rewritten line then flows through the timestamp hook.
  rcp->hooks->Add("faction_vision", static_cast<int>(kDspChat), dsp_chat_hk, hook_type_detour);
  g_orig = rcp->hooks->hook_map["faction_vision"]->original(dsp_chat_hk);
  logger::logf("[faction] dsp_chat faction-vision detour installed at 0x%x; enabled=%d", (unsigned)kDspChat,
               (int)g_enabled);

  rcp->commands_hook->Add(
      "/rcpfaction", {},
      "Faction vision: rewrites the server's [RcpFac] faction lines (faction id + value on kills and "
      "consider) into readable text. '/rcpfaction on|off' toggles the client-side rewrite.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1")
            faction_vision_settings::set_enabled(true);
          else if (a == "off" || a == "0")
            faction_vision_settings::set_enabled(false);
          else
            faction_vision_settings::set_enabled(!faction_vision_settings::get_enabled());
        } else {
          faction_vision_settings::set_enabled(!faction_vision_settings::get_enabled());
        }
        print_status();
        return true;
      });
  logger::log("[faction] /rcpfaction registered");
}
