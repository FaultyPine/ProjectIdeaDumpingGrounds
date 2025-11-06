# VirtualAlloc Hook with rpmalloc

This project implements a DLL that hooks the `VirtualAlloc` Windows API call in a target process and redirects memory allocations to use the rpmalloc memory allocator instead. It consists of two main components:

1. A DLL (`alloc_hook_dll`) that implements the hook
2. An injector executable that injects the DLL into a target process

## Requirements

- Visual Studio 2019 or later
- CMake 3.10 or later
- Microsoft Detours library
- rpmalloc library

## Building

1. Clone the repository
2. Create a build directory:
   ```
   mkdir build
   cd build
   ```
3. Configure with CMake:
   ```
   cmake ..
   ```
4. Build:
   ```
   cmake --build .
   ```

## Usage

```bash
injector.exe <process_id> <path_to_alloc_hook_dll.dll>
```

Example:
```bash
injector.exe 1234 C:\path\to\alloc_hook_dll.dll
```

## How it Works

1. The injector injects the DLL into the target process using `CreateRemoteThread` and `LoadLibraryW`
2. When the DLL is loaded, it:
   - Initializes rpmalloc
   - Sets up a hook for `VirtualAlloc` using Microsoft Detours
3. Any subsequent `VirtualAlloc` calls in the target process that use `MEM_COMMIT` are redirected to use rpmalloc instead
4. Other types of `VirtualAlloc` calls (like `MEM_RESERVE`) are passed through to the original function

## Notes

- This hook only intercepts `MEM_COMMIT` allocations, allowing other allocation types to proceed normally
- The hook is applied process-wide
- Memory allocated through the hook must be freed using the hooked functions to prevent memory leaks