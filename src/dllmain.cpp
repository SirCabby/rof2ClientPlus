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

#include "hook_wrapper.h"
#include "logger.h"
#include "mouse_mods.h"
#include "rcp.h"
#include "rcp_options_ui.h"

// stock RoF2 __ProcessGameEvents (eqlib). Runs on the main thread.
static const int kProcessGameEventsAddr = 0x53A6C0;

static HookWrapper *g_boot = nullptr;
static bool g_service_created = false;

// Fires on the main thread. Creates the service once, then chains to the game.
static int __cdecl ProcessGameEvents_hk() {
    if (!g_service_created) {
        g_service_created = true;
        logger::log("ProcessGameEvents first call (main thread); creating RcpService");
        RcpService::create();
        logger::log("RcpService created");
    }
    // Install the mouse hook once in-game (kept off the login/char-select input
    // path), then drive the options-window UI poll.
    if (RcpService *svc = RcpService::get_instance()) {
        if (svc->mouse_mods) svc->mouse_mods->ensure_hooked();
        if (svc->options_ui) svc->options_ui->on_frame();
    }
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

    g_boot = new HookWrapper();
    g_boot->Add("ProcessGameEvents", kProcessGameEventsAddr, ProcessGameEvents_hk, hook_type_detour);
    logger::logf("boot hook installed on ProcessGameEvents (0x%X)", kProcessGameEventsAddr);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        on_attach();
    }
    return TRUE;
}
