// rof2ClientPlus - Direct3D9 render-loop hook.
//
// Creates a throwaway D3D9 device to read the (shared) IDirect3DDevice9 vtable,
// then swaps the EndScene entry. Because every D3D9 device from the same
// d3d9.dll (DXVK here) shares one vtable, the game's device then calls our
// EndScene. Our hook draws a small marker each frame and calls the original.
#pragma once

namespace directx {
// Install the EndScene hook. Idempotent; returns true once the hook is live.
bool install();
}
