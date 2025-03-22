#include <iostream>
#include <vector>
#include <Windows.h>
#include <winternl.h>

// Define necessary structures
typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG NumberOfHandles;
    struct {
        HANDLE HandleValue;
        ULONG ObjectTypeIndex;
        UCHAR HandleAttributes;
        UCHAR Flags;
        USHORT GrantedAccess;
        ULONG ProcessId;
        PVOID Object;
    } Handles[1]; // This is a variable-size array
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

// Define NtQuerySystemInformation function
typedef NTSTATUS(NTAPI* pNtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

int main() {
    // Load ntdll.dll and get NtQuerySystemInformation address
    HMODULE ntdll = GetModuleHandle(L"ntdll.dll");
    if (!ntdll) {
        std::cerr << "Failed to load ntdll.dll" << std::endl;
        return 1;
    }
    pNtQuerySystemInformation NtQuerySystemInformation = (pNtQuerySystemInformation)GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (!NtQuerySystemInformation) {
        std::cerr << "Failed to get NtQuerySystemInformation address" << std::endl;
        return 1;
    }

    // Get current process ID
    DWORD currentProcessId = GetCurrentProcessId();

    // Allocate initial buffer for system handle information
    ULONG bufferSize = 0x10000;
    PSYSTEM_HANDLE_INFORMATION handleInfo = (PSYSTEM_HANDLE_INFORMATION)malloc(bufferSize);
    if (!handleInfo) {
        std::cerr << "Failed to allocate initial buffer" << std::endl;
        return 1;
    }

    NTSTATUS status;
    ULONG returnLength;

    // Call NtQuerySystemInformation until buffer is large enough
    while ((status = NtQuerySystemInformation(SystemHandleInformation, handleInfo, bufferSize, &returnLength)) == STATUS_INFO_LENGTH_MISMATCH) {
        free(handleInfo);
        bufferSize *= 2;
        handleInfo = (PSYSTEM_HANDLE_INFORMATION)malloc(bufferSize);
        if (!handleInfo) {
            std::cerr << "Failed to reallocate buffer" << std::endl;
            return 1;
        }
    }

    if (!NT_SUCCESS(status)) {
        std::cerr << "NtQuerySystemInformation failed with status: " << std::hex << status << std::endl;
        free(handleInfo);
        return 1;
    }

    // Iterate through handles and filter for current process and file handles
    for (ULONG i = 0; i < handleInfo->NumberOfHandles; ++i) {
        if (handleInfo->Handles[i].ProcessId == currentProcessId) {
            // Check object type index for file handles
            // Note: Object type index values can change between Windows versions
            // This example assumes the value for file handles is 0x28 (adjust if needed)
            if (handleInfo->Handles[i].ObjectTypeIndex == 0x28) {
                HANDLE fileHandle = handleInfo->Handles[i].HandleValue;
                std::cout << "File Handle: " << fileHandle << std::endl;
                // Use the file handle as needed
                // Be sure to close it with CloseHandle when done if necessary
            }
        }
    }

    free(handleInfo);
    return 0;
}