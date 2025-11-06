#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include "memory_hooks.h"

static const size_t PAGE = 4096;

bool check_pattern(void* p, size_t size, unsigned char v) {
    unsigned char* b = reinterpret_cast<unsigned char*>(p);
    for (size_t i = 0; i < size; ++i) if (b[i] != v) return false;
    return true;
}

bool test_basic(SaveFn save_state, RestoreFn restore_state) {
    void* p = VirtualAlloc(nullptr, PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { std::wcerr << L"basic: alloc failed\n"; return false; }
    memset(p, 0xAA, PAGE);
    if (!save_state()) { std::wcerr << L"basic: save failed\n"; return false; }
    memset(p, 0xBB, PAGE);
    if (!restore_state()) { std::wcerr << L"basic: restore failed\n"; return false; }
    bool ok = check_pattern(p, PAGE, 0xAA);
    VirtualFree(p, 0, MEM_RELEASE);
    std::wcout << L"Restore memory check: " << (ok ? L"PASS" : L"FAIL") << L"\n";
    return ok;
}

bool test_free_restore(SaveFn save_state, RestoreFn restore_state) {
    void* q = VirtualAlloc(nullptr, PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!q) { std::wcerr << L"free: alloc q failed\n"; return false; }
    memset(q, 0x11, PAGE);
    if (!save_state()) { std::wcerr << L"free: save_state failed\n"; return false; }
    // Call VirtualFree (hook will defer)
    if (!VirtualFree(q, 0, MEM_RELEASE)) { std::wcerr << L"free: VirtualFree failed: " << GetLastError() << L"\n"; }
    // Commit frees
    if (!save_state()) { std::wcerr << L"free: save_state commit failed\n"; return false; }
    // Now restore
    if (!restore_state()) { std::wcerr << L"free: restore failed\n"; return false; }
    bool ok2 = check_pattern(q, PAGE, 0x11);
    std::wcout << L"Free+Restore memory check: " << (ok2 ? L"PASS" : L"FAIL") << L"\n";
    if (!ok2) return false;
    // Finally physically free if still allocated
    VirtualFree(q, 0, MEM_RELEASE);
    return true;
}

bool test_multipage(SaveFn save_state, RestoreFn restore_state) {
    const size_t pages = 3;
    void* p = VirtualAlloc(nullptr, PAGE * pages, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { std::wcerr << L"multipage: alloc failed\n"; return false; }
    // Fill all with A
    memset(p, 0xAA, PAGE * pages);
    if (!save_state()) { std::wcerr << L"multipage: save failed\n"; return false; }
    // Modify middle page
    unsigned char* mid = reinterpret_cast<unsigned char*>(p) + PAGE;
    memset(mid, 0xBB, PAGE);
    if (!restore_state()) { std::wcerr << L"multipage: restore failed\n"; return false; }
    bool ok = check_pattern(reinterpret_cast<void*>(mid), PAGE, 0xAA);
    VirtualFree(p, 0, MEM_RELEASE);
    std::wcout << L"Multipage restore check: " << (ok ? L"PASS" : L"FAIL") << L"\n";
    return ok;
}

bool test_multiple_allocs(SaveFn save_state, RestoreFn restore_state) {
    void* a = VirtualAlloc(nullptr, PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    void* b = VirtualAlloc(nullptr, PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!a || !b) { std::wcerr << L"multi-alloc: alloc failed\n"; return false; }
    memset(a, 0x21, PAGE);
    memset(b, 0x22, PAGE);
    if (!save_state()) { std::wcerr << L"multi-alloc: save failed\n"; return false; }
    memset(a, 0x33, PAGE);
    if (!restore_state()) { std::wcerr << L"multi-alloc: restore failed\n"; return false; }
    bool ok = check_pattern(a, PAGE, 0x21) && check_pattern(b, PAGE, 0x22);
    VirtualFree(a, 0, MEM_RELEASE);
    VirtualFree(b, 0, MEM_RELEASE);
    std::wcout << L"Multiple allocations restore check: " << (ok ? L"PASS" : L"FAIL") << L"\n";
    return ok;
}

bool test_concurrent_writes(SaveFn save_state, RestoreFn restore_state) {
    const size_t pages = 4;
    void* p = VirtualAlloc(nullptr, PAGE * pages, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { std::wcerr << L"concurrent: alloc failed\n"; return false; }
    memset(p, 0x44, PAGE * pages);
    if (!save_state()) { std::wcerr << L"concurrent: save failed\n"; return false; }

    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) std::this_thread::yield();
            // each thread writes to one page
            unsigned char* target = reinterpret_cast<unsigned char*>(p) + (t % pages) * PAGE;
            memset(target, 0x55 + t, PAGE);
        });
    }
    start = true;
    for (auto &th : threads) th.join();

    if (!restore_state()) { std::wcerr << L"concurrent: restore failed\n"; return false; }
    bool ok = check_pattern(p, PAGE * pages, 0x44);
    VirtualFree(p, 0, MEM_RELEASE);
    std::wcout << L"Concurrent writes restore check: " << (ok ? L"PASS" : L"FAIL") << L"\n";
    return ok;
}

int wmain(int argc, wchar_t** argv) {
    const wchar_t* dllName = L"alloc_hook_dll.dll";
    if (argc > 1) dllName = argv[1];

    HMODULE h = NULL;
    SaveFn save_state = nullptr;
    RestoreFn restore_state = nullptr;
    if (!InitializeMemoryHooks(dllName, &save_state, &restore_state, &h)) {
        std::wcerr << L"Failed to initialize memory hooks for DLL: " << dllName << L"\n";
        return 1;
    }

    int failures = 0;
    failures += !test_basic(save_state, restore_state);
    failures += !test_free_restore(save_state, restore_state);
    failures += !test_multipage(save_state, restore_state);
    failures += !test_multiple_allocs(save_state, restore_state);
    failures += !test_concurrent_writes(save_state, restore_state);

    FreeLibrary(h);
    if (failures == 0) {
        std::wcout << L"All tests PASS\n";
        return 0;
    } else {
        std::wcout << L"Some tests FAILED: " << failures << L"\n";
        return 2;
    }
}
