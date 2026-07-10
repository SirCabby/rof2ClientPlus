#include "directx.h"
#include "crash_handler.h"
#include "hooks.h"
#include "logger.h"

#include <windows.h>
#include <d3d9.h>

#include <vector>

// IDirect3DDevice9 vtable index for EndScene (IUnknown = 0..2, then the
// IDirect3DDevice9 methods; EndScene lands at 42, Clear at 43).
static const int VTBL_ENDSCENE = 42;

typedef HRESULT(WINAPI* EndScene_t)(IDirect3DDevice9*);
static EndScene_t   g_original_endscene = nullptr;
static volatile LONG g_frame = 0;

// The live device from the most recent EndScene, and the render callbacks that draw
// into each frame. The callback list is only mutated at load (single-threaded) via
// add_render_callback and read on the render thread, so no lock is needed.
static IDirect3DDevice9* g_device = nullptr;
static HWND g_focus_hwnd = nullptr;  // The game's main render window (see get_focus_window).
static std::vector<std::function<void(IDirect3DDevice9*)>>* g_callbacks = nullptr;

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
        logger::logf("directx: device=%p vtable=%p original EndScene=%p",
                     (void*)dev, (void*)vtable, (void*)g_original_endscene);

        void* prev = hooks::swap_vtable_entry(vtable, VTBL_ENDSCENE,
                                              reinterpret_cast<void*>(&hkEndScene));
        ok = (prev != nullptr);
        logger::logf("directx: EndScene vtable swap %s", ok ? "OK" : "FAILED");
        if (!ok) g_original_endscene = nullptr;  // allow retry
        dev->Release();
    }

    // The patched vtable is static in d3d9.dll and survives teardown of the
    // throwaway device/window - the game's device will use it.
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    d3d->Release();
    return ok;
}
