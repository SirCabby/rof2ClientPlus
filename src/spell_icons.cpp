// rof2ClientPlus - live classic/revamped spell-icon swap. See spell_icons.h.
//
// Mechanism (eqlib 2013-05-10 layouts; game/UI.h + graphics/GraphicsResources.h):
// UI textures are cached once per file in the global CEQSuiteTextureLoader -
// a STATIC INSTANCE at 0xB64CEC (eqlib casts the address itself, not a slot):
//   loader+0x04  ArrayClass<_SuiteTexture>  { int len; _SuiteTexture* arr; }
//   _SuiteTexture (stride 0x10): bool bUsed@0, CXStr Name@4, enDir@8, BMI*@0xC
//   BMI: const char* Name@0, uint Flags@4, CEQGBitmap* pBmp@8
//   CEQGBitmap: const char* m_pszFileName@0x28, bool m_bHasTexture@0x44,
//               IDirect3DTexture9* m_pD3DTexture@0x48
// Every CTextureAnimation frame (and every widget's private copy of one) keeps
// only a CUITextureInfo naming the sheet, so all spell-icon draws funnel into
// the same per-sheet CEQGBitmap. Writing its m_pD3DTexture is the whole swap.
// The ten entries are found via the A_SpellIcons/A_SpellGems animations' frame
// TextureIds (in-game lesson: the cache's BMI/bitmap name fields are not
// reliably the sheet filename - only 1/10 matched by name).
//
// Refcount discipline (COM): we hold one base ref on each classic texture we
// load (released never - the module lives for the process). Installing into a
// bitmap takes one extra AddRef so the ENGINE owns a real reference; if it
// tears the bitmap down (/loadskin -> UnloadAllTextures, camp to char select),
// its Release consumes that install ref and our base ref keeps the texture
// alive. When we detect an install was consumed (record invalid or the pointer
// changed under us), the displaced original texture is orphaned - the bitmap
// held its only handle and now points elsewhere - so we Release the saved
// original to reap it, and re-resolve fresh. A normal user-toggle-off instead
// writes the original pointer back and releases our install ref.
//
// All D3D + loader-walking runs on the render thread inside the prerender
// (BeginScene) callback - before the frame's UI raster, so a toggle shows the
// same frame - and every callback is wrapped in directx's rcp_guard, so a
// mid-/loadskin race degrades to one skipped frame, re-validated the next.
#include "spell_icons.h"
#include "rebase.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>  // D3DXCreateTextureFromFileA (classic sheet load).

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "commands.h"
#include "directx.h"
#include "game_functions.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

constexpr char kIniSection[] = "SpellIcons";
constexpr char kIniKey[] = "Classic";
constexpr char kAssetSubDir[] = "spellicons";  // uifiles/rcp/spellicons/*.tga

// ---- CEQSuiteTextureLoader / bitmap layout (see header comment; eqlib-confirmed). ----
const uintptr_t kSuiteTextureLoader = ::Rcp::eqva(0xB64CEC);  // static instance (the object itself)
constexpr int kLoaderArrayLen = 0x04;                // ArrayClass: int m_length
constexpr int kLoaderArrayPtr = 0x08;                // ArrayClass: _SuiteTexture* m_array
constexpr int kSuiteEntrySize = 0x10;
constexpr int kSuiteEntryUsed = 0x00;  // bool bUsed
constexpr int kSuiteEntryBmi = 0x0C;   // BMI*
constexpr int kBmiName = 0x00;         // const char*
constexpr int kBmiBmp = 0x08;          // CEQGBitmap*
constexpr int kBmpFileName = 0x28;     // const char* m_pszFileName
constexpr int kBmpHasTexture = 0x44;   // bool m_bHasTexture
constexpr int kBmpD3DTexture = 0x48;   // IDirect3DTexture9* m_pD3DTexture
constexpr int kSuiteEntryName = 0x04;  // _SuiteTexture: CXStr Name (the loader's own cache key)

// SIDL-manager side (authoritative resolution): the A_SpellIcons / A_SpellGems
// animations' frames name the exact textures every spell-icon draw uses and
// carry the loader TextureId once the texture is registered. In-game proof that
// name-guessing the loader array is NOT enough: only gemicons02.tga's cache
// entry carried a matchable BMI/bitmap name (1/10 sheets resolved). Everything
// here is walked READ-ONLY on the render thread - deliberately NOT via
// FindAnimation1@0x86E010, whose CXStr temp would allocate on the game's string
// pool from the wrong thread; that function hashes over this same array
// (disasm: ArrayClass at mgr+0x94).
static void **const kSidlManager = reinterpret_cast<void **>(::Rcp::eqva(0x15D3D08));  // pinstCSidlManager
constexpr int kMgrAnimsLen = 0x94;    // CSidlManagerBase: ArrayClass<CTextureAnimation*> {len@+0x94, arr@+0x98}
constexpr int kMgrAnimsArr = 0x98;
constexpr int kAnimName = 0x04;       // CTextureAnimation: vtable@0, CXStr Name@4
constexpr int kAnimFramesLen = 0x08;  //   ArrayClass<STextureAnimationFrame> {len@+0x08, arr@+0x0C}
constexpr int kAnimFramesArr = 0x0C;
constexpr int kFrameSize = 0x34;      // STextureAnimationFrame: Piece@0, Ticks@0x28, Hotspot@0x2C
constexpr int kInfoName = 0x08;       // frame Piece.m_info: bValid@0, Directory@4, CXStr Name@8, ...
constexpr int kInfoTexId = 0x14;      //   ..., CXSize TextureSize@0xC, uint32 TextureId@0x14
// CXStr rep layout (same constants chat_stml_select.cpp uses).
constexpr int kRepLength = 0x08, kRepEncoding = 0x0c, kRepData = 0x14;

// The ten sheets the 2013 revamp replaced (grid layout unchanged since 2001, so
// the pre-2013 sheets are drop-in pixel replacements; see uifiles/rcp/spellicons/README.md).
struct Sheet {
  const char *file;  // sheet filename, also the classic .tga name under uifiles/rcp/spellicons
  // Classic texture (render-thread owned; one base ref held for the process lifetime).
  IDirect3DTexture9 *classic = nullptr;
  bool load_failed = false;  // don't retry a missing/broken file every frame
  // Live install record into the client's texture cache.
  int suite_idx = -1;                    // index into the loader's Textures array
  char *bmi = nullptr;                   // BMI* seen at that index
  char *bmp = nullptr;                   // CEQGBitmap* behind it
  IDirect3DTexture9 *orig = nullptr;     // engine texture our pointer displaced
  bool installed = false;
};
Sheet g_sheets[] = {
    {"spells01.tga"}, {"spells02.tga"}, {"spells03.tga"}, {"spells04.tga"}, {"spells05.tga"},
    {"spells06.tga"}, {"spells07.tga"}, {"gemicons01.tga"}, {"gemicons02.tga"}, {"gemicons03.tga"},
};
constexpr int kSheetCount = sizeof(g_sheets) / sizeof(g_sheets[0]);

bool g_classic = false;    // the persisted setting (main thread writes, render thread reads)
int g_debug_frames = 0;    // log per-sheet state for a few frames after /rcpspellicons debug
bool g_dump = false;       // one-shot full dump (loader array + animation frames) after /rcpspellicons dump
bool g_missing_warned = false;  // one-time chat warning when the classic .tga files are absent

// Read a CXStr field (a single CStrRep*) into a std::string; ASCII reps only.
std::string read_cxstr(const void *cxstr_field) {
  std::string s;
  char *rep = *reinterpret_cast<char *const *>(cxstr_field);
  if (!rep) return s;
  const uint32_t len = *reinterpret_cast<uint32_t *>(rep + kRepLength);
  const uint32_t enc = *reinterpret_cast<uint32_t *>(rep + kRepEncoding);
  if (enc != 0 || len > 4096) return s;
  s.assign(rep + kRepData, rep + kRepData + len);
  return s;
}

// uifiles/rcp/spellicons next to the game exe (same resolution as target-ring graphics).
std::filesystem::path get_assets_dir() {
  char module_path[MAX_PATH] = {};
  GetModuleFileNameA(NULL, module_path, MAX_PATH);
  return std::filesystem::path(module_path).parent_path() / "uifiles" / "rcp" / kAssetSubDir;
}

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKey)) g_classic = ini.getValue<bool>(kIniSection, kIniKey);
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, kIniKey, g_classic);
}

// Case-insensitive match of `name`'s basename (either path separator) against a sheet file.
bool name_matches(const char *name, const char *want) {
  if (!name) return false;
  const char *base = name;
  for (const char *p = name; *p; ++p)
    if (*p == '/' || *p == '\\') base = p + 1;
  return _stricmp(base, want) == 0;
}

// The engine consumed our install (bitmap destroyed or its texture pointer
// replaced): its Release took the install ref, and the displaced original -
// whose only handle the bitmap gave up when we swapped - is ours to reap.
void reap_consumed_install(Sheet &s) {
  if (s.orig) s.orig->Release();
  s.orig = nullptr;
  s.installed = false;
}

void drop_record(Sheet &s) {
  if (s.installed) reap_consumed_install(s);
  s.suite_idx = -1;
  s.bmi = nullptr;
  s.bmp = nullptr;
}

// The loader's texture array (bounds-sanitized; {nullptr,0} when not ready).
struct LoaderArray {
  char *arr;
  int len;
};
LoaderArray loader_array() {
  char *loader = reinterpret_cast<char *>(kSuiteTextureLoader);
  LoaderArray la{*reinterpret_cast<char **>(loader + kLoaderArrayPtr),
                 *reinterpret_cast<int *>(loader + kLoaderArrayLen)};
  if (!la.arr || la.len <= 0 || la.len > 100000) la = {nullptr, 0};
  return la;
}

// Bind a sheet to loader entry `id` if that entry is fully formed (used, BMI
// and bitmap present). False = not ready yet; retried on a later frame.
bool bind_by_id(Sheet &s, int id, const LoaderArray &la) {
  if (id < 0 || id >= la.len) return false;
  char *entry = la.arr + id * kSuiteEntrySize;
  if (!*reinterpret_cast<bool *>(entry + kSuiteEntryUsed)) return false;
  char *bmi = *reinterpret_cast<char **>(entry + kSuiteEntryBmi);
  if (!bmi) return false;
  char *bmp = *reinterpret_cast<char **>(bmi + kBmiBmp);
  if (!bmp) return false;
  s.suite_idx = id;
  s.bmi = bmi;
  s.bmp = bmp;
  return true;
}

// Find a Ui2DAnimation by name with a read-only walk of the SIDL manager's
// animation array (what FindAnimation1 hashes over; no CXStr temp needed).
char *find_animation(const char *name) {
  char *mgr = static_cast<char *>(*kSidlManager);
  if (!mgr) return nullptr;
  const int len = *reinterpret_cast<int *>(mgr + kMgrAnimsLen);
  char **arr = *reinterpret_cast<char ***>(mgr + kMgrAnimsArr);
  if (!arr || len <= 0 || len > 100000) return nullptr;
  for (int i = 0; i < len; ++i) {
    char *anim = arr[i];
    if (!anim) continue;
    if (_stricmp(read_cxstr(anim + kAnimName).c_str(), name) == 0) return anim;
  }
  return nullptr;
}

// Which animation carries which sheets (frames matched to sheets by texture
// name, so frame order doesn't matter).
struct AnimSheets {
  const char *anim;
  int first, count;  // range in g_sheets
};
constexpr AnimSheets kAnimSheets[] = {{"A_SpellIcons", 0, 7}, {"A_SpellGems", 7, 3}};

// (Re)bind any sheet without a live record. Primary: the two spell-icon
// animations' frames -> TextureId -> loader entry (the exact textures the
// draws use). Fallback: walk the loader array matching the entry's own CXStr
// cache key, the BMI name, or the bitmap's resolved file path.
void resolve_missing() {
  const LoaderArray la = loader_array();
  if (!la.arr) return;

  for (const AnimSheets &as : kAnimSheets) {
    bool need = false;
    for (int k = 0; k < as.count; ++k) need |= (g_sheets[as.first + k].suite_idx < 0);
    if (!need) continue;
    char *anim = find_animation(as.anim);
    if (!anim) continue;
    const int flen = *reinterpret_cast<int *>(anim + kAnimFramesLen);
    char *farr = *reinterpret_cast<char **>(anim + kAnimFramesArr);
    if (!farr || flen <= 0 || flen > 64) continue;
    for (int i = 0; i < flen; ++i) {
      char *info = farr + i * kFrameSize;  // Piece.m_info sits at frame offset 0
      const std::string fname = read_cxstr(info + kInfoName);
      for (int k = 0; k < as.count; ++k) {
        Sheet &s = g_sheets[as.first + k];
        if (s.suite_idx >= 0 || !name_matches(fname.c_str(), s.file)) continue;
        bind_by_id(s, *reinterpret_cast<int *>(info + kInfoTexId), la);
        break;
      }
    }
  }

  bool need_fallback = false;
  for (const Sheet &s : g_sheets) need_fallback |= (s.suite_idx < 0);
  if (!need_fallback) return;
  for (int i = 0; i < la.len; ++i) {
    char *entry = la.arr + i * kSuiteEntrySize;
    if (!*reinterpret_cast<bool *>(entry + kSuiteEntryUsed)) continue;
    char *bmi = *reinterpret_cast<char **>(entry + kSuiteEntryBmi);
    if (!bmi) continue;
    char *bmp = *reinterpret_cast<char **>(bmi + kBmiBmp);
    if (!bmp) continue;  // not loaded yet; bind once the bitmap exists
    const std::string key = read_cxstr(entry + kSuiteEntryName);
    const char *bmi_name = *reinterpret_cast<const char **>(bmi + kBmiName);
    const char *file = *reinterpret_cast<const char **>(bmp + kBmpFileName);
    for (Sheet &s : g_sheets) {
      if (s.suite_idx >= 0) continue;
      if (!name_matches(key.c_str(), s.file) && !name_matches(bmi_name, s.file) && !name_matches(file, s.file))
        continue;
      s.suite_idx = i;
      s.bmi = bmi;
      s.bmp = bmp;
      break;
    }
  }
}

// One-shot diagnostic dump (/rcpspellicons dump): the two animations' frames
// and every used loader entry, so a resolution miss is diagnosable from names.
void dump_state() {
  const LoaderArray la = loader_array();
  logger::logf("[icons] DUMP: loader=%08x len=%d arr=%p sidlmgr=%p", (unsigned)kSuiteTextureLoader, la.len,
               (void *)la.arr, *kSidlManager);
  for (const AnimSheets &as : kAnimSheets) {
    char *anim = find_animation(as.anim);
    if (!anim) {
      logger::logf("[icons] DUMP: animation %s NOT FOUND", as.anim);
      continue;
    }
    const int flen = *reinterpret_cast<int *>(anim + kAnimFramesLen);
    char *farr = *reinterpret_cast<char **>(anim + kAnimFramesArr);
    logger::logf("[icons] DUMP: animation %s @%p frames=%d", as.anim, (void *)anim, flen);
    for (int i = 0; i < flen && farr && i < 64; ++i) {
      char *info = farr + i * kFrameSize;
      logger::logf("[icons] DUMP:   frame %d: name='%s' texid=%d", i, read_cxstr(info + kInfoName).c_str(),
                   *reinterpret_cast<int *>(info + kInfoTexId));
    }
  }
  for (int i = 0; i < la.len; ++i) {
    char *entry = la.arr + i * kSuiteEntrySize;
    if (!*reinterpret_cast<bool *>(entry + kSuiteEntryUsed)) continue;
    char *bmi = *reinterpret_cast<char **>(entry + kSuiteEntryBmi);
    char *bmp = bmi ? *reinterpret_cast<char **>(bmi + kBmiBmp) : nullptr;
    logger::logf("[icons] DUMP: entry %d: key='%s' bmi=%p('%s') bmp=%p file='%s' hasTex=%d tex=%p", i,
                 read_cxstr(entry + kSuiteEntryName).c_str(), (void *)bmi,
                 bmi && *reinterpret_cast<char **>(bmi + kBmiName) ? *reinterpret_cast<char **>(bmi + kBmiName) : "",
                 (void *)bmp, bmp && *reinterpret_cast<char **>(bmp + kBmpFileName) ? *reinterpret_cast<char **>(bmp + kBmpFileName) : "",
                 bmp ? (int)*reinterpret_cast<bool *>(bmp + kBmpHasTexture) : -1,
                 bmp ? *reinterpret_cast<void **>(bmp + kBmpD3DTexture) : nullptr);
  }
}

// Validate a bound sheet against the live array; drops the record when the
// loader re-registered (/loadskin) or the bitmap behind it changed.
bool record_valid(const Sheet &s) {
  if (s.suite_idx < 0) return false;
  char *loader = reinterpret_cast<char *>(kSuiteTextureLoader);
  const int len = *reinterpret_cast<int *>(loader + kLoaderArrayLen);
  char *arr = *reinterpret_cast<char **>(loader + kLoaderArrayPtr);
  if (!arr || s.suite_idx >= len) return false;
  char *entry = arr + s.suite_idx * kSuiteEntrySize;
  if (!*reinterpret_cast<bool *>(entry + kSuiteEntryUsed)) return false;
  if (*reinterpret_cast<char **>(entry + kSuiteEntryBmi) != s.bmi) return false;
  return *reinterpret_cast<char **>(s.bmi + kBmiBmp) == s.bmp && s.bmp != nullptr;
}

// Lazy-load one classic sheet on the render thread (D3DPOOL_MANAGED via D3DX,
// so it survives device resets). A missing file marks the sheet failed - that
// sheet just keeps the revamped art (graceful for partial icon packs).
void ensure_classic_loaded(Sheet &s, IDirect3DDevice9 *device) {
  if (s.classic || s.load_failed) return;
  const std::string path = (get_assets_dir() / s.file).string();
  HRESULT hr = D3DXCreateTextureFromFileA(device, path.c_str(), &s.classic);
  if (FAILED(hr)) {
    s.classic = nullptr;
    s.load_failed = true;
    logger::logf("[icons] classic sheet load failed (0x%08x): %s", (unsigned)hr, path.c_str());
  } else {
    logger::logf("[icons] classic sheet loaded: %s", s.file);
  }
}

// Per-frame worker at the BeginScene seam (render thread, inside rcp_guard).
// Steady state is pointer-compares only; walks/loads happen on transitions.
void frame_tick(IDirect3DDevice9 *device) {
  if (!device || !Rcp::Game::is_in_game()) return;
  if (g_dump) {
    g_dump = false;
    dump_state();
  }
  const bool want = g_classic;

  // Fast out for users who never enable the feature.
  bool any_installed = false, any_unresolved = false;
  for (Sheet &s : g_sheets) {
    any_installed |= s.installed;
    any_unresolved |= (s.suite_idx < 0 && !s.load_failed);
  }
  if (!want && !any_installed) return;

  if (want && any_unresolved) resolve_missing();

  bool all_failed = true;
  for (Sheet &s : g_sheets) {
    if (!s.load_failed) all_failed = false;

    // Revalidate the binding before touching anything through it.
    if (s.suite_idx >= 0 && !record_valid(s)) drop_record(s);
    if (s.suite_idx < 0) continue;

    IDirect3DTexture9 **slot = reinterpret_cast<IDirect3DTexture9 **>(s.bmp + kBmpD3DTexture);
    if (s.installed && *slot != s.classic) reap_consumed_install(s);  // engine reloaded under us

    if (want && !s.installed) {
      // Only swap a bitmap that actually holds a loaded texture (m_pD3DTexture
      // is a union with a raw-bitmap pointer until m_bHasTexture is set).
      if (!*reinterpret_cast<bool *>(s.bmp + kBmpHasTexture) || !*slot) continue;
      ensure_classic_loaded(s, device);
      if (!s.classic) continue;
      s.orig = *slot;
      s.classic->AddRef();  // the engine's reference; consumed by it on teardown or by our restore
      *slot = s.classic;
      s.installed = true;
    } else if (!want && s.installed) {
      *slot = s.orig;  // give the engine its texture back
      s.orig = nullptr;
      s.installed = false;
      s.classic->Release();  // drop the install ref (base ref keeps it for re-enable)
    }

    if (g_debug_frames > 0) {
      logger::logf("[icons] %s: idx=%d bmi=%p bmp=%p tex=%p orig=%p classic=%p installed=%d failed=%d", s.file,
                   s.suite_idx, (void *)s.bmi, (void *)s.bmp, (void *)*slot, (void *)s.orig, (void *)s.classic,
                   (int)s.installed, (int)s.load_failed);
    }
  }
  if (g_debug_frames > 0) --g_debug_frames;

  // The classic art isn't installed at all (fresh clone without `make install`,
  // or a moved game dir): say so once in chat instead of silently doing nothing.
  if (want && all_failed && !g_missing_warned) {
    g_missing_warned = true;
    RcpService::get_instance()->queue_chat_message(
        "rof2ClientPlus: classic spell icons not found - expected uifiles/rcp/spellicons/spells01.tga etc. "
        "(re-run 'make install').");
  }
}

// Device Reset (window resize): hand the engine back its original texture
// pointers and drop all records. Our classic sheets are D3DPOOL_MANAGED (they
// survive Reset), but the DISPLACED engine textures we hold in s.orig are of
// unknown pool - holding one across Reset could fail it, and the client treats
// a failed Reset as fatal. Uninstalling returns the cache to its stock state
// for the Reset; the next frame_tick re-resolves and re-installs (g_classic
// still holds the user's setting). Runs on the render thread from the hook.
void on_device_reset() {
  int uninstalled = 0;
  for (Sheet &s : g_sheets) {
    if (s.installed) {
      IDirect3DTexture9 **slot = reinterpret_cast<IDirect3DTexture9 **>(s.bmp + kBmpD3DTexture);
      if (record_valid(s) && *slot == s.classic) {
        *slot = s.orig;  // give the engine its texture back
        s.orig = nullptr;
        s.installed = false;
        s.classic->Release();  // drop the install ref (base ref keeps it for re-enable)
        ++uninstalled;
      } else {
        reap_consumed_install(s);  // engine already reloaded under us
      }
    }
    drop_record(s);
  }
  if (uninstalled) logger::logf("[icons] device reset: restored %d engine sheet(s)", uninstalled);
}

void print_status() {
  int installed = 0, failed = 0;
  for (const Sheet &s : g_sheets) {
    if (s.installed) ++installed;
    if (s.load_failed) ++failed;
  }
  Rcp::Game::print_chat("rof2ClientPlus spell icons: %s (%d/%d sheets swapped, %d missing)",
                        g_classic ? "CLASSIC (pre-2013)" : "REVAMPED (client default)", installed, kSheetCount, failed);
}

}  // namespace

// ---- options-UI / command accessors (apply next frame + persist) ----
namespace spell_icons_settings {
bool get_classic() { return g_classic; }
void set_classic(bool on) {
  g_classic = on;
  save_settings();
}
}  // namespace spell_icons_settings

namespace spell_icons_api {
char *find_animation(const char *name) { return ::find_animation(name); }
}  // namespace spell_icons_api

SpellIcons::SpellIcons(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  logger::logf("[icons] settings loaded: classic=%d (swap applies at the prerender seam)", (int)g_classic);

  // The swap/validate worker runs before each frame's UI raster (BeginScene).
  directx::add_prerender_callback([](IDirect3DDevice9 *dev) { frame_tick(dev); });

  // Window resize resets the device: give the engine back its texture pointers
  // first, re-resolve + re-install on the frames after (see on_device_reset).
  directx::add_reset_callback([](IDirect3DDevice9 *) { on_device_reset(); });

  rcp->commands_hook->Add(
      "/rcpspellicons", {"/rcpicons"},
      "Spell icons: '/rcpspellicons' toggles between the original pre-2013 icon art and the revamped "
      "client default, live (no reload). 'on' = classic, 'off' = revamped, 'status', 'debug'.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1" || a == "classic") {
            g_classic = true;
          } else if (a == "off" || a == "0" || a == "revamped" || a == "new") {
            g_classic = false;
          } else if (a == "status") {
            print_status();
            return true;
          } else if (a == "debug") {
            g_debug_frames = 5;
            Rcp::Game::print_chat("rof2ClientPlus: spell-icon state logging for 5 frames (see log).");
            return true;
          } else if (a == "dump") {
            g_dump = true;
            Rcp::Game::print_chat("rof2ClientPlus: dumping spell-icon animations + texture cache to the log.");
            return true;
          } else {
            Rcp::Game::print_chat("rof2ClientPlus: '/rcpspellicons on|off|status|debug|dump'");
            return true;
          }
        } else {
          g_classic = !g_classic;
        }
        save_settings();
        print_status();
        return true;
      });
  logger::log("[icons] SpellIcons constructed; /rcpspellicons registered");
}
