// rof2ClientPlus - prepend a local-time timestamp to every chat line (Zeal-style).
//
// The way Zeal does it: detour the client's central chat-display function
// (CEverQuest::dsp_chat @ 0x51F1A0 - every displayed line, incoming and the
// client's own, funnels through it) and, when the feature is on, prepend a
// formatted local time to the text before the client renders it. The timestamp
// inherits the line's own color (it is one dsp_chat call), exactly like Zeal.
//
// The format is a strftime string; the default is Zeal's 24-hour "[%H:%M:%S] "
// (e.g. "[14:23:07] Soandso tells you, ...").
//
// '/timestamp on|off' toggles it, '/timestamp format <strftime>' sets the
// format; both persist in rof2ClientPlus.ini [Chat]. Off by default.
#pragma once

#include <string>

class RcpService;

class ChatTimestamp {
 public:
  explicit ChatTimestamp(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Accessors for the options-window UI (the Chat tab) and the /timestamp command.
namespace chat_timestamp_settings {
bool get_enabled();               // true = chat lines are prefixed with a timestamp.
void set_enabled(bool on);        // Applies live (next line) + persists to ini.
std::string get_format();         // The current strftime format string.
void set_format(const std::string &fmt);  // Sets + persists the format (empty is ignored).
}  // namespace chat_timestamp_settings
