#include "hooks.h"

namespace hooks {

void* swap_vtable_entry(void** vtable, int index, void* new_func) {
    if (!vtable) return nullptr;

    void** slot = &vtable[index];
    DWORD  old_prot = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_prot))
        return nullptr;

    void* prev = *slot;
    *slot = new_func;

    VirtualProtect(slot, sizeof(void*), old_prot, &old_prot);
    return prev;
}

}  // namespace hooks
