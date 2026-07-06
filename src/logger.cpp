#include "logger.h"

#include <windows.h>
#include <cstdio>
#include <cstdarg>

static std::string      g_path;
static CRITICAL_SECTION g_cs;
static bool             g_ready = false;

// Full path to the directory that contains the running eqgame.exe.
static std::string game_dir() {
    char buf[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, buf, MAX_PATH);   // NULL => the host process (eqgame.exe)
    std::string p(buf);
    size_t slash = p.find_last_of("\\/");
    return (slash == std::string::npos) ? std::string() : p.substr(0, slash + 1);
}

void logger::init(const char* filename) {
    g_path = game_dir() + filename;
    InitializeCriticalSection(&g_cs);
    g_ready = true;
    FILE* f = fopen(g_path.c_str(), "w");      // truncate/create
    if (f) fclose(f);
}

static void write_line(const char* line) {
    if (!g_ready) return;
    EnterCriticalSection(&g_cs);
    FILE* f = fopen(g_path.c_str(), "a");
    if (f) {
        fputs(line, f);
        fputc('\n', f);
        fclose(f);
    }
    LeaveCriticalSection(&g_cs);
}

void logger::log(const std::string& msg) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1200];
    snprintf(line, sizeof(line), "[%02d:%02d:%02d.%03d] %s",
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg.c_str());
    write_line(line);
}

void logger::logf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logger::log(buf);
}
