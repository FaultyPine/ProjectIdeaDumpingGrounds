#include <windows.h>
#include <cstdint>
#include <optional>
#include <vector>

// parsing executable's import table by hand

struct ExecutableImportTable
{
    HANDLE mappedExecutableHdl;
    void* mappedExecutable;
    struct FunctionImport
    {
        uint64_t rva;
        char* name; 
    };
    std::vector<FunctionImport> functions = {};
};

std::optional<ExecutableImportTable> ParseImportTable(const char* executablePath)
{
    std::optional<ExecutableImportTable> result = {};
    if (!executablePath)
        return {};

    //read the file
    auto hFile = CreateFileA(executablePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return {};

    //map the file
    auto hMappedFile = CreateFileMappingA(hFile, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    if (!hMappedFile)
        return {};

    //map the sections appropriately
    auto fileMap = MapViewOfFile(hMappedFile, FILE_MAP_READ, 0, 0, 0);
    if (!fileMap)
        return {};

    result->mappedExecutable = fileMap;
    result->mappedExecutableHdl = hMappedFile;

    auto pidh = PIMAGE_DOS_HEADER(fileMap);
    if (pidh->e_magic != IMAGE_DOS_SIGNATURE)
        return {};

    auto pnth = PIMAGE_NT_HEADERS(ULONG_PTR(fileMap) + pidh->e_lfanew);
    if (pnth->Signature != IMAGE_NT_SIGNATURE)
        return {};

    //if (pnth->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        //return gtfo("IMAGE_FILE_MACHINE_I386");

    if (pnth->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
        return {};

    auto importDir = pnth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    puts("Import Directory");
    printf(" RVA: %08X\n", importDir.VirtualAddress);
    printf("Size: %08X\n\n", importDir.Size);

    if (!importDir.VirtualAddress || !importDir.Size)
        return {};

    auto importDescriptor = PIMAGE_IMPORT_DESCRIPTOR(ULONG_PTR(fileMap) + importDir.VirtualAddress);
    if (!IsBadReadPtr((char*)fileMap + importDir.VirtualAddress, 0x1000))
    {
        for (; importDescriptor->FirstThunk; importDescriptor++)
        {
            printf("OriginalFirstThunk: %08X\n", importDescriptor->OriginalFirstThunk);
            printf("     TimeDateStamp: %08X\n", importDescriptor->TimeDateStamp);
            printf("    ForwarderChain: %08X\n", importDescriptor->ForwarderChain);
            if (!IsBadReadPtr((char*)fileMap + importDescriptor->Name, 0x1000))
                printf("              Name: %08X \"%s\"\n", importDescriptor->Name, (char*)fileMap + importDescriptor->Name);
            else
                printf("              Name: %08X INVALID\n", importDescriptor->Name);
            printf("              Name: %08X\n", importDescriptor->Name);
            printf("        FirstThunk: %08X\n", importDescriptor->FirstThunk);

            auto thunkData = PIMAGE_THUNK_DATA(ULONG_PTR(fileMap) + importDescriptor->FirstThunk);
            for (; thunkData->u1.AddressOfData; thunkData++)
            {
                auto rva = ULONG_PTR(thunkData) - ULONG_PTR(fileMap);

                auto data = thunkData->u1.AddressOfData;
                if (data & IMAGE_ORDINAL_FLAG)
                    printf("              Ordinal: %08X\n", data & ~IMAGE_ORDINAL_FLAG);
                else
                {
                    auto importByName = PIMAGE_IMPORT_BY_NAME(ULONG_PTR(fileMap) + data);
                    if (!IsBadReadPtr(importByName, 0x1000))
                    {
                        printf("             Function: %08X \"%s\"\n", data, (char*)importByName->Name);
                        ExecutableImportTable::FunctionImport func;
                        func.name = importByName->Name;
                        func.rva = data;
                        result->functions.push_back(func);
                    }
                    else
                        printf("             Function: %08X INVALID\n", data);
                }
            }

            puts("");
        }
    }
    else
    {
        puts("INVALID IMPORT DESCRIPTOR");
    }

    return result;
}



/*
>main.exe main.exe
Import Directory
 RVA: 00021224
Size: 00000028

OriginalFirstThunk: 00021250
     TimeDateStamp: 00000000
    ForwarderChain: 00000000
              Name: 00021664 "KERNEL32.dll"
              Name: 00021664
        FirstThunk: 00017000
             Function: 000214D8 "CreateFileA"
             Function: 000214E6 "CreateFileMappingA"
             Function: 000214FC "MapViewOfFile"
             Function: 0002150C "IsBadReadPtr"
             Function: 0002151C "QueryPerformanceCounter"
             Function: 00021536 "GetCurrentProcessId"
             Function: 0002154C "GetCurrentThreadId"
             Function: 00021562 "GetSystemTimeAsFileTime"
             Function: 0002157C "InitializeSListHead"
             Function: 00021592 "RtlCaptureContext"
             Function: 000215A6 "RtlLookupFunctionEntry"
             Function: 000215C0 "RtlVirtualUnwind"
             Function: 000215D4 "IsDebuggerPresent"
             Function: 000215E8 "UnhandledExceptionFilter"
             Function: 00021604 "SetUnhandledExceptionFilter"
             Function: 00021622 "GetStartupInfoW"
             Function: 00021634 "IsProcessorFeaturePresent"
             Function: 00021650 "GetModuleHandleW"
             Function: 00021A9E "WriteConsoleW"
             Function: 00021672 "RtlUnwindEx"
             Function: 00021680 "GetLastError"
             Function: 00021690 "SetLastError"
             Function: 000216A0 "EnterCriticalSection"
             Function: 000216B8 "LeaveCriticalSection"
             Function: 000216D0 "DeleteCriticalSection"
             Function: 000216E8 "InitializeCriticalSectionAndSpinCount"
             Function: 00021710 "TlsAlloc"
             Function: 0002171C "TlsGetValue"
             Function: 0002172A "TlsSetValue"
             Function: 00021738 "TlsFree"
             Function: 00021742 "FreeLibrary"
             Function: 00021750 "GetProcAddress"
             Function: 00021762 "LoadLibraryExW"
             Function: 00021774 "EncodePointer"
             Function: 00021784 "RaiseException"
             Function: 00021796 "RtlPcToFileHeader"
             Function: 000217AA "GetStdHandle"
             Function: 000217BA "WriteFile"
             Function: 000217C6 "GetModuleFileNameW"
             Function: 000217DC "GetCurrentProcess"
             Function: 000217F0 "ExitProcess"
             Function: 000217FE "TerminateProcess"
             Function: 00021812 "GetModuleHandleExW"
             Function: 00021828 "GetCommandLineA"
             Function: 0002183A "GetCommandLineW"
             Function: 0002184C "HeapAlloc"
             Function: 00021858 "HeapFree"
             Function: 00021864 "FlsAlloc"
             Function: 00021870 "FlsGetValue"
             Function: 0002187E "FlsSetValue"
             Function: 0002188C "FlsFree"
             Function: 00021896 "InitializeCriticalSectionEx"
             Function: 000218B4 "VirtualProtect"
             Function: 000218C6 "CompareStringW"
             Function: 000218D8 "LCMapStringW"
             Function: 000218E8 "GetFileType"
             Function: 000218F6 "FindClose"
             Function: 00021902 "FindFirstFileExW"
             Function: 00021916 "FindNextFileW"
             Function: 00021926 "IsValidCodePage"
             Function: 00021938 "GetACP"
             Function: 00021942 "GetOEMCP"
             Function: 0002194E "GetCPInfo"
             Function: 0002195A "MultiByteToWideChar"
             Function: 00021970 "WideCharToMultiByte"
             Function: 00021986 "GetEnvironmentStringsW"
             Function: 000219A0 "FreeEnvironmentStringsW"
             Function: 000219BA "SetEnvironmentVariableW"
             Function: 000219D4 "SetStdHandle"
             Function: 000219E4 "GetStringTypeW"
             Function: 000219F6 "GetProcessHeap"
             Function: 00021A08 "FlushFileBuffers"
             Function: 000219F6 "GetProcessHeap"
             Function: 00021A08 "FlushFileBuffers"
             Function: 000219F6 "GetProcessHeap"
             Function: 00021A08 "FlushFileBuffers"
             Function: 00021A1C "GetConsoleOutputCP"
             Function: 00021A32 "GetConsoleMode"
             Function: 000219F6 "GetProcessHeap"
             Function: 00021A08 "FlushFileBuffers"
             Function: 00021A1C "GetConsoleOutputCP"
             Function: 00021A32 "GetConsoleMode"
             Function: 00021A08 "FlushFileBuffers"
             Function: 00021A1C "GetConsoleOutputCP"
             Function: 00021A1C "GetConsoleOutputCP"
             Function: 00021A32 "GetConsoleMode"
             Function: 00021A44 "GetFileSizeEx"
             Function: 00021A44 "GetFileSizeEx"
             Function: 00021A54 "SetFilePointerEx"
             Function: 00021A54 "SetFilePointerEx"
             Function: 00021A68 "HeapSize"
             Function: 00021A68 "HeapSize"
             Function: 00021A74 "HeapReAlloc"
             Function: 00021A82 "CloseHandle"
             Function: 00021A90 "CreateFileW"
             Function: 00021A90 "CreateFileW"
*/