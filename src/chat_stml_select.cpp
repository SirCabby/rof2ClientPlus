// rof2ClientPlus - character-precise drag-select + copy for chat history. See chat_stml_select.h.
#include "chat_stml_select.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "crash_handler.h"
#include "hook_wrapper.h"
#include "logger.h"
#include "rcp.h"

namespace {

// ---------------------------------------------------------------------------
// Stock RoF2 addresses / offsets (eqlib + disasm, May 10 2013 build).
// ---------------------------------------------------------------------------
void **const kChatMgr = reinterpret_cast<void **>(0xF71070);  // pinstCChatWindowManager
constexpr int kMaxChatWindows = 32;
constexpr int kChatOutputWnd = 0x228;  // CChatWindow::OutputWnd (a CStmlWnd)

// CXWnd
constexpr int kWndVScroll = 0x17c;              // int VScrollPos
constexpr uintptr_t kGetScreenRect = 0x8638D0;  // CXRect GetScreenRect() - __thiscall, struct return

// CStmlWnd
constexpr int kStmlLines = 0x1dc;  // CircularArrayClass2<STextLine> (embedded); +0x00 == this field
// CircularArrayClass2 fields, relative to the container base (CStmlWnd+0x1dc).
constexpr int kCaCount = 0x00, kCaHead = 0x04, kCaWrap = 0x08, kCaChunkMask = 0x14, kCaChunkShift = 0x18,
              kCaChunks = 0x1c;
// STextLine (0x1c bytes)
constexpr int kSizeofLine = 0x1c;
constexpr int kTlPieces = 0x00;  // ArrayClass<SFormattedText>
constexpr int kTlYBottom = 0x10, kTlYTop = 0x14;
// ArrayClass<T>
constexpr int kAcLength = 0x00, kAcArray = 0x04;
// SFormattedText (0x28 bytes)
constexpr int kSizeofPiece = 0x28;
constexpr int kFtLeft = 0x0c, kFtRight = 0x10, kFtText = 0x14 /*CXStr*/;
// CStrRep
constexpr int kRepLength = 0x08, kRepEncoding = 0x0c, kRepData = 0x14;

// CXWnd shared vtable slots (indices). A drag-release fires LButtonUpAfterHeld
// (+0x44), NOT LButtonUp (+0x3c) - both must end the selection.
constexpr int kVtDraw = 0x0c / 4, kVtLBD = 0x38 / 4, kVtLBU = 0x3c / 4, kVtLBUAH = 0x44 / 4, kVtMM = 0x60 / 4;

constexpr uintptr_t kDrawColoredRect = 0x863010;  // __cdecl(const CXRect*, COLORREF, const CXRect*)

// DI8__MouseState (0xE67884) is the game's polled DirectInput mouse state
// {LONG lX,lY,lZ; BYTE rgbButtons[8]}; left button = rgbButtons[0] @ +0xC. This
// is the authoritative "is the button down" signal, refreshed every frame - far
// more reliable than the up-events, which Wine drops when a slow drag crosses
// EQ's "held" threshold (the release then never reaches the window).
constexpr uintptr_t kMouseLButton = 0xE67890;

struct Rect {
  int left = 0, top = 0, right = 0, bottom = 0;
};

typedef int(__fastcall *MouseFn)(void *wnd, int edx, void *pos, uint32_t flags);
typedef int(__fastcall *DrawFn)(void *wnd, int edx);

MouseFn g_lbd_orig = nullptr, g_lbu_orig = nullptr, g_lbuah_orig = nullptr, g_mm_orig = nullptr;
DrawFn g_draw_orig = nullptr;

RcpService *g_rcp = nullptr;
bool g_installed = false;

// Selection state (main thread only). A selection is a (line, column) range,
// where line is a logical index into the TextLines ring and column is a
// character offset into that line's concatenated (tag-free) text.
void *g_stml = nullptr;
int g_anchor_line = -1, g_anchor_col = 0;
int g_caret_line = -1, g_caret_col = 0;
bool g_selecting = false;

// --- CXStr / container helpers ---------------------------------------------

void get_screen_rect(void *wnd, Rect *out) {
  reinterpret_cast<void(__thiscall *)(void *, Rect *)>(kGetScreenRect)(wnd, out);
}

bool left_button_down() { return (*reinterpret_cast<volatile uint8_t *>(kMouseLButton) & 0x80) != 0; }

std::string read_cxstr(void *cxstr_field) {
  std::string s;
  char *rep = *reinterpret_cast<char **>(cxstr_field);
  if (!rep) return s;
  const uint32_t len = *reinterpret_cast<uint32_t *>(rep + kRepLength);
  const uint32_t enc = *reinterpret_cast<uint32_t *>(rep + kRepEncoding);
  if (len > 100000) return s;
  if (enc == 1) {
    const wchar_t *w = reinterpret_cast<const wchar_t *>(rep + kRepData);
    for (uint32_t i = 0; i < len; ++i) s += (w[i] >= 0x20 && w[i] < 0x7f) ? static_cast<char>(w[i]) : ' ';
  } else {
    s.assign(rep + kRepData, rep + kRepData + len);
  }
  return s;
}

int line_count(void *stml) { return *reinterpret_cast<int *>(static_cast<char *>(stml) + kStmlLines + kCaCount); }

// Ring element access (matches CStmlWnd::GetVisibleText 0x880430): logical index i
// -> physical (i < Wrap ? Head+i : i-Wrap), then Chunks[phys>>Shift] + (phys&Mask)*0x1c.
char *line_at(void *stml, int i) {
  if (i < 0) return nullptr;
  char *ca = static_cast<char *>(stml) + kStmlLines;
  const int wrap = *reinterpret_cast<int *>(ca + kCaWrap);
  const int head = *reinterpret_cast<int *>(ca + kCaHead);
  const int mask = *reinterpret_cast<int *>(ca + kCaChunkMask);
  const int shift = *reinterpret_cast<int *>(ca + kCaChunkShift);
  char **chunks = *reinterpret_cast<char ***>(ca + kCaChunks);
  if (!chunks) return nullptr;
  const int phys = (i < wrap) ? (head + i) : (i - wrap);
  char *chunk = chunks[phys >> shift];
  if (!chunk) return nullptr;
  return chunk + (phys & mask) * kSizeofLine;
}

int piece_count(char *line) { return *reinterpret_cast<int *>(line + kTlPieces + kAcLength); }
char *piece_at(char *line, int p) {
  char *arr = *reinterpret_cast<char **>(line + kTlPieces + kAcArray);
  return arr ? arr + p * kSizeofPiece : nullptr;
}
int piece_len(char *piece) {
  char *rep = *reinterpret_cast<char **>(piece + kFtText);
  return rep ? static_cast<int>(*reinterpret_cast<uint32_t *>(rep + kRepLength)) : 0;
}

// A line's full (tag-free) text: the concatenation of its formatted pieces.
std::string line_text(char *line) {
  std::string s;
  const int pc = piece_count(line);
  for (int p = 0; p < pc; ++p) {
    char *pce = piece_at(line, p);
    if (pce) s += read_cxstr(pce + kFtText);
  }
  return s;
}
int line_len(char *line) {
  int n = 0;
  const int pc = piece_count(line);
  for (int p = 0; p < pc; ++p) {
    char *pce = piece_at(line, p);
    if (pce) n += piece_len(pce);
  }
  return n;
}

// content X (client-relative) -> column within the line (linear interp per piece).
int line_x_to_col(char *line, int cx) {
  const int pc = piece_count(line);
  int base = 0;
  for (int p = 0; p < pc; ++p) {
    char *pce = piece_at(line, p);
    if (!pce) continue;
    const int l = *reinterpret_cast<int *>(pce + kFtLeft);
    const int r = *reinterpret_cast<int *>(pce + kFtRight);
    const int tl = piece_len(pce);
    if (p == 0 && cx <= l) return 0;
    if (cx >= l && cx <= r) {
      const int span = (r > l) ? (r - l) : 1;
      int off = ((cx - l) * tl + span / 2) / span;
      return base + std::max(0, std::min(off, tl));
    }
    base += tl;
  }
  return base;  // past the last piece -> end of line
}

// column within the line -> content X (inverse of the above).
int line_col_to_x(char *line, int col) {
  const int pc = piece_count(line);
  int base = 0, lastRight = 0, firstLeft = 0;
  for (int p = 0; p < pc; ++p) {
    char *pce = piece_at(line, p);
    if (!pce) continue;
    const int l = *reinterpret_cast<int *>(pce + kFtLeft);
    const int r = *reinterpret_cast<int *>(pce + kFtRight);
    const int tl = piece_len(pce);
    if (p == 0) firstLeft = l;
    if (col >= base && col <= base + tl) {
      const int span = (tl > 0) ? tl : 1;
      return l + (col - base) * (r - l) / span;
    }
    base += tl;
    lastRight = r;
  }
  return col <= 0 ? firstLeft : lastRight;
}

// screen point -> (line, col). Clamps to the nearest line by content-Y.
void point_to_linecol(void *stml, int sx, int sy, int *outLine, int *outCol) {
  *outLine = -1;
  *outCol = 0;
  Rect scr{};
  get_screen_rect(stml, &scr);
  const int vscroll = *reinterpret_cast<int *>(static_cast<char *>(stml) + kWndVScroll);
  const int cx = sx - scr.left;
  const int cy = sy - scr.top + vscroll;
  const int n = line_count(stml);
  if (n <= 0) return;
  int best = n - 1;
  for (int i = 0; i < n; ++i) {
    char *L = line_at(stml, i);
    if (!L) continue;
    const int yt = *reinterpret_cast<int *>(L + kTlYTop);
    const int yb = *reinterpret_cast<int *>(L + kTlYBottom);
    if (cy <= yb) {
      best = i;
      break;
    }
  }
  char *L = line_at(stml, best);
  *outLine = best;
  *outCol = L ? line_x_to_col(L, cx) : 0;
}

// Order the two selection endpoints so a<=b in (line, col) reading order.
bool have_selection() {
  return g_stml && g_anchor_line >= 0 && g_caret_line >= 0 &&
         (g_anchor_line != g_caret_line || g_anchor_col != g_caret_col);
}
void ordered(int *sL, int *sC, int *eL, int *eC) {
  const bool anchorFirst =
      g_anchor_line < g_caret_line || (g_anchor_line == g_caret_line && g_anchor_col <= g_caret_col);
  *sL = anchorFirst ? g_anchor_line : g_caret_line;
  *sC = anchorFirst ? g_anchor_col : g_caret_col;
  *eL = anchorFirst ? g_caret_line : g_anchor_line;
  *eC = anchorFirst ? g_caret_col : g_anchor_col;
}

std::string extract_selection() {
  if (!have_selection()) return "";
  int sL, sC, eL, eC;
  ordered(&sL, &sC, &eL, &eC);
  std::string out;
  for (int i = sL; i <= eL; ++i) {
    char *L = line_at(g_stml, i);
    if (!L) continue;
    std::string t = line_text(L);
    const int a = (i == sL) ? sC : 0;
    const int b = (i == eL) ? eC : static_cast<int>(t.size());
    const int aa = std::max(0, std::min(a, static_cast<int>(t.size())));
    const int bb = std::max(aa, std::min(b, static_cast<int>(t.size())));
    out += t.substr(aa, bb - aa);
    if (i != eL) out += "\n";
  }
  return out;
}

void write_clipboard(const std::string &text) {
  HWND owner = GetForegroundWindow();
  if (!OpenClipboard(owner)) return;
  EmptyClipboard();
  if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1)) {
    if (char *dst = static_cast<char *>(GlobalLock(h))) {
      memcpy(dst, text.c_str(), text.size());
      dst[text.size()] = '\0';
      GlobalUnlock(h);
      if (!SetClipboardData(CF_TEXT, h)) GlobalFree(h);
    } else {
      GlobalFree(h);
    }
  }
  CloseClipboard();
}

// --- hooks -----------------------------------------------------------------

// pos is a CXPoint* {int x, int y} in the game's screen space (same as
// GetScreenRect) - use it, not GetCursorPos (OS-desktop coordinates).
int __fastcall Lbd_hk(void *wnd, int edx, void *pos, uint32_t flags) {
  int r = g_lbd_orig(wnd, edx, pos, flags);  // native link/scroll handling first
  rcp_guard::run("stml.lbd", [&] {
    const int *pt = reinterpret_cast<const int *>(pos);
    int line, col;
    point_to_linecol(wnd, pt[0], pt[1], &line, &col);
    g_stml = wnd;
    g_anchor_line = g_caret_line = line;
    g_anchor_col = g_caret_col = col;
    g_selecting = true;
  });
  return r;
}

int __fastcall Mm_hk(void *wnd, int edx, void *pos, uint32_t flags) {
  if (g_selecting && wnd == g_stml) {
    if (!left_button_down()) {
      g_selecting = false;  // button released - freeze the selection here
    } else {
      rcp_guard::run("stml.mm", [&] {
        const int *pt = reinterpret_cast<const int *>(pos);
        point_to_linecol(wnd, pt[0], pt[1], &g_caret_line, &g_caret_col);
      });
    }
  }
  return g_mm_orig(wnd, edx, pos, flags);
}

int __fastcall Lbu_hk(void *wnd, int edx, void *pos, uint32_t flags) {
  if (g_selecting && wnd == g_stml) g_selecting = false;  // plain click-release
  return g_lbu_orig(wnd, edx, pos, flags);
}

int __fastcall Lbuah_hk(void *wnd, int edx, void *pos, uint32_t flags) {
  if (g_selecting && wnd == g_stml) g_selecting = false;  // release after a drag
  return g_lbuah_orig(wnd, edx, pos, flags);
}

// After the window draws its text, paint the selection highlight over it: for
// each selected line, a rect from the start column's X to the end column's X.
int __fastcall Draw_hk(void *wnd, int edx) {
  int r = g_draw_orig(wnd, edx);
  // Freeze the selection the frame the button comes up, even if the cursor never
  // moves again (the up-event can be lost under Wine on a held drag).
  if (g_selecting && !left_button_down()) g_selecting = false;
  if (wnd == g_stml && have_selection()) {
    rcp_guard::run("stml.draw", [&] {
      Rect scr{};
      get_screen_rect(wnd, &scr);
      const int vscroll = *reinterpret_cast<int *>(static_cast<char *>(wnd) + kWndVScroll);
      const COLORREF kHi = 0x40FFFF80;
      const Rect clip = scr;
      int sL, sC, eL, eC;
      ordered(&sL, &sC, &eL, &eC);
      for (int i = sL; i <= eL; ++i) {
        char *L = line_at(wnd, i);
        if (!L) continue;
        const int ll = line_len(L);
        const int c1 = (i == sL) ? sC : 0;
        const int c2 = (i == eL) ? eC : ll;
        const int yt = *reinterpret_cast<int *>(L + kTlYTop);
        const int yb = *reinterpret_cast<int *>(L + kTlYBottom);
        Rect rc;
        rc.left = scr.left + line_col_to_x(L, c1);
        rc.right = scr.left + line_col_to_x(L, c2);
        rc.top = scr.top + yt - vscroll;
        rc.bottom = scr.top + yb - vscroll;
        if (rc.right < rc.left) std::swap(rc.left, rc.right);
        if (rc.right <= rc.left) rc.right = rc.left + 2;  // thin caret marker
        reinterpret_cast<void(__cdecl *)(const Rect *, COLORREF, const Rect *)>(kDrawColoredRect)(&rc, kHi, &clip);
      }
    });
  }
  return r;
}

}  // namespace

bool chat_stml_copy_selection() {
  std::string text;
  rcp_guard::run("stml.copy", [&] { text = extract_selection(); });
  if (text.empty()) return false;
  write_clipboard(text);
  logger::logf("[chat] history copy -> %d bytes", (int)text.size());
  return true;
}

ChatStmlSelect::ChatStmlSelect(RcpService *rcp) { g_rcp = rcp; }
ChatStmlSelect::~ChatStmlSelect() {}

void ChatStmlSelect::on_frame() {
  if (g_installed || !g_rcp) return;
  void *mgr = *kChatMgr;
  if (!mgr) return;
  void *output = nullptr;
  for (int i = 0; i < kMaxChatWindows && !output; ++i) {
    void *cw = *reinterpret_cast<void **>(static_cast<char *>(mgr) + i * 4);
    if (cw) output = *reinterpret_cast<void **>(static_cast<char *>(cw) + kChatOutputWnd);
  }
  if (!output) return;
  void **vt = *reinterpret_cast<void ***>(output);
  auto add = [&](const char *name, int slot, void *fn) -> void * {
    g_rcp->hooks->Add(name, reinterpret_cast<int>(vt[slot]), fn, hook_type_detour);
    return g_rcp->hooks->hook_map[name]->original(fn);
  };
  g_lbd_orig = reinterpret_cast<MouseFn>(add("stml_lbd", kVtLBD, reinterpret_cast<void *>(Lbd_hk)));
  g_lbu_orig = reinterpret_cast<MouseFn>(add("stml_lbu", kVtLBU, reinterpret_cast<void *>(Lbu_hk)));
  g_lbuah_orig = reinterpret_cast<MouseFn>(add("stml_lbuah", kVtLBUAH, reinterpret_cast<void *>(Lbuah_hk)));
  g_mm_orig = reinterpret_cast<MouseFn>(add("stml_mm", kVtMM, reinterpret_cast<void *>(Mm_hk)));
  g_draw_orig = reinterpret_cast<DrawFn>(add("stml_draw", kVtDraw, reinterpret_cast<void *>(Draw_hk)));
  g_installed = true;
  logger::logf("[chat] history drag-select hooks installed (CStmlWnd vtable=%p)", (void *)vt);
}
