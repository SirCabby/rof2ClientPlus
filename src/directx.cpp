#include "directx.h"
#include "crash_handler.h"
#include "hooks.h"
#include "logger.h"
#include "rebase.h"  // eqva() for the game-device locator (ensure_game_device)

#include <windows.h>
#include <d3d9.h>

#include <vector>

// IDirect3DDevice9 vtable indices (IUnknown = 0..2, then the IDirect3DDevice9
// methods; Reset lands at 16, BeginScene at 41, EndScene at 42, Clear at 43).
static const int VTBL_RESET = 16;
static const int VTBL_BEGINSCENE = 41;
static const int VTBL_ENDSCENE = 42;

typedef HRESULT(WINAPI* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT(WINAPI* BeginScene_t)(IDirect3DDevice9*);
typedef HRESULT(WINAPI* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static EndScene_t   g_original_endscene = nullptr;
static BeginScene_t g_original_beginscene = nullptr;
static Reset_t      g_original_reset = nullptr;
static volatile LONG g_frame = 0;

// The live device from the most recent EndScene, and the render callbacks that draw
// into each frame. The callback lists are only mutated at load (single-threaded) via
// add_render_callback / add_prerender_callback and read on the render thread, so no
// lock is needed.
static IDirect3DDevice9* g_device = nullptr;
static HWND g_focus_hwnd = nullptr;  // The game's main render window (see get_focus_window).
static std::vector<std::function<void(IDirect3DDevice9*)>>* g_callbacks = nullptr;
static std::vector<std::function<void(IDirect3DDevice9*)>>* g_pre_callbacks = nullptr;
static std::vector<std::function<void(IDirect3DDevice9*)>>* g_reset_callbacks = nullptr;

// The vtable our hooks are confirmed to own for the GAME's device (set by
// ensure_game_device). Null until the real device has been seen once.
static void** g_game_vtable = nullptr;
// install()'s dummy-device patch bookkeeping, so ensure_game_device can revert it
// when the game's device turns out to use a DIFFERENT vtable (native Windows):
// leaving hooks on a foreign device class's vtable would chain them into the
// wrong class's methods if anything in-process ever created such a device.
static void** g_dummy_vtable = nullptr;
static void* g_dummy_prev_es = nullptr;
static void* g_dummy_prev_bs = nullptr;
static void* g_dummy_prev_rs = nullptr;

// Game-side locator for the real device: eqlib pinstRenderInterface ->
// CRender::pD3DDevice. The same pair the nameplate pre-UI seam reads
// (font_overlay.cpp), disasm-verified on the May-2013 build.
static const uintptr_t kRenderInterfacePtr = ::Rcp::eqva(0x15D46A4);
static const int kRenderDeviceOffset = 0xF08;

IDirect3DDevice9* directx::get_device() { return g_device; }
void* directx::get_focus_window() { return g_focus_hwnd; }

// The game window from the device itself: creation params carry hFocusWindow; if that
// is null (some windowed devices) fall back to swapchain 0's device window. Both are
// pure reads of stored params - safe to call on the render thread inside EndScene.
static HWND query_focus_window(IDirect3DDevice9* dev) {
    D3DDEVICE_CREATION_PARAMETERS cp = {};
    if (SUCCEEDED(dev->GetCreationParameters(&cp)) && cp.hFocusWindow) return cp.hFocusWindow;
    IDirect3DSwapChain9* sc = nullptr;
    HWND h = nullptr;
    if (SUCCEEDED(dev->GetSwapChain(0, &sc)) && sc) {
        D3DPRESENT_PARAMETERS pp = {};
        if (SUCCEEDED(sc->GetPresentParameters(&pp))) h = pp.hDeviceWindow;
        sc->Release();
    }
    return h;
}

void directx::add_render_callback(std::function<void(IDirect3DDevice9*)> callback) {
    if (!g_callbacks) g_callbacks = new std::vector<std::function<void(IDirect3DDevice9*)>>();
    g_callbacks->push_back(std::move(callback));
}

void directx::add_prerender_callback(std::function<void(IDirect3DDevice9*)> callback) {
    if (!g_pre_callbacks) g_pre_callbacks = new std::vector<std::function<void(IDirect3DDevice9*)>>();
    g_pre_callbacks->push_back(std::move(callback));
}

void directx::add_reset_callback(std::function<void(IDirect3DDevice9*)> callback) {
    if (!g_reset_callbacks) g_reset_callbacks = new std::vector<std::function<void(IDirect3DDevice9*)>>();
    g_reset_callbacks->push_back(std::move(callback));
}

// Our replacement Reset: the client resets the device on window resize / display-mode
// change. D3D9 fails Reset while ANY D3DPOOL_DEFAULT resource is alive (DXVK enforces
// this), and EQGraphicsDX9's CRender::ResetDevice treats a failed Reset as fatal
// ("ResetDevice() failed!"). So first give every registered holder the chance to
// release its default-pool resources, then chain. Runs on the render thread; the
// device interface pointer survives Reset, so callers recreate lazily on later draws.
static HRESULT WINAPI hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    if (crash_handler::shutting_down()) return g_original_reset(dev, pp);
    logger::logf("directx: device Reset (backbuffer %ux%u), releasing default-pool resources",
                 pp ? (unsigned)pp->BackBufferWidth : 0, pp ? (unsigned)pp->BackBufferHeight : 0);
    if (g_reset_callbacks) {
        for (auto& cb : *g_reset_callbacks)
            rcp_guard::run("directx.reset_cb", [&] { cb(dev); });
    }
    HRESULT hr = g_original_reset(dev, pp);
    logger::logf("directx: device Reset -> 0x%08lX%s", (unsigned long)hr,
                 SUCCEEDED(hr) ? "" : " (FAILED - client will likely abort)");
    return hr;
}

// Our replacement BeginScene: run the pre-render callbacks (post-sim, pre-draw
// state fixups), then chain. Same guard rules as hkEndScene.
static HRESULT WINAPI hkBeginScene(IDirect3DDevice9* dev) {
    if (crash_handler::shutting_down()) return g_original_beginscene(dev);
    if (g_pre_callbacks) {
        for (auto& cb : *g_pre_callbacks)
            rcp_guard::run("directx.prerender_cb", [&] { cb(dev); });
    }
    return g_original_beginscene(dev);
}

// Our replacement EndScene: capture the live device, run any registered render
// callbacks (each guarded so a bad pointer degrades to a dropped frame instead of a
// crash), then chain to the real EndScene. Runs on the game's render thread.
static HRESULT WINAPI hkEndScene(IDirect3DDevice9* dev) {
    // During teardown, present the frame but run none of our render callbacks -
    // the device/game structures they read are being released.
    if (crash_handler::shutting_down()) return g_original_endscene(dev);

    g_device = dev;
    if (!g_focus_hwnd) {  // Capture the game window once, off the device itself.
        g_focus_hwnd = query_focus_window(dev);
        if (g_focus_hwnd) logger::logf("directx: game focus window = %p", (void*)g_focus_hwnd);
    }
    LONG frame = InterlockedIncrement(&g_frame);
    if (frame == 1 || (frame % 300) == 0)
        logger::logf("EndScene hook fired, frame %ld", frame);

    if (g_callbacks) {
        for (auto& cb : *g_callbacks)
            rcp_guard::run("directx.render_cb", [&] { cb(dev); });
    }

    return g_original_endscene(dev);
}

bool directx::install() {
    if (g_original_endscene) return true;  // already hooked

    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    if (!d3d9) { logger::log("directx: LoadLibrary(d3d9.dll) failed"); return false; }

    typedef IDirect3D9*(WINAPI * Direct3DCreate9_t)(UINT);
    auto pDirect3DCreate9 =
        reinterpret_cast<Direct3DCreate9_t>(GetProcAddress(d3d9, "Direct3DCreate9"));
    if (!pDirect3DCreate9) { logger::log("directx: no Direct3DCreate9 export"); return false; }

    IDirect3D9* d3d = pDirect3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { logger::log("directx: Direct3DCreate9 returned null"); return false; }

    // Hidden window to host the throwaway device.
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "rof2cpDummyWnd";
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "rof2cp", WS_OVERLAPPEDWINDOW,
                                0, 0, 64, 64, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) {
        logger::log("directx: CreateWindow failed");
        d3d->Release();
        return false;
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    bool ok = false;
    if (FAILED(hr) || !dev) {
        logger::logf("directx: CreateDevice failed hr=0x%08lX", (unsigned long)hr);
    } else {
        // First member of a COM object is its vtable pointer.
        void** vtable = *reinterpret_cast<void***>(dev);
        g_original_endscene = reinterpret_cast<EndScene_t>(vtable[VTBL_ENDSCENE]);
        g_original_beginscene = reinterpret_cast<BeginScene_t>(vtable[VTBL_BEGINSCENE]);
        logger::logf("directx: device=%p vtable=%p original EndScene=%p BeginScene=%p",
                     (void*)dev, (void*)vtable, (void*)g_original_endscene,
                     (void*)g_original_beginscene);

        void* prev = hooks::swap_vtable_entry(vtable, VTBL_ENDSCENE,
                                              reinterpret_cast<void*>(&hkEndScene));
        ok = (prev != nullptr);
        logger::logf("directx: EndScene vtable swap %s", ok ? "OK" : "FAILED");
        if (!ok) g_original_endscene = nullptr;  // allow retry
        void* prev_bs = hooks::swap_vtable_entry(vtable, VTBL_BEGINSCENE,
                                                 reinterpret_cast<void*>(&hkBeginScene));
        logger::logf("directx: BeginScene vtable swap %s", prev_bs ? "OK" : "FAILED");
        if (!prev_bs) g_original_beginscene = nullptr;
        g_original_reset = reinterpret_cast<Reset_t>(vtable[VTBL_RESET]);
        void* prev_rs = hooks::swap_vtable_entry(vtable, VTBL_RESET,
                                                 reinterpret_cast<void*>(&hkReset));
        logger::logf("directx: Reset vtable swap %s", prev_rs ? "OK" : "FAILED");
        if (!prev_rs) g_original_reset = nullptr;
        // Remember what we patched so ensure_game_device can revert it if the game's
        // device turns out to live on a different vtable (native Windows).
        if (ok) {
            g_dummy_vtable = vtable;
            g_dummy_prev_es = prev;
            g_dummy_prev_bs = prev_bs;
            g_dummy_prev_rs = prev_rs;
        }
        dev->Release();
    }

    // The patched vtable is static in d3d9.dll and survives teardown of the
    // throwaway device/window. On shared-vtable runtimes (Wine/DXVK) the game's
    // device uses this same vtable, so the seam is live from here; on native
    // Windows the game's device class has its own vtable and ensure_game_device
    // finishes the job.
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    d3d->Release();
    return ok;
}

// Native-Windows completion of the seam (called once per ProcessGameEvents tick).
// Windows' d3d9.dll keeps separate static vtables per device class, so the game's
// device - created by EQGraphicsDX9 with different flags than install()'s dummy -
// can bypass the dummy-patched vtable entirely: EndScene/BeginScene/Reset never
// fire and every render-callback feature (fog removal, view distance, billboard
// nameplates, ...) silently starves while game-side detours keep working. Wine/
// DXVK shares one vtable across devices, which masked all of this.
//
// Resolve the REAL device through the client (pinstRenderInterface ->
// CRender::pD3DDevice) and make sure ITS vtable carries our hooks. Under Wine the
// first call just records the already-patched vtable and it is a no-op from then
// on. Runs on the main thread, which is also the thread that calls EndScene, so
// re-pointing the g_original_* chain here cannot race a hooked call.
void directx::ensure_game_device() {
    if (crash_handler::shutting_down()) return;
    rcp_guard::run("directx.ensure_game_device", [] {
        void* pRender = *reinterpret_cast<void**>(kRenderInterfacePtr);
        if (!pRender) return;  // graphics engine not up yet - retry next tick
        IDirect3DDevice9* dev = *reinterpret_cast<IDirect3DDevice9**>(
            reinterpret_cast<char*>(pRender) + kRenderDeviceOffset);
        if (!dev) return;
        void** vt = *reinterpret_cast<void***>(dev);
        if (!vt || vt == g_game_vtable) return;  // already confirmed (the steady state)

        if (vt[VTBL_ENDSCENE] == reinterpret_cast<void*>(&hkEndScene)) {
            // Shared-vtable runtime (Wine/DXVK): the dummy patch already covers the game.
            g_game_vtable = vt;
            logger::logf("directx: game device %p uses the patched vtable %p (shared-vtable runtime)",
                         (void*)dev, (void*)vt);
            return;
        }

        // Distinct vtable: the dummy patch never reached the game (native Windows).
        // Undo it first so no foreign-class vtable keeps chaining into the originals
        // we are about to re-point, then hook the game's vtable directly.
        if (g_dummy_vtable && g_dummy_vtable != vt) {
            if (g_dummy_prev_es) hooks::swap_vtable_entry(g_dummy_vtable, VTBL_ENDSCENE, g_dummy_prev_es);
            if (g_dummy_prev_bs) hooks::swap_vtable_entry(g_dummy_vtable, VTBL_BEGINSCENE, g_dummy_prev_bs);
            if (g_dummy_prev_rs) hooks::swap_vtable_entry(g_dummy_vtable, VTBL_RESET, g_dummy_prev_rs);
            logger::logf("directx: dummy-device vtable %p patch reverted (game vtable differs)",
                         (void*)g_dummy_vtable);
            g_dummy_vtable = nullptr;
        }

        g_original_endscene = reinterpret_cast<EndScene_t>(vt[VTBL_ENDSCENE]);
        g_original_beginscene = reinterpret_cast<BeginScene_t>(vt[VTBL_BEGINSCENE]);
        g_original_reset = reinterpret_cast<Reset_t>(vt[VTBL_RESET]);
        void* prev_es = hooks::swap_vtable_entry(vt, VTBL_ENDSCENE, reinterpret_cast<void*>(&hkEndScene));
        if (!prev_es) {
            // VirtualProtect refused - leave g_game_vtable null so the next tick retries.
            logger::logf("directx: game vtable %p EndScene swap FAILED", (void*)vt);
            g_original_endscene = nullptr;
            return;
        }
        void* prev_bs = hooks::swap_vtable_entry(vt, VTBL_BEGINSCENE, reinterpret_cast<void*>(&hkBeginScene));
        if (!prev_bs) g_original_beginscene = nullptr;
        void* prev_rs = hooks::swap_vtable_entry(vt, VTBL_RESET, reinterpret_cast<void*>(&hkReset));
        if (!prev_rs) g_original_reset = nullptr;
        g_game_vtable = vt;
        logger::logf("directx: game device %p vtable %p hooked directly (EndScene%s%s)",
                     (void*)dev, (void*)vt, prev_bs ? "/BeginScene" : "", prev_rs ? "/Reset" : "");
    });
}
