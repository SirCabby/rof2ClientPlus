// rof2ClientPlus - Copy Layout also copies chat filters/windows. See copy_layout_chat.h.
#include "copy_layout_chat.h"
#include "rebase.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"  // Rcp::Game::print_chat
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"
#include "rcp_profiles.h"

namespace {

// ---- eqgame "May 10 2013" addresses (eqlib-verified; call shapes re-verified in disasm) ----

// bool __cdecl CopyLayout(CXStr* srcIni, CXStr* resolution, bool bHotbuttons, bool bLoadouts,
//                         bool bSocials, CXStr* errorOut, bool bForceReload)
// srcIni = the picked layout's filename ("UI_<char>_<server>.ini"), resolution = "Windowed" or
// "2560x1440"-style (chat keys are resolution-independent, so we ignore it). Two client call
// sites: the NORMAL Copy Layout dialog @0x5FD71C passes bForceReload=0 (positions applied via
// SaveGameUI + 0x48E790, no UI reload); the resolution-mismatch confirm @0x55C9DF passes 1
// (synchronous ReloadUI).
const uintptr_t kCopyLayout = ::Rcp::eqva(0x55BE50);
typedef unsigned char(__cdecl *CopyLayoutFn)(void *src, void *res, int hot, int load, int soc,
                                             void *err, int force);
CopyLayoutFn g_orig_copy = nullptr;

// void __thiscall CChatWindowManager::SaveChatSettings() @0x64FE40 (zero stack args; sole caller
// is SaveGameUI @0x48920E with this = *(void**)0xF71070). Writes [ChatManager] from live state -
// exactly the step that would overwrite our pre-merge during the reload, so we skip it once.
const uintptr_t kChatSettingsSave = ::Rcp::eqva(0x64FE40);
typedef void(__fastcall *ChatSaveFn)(void *mgr, int edx);
ChatSaveFn g_orig_save = nullptr;

// The client's cached ini object for the CURRENT character's UI_<char>_<server>.ini, and its
// native writers (both __thiscall; arg order verified from the saver's own call sites):
//   WriteInt    @0x543540 (int value, const char* section, const char* key)
//   WriteString @0x543670 (const char* value, const char* section, const char* key)
// Writing through this object (not WritePrivateProfileString on the file) keeps us coherent with
// whatever the client has cached/flushes later.
void **const kUiIniObj = reinterpret_cast<void **>(::Rcp::eqva(0xE67CCC));
typedef void(__fastcall *IniWriteIntFn)(void *ini, int edx, int value, const char *section,
                                        const char *key);
typedef void(__fastcall *IniWriteStrFn)(void *ini, int edx, const char *value, const char *section,
                                        const char *key);
const IniWriteIntFn kIniWriteInt = reinterpret_cast<IniWriteIntFn>(::Rcp::eqva(0x543540));
const IniWriteStrFn kIniWriteStr = reinterpret_cast<IniWriteStrFn>(::Rcp::eqva(0x543670));

void **const kChatMgrPtr = reinterpret_cast<void **>(::Rcp::eqva(0xF71070));      // pinstCChatWindowManager
void **const kLocalPlayerPtr = reinterpret_cast<void **>(::Rcp::eqva(0xDD2630));  // pinstLocalPlayer
const char *const kServerName = reinterpret_cast<const char *>(::Rcp::eqva(0xE15E10));  // __ServerName

// void __thiscall CDisplay::ReloadUiWrapper(bool bUseIni) @0x49D990 - the exact call
// CopyLayout's own bForceReload path makes (mov ecx,[0xDD2660]; push 1; call 0x49D990 at
// 0x55C713): a synchronous full UI reload whose rebuild re-runs the chat loader @0x654C80.
// The NORMAL options-dialog copy (call site 0x5FD71C) passes bForceReload=0 and only applies
// window positions (SaveGameUI + 0x48E790) - no chat rebuild ever happens on that path, so
// after a merge we trigger this reload ourselves. Stock calls it from inside a window
// callback too (0x55C9DF path), so the in-callback teardown is a proven-safe pattern
// (window deletion is deferred).
void **const kDisplayPtr = reinterpret_cast<void **>(::Rcp::eqva(0xDD2660));  // pinstCDisplay
typedef void(__fastcall *ReloadUiFn)(void *display, int edx, int bUseIni);
const ReloadUiFn kReloadUi = reinterpret_cast<ReloadUiFn>(::Rcp::eqva(0x49D990));

constexpr int kChannelMapCount = 57;  // ChannelMap0..56 (CChatWindowManager +0x8C, count 0x39 in the saver loop)
constexpr int kHitModeCount = 8;      // HitMode0..7 (+0x224)
constexpr int kMaxChatWindows = 32;   // client MAX_CHAT_WINDOWS

bool g_enabled = true;  // On by default; this is the whole point of the module.
bool g_suppress_save = false;  // One-shot: true only while the original CopyLayout runs.

constexpr char kIniSection[] = "UI";
constexpr char kIniKeyEnabled[] = "CopyLayoutChat";
constexpr char kChatSection[] = "ChatManager";

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKeyEnabled)) g_enabled = ini.getValue<bool>(kIniSection, kIniKeyEnabled);
}

// Reads a CXStr (a single pointer to CStrRep: refCount@0, alloc@4, length@8, encoding@C,
// freeList@10, data@14) into utf8. Filenames are ASCII; utf16 is converted just in case.
std::string cxstr_utf8(void *cxstr) {
  if (!cxstr) return {};
  const char *rep = *reinterpret_cast<const char *const *>(cxstr);
  if (!rep) return {};
  const unsigned len = *reinterpret_cast<const unsigned *>(rep + 0x08);
  const unsigned enc = *reinterpret_cast<const unsigned *>(rep + 0x0C);
  if (!len || len > 0x10000) return {};
  if (enc == 0) return std::string(rep + 0x14, len);
  if (enc == 1) {
    std::string out(len * 3 + 1, '\0');
    int n = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t *>(rep + 0x14),
                                static_cast<int>(len), &out[0], static_cast<int>(out.size()), nullptr, nullptr);
    out.resize(n > 0 ? n : 0);
    return out;
  }
  return {};
}

// "UI_<self>_<server>.ini" for the logged-in character, or "" if unavailable. Used to skip
// self-copies: with the saver suppressed, merging our own last-camped file would revert any
// filter changes made this session.
std::string self_ini_name() {
  const char *self = *reinterpret_cast<const char *const *>(kLocalPlayerPtr);
  if (!self || !kServerName[0]) return {};
  const char *name = self + 0xA4;  // PlayerBase::Name (eqlib RoF2)
  if (!name[0] || static_cast<unsigned char>(name[0]) < 'A') return {};
  char buf[0x120];
  std::snprintf(buf, sizeof(buf), "UI_%.63s_%.127s.ini", name, kServerName);
  return buf;
}

// True if the key exists in [ChatManager] of the source file (two-default probe).
bool src_int(const char *path, const char *key, int *out) {
  int a = static_cast<int>(GetPrivateProfileIntA(kChatSection, key, -1234567, path));
  if (a != -1234567) { *out = a; return true; }
  int b = static_cast<int>(GetPrivateProfileIntA(kChatSection, key, -7654321, path));
  if (b != -7654321) { *out = b; return true; }
  return false;
}

bool src_str(const char *path, const char *key, char *buf, DWORD cap) {
  DWORD n = GetPrivateProfileStringA(kChatSection, key, "\x01", buf, cap, path);
  if (n == 1 && buf[0] == '\x01') return false;
  return true;
}

// Merges [ChatManager] from the source UI ini into the current character's UI ini via the
// client's own ini object. Returns true if a merge happened (=> suppress the reload's save).
bool merge_chat_section(const std::string &src_file) {
  void *ini = *kUiIniObj;
  void *mgr = *kChatMgrPtr;
  if (!ini || !mgr) return false;

  const std::string path = ".\\" + src_file;  // cwd is the game dir (same convention as IO_ini)
  if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    logger::logf("[copylayout] source ini not found: %s", path.c_str());
    return false;
  }

  int num = 0;
  if (!src_int(path.c_str(), "NumWindows", &num) || num < 1 || num > kMaxChatWindows) {
    logger::logf("[copylayout] %s has no usable [ChatManager] (NumWindows=%d) - chat not copied",
                 src_file.c_str(), num);
    return false;
  }

  kIniWriteInt(ini, 0, num, kChatSection, "NumWindows");

  char key[64], base[32], val[0x100];
  int v = 0, wrote = 1;

  for (int i = 0; i < kChannelMapCount; ++i) {
    std::snprintf(key, sizeof(key), "ChannelMap%d", i);
    if (src_int(path.c_str(), key, &v) && v >= 0 && v < num) { kIniWriteInt(ini, 0, v, kChatSection, key); ++wrote; }
  }
  for (int i = 0; i < kHitModeCount; ++i) {
    std::snprintf(key, sizeof(key), "HitMode%d", i);
    if (src_int(path.c_str(), key, &v)) { kIniWriteInt(ini, 0, v, kChatSection, key); ++wrote; }
  }

  static const char *const kIntSuffixes[] = {"LanguageId", "DefaultChannel", "ChatChannel",
                                             "FontStyle", "Scrollbar"};
  for (int w = 0; w < num; ++w) {
    std::snprintf(base, sizeof(base), "ChatWindow%d", w);
    for (const char *sfx : kIntSuffixes) {
      std::snprintf(key, sizeof(key), "%s_%s", base, sfx);
      if (src_int(path.c_str(), key, &v)) { kIniWriteInt(ini, 0, v, kChatSection, key); ++wrote; }
    }
    std::snprintf(key, sizeof(key), "%s_Name", base);
    if (src_str(path.c_str(), key, val, sizeof(val)) && val[0]) { kIniWriteStr(ini, 0, val, kChatSection, key); ++wrote; }
    std::snprintf(key, sizeof(key), "%s_TellTarget", base);
    if (src_str(path.c_str(), key, val, sizeof(val))) { kIniWriteStr(ini, 0, val, kChatSection, key); ++wrote; }
  }

  logger::logf("[copylayout] merged [ChatManager] from %s (%d windows, %d keys)", src_file.c_str(),
               num, wrote);
  return true;
}

// The CopyLayout detour. Merge first, suppress the chat-settings save while the original (and,
// if needed, our own reload) runs, then make sure a full UI reload happens so the chat loader
// rebuilds every window from the merged section:
//   - bForceReload=1 (resolution-mismatch confirm path, 0x55C9DF): the original reloads
//     synchronously itself - nothing extra to do.
//   - bForceReload=0 (the NORMAL Copy Layout dialog, 0x5FD71C): the original only applies
//     window positions, so after it succeeds we invoke the client's own reload wrapper.
unsigned char __cdecl copy_layout_hk(void *src, void *res, int hot, int load, int soc, void *err,
                                     int force) {
  bool merged = false;
  const std::string src_file = cxstr_utf8(src);
  logger::logf("[copylayout] CopyLayout(src=\"%s\", hot=%d load=%d soc=%d force=%d) enabled=%d",
               src_file.c_str(), hot, load, soc, force, (int)g_enabled);

  if (g_enabled && !crash_handler::shutting_down()) {
    if (src_file.size() > 7 && _strnicmp(src_file.c_str(), "UI_", 3) == 0) {
      const std::string self = self_ini_name();
      if (!self.empty() && _stricmp(self.c_str(), src_file.c_str()) == 0) {
        logger::logf("[copylayout] self-copy (%s) - chat merge skipped", src_file.c_str());
      } else {
        merged = merge_chat_section(src_file);
      }
    }
  }

  g_suppress_save = merged;
  const unsigned char ok = g_orig_copy(src, res, hot, load, soc, err, force);

  if (merged && ok && !force) {
    void *display = *kDisplayPtr;
    if (display) {
      logger::log("[copylayout] non-reload copy path - forcing UI reload so merged chat applies");
      kReloadUi(display, 0, 1);  // chat-save still suppressed: its save-first step must not clobber the merge
    }
  }
  g_suppress_save = false;

  logger::logf("[copylayout] CopyLayout done: ok=%d merged=%d", (int)ok, (int)merged);
  if (merged && ok) {
    Rcp::Game::print_chat("rof2ClientPlus: chat windows, filters and names copied too. "
                          "('/rcpcopylayout off' to disable)");
  }
  return ok;
}

// The chat-settings-save detour: skip exactly the save that runs inside our copy (the reload's
// save-first step); every other save (camp, zoning, /loadskin, non-copy reloads) passes through.
void __fastcall chat_save_hk(void *mgr, int edx) {
  if (g_suppress_save) {
    logger::log("[copylayout] chat-settings save suppressed (merge in flight)");
    return;
  }
  g_orig_save(mgr, edx);
}

void print_status() {
  Rcp::Game::print_chat("rof2ClientPlus Copy Layout chat copy: %s. Options -> General -> Copy "
                        "Layout will %scarry chat windows/filters/names. '/rcpcopylayout on|off'.",
                        g_enabled ? "ON" : "off", g_enabled ? "" : "NOT ");
}

}  // namespace

namespace copy_layout_chat_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  IO_ini(IO_ini::kRcpIniFilename).setValue<bool>(kIniSection, kIniKeyEnabled, g_enabled);
}
}  // namespace copy_layout_chat_settings

CopyLayoutChat::CopyLayoutChat(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  // Settings profiles: re-read + re-apply this module's settings when the active
  // profile changes (rcp_profiles.h).
  rcp_profiles::add_reload_handler([] {
    load_settings();
  });

  rcp->hooks->Add("copy_layout", static_cast<int>(kCopyLayout), copy_layout_hk, hook_type_detour);
  g_orig_copy = rcp->hooks->hook_map["copy_layout"]->original(copy_layout_hk);

  rcp->hooks->Add("copy_layout_chat_save", static_cast<int>(kChatSettingsSave), chat_save_hk,
                  hook_type_detour);
  g_orig_save = rcp->hooks->hook_map["copy_layout_chat_save"]->original(chat_save_hk);

  logger::logf("[copylayout] CopyLayout detour @0x%x + chat-save detour @0x%x installed; enabled=%d",
               (unsigned)kCopyLayout, (unsigned)kChatSettingsSave, (int)g_enabled);

  rcp->commands_hook->Add(
      "/rcpcopylayout", {},
      "Copy Layout chat copy: '/rcpcopylayout on|off' - when on, Options -> Copy Layout also "
      "copies chat windows, filters (ChannelMaps), hit modes, names and fonts from the picked "
      "character's UI ini.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1")
            copy_layout_chat_settings::set_enabled(true);
          else if (a == "off" || a == "0")
            copy_layout_chat_settings::set_enabled(false);
          else
            copy_layout_chat_settings::set_enabled(!copy_layout_chat_settings::get_enabled());
        }
        print_status();
        return true;
      });
}
