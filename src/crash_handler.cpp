// rof2ClientPlus - crash diagnostics + detour guard implementation. See crash_handler.h.
#include "crash_handler.h"

#include <windows.h>
// dbghelp + psapi must follow windows.h.
#include <dbghelp.h>
#include <psapi.h>

#include <csetjmp>
#include <cstdint>
#include <cstdio>

#include "logger.h"

namespace rcp_guard {
thread_local Frame *g_top = nullptr;
static bool g_enabled = true;  // Safety net defaults on; ini/command can disable.
bool enabled() { return g_enabled; }
void set_enabled(bool v) { g_enabled = v; }
}  // namespace rcp_guard

namespace {

// Base address of our own module, so a dump can flag "the fault is in OUR code".
HMODULE g_self_module = nullptr;
bool g_installed = false;

// Resolves an address to "module.dll+0xoffset (0xADDRESS)". Falls back to the bare
// address when no owning module is found (JIT'd/thunk memory). Never allocates.
void format_addr(const void *addr, char *out, size_t n) {
  HMODULE mod = nullptr;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(addr), &mod) &&
      mod) {
    char name[MAX_PATH] = {0};
    if (!GetModuleBaseNameA(GetCurrentProcess(), mod, name, sizeof(name))) {
      // psapi missed it; try the file name off the module handle.
      GetModuleFileNameA(mod, name, sizeof(name));
    }
    const uintptr_t off = reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(mod);
    const char *me = (mod == g_self_module) ? " <-- rof2ClientPlus" : "";
    snprintf(out, n, "%s+0x%X (0x%p)%s", name[0] ? name : "?", (unsigned)off, addr, me);
  } else {
    snprintf(out, n, "0x%p (no module)", addr);
  }
}

const char *exception_name(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE";
    default: return "OTHER";
  }
}

void log_registers(const CONTEXT *c) {
  logger::logf("  EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX", c->Eax, c->Ebx, c->Ecx, c->Edx);
  logger::logf("  ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX", c->Esi, c->Edi, c->Ebp, c->Esp);
  logger::logf("  EIP=%08lX EFLAGS=%08lX CS=%04lX DS=%04lX SS=%04lX", c->Eip, c->EFlags, c->SegCs, c->SegDs, c->SegSs);
}

// 16 instruction bytes at EIP - enough to pin the faulting op in the disassembly.
void log_code_bytes(const void *eip) {
  if (IsBadReadPtr(eip, 16)) {
    logger::log("  bytes@EIP: <unreadable>");
    return;
  }
  const unsigned char *p = static_cast<const unsigned char *>(eip);
  logger::logf("  bytes@EIP: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", p[0],
               p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}

// Walks the call stack with dbghelp; returns the number of frames logged.
int log_stack_dbghelp(const CONTEXT *ctx) {
  HANDLE proc = GetCurrentProcess();
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
  if (!SymInitialize(proc, nullptr, TRUE)) return 0;

  CONTEXT c = *ctx;  // StackWalk64 mutates the context; keep the original intact.
  STACKFRAME64 sf = {};
  sf.AddrPC.Offset = c.Eip;
  sf.AddrPC.Mode = AddrModeFlat;
  sf.AddrFrame.Offset = c.Ebp;
  sf.AddrFrame.Mode = AddrModeFlat;
  sf.AddrStack.Offset = c.Esp;
  sf.AddrStack.Mode = AddrModeFlat;

  int frames = 0;
  for (int i = 0; i < 40; ++i) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_I386, proc, GetCurrentThread(), &sf, &c, nullptr, SymFunctionTableAccess64,
                     SymGetModuleBase64, nullptr))
      break;
    if (sf.AddrPC.Offset == 0) break;
    char loc[256];
    format_addr(reinterpret_cast<void *>(static_cast<uintptr_t>(sf.AddrPC.Offset)), loc, sizeof(loc));
    logger::logf("  #%02d %s", frames, loc);
    ++frames;
  }
  SymCleanup(proc);
  return frames;
}

// Fallback frame-pointer walk for when dbghelp yields nothing (RoF2 is a release
// build with frame pointers, so the EBP chain is usually intact).
void log_stack_ebp(const CONTEXT *ctx) {
  logger::log("  (EBP-chain fallback)");
  uintptr_t *ebp = reinterpret_cast<uintptr_t *>(ctx->Ebp);
  for (int i = 0; i < 40; ++i) {
    if (!ebp || IsBadReadPtr(ebp, 2 * sizeof(uintptr_t))) break;
    const uintptr_t ret = ebp[1];
    if (!ret) break;
    char loc[256];
    format_addr(reinterpret_cast<void *>(ret), loc, sizeof(loc));
    logger::logf("  #%02d %s", i, loc);
    uintptr_t *next = reinterpret_cast<uintptr_t *>(ebp[0]);
    if (next <= ebp) break;  // Stack grows down; a non-increasing frame ptr means we're done.
    ebp = next;
  }
}

void dump_crash(EXCEPTION_POINTERS *ep, const char *origin) {
  static thread_local int in_dump = 0;  // Never let the dump recurse on its own fault.
  if (in_dump) return;
  in_dump = 1;

  const EXCEPTION_RECORD *er = ep->ExceptionRecord;
  const CONTEXT *ctx = ep->ContextRecord;

  logger::log("==================== rof2ClientPlus CRASH ====================");
  char at[256];
  format_addr(er->ExceptionAddress, at, sizeof(at));
  logger::logf("  %s exception %s (0x%08lX) at %s", origin, exception_name(er->ExceptionCode),
               (unsigned long)er->ExceptionCode, at);
  if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
    const char *op = er->ExceptionInformation[0] == 0 ? "read" : er->ExceptionInformation[0] == 1 ? "write" : "execute";
    logger::logf("  access violation: %s of 0x%p", op, reinterpret_cast<void *>(er->ExceptionInformation[1]));
  }
  log_registers(ctx);
  log_code_bytes(er->ExceptionAddress);
  logger::log("  --- stack ---");
  if (log_stack_dbghelp(ctx) < 2) log_stack_ebp(ctx);
  logger::log("=============================================================");

  in_dump = 0;
}

// Vectored handler: FIRST job is guard recovery for our own detours, which must
// happen before the game's frame-based SEH runs. It deliberately does NOT dump or
// handle anything else - returning CONTINUE_SEARCH leaves the game's own __try/
// __except and, ultimately, the unhandled filter below entirely undisturbed.
LONG CALLBACK vectored_handler(EXCEPTION_POINTERS *ep) {
  const DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (rcp_guard::g_top && code == EXCEPTION_ACCESS_VIOLATION) {
    rcp_guard::Frame *f = rcp_guard::g_top;
    char at[256];
    format_addr(ep->ExceptionRecord->ExceptionAddress, at, sizeof(at));
    logger::logf("[guard] swallowed ACCESS_VIOLATION in '%s' at %s - feature body skipped this frame",
                 f->name ? f->name : "?", at);
    std::longjmp(f->buf, 1);  // Back into rcp_guard::run(); does not return.
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

// Runs only when nothing handled the fault, i.e. a genuine crash. We log the full
// post-mortem, then CONTINUE_SEARCH so Wine's own crash reporting/termination still
// happens (behavior is unchanged except that we captured the dump first).
LONG WINAPI unhandled_filter(EXCEPTION_POINTERS *ep) {
  dump_crash(ep, "UNHANDLED");
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

namespace crash_handler {
void install() {
  if (g_installed) return;
  g_installed = true;

  // Identify our own module by an address we own, so dumps can flag our frames.
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(&install), &g_self_module);

  AddVectoredExceptionHandler(1, vectored_handler);  // 1 = first in the chain (guard recovery).
  SetUnhandledExceptionFilter(unhandled_filter);
  logger::logf("[crash] handlers installed (self module base=%p)", (void *)g_self_module);
}
}  // namespace crash_handler
