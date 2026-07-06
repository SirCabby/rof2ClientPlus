// rof2ClientPlus - tiny thread-safe file logger.
// Writes next to eqgame.exe (the game root) so output is easy to find.
#pragma once
#include <string>

namespace logger {
// Initialize the log file. `filename` is relative to the game root directory
// (where eqgame.exe lives). Truncates any existing file.
void init(const char* filename);

// Append a timestamped line.
void log(const std::string& msg);
void logf(const char* fmt, ...);
}
