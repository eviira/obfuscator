#include <windows.h>
#include <tchar.h>
#include <stdexcept>
#include <iostream>

HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEXA* psi)
{
    // Prepare Startup Information structure
    STARTUPINFOEXA si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXA);

    // Discover the size required for the list
    SIZE_T bytesRequired;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

    // Allocate memory to represent the list
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList)
    {
        return E_OUTOFMEMORY;
    }

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired))
    {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Set the pseudoconsole information into the list
    auto result = UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL);
    if (!result) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;

    return S_OK;
}

class PTY {
    HPCON pseudoConsoleHandle;

    // Close these after CreateProcess of child application with pseudoconsole object
    HANDLE inputReadSide, outputWriteSide;

public:
    // communication channels
    HANDLE outputReadSide, inputWriteSide;

	PTY();
    ~PTY();
    
    void close();
    HRESULT createPseudoConsole();
};

PTY::PTY() {

}

PTY::~PTY() {
    ClosePseudoConsole(pseudoConsoleHandle);

    if (inputReadSide != nullptr)
        CloseHandle(inputReadSide);
    if (outputWriteSide != nullptr)
        CloseHandle(outputWriteSide);

    if (outputReadSide != nullptr)
        CloseHandle(outputReadSide);
    if (inputWriteSide != nullptr)
        CloseHandle(inputWriteSide);
    
}

void PTY::close() {
    ClosePseudoConsole(pseudoConsoleHandle);
}

// HRESULT PTY::Setup() {
//     HRESULT result;

//     result = createPseudoConsole();
//     if (FAILED(result)) {
//         std::cerr << "hresult: " << result << "\n";
//         throw std::exception("creating pseudoconsole object");
//     }


// }

HRESULT PTY::createPseudoConsole() {
	COORD size;
	size.X = 200;
	size.Y = 100;

    HANDLE inputReadSide, outputWriteSide;
    if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HRESULT result;
    result = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, &pseudoConsoleHandle);
    if FAILED(result) {
        return result;
    }

    STARTUPINFOEXA startupInfo;
    result = PrepareStartupInformation(pseudoConsoleHandle, &startupInfo);
    if (FAILED(result)) {
        return result;
    }

    // PCWSTR childApplication = L"powershell"; // L"C:\\windows\\system32\\cmd.exe";

    // // Create mutable text string for CreateProcessW command line string.
    // const size_t charsRequired = wcslen(childApplication) + 1; // +1 null terminator
    // PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);
    // if (!cmdLineMutable)
    // {
    //     return E_OUTOFMEMORY;
    // }

    // wcscpy_s(cmdLineMutable, charsRequired, childApplication);

    PROCESS_INFORMATION processInformation;
    ZeroMemory(&processInformation, sizeof(processInformation));

    // Call CreateProcess
    if (!CreateProcessA("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                        NULL,
                        NULL,
                        NULL,
                        FALSE,
                        EXTENDED_STARTUPINFO_PRESENT,
                        NULL,
                        NULL,
                        &startupInfo.StartupInfo,
                        &processInformation))
    {
        std::cerr << "error is from here" << "\n";
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}
