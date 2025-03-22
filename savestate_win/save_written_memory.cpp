#include <windows.h>
#include <iostream>
#include <vector>

// IMPORTANT NOTE TO SELF PLEASE READ:
// hi me, this code is generated from chatgpt lmao.
// i just put this here since this gets the general idea of this program across.
// rewrite this yourself when you go to actually do this, as things are obviously incorrect here 

void SaveAndRestoreMemoryPages(HANDLE processHandle) {
    // Step 1: Query all memory regions in the target process using VirtualQueryEx
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    MEMORY_BASIC_INFORMATION mbi;
    std::vector<LPVOID> modifiedPages;
    std::vector<BYTE*> savedData;
    
    // Query memory pages
    for (LPVOID addr = 0; addr < sysInfo.lpMaximumApplicationAddress; addr = (LPBYTE)addr + mbi.RegionSize) {
        if (VirtualQueryEx(processHandle, addr, &mbi, sizeof(mbi)) == 0) {
            continue;
        }

        // Only track writable pages
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY))) {
            // Use GetWriteWatch to track pages that have been written to
            PVOID pageBaseAddr = mbi.BaseAddress;
            SIZE_T pageSize = mbi.RegionSize;
            
            // Attempt to get the written pages for the region
            DWORD* pageArray = new DWORD[pageSize / 4096];  // Assumes 4KB pages
            DWORD pageCount = 0;
            
            if (GetWriteWatch(processHandle, GWW_FILTER_WRITES, pageBaseAddr, pageSize, pageArray, &pageCount)) {
                for (DWORD i = 0; i < pageCount; ++i) {
                    // Save the modified pages and their data
                    modifiedPages.push_back(pageBaseAddr);
                    BYTE* pageData = new BYTE[pageSize];
                    if (ReadProcessMemory(processHandle, pageBaseAddr, pageData, pageSize, NULL)) {
                        savedData.push_back(pageData);
                    } else {
                        delete[] pageData;
                    }
                }
            }
            delete[] pageArray;
        }
    }

    // Step 2: Modify or Save pages (for demonstration, let's print the page addresses and saved data)
    std::cout << "Pages that were modified and saved:" << std::endl;
    for (size_t i = 0; i < modifiedPages.size(); ++i) {
        std::cout << "Page at address: " << modifiedPages[i] << std::endl;
    }

    // Step 3: Restore the saved data (write the saved data back to the memory)
    for (size_t i = 0; i < modifiedPages.size(); ++i) {
        if (!WriteProcessMemory(processHandle, modifiedPages[i], savedData[i], mbi.RegionSize, NULL)) {
            std::cerr << "Failed to restore page at address: " << modifiedPages[i] << std::endl;
        }
        delete[] savedData[i];  // Clean up saved data after restoring
    }
}

int main() {
    // Replace this with the actual process handle of the target process
    DWORD processId = 1234; // Example process ID (replace with an actual PID)
    HANDLE processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, processId);

    if (processHandle == NULL) {
        std::cerr << "Failed to open process: " << GetLastError() << std::endl;
        return 1;
    }

    SaveAndRestoreMemoryPages(processHandle);

    // Clean up and close the process handle
    CloseHandle(processHandle);

    return 0;
}
