// rof2ClientPlus - Direct3D9 render-loop hook.
//
// Creates a throwaway D3D9 device to read the (shared) IDirect3DDevice9 vtable,
// then swaps the EndScene entry. Because every D3D9 device from the same
// d3d9.dll (DXVK here) shares one vtable, the game's device then calls our
// EndScene. RoF2 renders through EQGraphicsDX9.dll + d3dx9_30.dll, so this is the
// correct D3D level (see PORTING_NOTES.md "N4"). Registered render callbacks are
// invoked each EndScene with the live device so features (e.g. custom-font
// nameplates) can draw into the current frame.
#pragma once

#include <functional>

struct IDirect3DDevice9;

namespace directx {
// Install the EndScene hook. Idempotent; returns true once the hook is live.
bool install();

// The live device captured from the most recent EndScene, or nullptr before the
// first hooked frame. Valid only on the render thread while inside a callback.
IDirect3DDevice9 *get_device();

// The game's main render window (an HWND, returned as void* to keep this header
// free of <windows.h>). Captured once from the D3D9 device's creation parameters
// on the first hooked EndScene - the deterministic identity of the game window,
// unlike an EnumWindows heuristic. nullptr until that first frame.
void *get_focus_window();

// Register a callback invoked every EndScene (before the original), on the render
// thread, with the live device. Each call is wrapped in rcp_guard so a fault in one
// callback is contained rather than crashing the client.
void add_render_callback(std::function<void(IDirect3DDevice9 *)> callback);
}  // namespace directx
