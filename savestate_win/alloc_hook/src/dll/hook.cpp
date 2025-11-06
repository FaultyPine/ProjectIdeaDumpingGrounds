#include "hook.h"
#include "memory.h"
#include <detours/detours.h>

namespace hook {
    VirtualAllocFn original_VirtualAlloc = nullptr;
    VirtualFreeFn original_VirtualFree = nullptr;

    bool initialize() {
        HMODULE kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
        if (!kernel32) return false;

        // Get the original function addresses
        original_VirtualAlloc = reinterpret_cast<VirtualAllocFn>(
            GetProcAddress(kernel32, "VirtualAlloc")
        );

        original_VirtualFree = reinterpret_cast<VirtualFreeFn>(
            GetProcAddress(kernel32, "VirtualFree")
        );

        if (!original_VirtualAlloc || !original_VirtualFree) {
            return false;
        }

        // Initialize our memory system
        if (!memory::initialize()) {
            return false;
        }

        // Begin the detours transaction
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        // Attach our hooks
        DetourAttach(&(PVOID&)original_VirtualAlloc, hooked_VirtualAlloc);
        DetourAttach(&(PVOID&)original_VirtualFree, hooked_VirtualFree);

        // Commit the transaction
        LONG error = DetourTransactionCommit();
        return error == NO_ERROR;
    }

    void cleanup() {
        if (original_VirtualAlloc || original_VirtualFree) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            if (original_VirtualAlloc) {
                DetourDetach(&(PVOID&)original_VirtualAlloc, hooked_VirtualAlloc);
            }
            if (original_VirtualFree) {
                DetourDetach(&(PVOID&)original_VirtualFree, hooked_VirtualFree);
            }
            DetourTransactionCommit();
        }
        memory::cleanup();
    }

    LPVOID WINAPI hooked_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
        // Handle the allocation through our memory system if it's a commit
        if (flAllocationType & MEM_COMMIT) {
            return memory::allocate(dwSize);
        }

        // For other allocation types, use the original VirtualAlloc
        return original_VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
    }

    BOOL WINAPI hooked_VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
        std::printf("hooked_VirtualFree called for %p freeType=0x%X\n", lpAddress, dwFreeType);
        // If this is a full release, handle it through our memory system
        if (dwFreeType & MEM_RELEASE) {
            // Mark logically freed; defer the real VirtualFree until save_state()
            memory::logical_free(lpAddress);
            return TRUE;
        }

        // For other free types (like MEM_DECOMMIT), use the original VirtualFree
        return original_VirtualFree(lpAddress, dwSize, dwFreeType);
    }

    // Export save state functionality
    extern "C" {
        __declspec(dllexport) bool save_state() {
            return memory::save_state();
        }

        __declspec(dllexport) bool restore_state() {
            return memory::restore_state();
        }
    }
}