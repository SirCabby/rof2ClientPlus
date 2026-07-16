// rof2ClientPlus - character-precise drag-select + copy for the chat HISTORY pane.
//
// The scrollback is a CStmlWnd (rich-text/STML display) with NO native text
// selection. This module builds it: detour the CStmlWnd mouse handlers to track
// a character-level selection, draw the highlight from a CStmlWnd::Draw hook, and
// hand the covered text to the clipboard on Ctrl+C. Hooks are installed lazily
// (the chat window only exists in-game), driven by on_frame().
//
// See PORTING_NOTES.md ("Chat history drag-select") for the full RE.
#pragma once

class RcpService;

class ChatStmlSelect {
 public:
  explicit ChatStmlSelect(RcpService *rcp);
  ~ChatStmlSelect();

  // Lazy-install the CStmlWnd hooks once the chat window exists. Cheap no-op
  // after the first success. Call every frame.
  void on_frame();
};

// Copy the current history selection to the OS clipboard. Returns true if there
// was a selection to copy (so the Ctrl+C key handler can consume the key).
bool chat_stml_copy_selection();
