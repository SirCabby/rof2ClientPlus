// rof2ClientPlus - Zeal-style chat timestamps. See chat_timestamp.h.
#include "chat_timestamp.h"
#include "rebase.h"

#include <windows.h>

#include <ctime>
#include <cstdio>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"  // Rcp::Game::print_chat / is_in_game
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// CEverQuest::dsp_chat(const char* text, int color, bool bLogOk, bool bConvertPercent) - __thiscall,
// this = the game object. The central chat-display choke: every line the client shows (incoming say/
// tell/group/guild/system, and the mod's own print_chat) passes through here. We detour it and, when
// enabled, prepend a formatted local time to the text before the client renders it. The 4-stack-arg
// signature mirrors the proven wrapper in game_structures.h (GameClass::dsp_chat), so the ABI matches
// exactly; the __fastcall(this, edx, ...) shape is the standard __thiscall-detour idiom used across
// this codebase (chat_shortcuts.cpp / sound_mods.cpp).
const uintptr_t kDspChat = ::Rcp::eqva(0x51F1A0);
typedef void(__fastcall *DspChatFn)(void *game, int edx, char *text, int color, bool log_ok, bool convert_pct);
DspChatFn g_orig = nullptr;

bool g_enabled = false;                      // Off by default (Zeal default); opt-in via /timestamp on.
std::string g_format = "[%H:%M:%S] ";        // strftime format; Zeal's default is 24-hour [HH:MM:SS].

constexpr char kIniSection[] = "Chat";
constexpr char kIniKeyEnabled[] = "Timestamp";
constexpr char kIniKeyFormat[] = "TimestampFormat";

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKeyEnabled)) g_enabled = ini.getValue<bool>(kIniSection, kIniKeyEnabled);
  if (ini.exists(kIniSection, kIniKeyFormat)) {
    std::string f = ini.getValue<std::string>(kIniSection, kIniKeyFormat);
    if (!f.empty()) g_format = f;
  }
}

// Build the timestamp prefix from the local clock via strftime, falling back to a fixed 24-hour
// [HH:MM:SS] if the configured format yields nothing (bad/oversized format). GetLocalTime avoids the
// CRT's time()/localtime() and gives the wall-clock fields we copy straight into a tm for strftime.
std::string make_prefix() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  struct tm t = {};
  t.tm_year = st.wYear - 1900;
  t.tm_mon = st.wMonth - 1;
  t.tm_mday = st.wDay;
  t.tm_hour = st.wHour;
  t.tm_min = st.wMinute;
  t.tm_sec = st.wSecond;
  t.tm_wday = st.wDayOfWeek;
  t.tm_isdst = -1;
  char buf[128];
  if (!g_format.empty() && std::strftime(buf, sizeof(buf), g_format.c_str(), &t) > 0) return buf;
  std::snprintf(buf, sizeof(buf), "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
  return buf;
}

// The detour. When on (and we have a real line, in game, not shutting down), prepend the timestamp and
// forward our own buffer; otherwise tail-call the original unchanged. g_orig is the trampoline to the
// original dsp_chat, so this never re-enters itself - no recursion. We build a heap string and pass its
// (writable, null-terminated) storage; we never touch the client's own text buffer, matching Zeal.
void __fastcall dsp_chat_hk(void *game, int edx, char *text, int color, bool log_ok, bool convert_pct) {
  if (g_enabled && text && text[0] && !crash_handler::shutting_down() && Rcp::Game::is_in_game()) {
    std::string out = make_prefix();
    out += text;
    g_orig(game, edx, out.data(), color, log_ok, convert_pct);
    return;
  }
  g_orig(game, edx, text, color, log_ok, convert_pct);
}

void print_status() {
  Rcp::Game::print_chat("rof2ClientPlus chat timestamps: %s (format \"%s\"). '/timestamp on|off', "
                        "'/timestamp format <strftime>'.",
                        g_enabled ? "ON" : "off", g_format.c_str());
}

}  // namespace

namespace chat_timestamp_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  IO_ini(IO_ini::kRcpIniFilename).setValue<bool>(kIniSection, kIniKeyEnabled, g_enabled);
}
std::string get_format() { return g_format; }
void set_format(const std::string &fmt) {
  if (fmt.empty()) return;
  g_format = fmt;
  IO_ini(IO_ini::kRcpIniFilename).setValue<std::string>(kIniSection, kIniKeyFormat, g_format);
}
}  // namespace chat_timestamp_settings

ChatTimestamp::ChatTimestamp(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  rcp->hooks->Add("chat_timestamp", static_cast<int>(kDspChat), dsp_chat_hk, hook_type_detour);
  g_orig = rcp->hooks->hook_map["chat_timestamp"]->original(dsp_chat_hk);
  logger::logf("[chat] dsp_chat timestamp detour installed at 0x%x; enabled=%d format=\"%s\"",
               (unsigned)kDspChat, (int)g_enabled, g_format.c_str());

  rcp->commands_hook->Add(
      "/timestamp", {},
      "Chat timestamps (Zeal-style): '/timestamp on|off' toggles a local-time prefix on every chat "
      "line; '/timestamp format <strftime>' sets the format (default \"[%H:%M:%S] \").",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "format") {
            if (args.size() < 3) {
              Rcp::Game::print_chat("rof2ClientPlus timestamp format: \"%s\". Usage: '/timestamp format "
                                    "<strftime>' (e.g. [%%H:%%M:%%S] , [%%I:%%M:%%S %%p] for 12-hour).",
                                    chat_timestamp_settings::get_format().c_str());
              return true;
            }
            std::string fmt = args[2];
            for (size_t i = 3; i < args.size(); ++i) fmt += " " + args[i];
            chat_timestamp_settings::set_format(fmt);
            Rcp::Game::print_chat("rof2ClientPlus timestamp format -> \"%s\"",
                                  chat_timestamp_settings::get_format().c_str());
            return true;
          }
          if (a == "on" || a == "1")
            chat_timestamp_settings::set_enabled(true);
          else if (a == "off" || a == "0")
            chat_timestamp_settings::set_enabled(false);
          else
            chat_timestamp_settings::set_enabled(!chat_timestamp_settings::get_enabled());
        } else {
          chat_timestamp_settings::set_enabled(!chat_timestamp_settings::get_enabled());
        }
        print_status();
        return true;
      });
  logger::log("[chat] /timestamp registered");
}
