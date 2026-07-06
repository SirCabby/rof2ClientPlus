// rof2ClientPlus - baseline entry point.
//
// Loaded into eqgame.exe by the Miles Sound System (mss32.dll), which
// LoadLibrary's every *.asi in the game root at sound init. DllMain pins the
// module (so we are never unloaded - our hook code lives here), then hands off
// to a worker thread that installs the render hook. No heavy work runs under
// the loader lock.
#include <windows.h>

#include "logger.h"
#include "directx.h"

static DWORD WINAPI bootstrap(LPVOID) {
    logger::log("bootstrap thread started");

    // The graphics stack (DXVK) may not be ready the instant sound inits, so
    // retry the render-hook install a few times.
    bool hooked = false;
    for (int attempt = 1; attempt <= 10 && !hooked; ++attempt) {
        Sleep(1500);
        logger::logf("render-hook install attempt %d", attempt);
        hooked = directx::install();
    }
    logger::logf("render hook installed = %s", hooked ? "true" : "false");

    MessageBoxA(NULL,
                hooked ? "rof2ClientPlus baseline loaded.\n\n"
                         "EndScene hook installed - look for the red bar\n"
                         "in the top-left once you are in the game."
                       : "rof2ClientPlus baseline loaded.\n\n"
                         "Render hook did NOT install - see rof2ClientPlus.log.",
                "rof2ClientPlus", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
    return 0;
}

static void on_attach() {
    // Pin ourselves: GET_MODULE_HANDLE_EX_FLAG_PIN makes the loader treat this
    // module as never-unloadable. Uses this function's own address to identify
    // the module.
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&on_attach), &pinned);

    logger::init("rof2ClientPlus.log");
    logger::logf("DllMain PROCESS_ATTACH: module=%p pid=%lu",
                 (void*)pinned, (unsigned long)GetCurrentProcessId());

    HANDLE h = CreateThread(NULL, 0, bootstrap, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        logger::log("ERROR: CreateThread(bootstrap) failed");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        on_attach();
    }
    return TRUE;
}
