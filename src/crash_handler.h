// rof2ClientPlus - crash diagnostics + a MinGW-friendly detour guard.
//
// Two things live here, both aimed at the "sometimes the client crashes" report:
//
//  1) crash_handler::install() wires a top-level unhandled-exception filter (and a
//     vectored handler) that, on a fatal fault, writes an exact post-mortem to
//     rof2ClientPlus.log: the exception code, the faulting address resolved to
//     MODULE+offset (so we instantly learn whether the fault is in eqgame.exe, our
//     rof2ClientPlus.asi, DXVK's d3d9.dll, eqgfx*, ...), the CPU registers, the
//     instruction bytes at EIP, and a stack backtrace (dbghelp StackWalk64 with an
//     EBP-chain fallback). The logger flushes every line, so the dump survives the
//     process dying immediately after.
//
//  2) rcp_guard::run() contains our own per-frame detour bodies so a bad pointer in
//     OUR code degrades to a logged, swallowed frame instead of taking the whole
//     client down. GCC/mingw does NOT implement MSVC's __try/__except, so this is
//     the equivalent built from setjmp + the vectored handler: while a guard frame
//     is armed on this thread, an access violation long-jumps back out of the guard
//     rather than crashing. Guarded bodies must be POD-only (no locals with
//     non-trivial destructors between the setjmp and a possible fault) - the long
//     jump abandons the frame without unwinding.
#pragma once
#include <csetjmp>
#include <utility>

namespace crash_handler {
// Installs the unhandled-exception filter + vectored handler. Idempotent; call as
// early as possible (dllmain on_attach) so even first-frame faults are captured.
void install();

// Marks the client as shutting down (called from the window subclass on WM_CLOSE /
// WM_DESTROY). Two jobs, both aimed at the "/exit sometimes hangs or crashes":
//   1) Removes OUR exception handlers so Wine's own teardown (incl. the DXVK device
//      release) proceeds exactly as stock. The post-mortem MUST NOT run during
//      loader-locked process teardown - its dbghelp StackWalk64 deadlocks there,
//      which is the hang. Restoring the default filter avoids that entirely.
//   2) Flips shutting_down() so every per-frame / per-render detour body bails out
//      instead of touching game state that is being freed.
// Idempotent and safe from any thread; only the first call does the work.
void begin_shutdown();

// True once begin_shutdown() has run. Per-frame/per-render bodies check this and
// early-out (drop the frame, chain to the original) so we never read freed state
// during teardown. A relaxed atomic load - cheap enough for a hot path.
bool shutting_down();
}  // namespace crash_handler

namespace rcp_guard {
// One armed guard scope on the call stack. `buf` is the setjmp target the vectored
// handler long-jumps to; `prev` chains nested guards so an inner recovery restores
// the outer scope. Lives on the stack of run() - never heap/thread_local itself.
struct Frame {
  std::jmp_buf buf;
  Frame *prev;
  const char *name;
};

// Head of the per-thread armed-guard stack. The vectored handler reads THIS thread's
// value, so guards on other threads (e.g. the DInput poll thread) are independent.
extern thread_local Frame *g_top;

bool enabled();          // Master switch (ini [Window] GuardDetours, default on).
void set_enabled(bool);  // Live toggle from /rcpwindow guard on|off.

// Runs fn() with a fault net: if fn() (our detour body) hits an access violation, the
// vectored handler logs MODULE+offset and long-jumps here, and run() returns false
// with the body abandoned. Returns true when fn() completed normally. When guarding
// is disabled this is a straight call with zero overhead. fn must be POD-bodied.
template <class F>
inline bool run(const char *name, F &&fn) {
  if (!enabled()) {
    fn();
    return true;
  }
  Frame frame;
  frame.name = name;
  frame.prev = g_top;
  // setjmp returns 0 on the initial pass and (via the handler's longjmp) 1 on a fault.
  if (setjmp(frame.buf) == 0) {
    g_top = &frame;
    fn();
    g_top = frame.prev;
    return true;
  }
  // Fault path: g_top still points at our frame (the handler does not touch it).
  if (g_top) g_top = g_top->prev;
  return false;
}
}  // namespace rcp_guard
