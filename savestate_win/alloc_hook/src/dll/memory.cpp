#include "memory.h"
#include "hook.h" // to call hook::original_VirtualFree when committing frees
#include <windows.h>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <cstdint>
#include <cstdio>

namespace memory {
    // Notes: the real/original VirtualFree pointer is owned by the hook module
    // (hook::original_VirtualFree). Memory subsystem does not store its own copy.

    // Track currently allocated memory and their info
    static std::unordered_map<void*, AllocationInfo> tracked_allocations;
    // Saved snapshot across save_state() calls; persists even if allocation is freed
    static std::unordered_map<void*, AllocationInfo> saved_allocations;
    // Locks protecting the maps: use shared_mutex to allow concurrent readers
    static std::shared_mutex tracking_mutex;
    static const size_t PAGE_SIZE = 4096;

    bool initialize() {
        // No special initialization required here; the hook module manages
        // the original function pointers.
        return true;
    }

    void cleanup() {
        std::unique_lock<std::shared_mutex> lock(tracking_mutex);
        // Physically free any tracked allocations
        for (const auto& pair : tracked_allocations) {
            if (hook::original_VirtualFree) hook::original_VirtualFree(pair.first, 0, MEM_RELEASE);
        }
        tracked_allocations.clear();
        // We do not attempt to free saved snapshots here since they may refer to
        // already freed memory; just clear metadata.
        saved_allocations.clear();
    }

    void* allocate(size_t size) {
        // Round up to page size
        size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        // Allocate memory with write tracking. Use the original VirtualAlloc
        // (provided by the hook module) to avoid re-entering our hook.
        void* ptr = nullptr;
        if (hook::original_VirtualAlloc) {
            ptr = hook::original_VirtualAlloc(nullptr, aligned_size, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
        } else {
            ptr = ::VirtualAlloc(nullptr, aligned_size, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
        }

        if (ptr) {
            std::unique_lock<std::shared_mutex> lock(tracking_mutex);
            AllocationInfo info;
            info.address = ptr;
            info.size = aligned_size;
            info.pending_free = false;
            info.physically_freed = false;
            tracked_allocations[ptr] = std::move(info);
        }

        return ptr;
    }

    void logical_free(void* ptr) {
        if (!ptr) return;

        std::unique_lock<std::shared_mutex> lock(tracking_mutex);
        auto it = tracked_allocations.find(ptr);
        if (it != tracked_allocations.end()) {
            // Move allocation metadata into saved_allocations so we can restore it
            // but if a saved snapshot already exists, merge/keep the saved pages.
            AllocationInfo info = std::move(it->second);
            info.pending_free = true;
            info.physically_freed = false;

            auto sit = saved_allocations.find(ptr);
            if (sit != saved_allocations.end()) {
                std::fprintf(stderr, "logical_free: merging into existing saved snapshot for %p\n", ptr);
                // Merge pages: prefer existing saved pages (they came from previous save_state)
                for (auto &p : info.pages) {
                    if (sit->second.pages.find(p.first) == sit->second.pages.end()) {
                        sit->second.pages[p.first] = std::move(p.second);
                    }
                }
                sit->second.pending_free = true;
                sit->second.physically_freed = false;
            } else {
                std::fprintf(stderr, "logical_free: creating saved snapshot for %p\n", ptr);
                saved_allocations[ptr] = std::move(info);
            }
            tracked_allocations.erase(it);
        } else {
            // If the allocation isn't tracked but we don't have a saved snapshot,
            // create a minimal saved entry so restore can reallocate (size unknown)
            // In this implementation we ignore unknown frees.
        }
        // Do NOT call the real VirtualFree yet; it's deferred until save_state()
    }

    // Helper: query GetWriteWatch for an allocation and return the list of modified addresses
    static bool query_write_watch_all(void* base, size_t size, std::vector<void*>& out_addresses, ULONG* out_page_size) {
        // GetWriteWatch may return ERROR_INSUFFICIENT_BUFFER and tell us the
        // required count. We'll loop until we obtain all addresses.
        size_t buf = 64;
        while (true) {
            out_addresses.resize(buf);
            ULONG_PTR count = static_cast<ULONG_PTR>(buf);
            ULONG pageSize = 0;
            DWORD res = GetWriteWatch(WRITE_WATCH_FLAG_RESET, base, size, out_addresses.data(), &count, &pageSize);
            if (res == ERROR_INSUFFICIENT_BUFFER) {
                // 'count' contains required number of addresses
                buf = static_cast<size_t>(count);
                continue;
            }

            if (res != 0) {
                // Error
                return false;
            }

            // Resize to actual count
            out_addresses.resize(static_cast<size_t>(count));
            if (out_page_size) *out_page_size = pageSize;
            return true;
        }
    }

    bool save_state() {
        std::unique_lock<std::shared_mutex> lock(tracking_mutex);

        for (auto& pair : tracked_allocations) {
            AllocationInfo& info = pair.second;

            std::vector<void*> addresses;
            ULONG pageSize = 0;

            if (!query_write_watch_all(info.address, info.size, addresses, &pageSize)) {
                // Query failed; continue to next allocation
                continue;
            }

            if (addresses.empty()) {
                // Nothing dirty for this allocation
                continue;
            }

            // Ensure saved_allocations has an entry for this allocation
            AllocationInfo& saved = saved_allocations[info.address];
            saved.address = info.address;
            saved.size = info.size;
            // preserve any existing pending_free/physically_freed flags

            // For each dirty page returned, save the page contents
            for (void* addr : addresses) {
                size_t page_offset = static_cast<size_t>(reinterpret_cast<uint8_t*>(addr) - reinterpret_cast<uint8_t*>(info.address));
                size_t page_index = page_offset / PAGE_SIZE;
                size_t copy_size = PAGE_SIZE;
                if (page_offset + copy_size > info.size) copy_size = info.size - page_offset;

                std::vector<BYTE>& buf = saved.pages[page_index];
                buf.resize(copy_size);
                memcpy(buf.data(), reinterpret_cast<uint8_t*>(info.address) + page_index * PAGE_SIZE, copy_size);
            }
        }

        // Commit any pending frees: if an allocation was logically freed earlier,
        // now call the real VirtualFree and mark it physically freed.
        for (auto& pair : saved_allocations) {
            AllocationInfo& saved = pair.second;
            if (saved.pending_free && !saved.physically_freed) {
                if (hook::original_VirtualFree) {
                    hook::original_VirtualFree(saved.address, 0, MEM_RELEASE);
                    saved.physically_freed = true;
                }
            }
        }

        return true;
    }

    bool restore_state() {
        std::unique_lock<std::shared_mutex> lock(tracking_mutex);
        std::printf("restore_state: saved_allocations count=%zu\n", saved_allocations.size());
        for (const auto &p : saved_allocations) {
            std::printf("  saved: addr=%p size=%zu pages=%zu pending=%d phys=%d tracked=%d\n",
                p.first, p.second.size, p.second.pages.size(), (int)p.second.pending_free, (int)p.second.physically_freed,
                (int)(tracked_allocations.find(p.first) != tracked_allocations.end()));
        }

        // For each saved allocation snapshot, restore the saved pages to the current allocation.
        for (auto it = saved_allocations.begin(); it != saved_allocations.end(); ) {
            AllocationInfo& saved = it->second;
            void* base = saved.address;
            size_t asize = saved.size;

            // Check if current allocation exists
            auto cur_it = tracked_allocations.find(base);
            void* dest = nullptr;
            if (cur_it == tracked_allocations.end()) {
                // Strict behavior: attempt to reallocate at the same address only.
                // Try to allocate at the same address using the original VirtualAlloc
                if (hook::original_VirtualAlloc) {
                    dest = hook::original_VirtualAlloc(base, asize, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
                } else {
                    dest = ::VirtualAlloc(base, asize, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
                }
                if (!dest || dest != base) {
                    // Strict failure: abort restore
                    return false;
                }

                AllocationInfo newinfo;
                newinfo.address = dest;
                newinfo.size = asize;
                newinfo.pending_free = false;
                newinfo.physically_freed = false;
                tracked_allocations[dest] = std::move(newinfo);
            } else {
                dest = cur_it->first;
            }

            // Restore each saved page
            for (const auto& page_pair : saved.pages) {
                size_t page_index = page_pair.first;
                const std::vector<BYTE>& buf = page_pair.second;

                uint8_t* dst_ptr = reinterpret_cast<uint8_t*>(dest) + page_index * PAGE_SIZE;
                DWORD oldProtect = 0;
                VirtualProtect(dst_ptr, buf.size(), PAGE_READWRITE, &oldProtect);
                memcpy(dst_ptr, buf.data(), buf.size());
                VirtualProtect(dst_ptr, buf.size(), oldProtect, &oldProtect);
            }

            // Erase saved snapshot after restore
            it = saved_allocations.erase(it);
        }

        return true;
    }
}