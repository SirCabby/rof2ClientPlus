// rof2ClientPlus - clipboard copy/paste in chat + edit boxes. See chat_clipboard.h.
#include "chat_clipboard.h"
#include "rebase.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "chat_stml_select.h"
#include "crash_handler.h"
#include "hook_wrapper.h"
#include "logger.h"
#include "rcp.h"

namespace {

// ---------------------------------------------------------------------------
// Stock RoF2 addresses / offsets (eqlib offsets/eqgame.h + disasm of the
// May 10 2013 build; see PORTING_NOTES.md "Chat clipboard").
// ---------------------------------------------------------------------------
// CXWndManager::HandleKeyboardMsg(uint32 dikScanCode, uint32 down) - __thiscall,
// ret 0x8. The single UI keyboard choke point: KeypressHandler::HandleKeyDown
// (0x5594E0) calls it FIRST and, iff it returns 0 (consumed), SKIPS the keymap
// scan (`test eax,eax; je past-keymap`). So returning 0 swallows the key; any
// non-zero value lets the keymap run. It routes the key to the focused window
// and to record modifier state.
const uintptr_t kHandleKeyboardMsg = ::Rcp::eqva(0x876530);

// CXWndManager data members (disasm of 0x876530). The Ctrl byte is set by the
// LCONTROL(0x1d)/RCONTROL(0x9d) scancode handlers; Focus is the window the msg
// is routed to.
constexpr int kMgrCtrl = 0x9e;   // uint8: 1 while either Ctrl is held
constexpr int kMgrFocus = 0x64;  // CXWnd* focused window

// CXWnd vtable slot for `CEditWnd* GetActiveEditWnd()` (eqlib CXWnd.h: +0x15c).
// The native paste path uses this same call to find the edit target; it returns
// null for non-edit windows, so it doubles as our "is this an edit box" test.
constexpr int kVtGetActiveEditWnd = 0x15c / 4;

// CEditBaseWnd fields (eqlib UI.h + disasm of the mouse/key handlers). StartPos/
// EndPos bound the current selection (the native HandleLButtonDown 0x87db80 /
// HandleMouseMove 0x87dc40 set these on drag-select); SelDir @0x310 is the
// direction flag those handlers keep (which end holds the caret); InputText is a
// CXStr (a single CStrRep* by value).
constexpr int kEditStartPos = 0x1dc;   // int
constexpr int kEditEndPos = 0x1e0;     // int
constexpr int kEditSelDir = 0x310;     // uint8
constexpr int kEditInputText = 0x1ec;  // CStrRep*

// CStrRep fields (eqlib CXStr.h). Encoding 0 == Utf8/ASCII; the char data is
// inline at +0x14.
constexpr int kRepLength = 0x08;    // uint32
constexpr int kRepEncoding = 0x0c;  // 0 = Utf8
constexpr int kRepData = 0x14;      // char[] / wchar_t[]
constexpr uint32_t kEncodingUtf8 = 0;
constexpr uint32_t kEncodingUtf16 = 1;

// CEditWnd::ReplaceSelection(char ch, bool bFilter) @ 0x87d0e0 - the SINGLE-char
// overload. It builds a one-char CXStr from the low byte of its first arg (via
// the char ctor 0x805be0: `[rep+8]=1; [rep+0x14]=arg_byte`) - NOT a char*. So we
// paste by feeding one char at a time; the first call replaces the selection,
// each next inserts at the advancing caret. (Passing a char* here is the bug
// that pasted a single "(" - the pointer's low byte.)
const uintptr_t kReplaceSelChar = ::Rcp::eqva(0x87d0e0);

// CEditWnd::ReplaceSelection(CXStr, bool bFilter) @ 0x87cd30 - the CXStr overload.
// A null rep == empty string, so it deletes the current selection; it null-checks
// the rep and no-ops on read-only edits (style bit ewsReadOnly 0x200000). We use
// it only for cut's delete, so no CXStr allocation/refcount is needed.
const uintptr_t kReplaceSelStr = ::Rcp::eqva(0x87cd30);

// Safety caps: the chat input's own MaxChars/bFilter already bound growth, these
// just stop a pathological clipboard from looping forever / copying megabytes.
constexpr size_t kMaxPaste = 4096;
constexpr size_t kMaxCopy = 8192;

// DirectInput scancodes (same values Zeal keys off of).
constexpr uint32_t kDikA = 0x1e;  // select all
constexpr uint32_t kDikX = 0x2d;  // cut
constexpr uint32_t kDikC = 0x2e;  // copy
constexpr uint32_t kDikV = 0x2f;  // paste

// CEditWnd mouse handlers (CEditWnd vtable @ 0x61a524; all __thiscall(const
// CXPoint* pos, uint32 flags), ret 0x8). Native drag-select lives in
// HandleMouseMove: it maps the point to a char and extends StartPos/EndPos, but
// ONLY while CXWndManager's CurrDraggedWindow (@+0x68) equals this edit. That
// drag-tracking field can be left unset under Wine (so a click positions the
// caret but a drag never highlights). We track the press->release ourselves and
// set CurrDraggedWindow transiently around the native move so the extend runs.
const uintptr_t kLButtonDown = ::Rcp::eqva(0x87db80);
const uintptr_t kLButtonUp = ::Rcp::eqva(0x87b340);
const uintptr_t kMouseMove = ::Rcp::eqva(0x87dc40);
constexpr int kMgrCurrDragged = 0x68;  // CXWndManager::CurrDraggedWindow
// The native check reads *(*(0xb648fc)) to get the CXWndManager, then +0x68.
void **const kWndMgrChain = reinterpret_cast<void **>(::Rcp::eqva(0xb648fc));

typedef int(__fastcall *HandleKeyboardMsgFn)(void *mgr, int edx, uint32_t dik, uint32_t down);
typedef void *(__thiscall *GetActiveEditWndFn)(void *self);
typedef char(__thiscall *ReplaceSelCharFn)(void *edit, int ch, int b_filter);
typedef char(__thiscall *ReplaceSelStrFn)(void *edit, void *cxstr_rep, int b_filter);
typedef int(__fastcall *MouseFn)(void *wnd, int edx, void *pos, uint32_t flags);

HandleKeyboardMsgFn g_orig = nullptr;
MouseFn g_lbd_orig = nullptr, g_lbu_orig = nullptr, g_mm_orig = nullptr;
void *g_drag_edit = nullptr;  // the edit a left-press landed on (main thread only)
int g_drag_moves = 0;         // mouse-moves seen during the current drag (diagnostic)

// --- OS clipboard (our injected DLL imports the full user32 clipboard API that
// eqgame.exe itself lacks). OpenClipboard(nullptr) associates with the current
// task, which is what Wine's X11/Wayland clipboard bridge expects. -------------

std::string read_clipboard() {
  std::string text;
  if (!OpenClipboard(nullptr)) return text;
  if (HANDLE h = GetClipboardData(CF_TEXT)) {
    if (const char *p = static_cast<const char *>(GlobalLock(h))) {
      text = p;
      GlobalUnlock(h);
    }
  }
  CloseClipboard();
  return text;
}

void write_clipboard(const std::string &text) {
  // Under Wine the clipboard *owner* window is what services X11 CLIPBOARD-selection
  // requests, so pass the game's foreground window (a null owner can leave nothing
  // on the X11 clipboard even though SetClipboardData "succeeds"). The read path
  // (paste) works with a null handle, but the write path needs the owner.
  HWND owner = GetForegroundWindow();
  if (!OpenClipboard(owner)) {
    logger::logf("[chat] copy: OpenClipboard(%p) failed err=%lu", (void *)owner, GetLastError());
    return;
  }
  EmptyClipboard();
  bool ok = false;
  if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1)) {
    if (char *dst = static_cast<char *>(GlobalLock(h))) {
      memcpy(dst, text.c_str(), text.size());
      dst[text.size()] = '\0';
      GlobalUnlock(h);
      if (SetClipboardData(CF_TEXT, h))
        ok = true;
      else
        GlobalFree(h);  // On failure we still own h.
    } else {
      GlobalFree(h);
    }
  }
  CloseClipboard();
  logger::logf("[chat] copy: wrote %d bytes (owner=%p ok=%d)", (int)text.size(), (void *)owner, ok ? 1 : 0);
}

// Copy the focused edit window's text to the OS clipboard: the highlighted range
// if there is one, otherwise the whole line (so Ctrl+C is useful even without a
// selection - and lets you confirm the copy->clipboard path end to end). Cut
// deletes the selection afterwards. NOTE: this only ever sees the chat INPUT
// line (a CEditWnd); the scrollback/history is a CStmlWnd with no selection, so
// it cannot be reached here - see the header.
void do_copy(void *edit, bool cut) {
  std::string sel;
  bool had_selection = false;
  int enc = -1, rlen = 0, spos = 0, epos = 0;  // for the diagnostic log line
  rcp_guard::run("chat_clipboard.copy", [&] {
    auto rep = *reinterpret_cast<char **>(static_cast<char *>(edit) + kEditInputText);
    if (!rep) return;
    enc = static_cast<int>(*reinterpret_cast<uint32_t *>(rep + kRepEncoding));
    const int len = static_cast<int>(*reinterpret_cast<uint32_t *>(rep + kRepLength));
    rlen = len;
    int s = *reinterpret_cast<int *>(static_cast<char *>(edit) + kEditStartPos);
    int e = *reinterpret_cast<int *>(static_cast<char *>(edit) + kEditEndPos);
    spos = s;
    epos = e;
    if (s > e) std::swap(s, e);
    s = std::max(0, std::min(s, len));
    e = std::max(0, std::min(e, len));
    had_selection = (e > s);
    const int a = had_selection ? s : 0;              // No selection -> whole line.
    int b = had_selection ? e : len;
    if (b <= a) return;
    if (static_cast<size_t>(b - a) > kMaxCopy) b = a + static_cast<int>(kMaxCopy);
    if (enc == static_cast<int>(kEncodingUtf16)) {
      const wchar_t *w = reinterpret_cast<const wchar_t *>(rep + kRepData);
      for (int i = a; i < b; ++i) sel += (w[i] >= 0x20 && w[i] < 0x7f) ? static_cast<char>(w[i]) : '?';
    } else {  // Utf8 / ASCII.
      const char *text = rep + kRepData;
      sel.assign(text + a, text + b);
    }
  });

  logger::logf("[chat] copy: enc=%d len=%d sel=[%d,%d] cut=%d -> %d bytes", enc, rlen, spos, epos, cut ? 1 : 0,
               (int)sel.size());
  if (!sel.empty()) write_clipboard(sel);
  if (cut && had_selection) {
    rcp_guard::run("chat_clipboard.cut", [&] {
      reinterpret_cast<ReplaceSelStrFn>(kReplaceSelStr)(edit, nullptr, /*bFilter*/ 0);  // null rep = delete selection.
    });
  }
}

// Paste the OS clipboard into the focused edit window at the caret. We feed the
// text through the single-char ReplaceSelection one byte at a time: the first
// char replaces any selection, the rest insert at the advancing caret. Control
// chars (incl. newlines) collapse to spaces so a multi-line paste stays a single
// chat line - this mirrors the stock client's own paste filter.
void do_paste(void *edit) {
  std::string text = read_clipboard();
  if (text.empty()) return;
  rcp_guard::run("chat_clipboard.paste", [&] {
    auto insert = reinterpret_cast<ReplaceSelCharFn>(kReplaceSelChar);
    size_t n = 0;
    for (char c : text) {
      if (n++ >= kMaxPaste) break;
      unsigned char u = static_cast<unsigned char>(c);
      insert(edit, (u >= 0x20 && u <= 0x7e) ? u : ' ', /*bFilter*/ 1);
    }
  });
}

// Ctrl+A: highlight the whole input line. The native key handler doesn't select-
// all, so we set the selection span directly (StartPos..EndPos = 0..len), the
// same fields the drag handlers use, so copy/cut/paste-over then act on it.
void do_select_all(void *edit) {
  rcp_guard::run("chat_clipboard.selectall", [&] {
    auto rep = *reinterpret_cast<char **>(static_cast<char *>(edit) + kEditInputText);
    const int len = rep ? static_cast<int>(*reinterpret_cast<uint32_t *>(rep + kRepLength)) : 0;
    *reinterpret_cast<int *>(static_cast<char *>(edit) + kEditStartPos) = 0;
    *reinterpret_cast<int *>(static_cast<char *>(edit) + kEditEndPos) = len;
    *reinterpret_cast<uint8_t *>(static_cast<char *>(edit) + kEditSelDir) = 0;  // caret at the end.
  });
}

// Address of CXWndManager::CurrDraggedWindow via the exact chain the native
// HandleMouseMove dereferences (*(*(0xb648fc)) + 0x68). Null if the UI isn't up.
void **curr_dragged_field() {
  void *p = *kWndMgrChain;
  if (!p) return nullptr;
  void *mgr = *reinterpret_cast<void **>(p);
  if (!mgr) return nullptr;
  return reinterpret_cast<void **>(static_cast<char *>(mgr) + kMgrCurrDragged);
}

// A left-press on an edit begins a potential drag-select. Let the native handler
// set the caret/anchor first, then remember the edit.
int __fastcall LButtonDown_hk(void *wnd, int edx, void *pos, uint32_t flags) {
  int r = g_lbd_orig(wnd, edx, pos, flags);
  g_drag_edit = wnd;
  g_drag_moves = 0;
  return r;
}

// Release ends the drag. Log the move count so we can confirm HandleMouseMove
// actually fires during a Wine drag (moves>0 means the extend path is reachable).
int __fastcall LButtonUp_hk(void *wnd, int edx, void *pos, uint32_t flags) {
  if (g_drag_edit == wnd) {
    logger::logf("[chat] drag end edit=%p moves=%d", wnd, g_drag_moves);
    g_drag_edit = nullptr;
  }
  return g_lbu_orig(wnd, edx, pos, flags);
}

// While a drag is active on this edit, force it to be the CurrDraggedWindow just
// for the native move (which then extends StartPos/EndPos to the cursor), then
// restore the field so nothing else thinks a window-move is in progress.
int __fastcall MouseMove_hk(void *wnd, int edx, void *pos, uint32_t flags) {
  if (wnd && wnd == g_drag_edit) {
    ++g_drag_moves;
    int r = 0;
    rcp_guard::run("chat_clipboard.dragmove", [&] {
      void **cd = curr_dragged_field();
      void *saved = cd ? *cd : nullptr;
      if (cd) *cd = wnd;
      r = g_mm_orig(wnd, edx, pos, flags);
      if (cd) *cd = saved;
    });
    return r;
  }
  return g_mm_orig(wnd, edx, pos, flags);
}

// The detour. Fires for every UI keystroke, so the fast path is a couple of
// integer compares before anything touches game memory: only a Ctrl-held keydown
// of A/C/X/V that lands on an actual edit window is ours to consume.
int __fastcall HandleKeyboardMsg_hk(void *mgr, int edx, uint32_t dik, uint32_t down) {
  if (down && (dik == kDikA || dik == kDikC || dik == kDikV || dik == kDikX)) {
    const bool ctrl = *reinterpret_cast<uint8_t *>(static_cast<char *>(mgr) + kMgrCtrl) != 0;

    // A history (CStmlWnd) selection copies ahead of the input line: while you
    // drag-select in the scrollback the focused edit is still the input box, so
    // GetActiveEditWnd below would otherwise copy the wrong thing. No-op (returns
    // false) when there's no history selection.
    if (ctrl && (dik == kDikC || dik == kDikX) && chat_stml_copy_selection()) return 0;

    void *edit = nullptr;
    rcp_guard::run("chat_clipboard.focus", [&] {
      if (!ctrl) return;  // Ctrl not held.
      void *focus = *reinterpret_cast<void **>(static_cast<char *>(mgr) + kMgrFocus);
      if (!focus) return;
      void **vtbl = *reinterpret_cast<void ***>(focus);
      edit = reinterpret_cast<GetActiveEditWndFn>(vtbl[kVtGetActiveEditWnd])(focus);
    });
    if (edit) {
      if (dik == kDikV)
        do_paste(edit);
      else if (dik == kDikA)
        do_select_all(edit);
      else
        do_copy(edit, /*cut*/ dik == kDikX);
      return 0;  // Consumed: keeps the keymap from also acting on the key.
    }
  }
  return g_orig(mgr, edx, dik, down);
}

}  // namespace

ChatClipboard::ChatClipboard(RcpService *rcp) {
  rcp->hooks->Add("chat_clipboard", static_cast<int>(kHandleKeyboardMsg), HandleKeyboardMsg_hk, hook_type_detour);
  g_orig = rcp->hooks->hook_map["chat_clipboard"]->original(HandleKeyboardMsg_hk);

  // Drag-select repair: detour the CEditWnd mouse handlers so a click-drag can
  // extend the selection even when Wine leaves CurrDraggedWindow unset.
  rcp->hooks->Add("chat_clip_lbd", static_cast<int>(kLButtonDown), LButtonDown_hk, hook_type_detour);
  g_lbd_orig = rcp->hooks->hook_map["chat_clip_lbd"]->original(LButtonDown_hk);
  rcp->hooks->Add("chat_clip_lbu", static_cast<int>(kLButtonUp), LButtonUp_hk, hook_type_detour);
  g_lbu_orig = rcp->hooks->hook_map["chat_clip_lbu"]->original(LButtonUp_hk);
  rcp->hooks->Add("chat_clip_mm", static_cast<int>(kMouseMove), MouseMove_hk, hook_type_detour);
  g_mm_orig = rcp->hooks->hook_map["chat_clip_mm"]->original(MouseMove_hk);

  logger::logf("[chat] clipboard copy/paste + drag-select installed (HandleKeyboardMsg @ 0x%x)",
               (unsigned)kHandleKeyboardMsg);
}

ChatClipboard::~ChatClipboard() {}
