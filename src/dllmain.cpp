// rof2ClientPlus - entry point.
//
// Loaded into eqgame.exe by the Miles Sound System (mss32.dll), which
// LoadLibrary's every *.asi in the game root at sound init.
//
// The service MUST be created on the game's MAIN THREAD at a safe point, not on
// a background thread: our detours can fire on the main thread the instant they
// are installed, and if the service is still being constructed on another thread
// the handler would touch a half-built object and crash. So we install one boot
// detour on ProcessGameEvents (the main-loop event processor, stock RoF2
// 0x53A6C0) and create the service on its first call - while the main thread
// sits inside our constructor it cannot also be running the loop.
#include <windows.h>

#include "aa_exp.h"
#include "chat_stml_select.h"
#include "crash_handler.h"
#include "directx.h"
#include "hook_wrapper.h"
#include "keybinds.h"
#include "logger.h"
#include "model_swap.h"
#include "mouse_mods.h"
#include "npc_model_swap.h"
#include "rcp.h"
#include "rcp_options_ui.h"
#include "window_watch.h"

// stock RoF2 __ProcessGameEvents (eqlib). Runs on the main thread.
static const int kProcessGameEventsAddr = 0x53A6C0;

static HookWrapper *g_boot = nullptr;

// May fire from more than one thread. Create the service exactly once with a
// thread-safe magic-static: the first thread runs the initializer while every
// other thread BLOCKS here until it finishes, so no thread can ever observe (or
// race to build) a half-constructed service - the previous non-atomic
// g_service_created + create() guard let a second thread build a duplicate
// RcpService, so on_frame polled a different (empty) options-window instance
// than the one /rcpoptions built.
static int __cdecl ProcessGameEvents_hk() {
    static const bool created = []() {
        logger::logf("ProcessGameEvents first call (tid=%lu); creating RcpService", GetCurrentThreadId());
        RcpService::create();
        logger::log("RcpService created");
        // N4a: activate the (previously dormant) D3D9 EndScene hook now that we are
        // in-game on the main thread and d3d9.dll (DXVK) is loaded. This is the
        // render seam the custom-font / nameplate-bars overhaul builds on; it only
        // logs a proof-of-life marker today. Offset-independent (throwaway-device
        // vtable swap), so it is safe to arm before any game-struct work.
        logger::logf("directx::install() -> %s", directx::install() ? "OK" : "FAILED");
        return true;
    }();
    (void)created;

    // Once the client is tearing down, do none of our per-frame work (it would
    // touch game/UI state that is being freed) - just chain to the stock event
    // processor so the exit sequence runs normally.
    if (crash_handler::shutting_down())
        return g_boot->hook_map["ProcessGameEvents"]->original(ProcessGameEvents_hk)();

    // Install the mouse hook once in-game (kept off the login/char-select input
    // path), then drive the options-window UI poll. The cursor-lock re-clip runs
    // every frame regardless of game state (it is a pure Win32 no-op when off), so
    // the confine holds at login/char-select too.
    if (RcpService *svc = RcpService::get_instance()) {
        if (svc->mouse_mods) svc->mouse_mods->ensure_hooked();
        if (svc->options_ui) svc->options_ui->on_frame();
        if (svc->keybinds_mod) svc->keybinds_mod->on_frame();
        if (svc->chat_stml_select) svc->chat_stml_select->on_frame();
    }
    // Window diagnostics + opt-in self-heal run every frame regardless of service
    // state (they only need the main window, which exists by the time we get here).
    window_watch::on_frame();
    // Auto-AA-experience enforcement (self-gating: no-op unless enabled + in-game;
    // its own throttle + rcp_guard). Namespace fn like window_watch, driven here.
    aa_exp::on_frame();
    mouse_settings::apply_cursor_lock();
    return g_boot->hook_map["ProcessGameEvents"]->original(ProcessGameEvents_hk)();
}

static void on_attach() {
    // Pin so we are never unloaded (our hook code must stay mapped).
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&on_attach), &pinned);

    logger::init("rof2ClientPlus.log");
    logger::logf("DllMain PROCESS_ATTACH: module=%p pid=%lu", (void *)pinned,
                 (unsigned long)GetCurrentProcessId());

    // Install the crash handler FIRST so even faults during early init / the very
    // first detour are captured, then the window diagnostics (env + ini + leftover-
    // sibling snapshot). Both are independent of the RcpService, which isn't built
    // until the first ProcessGameEvents call.
    crash_handler::install();
    window_watch::install_early();

    g_boot = new HookWrapper();
    g_boot->Add("ProcessGameEvents", kProcessGameEventsAddr, ProcessGameEvents_hk, hook_type_detour);
    logger::logf("boot hook installed on ProcessGameEvents (0x%X)", kProcessGameEventsAddr);

    // Model-swap seams must be armed BEFORE client init: the char-select scene (clz world + the
    // preview player, weapons included) is built before the first ProcessGameEvents tick, so
    // ctor-time hooks miss it and char select ignores the model settings. Both load their ini
    // state themselves and only touch facilities installed above (logger/crash guard).
    model_swap_api::install_early(g_boot);
    npc_model_swap_api::install_early(g_boot);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        on_attach();
    }
    return TRUE;
}
