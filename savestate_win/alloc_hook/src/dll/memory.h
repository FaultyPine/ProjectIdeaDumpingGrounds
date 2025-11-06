#pragma once
#include <windows.h>
#include <cstddef>
#include <vector>
#include <unordered_map>

namespace memory {
    // Represents a single allocation which may span multiple pages.
    struct AllocationInfo {
        void* address;
        size_t size;
        // Map of page index -> saved page bytes
        std::unordered_map<size_t, std::vector<BYTE>> pages;
        // If the allocation was logically freed via VirtualFree and not yet
        // physically released. When true, the allocation's memory may still
        // exist until save_state commits the free.
        bool pending_free = false;
        // True if the allocation has been physically freed (original VirtualFree called)
        bool physically_freed = false;
    };

    // Initialize the memory system
    bool initialize();

    // Cleanup memory system
    void cleanup();

    // Allocate memory
    void* allocate(size_t size);

    // Mark an allocation as freed by the process. This will not immediately
    // call the real VirtualFree — the free is deferred until save_state() is
    // called. Use this from the hooked VirtualFree.
    void logical_free(void* ptr);

    // Save the current state of all tracked memory (save dirty pages into saved snapshot)
    bool save_state();

    // Restore memory from the last saved snapshot
    bool restore_state();

    // Get original VirtualFree function
    using VirtualFreeFn = BOOL(WINAPI*)(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
    extern VirtualFreeFn original_VirtualFree;
}