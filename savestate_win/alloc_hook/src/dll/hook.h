#pragma once
#include <windows.h>

namespace hook {
    // Function type definitions
    using VirtualAllocFn = LPVOID(WINAPI*)(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
    using VirtualFreeFn = BOOL(WINAPI*)(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

    // Initialize hooking
    bool initialize();

    // Cleanup hooking
    void cleanup();

    // Original function pointers
    extern VirtualAllocFn original_VirtualAlloc;
    extern VirtualFreeFn original_VirtualFree;

    // Our hooked functions
    LPVOID WINAPI hooked_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
    BOOL WINAPI hooked_VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

    // Exported functions for save state functionality
    extern "C" {
        __declspec(dllexport) bool save_state();
        __declspec(dllexport) bool restore_state();
    }
}