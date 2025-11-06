#include <windows.h>
#include <iostream>
#include <string>

bool InjectDLL(DWORD processId, const std::wstring& dllPath) {
    // Open the target process
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        processId
    );

    if (!hProcess) {
        std::cerr << "Failed to open process. Error: " << GetLastError() << std::endl;
        return false;
    }

    // Allocate memory for the DLL path in the target process
    LPVOID pDllPath = VirtualAllocEx(
        hProcess,
        nullptr,
        (dllPath.size() + 1) * sizeof(wchar_t),
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!pDllPath) {
        std::cerr << "Failed to allocate memory in target process. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    // Write the DLL path to the allocated memory
    if (!WriteProcessMemory(
        hProcess,
        pDllPath,
        dllPath.c_str(),
        (dllPath.size() + 1) * sizeof(wchar_t),
        nullptr
    )) {
        std::cerr << "Failed to write to target process. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Get the address of LoadLibraryW
    LPTHREAD_START_ROUTINE pLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "LoadLibraryW")
    );

    if (!pLoadLibrary) {
        std::cerr << "Failed to get LoadLibraryW address. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Create a remote thread to load our DLL
    HANDLE hThread = CreateRemoteThread(
        hProcess,
        nullptr,
        0,
        pLoadLibrary,
        pDllPath,
        0,
        nullptr
    );

    if (!hThread) {
        std::cerr << "Failed to create remote thread. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Wait for the thread to complete
    WaitForSingleObject(hThread, INFINITE);

    // Cleanup
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <process_id> <dll_path>" << std::endl;
        return 1;
    }

    DWORD processId = static_cast<DWORD>(std::stoul(argv[1]));
    std::wstring dllPath(strlen(argv[2]), L' ');
    mbstowcs(&dllPath[0], argv[2], strlen(argv[2]));

    if (InjectDLL(processId, dllPath)) {
        std::cout << "DLL injected successfully!" << std::endl;
        return 0;
    } else {
        std::cout << "Failed to inject DLL." << std::endl;
        return 1;
    }
}