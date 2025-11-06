#include <windows.h>
#include "hook.h"
#include "memory.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // Initialize memory subsystem first
            if (!memory::initialize()) {
                return FALSE;
            }
            // Then initialize hooks
            if (!hook::initialize()) {
                memory::cleanup();
                return FALSE;
            }
            break;

        case DLL_PROCESS_DETACH:
            // Cleanup in reverse order
            hook::cleanup();
            memory::cleanup();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}