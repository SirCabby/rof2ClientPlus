// rof2ClientPlus - minimal hooking helpers.
//
// The baseline only needs a virtual-table swap (used for the DirectX EndScene
// hook). This is intentionally tiny; Zeal-style inline detours (replace-call /
// entry-point trampolines) can be added here later when feature work needs them.
#pragma once
#include <windows.h>

namespace hooks {
// Overwrite one function pointer in a vtable (an array of void* at `vtable`).
// Handles read-only vtable pages via VirtualProtect. Returns the previous
// pointer (to call the original), or nullptr on failure.
void* swap_vtable_entry(void** vtable, int index, void* new_func);
}
