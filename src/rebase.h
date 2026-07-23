#pragma once
#include <windows.h>

#include <cstdint>

namespace Rcp {

// eqgame.exe ("May 10 2013") is linked with DYNAMICBASE and keeps its .reloc section, so
// on native Windows ASLR loads it away from the 0x400000 preferred base (typically a new
// base every boot). Wine maps the image at its preferred base, which masked this for a
// long time. Every absolute eqgame VA in this repo is written at PREFERRED base and must
// pass through eqva() at use/definition time; the delta is 0 under Wine, so behavior
// there is byte-for-byte unchanged.
//
// EQGraphicsDX9.dll is likewise ASLR-enabled; its addresses are NOT handled here because
// the DLL loads after our attach — resolve them at use time against
// GetModuleHandleA("EQGraphicsDX9.dll") with a null guard (see font_overlay.cpp).
inline uintptr_t eqgame_delta() {
  // Magic-static: resolved on first call, safe from any static-init ordering.
  static const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) - 0x400000u;
  return delta;
}

inline uintptr_t eqva(uintptr_t preferred_va) { return preferred_va + eqgame_delta(); }

}  // namespace Rcp
