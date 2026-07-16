// rof2ClientPlus - clipboard copy/paste in chat and every edit box.
//
// Always-on Zeal-style "ZealInput" copy/paste, ported to stock RoF2. RoF2 can
// PASTE natively (it imports GetClipboardData) but the client was compiled
// WITHOUT SetClipboardData/EmptyClipboard, so it physically cannot COPY to the
// OS clipboard; and its native paste is a keymap command (CMD_CLIPBOARD_PASTE)
// that the focused-window routing never lets fire while you are typing in chat.
// This module intercepts the raw Ctrl+C / Ctrl+X / Ctrl+V keystrokes at the UI
// keyboard choke point and does the clipboard I/O itself, so both directions
// work in the chat input line and any other edit field.
//
// No command, no options tab - copy/paste is universally-expected behaviour.
// See PORTING_NOTES.md (Chat clipboard) for the full RE.
#pragma once

class RcpService;

class ChatClipboard {
 public:
  explicit ChatClipboard(RcpService *rcp);
  ~ChatClipboard();
};
