// rof2ClientPlus - make the Options -> General -> Copy Layout button copy CHAT
// settings too (per-window filters, window names, hit modes, fonts, channels).
//
// Stock RoF2's CopyLayout(@0x55BE50) copies window geometry (and optionally
// hotbuttons/loadouts/socials - flags its UI never exposes) but has NO chat
// parameter at all: the [ChatManager] section of UI_<char>_<server>.ini
// (ChannelMap0..56 = message-category -> window routing, HitMode0..7,
// ChatWindowN_Name/_DefaultChannel/_FontStyle/...) is never transferred, so a
// copied layout arrives with default filters and "Chat 2"-style names.
//
// How this works (all paths verified in the May 10 2013 disasm):
//   The normal Copy Layout dialog calls CopyLayout with bForceReload=0
//   (@0x5FD71C): positions are applied but chat is never rebuilt. Only a full
//   UI reload (CDisplay reload wrapper @0x49D990 -> ReloadUI @0x49D6B0, used
//   by the bForceReload=1 resolution-mismatch path @0x55C9DF) re-runs the chat
//   loader (@0x654C80), which re-CREATES every chat window from [ChatManager]
//   via CreateChatWindow @0x6549B0. The reload saves UI state first
//   (SaveGameUI @0x4891B0 -> chat-settings saver @0x64FE40).
// So we detour CopyLayout: BEFORE the original runs we merge the source ini's
// [ChatManager] section into the character's UI ini - written through the
// client's OWN cached ini object (*(void**)0xE67CCC) with its native WriteInt
// @0x543540 / WriteString @0x543670, so we stay cache-coherent - we one-shot
// suppress the chat-settings saver (a second detour @0x64FE40) so any
// save-first step can't clobber the merge with live state, and if the
// original didn't reload (force=0, the normal case) we invoke the client's
// own reload wrapper ourselves. The client's own loader then rebuilds the
// chat UI to the merged spec, including creating windows the target character
// didn't have. Copying your own layout is skipped (source ==
// UI_<self>_<server>.ini would revert unsaved state).
//
// '/rcpcopylayout on|off' toggles it (persisted in rof2ClientPlus.ini [UI]
// CopyLayoutChat). On by default.
#pragma once

class RcpService;

class CopyLayoutChat {
 public:
  explicit CopyLayoutChat(RcpService *rcp);

 private:
  RcpService *rcp_;
};

namespace copy_layout_chat_settings {
bool get_enabled();
void set_enabled(bool on);  // Applies live (next Copy Layout) + persists to ini.
}  // namespace copy_layout_chat_settings
