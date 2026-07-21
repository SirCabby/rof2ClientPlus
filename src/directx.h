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

// Register a callback invoked every BeginScene (before the original), i.e. AFTER
// the game sim wrote entity state for this frame but BEFORE anything is drawn.
// This is the seam for state that must be in place when the world renders (e.g.
// the boat-draft z re-assert - the sim re-floats boats every tick, so an
// EndScene write only ever shows for the tail of a frame). Same guard rules as
// add_render_callback.
void add_prerender_callback(std::function<void(IDirect3DDevice9 *)> callback);

// Register a callback invoked immediately BEFORE the client calls
// IDirect3DDevice9::Reset (window resize / display-mode change). Reset FAILS
// (D3DERR_INVALIDCALL - the client's "reset device failed" fatal) while any
// D3DPOOL_DEFAULT resource is still alive, so every feature holding one must
// release it here; recreate lazily on the next draw (managed-pool resources
// survive Reset untouched). Same guard rules as add_render_callback.
void add_reset_callback(std::function<void(IDirect3DDevice9 *)> callback);
}  // namespace directx
