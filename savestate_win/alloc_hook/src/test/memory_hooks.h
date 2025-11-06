// Public helper for the test harness to load the DLL and obtain exported hooks.
#pragma once

#include <windows.h>

// Function pointer types for the DLL exports.
using SaveFn = bool(*)();
using RestoreFn = bool(*)();

// Loads the DLL named by dllName, looks up the "save_state" and "restore_state"
// exports and returns them via outSave/outRestore. If outModule is provided,
// the loaded HMODULE is returned (the caller is responsible for FreeLibrary).
// Returns true on success and leaves the DLL loaded.
inline bool InitializeMemoryHooks(const wchar_t* dllName, SaveFn* outSave, RestoreFn* outRestore, HMODULE* outModule = nullptr) {
    if (!dllName || !outSave || !outRestore) return false;
    HMODULE h = LoadLibraryW(dllName);
    if (!h) return false;
    SaveFn s = reinterpret_cast<SaveFn>(GetProcAddress(h, "save_state"));
    RestoreFn r = reinterpret_cast<RestoreFn>(GetProcAddress(h, "restore_state"));
    if (!s || !r) {
        // cleanup on failure
        FreeLibrary(h);
        return false;
    }
    *outSave = s;
    *outRestore = r;
    if (outModule) *outModule = h;
    return true;
}
